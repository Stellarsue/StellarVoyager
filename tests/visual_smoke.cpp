#include "monitorwidget.h"
#include "starmapwidget.h"
#include "spacecraftmesh.h"
#include "flightpath.h"
#include "nebulaenvironment.h"
#include "themefonts.h"
#include "themeicons.h"
#include "systeminfo.h"
#include "statusdashboard.h"

#include <QApplication>
#include <QAction>
#include <QCursor>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QImage>
#include <QHelpEvent>
#include <QFontDatabase>
#include <QFontInfo>
#include <QLabel>
#include <QLineF>
#include <QMouseEvent>
#include <QMenu>
#include <QMessageBox>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QScreen>
#include <QSet>
#include <QSurfaceFormat>
#include <QTextLayout>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QWheelEvent>
#include <cmath>
#include <cstdlib>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition) qFatal("Smoke test failed: %s", message);
}
void mouse(QWidget *target, QEvent::Type type, QPointF point,
           Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QMouseEvent event(type, point, target->mapToGlobal(point.toPoint()),
                      button, buttons, Qt::NoModifier);
    QApplication::sendEvent(target, &event);
}
void drag(QWidget *target, QPointF delta)
{
    const QPointF start(180, 170);
    mouse(target, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    mouse(target, QEvent::MouseMove, start + delta, Qt::NoButton, Qt::LeftButton);
    mouse(target, QEvent::MouseButtonRelease, start + delta, Qt::LeftButton, Qt::NoButton);
}
void validateMesh()
{
    QString error;
    QStringList textures;
    const auto mesh = loadSpacecraftMesh(QStringLiteral(":/stellarsue/spacecraft.obj"), &error, &textures);
    require(error.isEmpty() && mesh.size() == 37004 * 3, "37004 assembled orbiter triangles loaded");
    require(textures.size() == 11, "eleven embedded NASA base-color textures");
    for (const auto &path : textures) require(!QImage(path).isNull(), "texture decodes offline");
    bool differentMaterials = false;
    bool textured = false;
    for (const auto &v : mesh) {
        for (int i = 0; i < 3; ++i) {
            require(std::isfinite(v.position[i]) && qAbs(v.position[i]) <= 1.001F,
                    "finite, normalized model bounds");
            require(std::isfinite(v.normal[i]), "finite normals");
            require(v.color[i] >= 0 && v.color[i] <= 1, "valid material colors");
        }
        differentMaterials |= v.color[0] != mesh.first().color[0];
        require(std::isfinite(v.uv[0]) && std::isfinite(v.uv[1]), "finite UV coordinates");
        require(v.surface[0] >= 0 && v.surface[0] <= textures.size(), "valid texture layer");
        require(v.surface[1] >= 0.1F && v.surface[1] <= 1, "valid surface roughness");
        require(v.surface[2] >= 0 && v.surface[2] <= 1, "valid metalness");
        textured |= v.surface[0] > 0;
    }
    require(differentMaterials, "material groups preserved");
    require(textured, "UV textured geometry retained");
    require(loadSpacecraftMesh(QStringLiteral(":/missing.obj"), &error).isEmpty()
                && !error.isEmpty(), "missing mesh handled");
    QTemporaryDir directory;
    require(directory.isValid(), "fixture directory");
    QFile invalid(directory.filePath(QStringLiteral("invalid.obj")));
    require(invalid.open(QIODevice::WriteOnly), "fixture open");
    invalid.write("v 0 0 0\nvt 0 0\nvn 0 1 0\nf 9/1/1 1/1/1 2/1/1\n");
    invalid.close();
    require(loadSpacecraftMesh(invalid.fileName(), &error).isEmpty()
                && error.contains(QStringLiteral("index")), "invalid indices rejected");
    require(QImage(QStringLiteral(":/stellarsue/vessel.png")).isNull(), "old image not bundled");
}

void validateFlightPath()
{
    require(FlightPath::ModelScale < 0.35F, "spacecraft uses scene-scale proportions");
    for (int i = -100; i <= 100; ++i) {
        const double seconds = i * 0.71;
        const auto pose = FlightPath::sample(seconds);
        const auto nextLap = FlightPath::sample(seconds + FlightPath::LapSeconds);
        require((pose.position - nextLap.position).length() < 0.00001F,
                "fixed closed route repeats every lap");
        require((pose.forward - nextLap.forward).length() < 0.00001F,
                "heading is continuous at lap boundary");
        const QVector3D nose = pose.model.mapVector(QVector3D(0, 0, 1));
        require(qAbs(nose.length() - FlightPath::ModelScale) < 0.00001F,
                "model scale stays constant throughout cruise");
        require(QVector3D::dotProduct(nose.normalized(), pose.forward) > 0.9999F,
                "nose follows route tangent");
        const QVector3D tangent = (FlightPath::sample(seconds + 0.001).position
                                  - FlightPath::sample(seconds - 0.001).position).normalized();
        require(QVector3D::dotProduct(tangent, pose.forward) > 0.999F,
                "heading matches actual travel direction");
        require((pose.model.map(QVector3D()) - pose.position).length() < 0.00001F,
                "transform preserves route center");
        require(pose.position.length() > 1.0F && pose.position.length() < 1.5F,
                "cruise stays in bounded orbit outside the black-hole center");
    }
    const auto before = FlightPath::sample(FlightPath::LapSeconds - 0.001);
    const auto after = FlightPath::sample(FlightPath::LapSeconds + 0.001);
    require((before.position - after.position).length() < 0.001F
                && (before.forward - after.forward).length() < 0.001F,
            "no position or attitude jump at wraparound");
}

void validateSkyAndFonts()
{
    constexpr qreal pi = 3.14159265358979323846;
    const auto initial = NebulaEnvironment::visibleClouds(0, 0, 1, 468.0 / 458.0);
    const auto rear = NebulaEnvironment::visibleClouds(pi, 0, 1, 468.0 / 458.0);
    require(initial.size() >= 1 && initial.size() <= 3, "initial view shows only a few clouds");
    require(!rear.isEmpty(), "there are clouds behind the initial viewing direction");
    for (int id : rear) require(!initial.contains(id), "opposite views show different sky fields");
    QSet<int> seen;
    int peakVisible = 0;
    for (qreal pitch : {-1.15, -0.55, 0.0, 0.55, 1.15}) {
        for (int step = 0; step < 36; ++step) {
            const qreal yaw = step * 2 * pi / 36;
            const auto visible = NebulaEnvironment::visibleClouds(yaw, pitch, 1, 468.0 / 458.0);
            require(visible == NebulaEnvironment::visibleClouds(yaw + 2 * pi, pitch, 1, 468.0 / 458.0),
                    "sky orientation repeats continuously after 360 degrees");
            peakVisible = qMax(peakVisible, int(visible.size()));
            for (int id : visible) seen.insert(id);
        }
    }
    require(seen.size() == NebulaEnvironment::CloudCount, "all clouds discoverable by yaw and pitch");
    require(peakVisible <= 6, "clouds cannot all pile up in a single view");
    qInfo("Sky: %d world clouds; %d initially visible; at most %d in sampled views",
          NebulaEnvironment::CloudCount, int(initial.size()), peakVisible);

    const QFont textFont = ThemeFonts::text();
    require(textFont.families().first() == QStringLiteral("Times New Roman"), "requested font has priority");
    const QString resolved = QFontInfo(textFont).family();
    if (QFontDatabase::families().contains(QStringLiteral("Times New Roman")))
        require(resolved == QStringLiteral("Times New Roman"), "actual Times New Roman is used when installed");
    else {
#if defined(Q_OS_LINUX)
        require(resolved == QStringLiteral("Liberation Serif"), "Linux serif fallback is explicit");
#else
        require(!resolved.isEmpty(), "native serif fallback resolves");
#endif
    }
    QTextLayout layout(QStringLiteral("本机遥测 CPU 75% ↑ ↓"), textFont);
    layout.beginLayout();
    layout.createLine().setLineWidth(250);
    layout.endLayout();
    bool chineseSerif = false;
    const QStringList cjkSerifFamilies = {
        QStringLiteral("Noto Serif CJK"),
        QStringLiteral("Songti"),
        QStringLiteral("STSong"),
        QStringLiteral("SimSun"),
        QStringLiteral("NSimSun"),
        QStringLiteral("MingLiU"),
        QStringLiteral("PMingLiU")
    };
    for (const auto &run : layout.glyphRuns()) {
        const QString family = run.rawFont().familyName();
        for (const auto &candidate : cjkSerifFamilies)
            chineseSerif |= family.contains(candidate, Qt::CaseInsensitive);
        for (quint32 glyph : run.glyphIndexes()) require(glyph != 0, "no missing Chinese or symbol glyphs");
    }
    require(chineseSerif, "Chinese text uses serif fallback");
    qInfo("Resolved Latin font: %s", qPrintable(resolved));
}

void validateSystemTelemetry()
{
    SystemInfoReader reader;
    const SystemSnapshot snapshot = reader.sample();
    require(snapshot.cpuCoreCount > 0, "CPU core count available");
    require(snapshot.memoryTotal > 0, "physical memory available");
    require(snapshot.storageTotal > 0, "system storage available");
    require(snapshot.uptimeSeconds > 0, "system uptime available");
}

void validateAppIcon()
{
    const QImage source(QStringLiteral(":/stellarsue/app-icon.png"));
    require(!source.isNull() && source.width() >= 512 && source.hasAlphaChannel(),
            "generated transparent app icon bundled");
    const QPixmap taskbar = ThemeIcons::applicationIcon().pixmap(32, 32);
    require(!taskbar.isNull()
                && qRound(taskbar.width() / taskbar.devicePixelRatioF()) == 32
                && qRound(taskbar.height() / taskbar.devicePixelRatioF()) == 32,
            "taskbar-sized app icon available");
}
}

int main(int argc, char **argv)
{
    QSurfaceFormat::setDefaultFormat(StarMapWidget::requiredSurfaceFormat());
    QApplication app(argc, argv);
    QApplication::setFont(ThemeFonts::text());
    const QString captureMode = argc > 1 ? QString::fromLocal8Bit(argv[1]) : QString();
    if (argc > 2 && (captureMode == QStringLiteral("--capture-running")
                    || captureMode == QStringLiteral("--capture-hover-running"))) {
        auto *screen = app.primaryScreen();
        require(screen, "primary screen");
        const QRect available = screen->availableGeometry();
        const int height = qMin(470, qMax(340, available.height() - 24));
        const QPoint origin(available.right() - 480 - 22, available.top() + 12);
        const QPoint cursor = QCursor::pos();
        if (captureMode == QStringLiteral("--capture-hover-running")) {
            QCursor::setPos(origin + QPoint(300, 130));
            QTimer::singleShot(1800, [&] {
                require(screen->grabWindow(0, origin.x(), origin.y(), 480, height)
                            .save(QString::fromLocal8Bit(argv[2])), "running hover screenshot");
                QCursor::setPos(cursor);
                app.quit();
            });
            return app.exec();
        }
        require(screen->grabWindow(0, origin.x(), origin.y(), 480, height)
                    .save(QString::fromLocal8Bit(argv[2])), "running application screenshot");
        return EXIT_SUCCESS;
    }
    validateMesh();
    validateFlightPath();
    validateSkyAndFonts();
    validateSystemTelemetry();
    validateAppIcon();
    const QString output = argc > 1 ? QString::fromLocal8Bit(argv[1])
                                   : QStringLiteral("/tmp/stellarsue-scene-smoke");
    require(QDir().mkpath(output), "screenshot directory");
    SystemMonitorWidget window;
    window.show();
    window.raise();
    window.activateWindow();
    app.processEvents();
    auto *settings = window.findChild<QToolButton *>(QStringLiteral("settingsButton"));
    require(settings && settings->menu(), "main window has a settings menu button");
    auto hasAction = [](const QMenu *menu, const QString &name) {
        for (const QAction *action : menu->actions())
            if (action->objectName() == name) return true;
        return false;
    };
    QAction *quitAction = nullptr;
    for (QAction *action : settings->menu()->actions()) {
        if (action->objectName() == QStringLiteral("settingsQuitAction")) {
            quitAction = action;
            break;
        }
    }
    require(quitAction, "settings menu provides a real exit action");
    QWidget *attachedDashboard = nullptr;
    for (QWidget *topLevel : QApplication::topLevelWidgets()) {
        if (topLevel->objectName() == QStringLiteral("statusDashboard")) {
            attachedDashboard = topLevel;
            break;
        }
    }
    auto *dashboardSettings = attachedDashboard
                                  ? attachedDashboard->findChild<QToolButton *>(
                                        QStringLiteral("dashboardSettingsButton"))
                                  : nullptr;
    require(dashboardSettings && dashboardSettings->menu()
                && hasAction(dashboardSettings->menu(), QStringLiteral("settingsQuitAction")),
            "tray dashboard has the same exit entry in its settings menu");
    auto inspectSettingsPopup = [&](QToolButton *button, const QString &fileName) {
        bool shown = false;
        QTimer::singleShot(180, [&] {
            QMenu *menu = button->menu();
            shown = menu->isVisible();
            if (shown)
                require(menu->grab().save(output + QLatin1Char('/') + fileName),
                        "visible settings menu screenshot");
            menu->hide();
        });
        button->showMenu();
        app.processEvents();
        require(shown, "settings button opens a visible menu");
    };
    inspectSettingsPopup(settings, QStringLiteral("settings-main.png"));
    // Qt may deliver the closing popup's focus event on the next event-loop
    // turn; let it settle before showing a second Qt::Popup window.
    QEventLoop menuTransition;
    QTimer::singleShot(120, &menuTransition, &QEventLoop::quit);
    menuTransition.exec();
    auto *attachedStatus = static_cast<StatusDashboard *>(attachedDashboard);
    attachedStatus->showAt(QPoint(app.primaryScreen()->availableGeometry().right() - 16,
                                  app.primaryScreen()->availableGeometry().top() + 4));
    app.processEvents();
    require(attachedStatus->isVisible(), "attached tray dashboard opens");
    inspectSettingsPopup(dashboardSettings, QStringLiteral("settings-dashboard.png"));
    attachedStatus->hide();
    QAction *aboutAction = nullptr;
    for (QAction *action : settings->menu()->actions()) {
        if (action->objectName() == QStringLiteral("settingsAboutAction")) {
            aboutAction = action;
            break;
        }
    }
    require(aboutAction, "settings menu provides an About action");
    QTimer::singleShot(180, [&] {
        auto *dialog = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        require(dialog && dialog->isVisible(), "About dialog is visible");
        require(dialog->grab().save(output + QStringLiteral("/settings-about.png")),
                "About dialog screenshot");
        dialog->accept();
    });
    aboutAction->trigger();
    {
        StatusDashboard dashboard;
        SystemInfoReader reader;
        dashboard.updateSnapshot(reader.sample());
        const QRect available = app.primaryScreen()->availableGeometry();
        dashboard.showAt(QPoint(available.right() - 16, available.top() + 4));
        app.processEvents();
        require(dashboard.isVisible(), "tray dashboard opens as a popup");
        require(QFontInfo(dashboard.font()).family() != QStringLiteral("Times New Roman"),
                "tray dashboard keeps a sans-serif UI font independently of the main scene");
        auto *cpuModel = dashboard.findChild<QLabel *>(QStringLiteral("cpuModel"));
        require(cpuModel && QFontInfo(cpuModel->font()).family()
                                != QStringLiteral("Times New Roman"),
                "tray card labels keep the sans-serif font before scroll reparenting");
        for (const auto &name : {"cpuCard", "storageCard", "memoryCard", "fanCard", "networkCard"})
            require(dashboard.findChild<QWidget *>(QLatin1String(name)) != nullptr,
                    "tray dashboard includes every requested telemetry card");
        require(dashboard.screen()->availableGeometry().contains(dashboard.geometry()),
                "tray dashboard remains fully on screen");
        require(dashboard.grab().save(output + QStringLiteral("/dashboard-live.png")),
                "live tray dashboard screenshot");
        dashboard.hide();
        require(!dashboard.isVisible(), "tray dashboard can be dismissed");
    }
    double initialFlightSeconds = -1;
    QPointF initialFlightAnchor;
    QTimer::singleShot(650, [&] {
        auto *scene = window.findChild<StarMapWidget *>();
        require(scene && scene->isValid(), "flight scene initialized");
        initialFlightSeconds = scene->flightElapsedSeconds();
        initialFlightAnchor = scene->spacecraftScreenPosition();
        require(window.grab().save(output + QStringLiteral("/scene-cruise-start.png")), "cruise start capture");
    });
    QTimer::singleShot(1500, [&] {
        auto *scene = window.findChild<StarMapWidget *>();
        require(scene && scene->isValid(), "OpenGL scene ready");
        require(scene->spacecraftReady() && scene->spacecraftVertexCount() == 37004 * 3,
                "mesh uploaded and shader ready");
        require(scene->format().depthBufferSize() >= 24, "depth buffer available");
        require(window.findChildren<QOpenGLWidget *>().size() == 1, "one shared scene");
        require(!window.findChild<QWidget *>(QStringLiteral("flightPanel")), "no separate flight panel");
        require(!window.findChild<QToolButton *>(QStringLiteral("flightPanelToggle")), "no panel toggle");
        require(window.height() <= 470, "single compact window");
        require(!scene->accessibleDescription().isEmpty(), "real telemetry populated");
        auto *title = window.findChild<QLabel *>(QStringLiteral("appTitle"));
        require(title && title->font().families().contains(QStringLiteral("Times New Roman")),
                "header uses the same serif font policy as telemetry");
        require(initialFlightSeconds >= 0 && scene->flightElapsedSeconds() > initialFlightSeconds + 0.3,
                "cruise advances with elapsed time");
        require(QLineF(initialFlightAnchor, scene->spacecraftScreenPosition()).length() > 2,
                "ship moves without camera input");
        const QImage frame = scene->grabFramebuffer();
        require(frame.hasAlphaChannel(), "translucent scene framebuffer");
        require(qAlpha(frame.pixel(0, 0)) < 3 && qAlpha(frame.pixel(frame.width() - 1, frame.height() - 1)) < 3,
                "corners fully merge into desktop");
        const qreal dpr = scene->devicePixelRatioF();
        const int cornerOutside = qAlpha(frame.pixel(qRound(4 * dpr), qRound(4 * dpr)));
        const int cornerInside = qAlpha(frame.pixel(qRound(10 * dpr), qRound(10 * dpr)));
        require(cornerOutside < 12 && cornerInside >= 220,
                "frame uses a real rounded corner instead of a square crop");
        const int fadeY = qRound(frame.height() * 0.72);
        const int outerAlpha = qAlpha(frame.pixel(0, fadeY));
        const int nearAlpha = qAlpha(frame.pixel(qRound(2 * dpr), fadeY));
        const int innerAlpha = qAlpha(frame.pixel(qRound(6 * dpr), fadeY));
        const int fullAlpha = qAlpha(frame.pixel(qRound(20 * dpr), fadeY));
        require(outerAlpha < 12 && nearAlpha >= 220 && innerAlpha >= 230,
                "rounded frame reaches full opacity within two logical pixels");
        require(qAbs(fullAlpha - innerAlpha) < 4,
                "solid frame has no broad alpha gradient inside its hairline");
        scene->makeCurrent();
        qInfo("Renderer: %s", scene->context()->functions()->glGetString(GL_RENDERER));
        require(scene->context()->functions()->glGetError() == GL_NO_ERROR, "no GL errors");
        scene->doneCurrent();
#if defined(Q_OS_MACOS)
        // A non-shared QOpenGLWidget context can render and pass framebuffer
        // checks while its native layer remains black on the macOS desktop.
        // Verify the bright accretion ring survives final window composition.
        const QRect windowRect = window.frameGeometry();
        const QImage composited = window.screen()
                                      ->grabWindow(0, windowRect.x(), windowRect.y(),
                                                   windowRect.width(), windowRect.height())
                                      .toImage();
        require(!composited.isNull(), "macOS composited window capture");
        const qreal scaleX = qreal(composited.width()) / window.width();
        const qreal scaleY = qreal(composited.height()) / window.height();
        const QRect ringRegion(qRound(100 * scaleX), qRound(90 * scaleY),
                               qRound(280 * scaleX), qRound(130 * scaleY));
        int brightSamples = 0;
        for (int y = ringRegion.top(); y <= ringRegion.bottom(); y += 4) {
            for (int x = ringRegion.left(); x <= ringRegion.right(); x += 4) {
                const QRgb pixel = composited.pixel(x, y);
                brightSamples += qRed(pixel) + qGreen(pixel) + qBlue(pixel) > 200;
            }
        }
        require(brightSamples > 100, "OpenGL scene reaches the macOS desktop compositor");
#endif
        require(window.grab().save(output + QStringLiteral("/scene-default.png")), "default screenshot");
        require(scene->toolTip().isEmpty(), "old native rectangular tooltip removed");
        QHelpEvent help(QEvent::ToolTip, QPoint(180, 220), scene->mapToGlobal(QPoint(180, 220)));
        QApplication::sendEvent(scene, &help);
        auto *tip = scene->findChild<QWidget *>(QStringLiteral("telemetryToolTip"));
        require(tip && tip->isVisible() && tip->width() <= 280, "compact themed hover card appears");
        require(tip->testAttribute(Qt::WA_TranslucentBackground)
                    && tip->testAttribute(Qt::WA_TransparentForMouseEvents), "transparent non-interactive tooltip");
        require(tip->accessibleDescription() == scene->accessibleDescription(), "full telemetry preserved");
        const QImage tipImage = tip->grab().toImage();
        require(tipImage.hasAlphaChannel() && qAlpha(tipImage.pixel(0, 0)) < 3,
                "tooltip rounded corners are transparent, not black");
        require(tipImage.save(output + QStringLiteral("/telemetry-tooltip.png")), "themed tooltip capture");
        require(tip->screen()->availableGeometry().contains(tip->geometry()), "tooltip stays on screen");
        QEvent leave(QEvent::Leave);
        QApplication::sendEvent(scene, &leave);
        require(!tip->isVisible(), "tooltip disappears on leave");
        QApplication::sendEvent(scene, &help);
        const auto initialClouds = scene->visibleNebulaClouds();
        for (int direction = 1; direction <= 4; ++direction) {
            drag(scene, QPointF(1.5707963267948966 / 0.0085, 0));
            require(!tip->isVisible(), "camera interaction dismisses tooltip");
            scene->grabFramebuffer();
            require(window.grab().save(output + QStringLiteral("/sky-direction-%1.png").arg(direction)),
                    "directional sky capture");
            if (direction == 2) {
                const auto back = scene->visibleNebulaClouds();
                require(!back.isEmpty(), "mouse rotation reveals rear clouds");
                for (int id : back) require(!initialClouds.contains(id), "no mirrored backside clouds");
            }
        }
        require(scene->visibleNebulaClouds() == initialClouds, "full mouse turn returns to the same clouds");
        drag(scene, QPointF(0, -1.15 / 0.007));
        bool upperCloud = false;
        for (int id : scene->visibleNebulaClouds()) upperCloud |= id >= 8 && id <= 10;
        require(upperCloud, "vertical mouse rotation reveals upper sky");
        scene->grabFramebuffer();
        require(window.grab().save(output + QStringLiteral("/sky-up.png")), "upper sky capture");
        drag(scene, QPointF(0, 2.30 / 0.007));
        bool lowerCloud = false;
        for (int id : scene->visibleNebulaClouds()) lowerCloud |= id >= 11;
        require(lowerCloud, "vertical mouse rotation reveals lower sky");
        scene->grabFramebuffer();
        require(window.grab().save(output + QStringLiteral("/sky-down.png")), "lower sky capture");
        mouse(scene, QEvent::MouseButtonDblClick, QPointF(180, 100), Qt::LeftButton, Qt::LeftButton);
        require(scene->visibleNebulaClouds() == initialClouds, "reset restores original sky direction");
        const QPointF initialAnchor = scene->spacecraftScreenPosition();
        drag(scene, QPointF(130, 35));
        // The orbit keeps advancing during the directional captures. Depending
        // on its phase, camera movement can be mostly in depth, not screen X/Y.
        require(QLineF(initialAnchor, scene->spacecraftScreenPosition()).length() > 1,
                "ship and telemetry anchor move with 3D camera");
        QTimer::singleShot(350, [&, scene] {
            require(window.grab().save(output + QStringLiteral("/scene-rotated.png")), "rotated screenshot");
            drag(scene, QPointF(200, -70));
            QTimer::singleShot(350, [&, scene] {
                require(window.grab().save(output + QStringLiteral("/scene-rear.png")), "rear view screenshot");
                drag(scene, QPointF(0, 120));
                scene->grabFramebuffer();
                require(window.grab().save(output + QStringLiteral("/scene-occlusion.png")), "occlusion screenshot");
                QWheelEvent wheel(QPointF(180, 100), scene->mapToGlobal(QPoint(180, 100)),
                                  QPoint(), QPoint(0, 120), Qt::NoButton, Qt::NoModifier,
                                  Qt::NoScrollPhase, false);
                QApplication::sendEvent(scene, &wheel);
                mouse(scene, QEvent::MouseButtonDblClick, QPointF(180, 100),
                      Qt::LeftButton, Qt::LeftButton);
                SystemSnapshot fixture;
                fixture.cpuUsagePercent = 73.5;
                fixture.cpuCoreCount = 24;
                fixture.memoryUsed = 8ULL << 30;
                fixture.memoryTotal = 32ULL << 30;
                fixture.memoryUsagePercent = 25;
                fixture.storageUsed = 300ULL << 30;
                fixture.storageTotal = 1000ULL << 30;
                fixture.storageUsagePercent = 30;
                fixture.networkReceiveBytesPerSecond = 1234567;
                fixture.networkTransmitBytesPerSecond = 765432;
                fixture.uptimeSeconds = 90061;
                scene->setSnapshot(fixture);
                auto *tipReadings = scene->findChild<QLabel *>(QStringLiteral("tooltipReadings"));
                require(tipReadings && tipReadings->text().contains(formatPercent(73.5))
                            && tipReadings->text().contains(formatBytes(8ULL << 30))
                            && tipReadings->text().contains(QStringLiteral("未检测到 GPU 数据")),
                        "themed tooltip updates CPU, RAM and missing GPU from the same snapshot");
                require(scene->accessibleDescription().contains(formatPercent(73.5)), "CPU mapping");
                require(scene->accessibleDescription().contains(formatBytes(8ULL << 30)), "RAM mapping");
                require(scene->accessibleDescription().contains(formatRate(1234567)), "network mapping");
                require(scene->accessibleDescription().contains(QStringLiteral("未检测到")), "GPU fallback");
                require(window.grab().save(output + QStringLiteral("/scene-missing-gpu.png")), "fallback capture");
                for (auto *button : window.findChildren<QToolButton *>())
                    if (button->isCheckable()) { button->click(); button->click(); break; }
                QTimer::singleShot(400, [&, scene] {
                    require(scene->isValid() && scene->spacecraftReady(), "context recreation");
                    scene->makeCurrent();
                    require(scene->context()->functions()->glGetError() == GL_NO_ERROR, "GL resource lifecycle");
                    scene->doneCurrent();
                    const double pausedAt = scene->flightElapsedSeconds();
                    window.hide();
                    QTimer::singleShot(350, [&, scene, pausedAt] {
                        require(scene->flightElapsedSeconds() == pausedAt, "hidden flight pauses");
                        window.show();
                        QTimer::singleShot(350, [&, scene, pausedAt] {
                            require(scene->flightElapsedSeconds() > pausedAt
                                        && scene->flightElapsedSeconds() < pausedAt + 0.65,
                                    "resume does not jump by hidden duration");
                            qInfo("PASS: spherical sky and 360-degree camera, directional cloud culling, serif fonts and Chinese fallback, NASA orbiter/textures, 48 s cruise, hide/resume, crisp rounded frame, telemetry, zoom/reset, GL lifecycle.");
                            window.hide();
                            QTimer::singleShot(1000, &app, [] {
                                qFatal("Smoke test failed: settings exit action did not quit");
                            });
                            quitAction->trigger();
                        });
                    });
                });
            });
        });
    });
    return app.exec();
}
