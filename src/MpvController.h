#pragma once

#include <QObject>
#include <QVariant>

#include <mpv/client.h>

// 封装 mpv_handle：命令、属性、事件观察。
// mpv 的唤醒回调来自内部线程，这里统一排队投递回 Qt 主线程再处理。
class MpvController : public QObject {
    Q_OBJECT

public:
    explicit MpvController(QObject *parent = nullptr);
    ~MpvController() override;

    bool isValid() const { return m_mpv != nullptr; }
    mpv_handle *handle() const { return m_mpv; }

    void command(const QStringList &args);
    void setProperty(const QString &name, const QVariant &value);
    QVariant getProperty(const QString &name) const;
    void observe(const QString &name);

signals:
    void propertyChanged(const QString &name, const QVariant &value);

private:
    static void onWakeup(void *context);

    void drainEvents();

    mpv_handle *m_mpv = nullptr;
};
