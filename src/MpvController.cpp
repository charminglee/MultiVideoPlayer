#include "MpvController.h"

#include <QByteArray>
#include <QList>
#include <QMetaObject>

namespace {

QVariant nodeToVariant(const mpv_node *node)
{
    switch (node->format) {
    case MPV_FORMAT_STRING:
    case MPV_FORMAT_OSD_STRING:
        return QString::fromUtf8(node->u.string);
    case MPV_FORMAT_FLAG:
        return node->u.flag != 0;
    case MPV_FORMAT_INT64:
        return qlonglong(node->u.int64);
    case MPV_FORMAT_DOUBLE:
        return node->u.double_;
    case MPV_FORMAT_NODE_ARRAY: {
        QVariantList list;
        list.reserve(node->u.list->num);
        for (int i = 0; i < node->u.list->num; ++i)
            list.append(nodeToVariant(&node->u.list->values[i]));
        return list;
    }
    case MPV_FORMAT_NODE_MAP: {
        QVariantMap map;
        for (int i = 0; i < node->u.list->num; ++i)
            map.insert(QString::fromUtf8(node->u.list->keys[i]),
                       nodeToVariant(&node->u.list->values[i]));
        return map;
    }
    default:
        return {};
    }
}

} // namespace

MpvController::MpvController(QObject *parent)
    : QObject(parent)
{
    m_mpv = mpv_create();
    if (!m_mpv) {
        qWarning("MpvController: mpv_create failed");
        return;
    }

    // 视频经 wid 嵌入 VideoWidget 的原生窗口直接渲染（用 mpv 默认 vo），
    // 不再使用 render API，也就不能强制 vo=libmpv
    // 调试/排查用：MPV_HWDEC 环境变量可覆盖硬解策略
    const QByteArray hwdec = qgetenv("MPV_HWDEC");
    mpv_set_option_string(m_mpv, "hwdec", !hwdec.isEmpty() ? hwdec.constData() : "auto-safe");
    // wid 嵌入模式：禁用 mpv 自身的输入处理，鼠标/键盘全部交给宿主界面
    mpv_set_option_string(m_mpv, "input-default-bindings", "no");
    mpv_set_option_string(m_mpv, "input-vo-keyboard", "no");
    // 播放结束停留在最后一帧而不是退出，由 UI 决定下一步
    mpv_set_option_string(m_mpv, "keep-open", "yes");
    // 界面自绘，禁用 mpv 内建 OSD 控件
    mpv_set_option_string(m_mpv, "osc", "no");

    if (mpv_initialize(m_mpv) < 0) {
        qWarning("MpvController: mpv_initialize failed");
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
        return;
    }

    // mpv 内部日志引到 Qt 警告流（落盘到 exe 旁的 MultiVideoPlayer.log）；
    // 默认 warn，需要详细日志时设 MVP_LOG=v
    const QByteArray logLevel = qgetenv("MVP_LOG");
    mpv_request_log_messages(m_mpv, !logLevel.isEmpty() ? logLevel.constData() : "warn");

    mpv_set_wakeup_callback(m_mpv, &MpvController::onWakeup, this);
}

MpvController::~MpvController()
{
    if (m_mpv) {
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
    }
}

void MpvController::onWakeup(void *context)
{
    auto *self = static_cast<MpvController *>(context);
    QMetaObject::invokeMethod(self, &MpvController::drainEvents, Qt::QueuedConnection);
}

void MpvController::drainEvents()
{
    if (!m_mpv)
        return;

    while (true) {
        mpv_event *event = mpv_wait_event(m_mpv, 0);
        if (event->event_id == MPV_EVENT_NONE)
            break;

        if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
            auto *prop = static_cast<mpv_event_property *>(event->data);
            const QString name = QString::fromUtf8(prop->name);
            if (prop->format == MPV_FORMAT_NODE) {
                emit propertyChanged(name, nodeToVariant(static_cast<mpv_node *>(prop->data)));
            } else {
                emit propertyChanged(name, {});
            }
        } else if (event->event_id == MPV_EVENT_LOG_MESSAGE) {
            auto *msg = static_cast<mpv_event_log_message *>(event->data);
            if (msg && msg->text)
                qWarning("[mpv/%s] %s", msg->prefix, msg->text);
        }
    }
}

void MpvController::command(const QStringList &args)
{
    if (!m_mpv)
        return;

    QList<QByteArray> utf8;
    utf8.reserve(args.size());
    for (const QString &arg : args)
        utf8.append(arg.toUtf8());

    QVector<const char *> pointers;
    pointers.reserve(utf8.size() + 1);
    for (const QByteArray &arg : utf8)
        pointers.append(arg.constData());
    pointers.append(nullptr);

    int rc = mpv_command(m_mpv, pointers.data());
    if (rc < 0)
        qWarning("MpvController: command '%s' failed: %s", qPrintable(args.join(' ')),
                 mpv_error_string(rc));
}

void MpvController::setProperty(const QString &name, const QVariant &value)
{
    if (!m_mpv)
        return;

    const QByteArray nameUtf8 = name.toUtf8();
    mpv_node node{};
    QByteArray stringStorage;

    switch (value.typeId()) {
    case QMetaType::Bool:
        node.format = MPV_FORMAT_FLAG;
        node.u.flag = value.toBool() ? 1 : 0;
        break;
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::Long:
    case QMetaType::ULong:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        node.format = MPV_FORMAT_INT64;
        node.u.int64 = value.toLongLong();
        break;
    case QMetaType::Float:
    case QMetaType::Double:
        node.format = MPV_FORMAT_DOUBLE;
        node.u.double_ = value.toDouble();
        break;
    default:
        stringStorage = value.toString().toUtf8();
        node.format = MPV_FORMAT_STRING;
        node.u.string = stringStorage.data();
        break;
    }

    int rc = mpv_set_property(m_mpv, nameUtf8.constData(), MPV_FORMAT_NODE, &node);
    if (rc < 0)
        qWarning("MpvController: set property '%s' failed: %s", qPrintable(name),
                 mpv_error_string(rc));
}

QVariant MpvController::getProperty(const QString &name) const
{
    if (!m_mpv)
        return {};

    const QByteArray nameUtf8 = name.toUtf8();
    mpv_node node{};
    if (mpv_get_property(m_mpv, nameUtf8.constData(), MPV_FORMAT_NODE, &node) < 0)
        return {};
    const QVariant result = nodeToVariant(&node);
    mpv_free_node_contents(&node);
    return result;
}

void MpvController::observe(const QString &name)
{
    if (!m_mpv)
        return;

    const QByteArray nameUtf8 = name.toUtf8();
    mpv_observe_property(m_mpv, 0, nameUtf8.constData(), MPV_FORMAT_NODE);
}
