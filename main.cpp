#include "monitorwidget.h"
#include "starmapwidget.h"
#include "themeicons.h"
#include "themefonts.h"
#include "fontbundle.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QSurfaceFormat>
#include <QTimer>

int main(int argc, char *argv[])
{
    // QOpenGLWidget contexts must share the same profile as Qt's application
    // context. On macOS this default has to be installed before QApplication.
    QSurfaceFormat::setDefaultFormat(StarMapWidget::requiredSurfaceFormat());
    QApplication app(argc, argv);
    ThemeFonts::registerBundledFallbacks();
    QApplication::setFont(ThemeFonts::text());
    QApplication::setApplicationName(QStringLiteral("StellarVoyager"));
    QApplication::setApplicationDisplayName(QStringLiteral("StellarVoyager · 飞行器遥测"));
    QGuiApplication::setDesktopFileName(QStringLiteral("linux-system-monitor"));
    QApplication::setApplicationVersion(QStringLiteral(STELLARVOYAGER_VERSION));
    QApplication::setOrganizationName(QStringLiteral("System Monitor"));
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setWindowIcon(ThemeIcons::applicationIcon());

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Interactive 3D desktop system telemetry"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("install-fonts"), QStringLiteral("Install bundled fonts for the current user.")});
    parser.addOption({QStringLiteral("accept-font-license"), QStringLiteral("Accept the bundled original Microsoft font EULA.")});
    parser.addOption({QStringLiteral("no-font-prompt"), QStringLiteral("Use installed/fallback fonts without a first-run prompt.")});
    parser.addOption({QStringLiteral("quit-after"), QStringLiteral("Exit after N seconds (packaging diagnostics)."), QStringLiteral("seconds")});
    parser.process(app);
    bool validTimeout = false;
    const int seconds = parser.value(QStringLiteral("quit-after")).toInt(&validTimeout);
    if (parser.isSet(QStringLiteral("quit-after")) && (!validTimeout || seconds < 1 || seconds > 3600)) {
        qCritical("--quit-after must be between 1 and 3600 seconds");
        return 1;
    }

    if (app.arguments().contains(QStringLiteral("--install-fonts"))) {
        const auto result = FontBundle::installForCurrentUser(
            app.arguments().contains(QStringLiteral("--accept-font-license")));
        if (!result.success) {
            qCritical().noquote() << result.error;
            return 1;
        }
        qInfo().noquote() << "Times New Roman:" << result.directory;
        if (!result.warning.isEmpty()) qWarning().noquote() << result.warning;
        return 0;
    }
    if (!parser.isSet(QStringLiteral("no-font-prompt"))) FontBundle::offerInstallation();

    SystemMonitorWidget monitor;
    monitor.show();
    monitor.showNormal();
    monitor.raise();
    monitor.activateWindow();
    if (validTimeout) QTimer::singleShot(seconds * 1000, &app, [&app, &monitor] {
        const auto *scene = monitor.findChild<StarMapWidget *>();
        if (!scene || !scene->isValid() || !scene->spacecraftReady()) {
            qCritical("StellarVoyager runtime check failed: OpenGL scene or spacecraft unavailable");
            app.exit(1);
            return;
        }
        qInfo("StellarVoyager runtime check passed: OpenGL scene and spacecraft ready");
        app.quit();
    });

    return app.exec();
}
