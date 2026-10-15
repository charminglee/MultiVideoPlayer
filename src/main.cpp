#include "MainWindow.h"

#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QFont>
#include <QMessageLogger>
#include <QtGlobal>

namespace {

// GUI 程序看不到控制台：把 Qt 警告/致命输出追加写到 exe 旁的 MultiVideoPlayer.log
void fileMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    Q_UNUSED(context);

    static QFile logFile(QCoreApplication::applicationDirPath()
                         + QStringLiteral("/MultiVideoPlayer.log"));
    if (!logFile.isOpen())
        (void)logFile.open(QIODevice::Append | QIODevice::Text);

    const char *level = "unknown";
    switch (type) {
    case QtDebugMsg:    level = "debug";    break;
    case QtInfoMsg:     level = "info";     break;
    case QtWarningMsg:  level = "warning";  break;
    case QtCriticalMsg: level = "critical"; break;
    case QtFatalMsg:    level = "fatal";    break;
    }

    if (logFile.isOpen()) {
        logFile.write(QStringLiteral("[%1] %2: %3\n")
                          .arg(QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss")),
                               QString::fromLatin1(level), message)
                          .toUtf8());
        logFile.flush();
    }
}

} // namespace

int main(int argc, char **argv)
{
    // QOpenGLWidget 官方建议：共享 GL 上下文，避免窗口模式切换时重建上下文
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);

    QApplication app(argc, argv);
    qInstallMessageHandler(&fileMessageHandler);
    app.setApplicationName(QStringLiteral("MultiVideoPlayer"));
    app.setOrganizationName(QStringLiteral("MultiVideoPlayer"));

    // 中文回退：Inter/Segoe UI 均无中文字形
    QFont font;
    font.setFamilies({QStringLiteral("Inter"), QStringLiteral("Segoe UI"),
                      QStringLiteral("Microsoft YaHei")});
    font.setPointSize(10);
    app.setFont(font);

    MainWindow window;
    window.show();

    // 支持把文件拖到 exe 图标上启动
    const QStringList args = app.arguments();
    if (args.size() > 1)
        window.openPath(args.at(1));

    return app.exec();
}
