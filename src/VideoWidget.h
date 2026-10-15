#pragma once

#include <QWidget>

class MpvController;

// mpv 以 wid（原生窗口嵌入）方式直接渲染到本控件：无 GL 上下文与帧缓冲
// 时序问题，窗口失焦不丢画面。此前的两条路线均被否——QOpenGLWidget 会使
// 顶层变成 OwnDC 窗口类而丢失 DWM 窗口动画；QOpenGLWindow+render API 在
// 部分显卡驱动下失焦丢帧（黑屏/闪烁）。mpv 在 wid 模式下不接管宿主窗口的
// 输入，鼠标/键盘事件仍由 Qt 正常分发。
class VideoWidget : public QWidget {
    Q_OBJECT

public:
    explicit VideoWidget(MpvController *controller, QWidget *parent = nullptr);
};
