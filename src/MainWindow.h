#pragma once

#include "Widgets/Window/Window.h"

#include <QRect>
#include <QVariant>

class QTimer;
class MediaSlider;
class MpvController;
class VideoWidget;

namespace fancy {
class TextLabel;
class ThemeButton;
class TitleBar;
class TransparentButton;
}

class MainWindow : public fancy::Window {
    Q_OBJECT

public:
    MainWindow();
    ~MainWindow() override;

    // 供拖放 / 命令行参数使用
    void openPath(const QString &path);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void buildUi();
    void setUpTitleBar();
    void connectPlaybackSignals();
    void connectShortcuts();

    void openFile();
    void togglePlayPause();
    void toggleMute();
    void toggleFullscreen();
    void seekBy(double seconds);

    void onPropertyChanged(const QString &name, const QVariant &value);
    void updatePlayButton();

    MpvController *m_mpv = nullptr;
    VideoWidget *m_video = nullptr;
    fancy::TitleBar *m_titleBar = nullptr;
    fancy::TextLabel *m_hint = nullptr;
    fancy::TextLabel *m_timeLabel = nullptr;
    fancy::TextLabel *m_durationLabel = nullptr;
    fancy::TransparentButton *m_openButton = nullptr;
    fancy::ThemeButton *m_playButton = nullptr;
    fancy::TransparentButton *m_muteButton = nullptr;
    MediaSlider *m_seekSlider = nullptr;
    MediaSlider *m_volumeSlider = nullptr;

    double m_duration = 0.0;
    bool m_paused = true;
    bool m_eof = false;
    bool m_muted = false;
    bool m_haveFile = false;
    bool m_seeking = false;
    bool m_ignoreNextRelease = false;
    bool m_videoPressed = false; // 视频区是否发生过左键按下（过滤无源释放）
    bool m_borderlessFullscreen = false;
    QRect m_savedGeometry;
    QTimer *m_clickTimer = nullptr;
};
