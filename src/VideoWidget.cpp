#include "VideoWidget.h"

#include "MpvController.h"

#include <QVariant>

VideoWidget::VideoWidget(MpvController *controller, QWidget *parent)
    : QWidget(parent)
{
    Q_ASSERT(controller);

    // mpv 需要原生窗口句柄作渲染目标；parent 已确定，winId() 创建的是子窗口
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);
    setMinimumSize(1, 1);
    setAcceptDrops(true);

    // mpv 在此窗口上自行呈现与缩放，程序内不参与其绘制
    controller->setProperty("wid", QVariant::fromValue<qlonglong>(qlonglong(winId())));
}
