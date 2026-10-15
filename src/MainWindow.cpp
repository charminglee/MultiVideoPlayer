#include "MainWindow.h"

#include "MediaSlider.h"
#include "MpvController.h"
#include "VideoWidget.h"

#include "Core/Defs.h"
#include "Core/Palette/Palette.h"
#include "Core/Theme/ThemeModeController.h"
#include "Widgets/PushButton/ThemeButton.h"
#include "Widgets/PushButton/TransparentButton.h"
#include "Widgets/TitleBar/Button/UniversalTitleBarButton.h"
#include "Widgets/TitleBar/TitleBar.h"
#include "Widgets/TextLabel/TextLabel.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMimeData>
#include <QMouseEvent>
#include <QShortcut>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QString formatTime(double seconds)
{
    if (seconds < 0.0 || qIsNaN(seconds))
        seconds = 0.0;
    const qint64 total = qint64(seconds + 0.5);
    const qint64 h = total / 3600;
    const qint64 m = (total % 3600) / 60;
    const qint64 s = total % 60;
    if (h > 0)
        return QString("%1:%2:%3").arg(h).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
    return QString("%1:%2").arg(m).arg(s, 2, 10, QChar('0'));
}

} // namespace

MainWindow::MainWindow()
{
    m_mpv = new MpvController(this);

    buildUi();
    setUpTitleBar();
    connectPlaybackSignals();
    connectShortcuts();

    // 订阅驱动 UI 的属性（不订阅则收不到任何状态更新）
    m_mpv->observe("time-pos");
    m_mpv->observe("duration");
    m_mpv->observe("pause");
    m_mpv->observe("eof-reached");
    m_mpv->observe("media-title");
    m_mpv->observe("volume");
    m_mpv->observe("mute");

    setAcceptDrops(true);
    setWindowTitle(QStringLiteral("MultiVideoPlayer"));
    resize(1180, 760);

    // 单击判定延迟：250ms 内出现双击则取消暂停切换
    m_clickTimer = new QTimer(this);
    m_clickTimer->setSingleShot(true);
    m_clickTimer->setInterval(250);
    connect(m_clickTimer, &QTimer::timeout, this, &MainWindow::togglePlayPause);
}

MainWindow::~MainWindow()
{
    // mpv 渲染进 VideoWidget 的原生窗口：先销毁 mpv，再由基类析构拆控件树
    delete m_mpv;
    m_mpv = nullptr;
    m_video = nullptr;
}

void MainWindow::buildUi()
{
    auto *client = setupUiWidget();
    auto *rootLayout = new QVBoxLayout(client);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    auto *videoWrap = new QWidget();
    auto *videoGrid = new QGridLayout(videoWrap);
    // 视频原生窗口（含 mpv 的子窗口）不能覆盖窗口边缘，否则会挡住顶层
    // 的边缘 resize 手柄（跨线程窗口无法可靠做鼠标穿透）；四周留出略宽于
    // 系统缩放边带的边距，边缘露出顶层背景，拖拽缩放即可正常工作
    videoGrid->setContentsMargins(6, 6, 6, 6);
    videoGrid->setSpacing(0);

    // 视频区：mpv wid 嵌入 VideoWidget 的原生窗口直接渲染（见其头注释）
    m_video = new VideoWidget(m_mpv, videoWrap);
    m_video->installEventFilter(this);
    videoGrid->addWidget(m_video, 0, 0);

    // 空状态提示：视频窗口是原生的，提示需原生化并抬到其上；再加
    // WS_EX_TRANSPARENT 使鼠标与拖放完全穿透到视频控件
    m_hint = new fancy::TextLabel(QStringLiteral("拖入视频文件，或点击「打开」"), videoWrap);
    m_hint->setAlignment(Qt::AlignCenter);
    m_hint->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_hint->setAttribute(Qt::WA_NativeWindow);
    videoGrid->addWidget(m_hint, 0, 0, Qt::AlignCenter);
    m_hint->raise();
    if (const HWND hint = reinterpret_cast<HWND>(m_hint->winId()))
        SetWindowLongPtr(hint, GWL_EXSTYLE,
                         GetWindowLongPtr(hint, GWL_EXSTYLE) | WS_EX_TRANSPARENT);

    // 控制栏
    auto *controls = new QWidget();
    controls->setFixedHeight(76);
    auto *controlsLayout = new QHBoxLayout(controls);
    controlsLayout->setContentsMargins(16, 8, 16, 8);
    controlsLayout->setSpacing(12);

    m_openButton = new fancy::TransparentButton(controls);
    m_openButton->setIcon(fancy::BootstrapIcons::Folder2Open);
    m_openButton->setIconSize(QSize(22, 22));
    m_openButton->setFixedSize(44, 44);
    m_openButton->setToolTip(QStringLiteral("打开文件 (O)"));

    m_playButton = new fancy::ThemeButton(controls);
    m_playButton->setIcon(fancy::BootstrapIcons::PlayFill);
    m_playButton->setIconSize(QSize(20, 20));
    m_playButton->setFixedSize(44, 44);
    m_playButton->setRadius(22);
    m_playButton->setToolTip(QStringLiteral("播放/暂停 (空格)"));

    m_timeLabel = new fancy::TextLabel(QStringLiteral("0:00"), controls);

    m_seekSlider = new MediaSlider(controls);

    m_durationLabel = new fancy::TextLabel(QStringLiteral("0:00"), controls);

    m_muteButton = new fancy::TransparentButton(controls);
    m_muteButton->setIcon(fancy::BootstrapIcons::VolumeUpFill);
    m_muteButton->setIconSize(QSize(20, 20));
    m_muteButton->setFixedSize(40, 40);
    m_muteButton->setToolTip(QStringLiteral("静音 (M)"));

    m_volumeSlider = new MediaSlider(controls);
    m_volumeSlider->setFixedWidth(90);
    m_volumeSlider->setPlaybackFraction(1.0);

    controlsLayout->addWidget(m_openButton);
    controlsLayout->addWidget(m_playButton);
    controlsLayout->addWidget(m_timeLabel);
    controlsLayout->addWidget(m_seekSlider, 1);
    controlsLayout->addWidget(m_durationLabel);
    controlsLayout->addWidget(m_muteButton);
    controlsLayout->addWidget(m_volumeSlider);

    rootLayout->addWidget(videoWrap, 1);
    rootLayout->addWidget(controls);
}

void MainWindow::setUpTitleBar()
{
    m_titleBar = qobject_cast<fancy::TitleBar *>(titleBar());
    m_titleBar->setTitleText(QStringLiteral("MultiVideoPlayer"));

    // 深浅主题切换按钮（做法同 FancyUI 示例）
    auto *themeButton = new fancy::UniversalTitleBarButton(m_titleBar);
    themeButton->setFixedSize(m_titleBar->titleBtnFixedSize());
    themeButton->setAutoEnlargeIcon(true);
    m_titleBar->insertWidget(4, themeButton);
    auto &themeController = fancy::ThemeModeController::controller();
    const auto updateIcon = [themeButton, &themeController] {
        themeButton->setIcon(fancy::iconId(themeController.isAppLight()
                                               ? fancy::IconPark::Sleep
                                               : fancy::IconPark::Sunrise));
    };
    updateIcon();
    connect(themeButton, &fancy::UniversalTitleBarButton::clicked,
            &themeController, &fancy::ThemeModeController::toggleAppTheme);
    connect(&fancy::Palette::palette(), &fancy::Palette::appThemeChange,
            themeButton, updateIcon);
}

void MainWindow::connectPlaybackSignals()
{
    connect(m_mpv, &MpvController::propertyChanged, this, &MainWindow::onPropertyChanged);

    connect(m_openButton, &fancy::TransparentButton::clicked, this, &MainWindow::openFile);
    connect(m_playButton, &fancy::ThemeButton::clicked, this, &MainWindow::togglePlayPause);
    connect(m_muteButton, &fancy::TransparentButton::clicked, this, &MainWindow::toggleMute);

    // 播放驱动的进度更新经 setPlaybackFraction 静默处理（blockSignals），
    // valueChanged 只会由用户点击/拖动触发，可以实时 seek。
    connect(m_seekSlider, &MediaSlider::sliderPressed, this, [this]() { m_seeking = true; });
    connect(m_seekSlider, &MediaSlider::sliderReleased, this, [this]() { m_seeking = false; });
    connect(m_seekSlider, &MediaSlider::valueChanged, this, [this](int value) {
        if (m_duration > 0.0)
            m_mpv->command({"seek", QString::number(double(value) / 1000.0 * m_duration),
                            "absolute"});
    });

    connect(m_volumeSlider, &MediaSlider::valueChanged, this, [this](int value) {
        m_mpv->setProperty("volume", double(value) / 10.0);
    });
}

void MainWindow::connectShortcuts()
{
    const auto shortcut = [this](QKeySequence key, auto &&slot) {
        auto *sc = new QShortcut(key, this);
        connect(sc, &QShortcut::activated, this, std::forward<decltype(slot)>(slot));
    };

    shortcut(Qt::Key_Space, [this]() { togglePlayPause(); });
    shortcut(Qt::Key_Right, [this]() { seekBy(5.0); });
    shortcut(Qt::Key_Left, [this]() { seekBy(-5.0); });
    shortcut(Qt::Key_Up, [this]() {
        const double v = m_mpv->getProperty("volume").toDouble();
        m_mpv->setProperty("volume", qMin(100.0, v + 5.0));
    });
    shortcut(Qt::Key_Down, [this]() {
        const double v = m_mpv->getProperty("volume").toDouble();
        m_mpv->setProperty("volume", qMax(0.0, v - 5.0));
    });
    shortcut(Qt::Key_F, [this]() { toggleFullscreen(); });
    shortcut(Qt::Key_O, [this]() { openFile(); });
    shortcut(Qt::Key_M, [this]() { toggleMute(); });
    shortcut(Qt::Key_Escape, [this]() {
        if (m_borderlessFullscreen)
            toggleFullscreen();
    });
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        if (url.isLocalFile()) {
            openPath(url.toLocalFile());
            break;
        }
    }
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_video) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton)
                m_videoPressed = true;
        } else if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() != Qt::LeftButton)
                return fancy::Window::eventFilter(watched, event);
            const bool hadPress = m_videoPressed;
            m_videoPressed = false;
            // 忽略没有对应按下的释放：文件对话框双击选文件时，对话框在第
            // 二次按下即关闭并销毁，其后的释放事件会泄漏到下层主窗口的视
            // 频区，否则 250ms 后会误触发播放/暂停切换
            if (!hadPress)
                return true;
            if (m_ignoreNextRelease) {
                // 双击事件自带的 release，不触发单击动作
                m_ignoreNextRelease = false;
            } else {
                m_clickTimer->start();
            }
        } else if (event->type() == QEvent::MouseButtonDblClick) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton) {
                m_clickTimer->stop();
                m_ignoreNextRelease = true;
                toggleFullscreen();
            }
        } else if (event->type() == QEvent::DragEnter) {
            auto *drag = static_cast<QDragEnterEvent *>(event);
            if (drag->mimeData()->hasUrls())
                drag->acceptProposedAction();
            return true;
        } else if (event->type() == QEvent::Drop) {
            auto *drop = static_cast<QDropEvent *>(event);
            const QList<QUrl> urls = drop->mimeData()->urls();
            for (const QUrl &url : urls) {
                if (url.isLocalFile()) {
                    openPath(url.toLocalFile());
                    break;
                }
            }
            return true;
        }
    }
    return fancy::Window::eventFilter(watched, event);
}

void MainWindow::openFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开媒体"), QString(),
        QStringLiteral("媒体文件 (*.mp4 *.mkv *.avi *.mov *.webm *.flv *.ts *.m2ts *.wmv *.mpg "
                        "*.mpeg *.m4v *.3gp *.ogv *.ogg *.opus *.flac *.mp3 *.wav);;所有文件 (*)"));
    if (!path.isEmpty())
        openPath(path);
}

void MainWindow::openPath(const QString &path)
{
    m_eof = false;
    m_hint->setText(QStringLiteral("载入中…"));
    m_hint->show();
    m_mpv->command({"loadfile", path});
    m_mpv->setProperty("pause", false);
}

void MainWindow::togglePlayPause()
{
    if (!m_haveFile) {
        openFile();
        return;
    }
    if (m_eof) {
        m_mpv->command({"seek", "0", "absolute"});
        m_mpv->setProperty("pause", false);
        return;
    }
    m_mpv->setProperty("pause", !m_paused);
}

void MainWindow::toggleMute()
{
    m_mpv->setProperty("mute", !m_muted);
}

void MainWindow::toggleFullscreen()
{
    if (m_borderlessFullscreen) {
        m_titleBar->show();
        setGeometry(m_savedGeometry);
        m_borderlessFullscreen = false;
        return;
    }

    m_savedGeometry = geometry();

    // 无边框窗口式全屏：覆盖整个屏幕并四边各溢出 1px。
    // 观感与全屏相同（任务栏同样被盖住），但不满足驱动对
    // "恰好覆盖屏幕" 的独占全屏判定，避免显示信号切换黑屏。
    // 窗口本身就是 frameless，隐藏标题栏后直接改几何即可，
    // 不能动 windowFlags（会重建原生窗口，丢失库设置的 Win32 样式）。
    m_titleBar->hide();
    const QRect screenGeometry = screen()->geometry();
    setGeometry(screenGeometry.left() - 1, screenGeometry.top() - 1,
               screenGeometry.width() + 2, screenGeometry.height() + 2);
    raise();
    activateWindow();
    m_borderlessFullscreen = true;
}

void MainWindow::seekBy(double seconds)
{
    if (!m_haveFile)
        return;
    m_mpv->command({"seek", QString::number(seconds), "relative"});
}

void MainWindow::onPropertyChanged(const QString &name, const QVariant &value)
{
    if (name == QLatin1String("time-pos")) {
        const double t = value.toDouble();
        if (!m_seeking)
            m_seekSlider->setPlaybackFraction(m_duration > 0.0 ? t / m_duration : 0.0);
        m_timeLabel->setText(formatTime(t));
    } else if (name == QLatin1String("duration")) {
        m_duration = value.toDouble();
        m_durationLabel->setText(formatTime(m_duration));
    } else if (name == QLatin1String("pause")) {
        m_paused = value.toBool();
        updatePlayButton();
    } else if (name == QLatin1String("eof-reached")) {
        m_eof = value.toBool();
        updatePlayButton();
    } else if (name == QLatin1String("media-title")) {
        const QString title = value.toString();
        if (!title.isEmpty()) {
            m_haveFile = true;
            m_hint->hide();
            const QString windowTitle = title + QStringLiteral(" — MultiVideoPlayer");
            setWindowTitle(windowTitle);
            m_titleBar->setTitleText(windowTitle);
        }
    } else if (name == QLatin1String("volume")) {
        m_volumeSlider->setPlaybackFraction(value.toDouble() / 100.0);
    } else if (name == QLatin1String("mute")) {
        m_muted = value.toBool();
        m_muteButton->setIcon(m_muted ? fancy::BootstrapIcons::VolumeMuteFill
                                      : fancy::BootstrapIcons::VolumeUpFill);
    }
}

void MainWindow::updatePlayButton()
{
    const bool showPlay = m_paused || m_eof || !m_haveFile;
    m_playButton->setIcon(showPlay ? fancy::BootstrapIcons::PlayFill
                                   : fancy::BootstrapIcons::PauseFill);
}
