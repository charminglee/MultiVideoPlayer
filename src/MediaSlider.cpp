#include "MediaSlider.h"

#include <QMouseEvent>
#include <QStyle>
#include <QStyleOptionSlider>

MediaSlider::MediaSlider(QWidget *parent)
    : fancy::Slider(Qt::Horizontal, parent)
{
    setRange(0, 1000);
}

void MediaSlider::setPlaybackFraction(double fraction)
{
    const int value = int(qRound(qBound(0.0, fraction, 1.0) * maximum()));
    blockSignals(true);
    setValue(value);
    blockSignals(false);
}

void MediaSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && orientation() == Qt::Horizontal && width() > 0) {
        // 点击即定位：先把值设到点击位置，再把位于手柄中心的合成按下
        // 事件交给父类，后续拖动即从手柄无缝衔接。
        const double ratio = qBound(0.0, event->position().x() / double(width()), 1.0);
        setValue(minimum() + int(qRound(ratio * (maximum() - minimum()))));

        QStyleOptionSlider option;
        initStyleOption(&option);
        const QRect handleRect = style()->subControlRect(
                QStyle::CC_Slider, &option, QStyle::SC_SliderHandle, this);
        QMouseEvent synthetic(event->type(), QPointF(handleRect.center()),
                              event->globalPosition(), event->button(),
                              event->buttons(), event->modifiers());
        fancy::Slider::mousePressEvent(&synthetic);
        return;
    }
    fancy::Slider::mousePressEvent(event);
}
