#pragma once

#include "Widgets/Slider/Slider.h"

#include <QSlider>

class QMouseEvent;

// 播放器滑块：在 fancy::Slider 之上补"点击轨道任意位置立即定位"
// （QSlider 原生点击轨道只做 pageStep 翻页）。进度与音量共用，
// 取值范围 0..1000（千分比），换算逻辑与旧 SeekSlider 保持一致。
class MediaSlider : public fancy::Slider {
    Q_OBJECT

public:
    explicit MediaSlider(QWidget *parent = nullptr);

    // 播放驱动的静默更新：不触发 valueChanged，避免回环 seek
    void setPlaybackFraction(double fraction);

protected:
    void mousePressEvent(QMouseEvent *event) override;
};
