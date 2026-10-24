#include "monitorwidget.h"
#include "themeicons.h"
#include "themefonts.h"
#include "fontbundle.h"

#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setFont(ThemeFonts::text());
    QApplication::setApplicationName(QStringLiteral("Stellarsue"));
    QApplication::setApplicationDisplayName(QStringLiteral("Stellarsue · 飞行器遥测"));
    QGuiApplication::setDesktopFileName(QStringLiteral("linux-system-monitor"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    QApplication::setOrganizationName(QStringLiteral("System Monitor"));
    QApplication::setQuitOnLastWindowClosed(false);
    QApplication::setWindowIcon(ThemeIcons::applicationIcon());

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
    FontBundle::offerInstallation();

    SystemMonitorWidget monitor;
    monitor.show();
    monitor.showNormal();
    monitor.raise();
    monitor.activateWindow();

    return app.exec();
}
