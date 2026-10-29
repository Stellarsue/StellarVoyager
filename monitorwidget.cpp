#include "monitorwidget.h"

#include "starmapwidget.h"
#include "statusdashboard.h"
#include "themeicons.h"
#include "themefonts.h"
#include "fontbundle.h"
#if defined(Q_OS_MACOS)
#include "macmenubarstatus.h"
#endif

#include <algorithm>
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QCursor>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGuiApplication>
#include <QGridLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QProcess>
#include <QSaveFile>
#include <QScreen>
#include <QScrollArea>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QString newsScriptPath()
{
    const QDir executableDir(QCoreApplication::applicationDirPath());
    const QStringList candidates = {
        executableDir.filePath(QStringLiteral("../automation/news_publisher.py")),
        executableDir.filePath(QStringLiteral("../share/stellarsue/automation/news_publisher.py")),
        QStringLiteral("/usr/share/stellarsue/automation/news_publisher.py")
    };
    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate))
            return QFileInfo(candidate).canonicalFilePath();
    }
    return {};
}

QString newsConfigPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/stellarvoyager/news.json");
}

QString newsStatePath()
{
    const QString stateRoot = qEnvironmentVariable("XDG_STATE_HOME",
                                                    QDir::homePath() + QStringLiteral("/.local/state"));
    return stateRoot + QStringLiteral("/stellarvoyager/news-state.json");
}

QJsonObject readJsonObject(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

} // namespace

SystemMonitorWidget::SystemMonitorWidget(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setWindowTitle(QStringLiteral("StellarVoyager · 飞行器遥测"));
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowIcon(ThemeIcons::applicationIcon());
    setFont(ThemeFonts::text());
    setFixedWidth(480);
    resize(480, 470);

    buildUi();
    m_dashboard = std::make_unique<StatusDashboard>();
    buildSettingsMenus();
    buildTray();

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(1000);
    connect(m_refreshTimer, &QTimer::timeout, this, &SystemMonitorWidget::refresh);
    m_refreshTimer->start();

    placeAtTopRight();
    refresh();
}

SystemMonitorWidget::~SystemMonitorWidget() = default;

void SystemMonitorWidget::buildUi()
{
    setStyleSheet(QStringLiteral(
        "QWidget {"
        " color: #fff8fa;"
        " font-family: \"Times New Roman\", \"Liberation Serif\", \"Noto Serif CJK SC\", serif;"
        "}"
        "QFrame#panel {"
        " background: transparent;"
        " border: none;"
        "}"
        "QFrame#header {"
        " background: rgba(7, 17, 27, 116);"
        " border: 1px solid rgba(185, 220, 233, 22);"
        " border-radius: 11px;"
        "}"
        "QLabel#brandMark { color: #9ce8ed; font-size: 17px; font-weight: 800; letter-spacing: -1px; }"
        "QLabel#appTitle { color: #f2f8ff; font-size: 16px; font-weight: 700; letter-spacing: 0.4px; }"
        "QLabel#appMode { color: #71cfe0; font-size: 9px; letter-spacing: 0.7px; }"
        "QLabel#liveBadge {"
        " color: #82e8b1;"
        " background: transparent;"
        " border: none;"
        " padding: 2px 4px;"
        " font-size: 9px;"
        " font-weight: 700;"
        "}"
        "QScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical {"
        " background: transparent;"
        " width: 6px;"
        " margin: 2px 0 2px 4px;"
        " border-radius: 3px;"
        "}"
        "QScrollBar::handle:vertical { background: rgba(159, 202, 218, 72); min-height: 32px; border-radius: 3px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QToolButton {"
        " background: rgba(255, 255, 255, 4);"
        " border: 1px solid rgba(186, 220, 232, 0);"
        " border-radius: 7px;"
        " padding: 4px;"
        " min-width: 16px;"
        " min-height: 16px;"
        "}"
        "QToolButton:hover { background: rgba(183, 222, 236, 18); border-color: rgba(183, 222, 236, 32); }"
        "QToolButton:pressed { background: rgba(0, 0, 0, 42); border-color: rgba(164, 210, 226, 22); }"
        "QToolButton:checked { background: rgba(255, 215, 122, 13); border-color: rgba(255, 215, 122, 18); }"
        "QToolButton::menu-indicator { image: none; width: 0px; height: 0px; }"
        "QToolButton#closeButton:hover { background: rgba(248, 113, 133, 38); border-color: rgba(248, 113, 133, 48); }"));

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    auto *panel = new QFrame(this);
    panel->setObjectName(QStringLiteral("panel"));
    outerLayout->addWidget(panel);

    auto *layout = new QGridLayout(panel);
    layout->setContentsMargins(6, 5, 6, 5);
    layout->setSpacing(0);

    m_header = new QFrame(panel);
    m_header->setObjectName(QStringLiteral("header"));
    m_header->setFixedHeight(31);
    m_header->installEventFilter(this);

    auto *headerLayout = new QHBoxLayout(m_header);
    headerLayout->setContentsMargins(8, 1, 5, 1);
    headerLayout->setSpacing(5);

    auto *brandMark = new QLabel(QStringLiteral("✦"), m_header);
    brandMark->setObjectName(QStringLiteral("brandMark"));
    brandMark->setAttribute(Qt::WA_TransparentForMouseEvents);
    headerLayout->addWidget(brandMark);

    auto *titleLabel = new QLabel(QStringLiteral("StellarVoyager"), m_header);
    titleLabel->setObjectName(QStringLiteral("appTitle"));
    titleLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    headerLayout->addWidget(titleLabel);

    auto *modeLabel = new QLabel(QStringLiteral("MISSION CONTROL"), m_header);
    modeLabel->setObjectName(QStringLiteral("appMode"));
    modeLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    headerLayout->addWidget(modeLabel);
    headerLayout->addStretch(1);

    auto *liveLabel = new QLabel(QStringLiteral("● LIVE"), m_header);
    liveLabel->setObjectName(QStringLiteral("liveBadge"));
    liveLabel->setAlignment(Qt::AlignCenter);
    liveLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    headerLayout->addWidget(liveLabel);

    m_pinButton = new QToolButton(m_header);
    m_pinButton->setIcon(ThemeIcons::toggleIcon(ThemeIcon::Pin,
                                                QColor(QStringLiteral("#E8D1D8")),
                                                QColor(QStringLiteral("#FFD77A"))));
    m_pinButton->setIconSize(QSize(15, 15));
    m_pinButton->setFixedSize(26, 26);
    m_pinButton->setCheckable(true);
    m_pinButton->setChecked(true);
    m_pinButton->setToolTip(QStringLiteral("切换窗口置顶"));
    m_pinButton->setAccessibleName(QStringLiteral("切换窗口置顶"));
    connect(m_pinButton, &QToolButton::toggled, this, &SystemMonitorWidget::toggleAlwaysOnTop);

    m_refreshButton = new QToolButton(m_header);
    m_refreshButton->setIcon(ThemeIcons::icon(ThemeIcon::Refresh));
    m_refreshButton->setIconSize(QSize(15, 15));
    m_refreshButton->setFixedSize(26, 26);
    m_refreshButton->setToolTip(QStringLiteral("立即刷新"));
    m_refreshButton->setAccessibleName(QStringLiteral("立即刷新"));
    connect(m_refreshButton, &QToolButton::clicked, this, &SystemMonitorWidget::refresh);

    m_settingsButton = new QToolButton(m_header);
    m_settingsButton->setObjectName(QStringLiteral("settingsButton"));
    m_settingsButton->setIcon(ThemeIcons::icon(ThemeIcon::Settings,
                                              QColor(QStringLiteral("#E8D1D8"))));
    m_settingsButton->setIconSize(QSize(16, 16));
    m_settingsButton->setFixedSize(26, 26);
    m_settingsButton->setPopupMode(QToolButton::InstantPopup);
    m_settingsButton->setToolTip(QStringLiteral("设置与功能菜单"));
    m_settingsButton->setAccessibleName(QStringLiteral("设置与功能菜单"));

    auto *closeButton = new QToolButton(m_header);
    closeButton->setObjectName(QStringLiteral("closeButton"));
    closeButton->setIcon(ThemeIcons::icon(ThemeIcon::Close));
    closeButton->setIconSize(QSize(15, 15));
    closeButton->setFixedSize(26, 26);
    closeButton->setToolTip(QStringLiteral("隐藏窗口（可从托盘重新打开）"));
    closeButton->setAccessibleName(QStringLiteral("隐藏窗口"));
    connect(closeButton, &QToolButton::clicked, this, &QWidget::close);

    headerLayout->addWidget(m_pinButton);
    headerLayout->addWidget(m_refreshButton);
    headerLayout->addWidget(m_settingsButton);
    headerLayout->addWidget(closeButton);
    auto *starScroll = new QScrollArea(panel);
    starScroll->setWidgetResizable(true);
    starScroll->setFrameShape(QFrame::NoFrame);
    starScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    starScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    starScroll->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));
    m_starMap = new StarMapWidget(starScroll);
    starScroll->setWidget(m_starMap);
    layout->addWidget(starScroll, 0, 0);
    layout->addWidget(m_header, 0, 0, Qt::AlignTop);
    m_header->raise();
}

void SystemMonitorWidget::buildSettingsMenus()
{
    m_settingsMenu = new QMenu(this);
    m_settingsMenu->setObjectName(QStringLiteral("mainSettingsMenu"));
    populateSettingsMenu(m_settingsMenu, false);
    m_settingsButton->setMenu(m_settingsMenu);

    auto *dashboardMenu = new QMenu(m_dashboard.get());
    dashboardMenu->setObjectName(QStringLiteral("dashboardSettingsMenu"));
    populateSettingsMenu(dashboardMenu, true);
    m_dashboard->settingsButton()->setMenu(dashboardMenu);
}

void SystemMonitorWidget::populateSettingsMenu(QMenu *menu, bool fromDashboard)
{
    menu->setStyleSheet(QStringLiteral(
        "QMenu { background: #F8FAFC; color: #202832; border: 1px solid #D7E0E7;"
        " border-radius: 10px; padding: 6px; font-family: Arial, sans-serif; font-size: 12px; }"
        "QMenu::item { padding: 6px 28px 6px 11px; border-radius: 5px; }"
        "QMenu::item:selected { background: #E1F1FA; color: #12394D; }"
        "QMenu::separator { height: 1px; background: #DFE6EB; margin: 5px 8px; }"));

    if (fromDashboard) {
        auto *show = menu->addAction(QStringLiteral("显示主窗口"));
        show->setObjectName(QStringLiteral("settingsShowWindowAction"));
        connect(show, &QAction::triggered, this, &SystemMonitorWidget::showFromTray);
    } else {
        auto *dashboard = menu->addAction(QStringLiteral("打开监控面板"));
        dashboard->setObjectName(QStringLiteral("settingsOpenDashboardAction"));
        connect(dashboard, &QAction::triggered, this, [this] {
            const QPoint anchor = QCursor::pos();
            QTimer::singleShot(0, this, [this, anchor] {
                if (m_dashboard && !m_dashboard->isVisible())
                    m_dashboard->showAt(anchor);
            });
        });
    }

    auto *refreshAction = menu->addAction(QStringLiteral("立即刷新数据"));
    refreshAction->setObjectName(QStringLiteral("settingsRefreshAction"));
    connect(refreshAction, &QAction::triggered, this, &SystemMonitorWidget::refresh);

    auto *pinAction = menu->addAction(QStringLiteral("主窗口置顶"));
    pinAction->setObjectName(QStringLiteral("settingsPinAction"));
    pinAction->setCheckable(true);
    pinAction->setChecked(m_pinButton->isChecked());
    connect(pinAction, &QAction::toggled, m_pinButton, &QToolButton::setChecked);
    connect(m_pinButton, &QToolButton::toggled, pinAction, &QAction::setChecked);

#if defined(Q_OS_LINUX)
    menu->addSeparator();
    auto *publishNews = menu->addAction(QStringLiteral("立即发布科技新闻"));
    publishNews->setObjectName(QStringLiteral("settingsPublishNewsAction"));
    connect(publishNews, &QAction::triggered, this, &SystemMonitorWidget::runNewsNow);
#endif

    if (FontBundle::available()) {
        menu->addSeparator();
        auto *fonts = menu->addAction(QStringLiteral("安装 Times New Roman…"));
        fonts->setObjectName(QStringLiteral("settingsInstallFontsAction"));
        connect(fonts, &QAction::triggered, this, [this] {
            FontBundle::offerInstallation(this, false);
        });
    }

    auto *about = menu->addAction(QStringLiteral("关于 StellarVoyager"));
    about->setObjectName(QStringLiteral("settingsAboutAction"));
    connect(about, &QAction::triggered, this, [this] {
        QMessageBox dialog(QMessageBox::Information,
                           QStringLiteral("关于 StellarVoyager"),
                           QStringLiteral("StellarVoyager %1\n本机实时系统监控")
                               .arg(QApplication::applicationVersion()),
                           QMessageBox::Ok, this);
        dialog.setOption(QMessageBox::Option::DontUseNativeDialog);
        dialog.setStyleSheet(QStringLiteral(
            "QMessageBox { background: #F6F7F8; color: #24282D;"
            " font-family: Arial, sans-serif; font-size: 12px; }"
            "QMessageBox QLabel { color: #24282D; background: transparent; }"
            "QMessageBox QPushButton { color: #24282D; background: #E8ECEF;"
            " border: 1px solid #C8CFD4; border-radius: 5px;"
            " padding: 5px 14px; min-width: 48px; }"
            "QMessageBox QPushButton:hover { background: #DCEEF9; }"));
        dialog.exec();
    });

    menu->addSeparator();
    auto *quit = menu->addAction(QStringLiteral("退出程序"));
    quit->setObjectName(QStringLiteral("settingsQuitAction"));
    quit->setIcon(ThemeIcons::icon(ThemeIcon::Power, QColor(QStringLiteral("#C74052"))));
    connect(quit, &QAction::triggered, this, &SystemMonitorWidget::quitFromTray);
}

void SystemMonitorWidget::buildTray()
{
#if defined(Q_OS_MACOS)
    m_menuBarStatus = std::make_unique<MacMenuBarStatus>(
        [this] {
            const QPoint anchor = QCursor::pos();
            QTimer::singleShot(0, this, [this, anchor] { toggleDashboardAt(anchor); });
        },
        [this] { showFromTray(); },
        [this] { FontBundle::offerInstallation(this, false); },
        [this] { quitFromTray(); },
        FontBundle::available());
    m_trayAvailable = true;
    return;
#endif
    m_trayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
    if (!m_trayAvailable)
        return;

    m_trayMenu = new QMenu(this);
    m_trayMenu->setStyleSheet(QStringLiteral(
        "QMenu { background: #101D29; color: #EEF6FA; border: 1px solid #344C5D;"
        " border-radius: 10px; padding: 6px; }"
        "QMenu::item { padding: 7px 24px 7px 9px; border-radius: 5px; }"
        "QMenu::item:selected { background: #1D4257; }"
        "QMenu::separator { height: 1px; background: #2B3F4D; margin: 5px 7px; }"));
    m_networkAction = m_trayMenu->addAction(QStringLiteral("↑ 0 B/s   ↓ 0 B/s"));
    m_networkAction->setEnabled(false);
    m_metricsAction = m_trayMenu->addAction(QStringLiteral("FAN N/A   SSD —   MEM —   CPU —"));
    m_metricsAction->setEnabled(false);
    m_trayMenu->addSeparator();
    m_dashboardAction = m_trayMenu->addAction(QStringLiteral("打开监控面板"));
    connect(m_dashboardAction, &QAction::triggered, this, [this] {
        const QPoint anchor = QCursor::pos();
        QTimer::singleShot(0, this, [this, anchor] { toggleDashboardAt(anchor); });
    });
    m_showAction = m_trayMenu->addAction(QStringLiteral("显示 StellarVoyager"));
    m_showAction->setIcon(ThemeIcons::icon(ThemeIcon::Show));
    connect(m_showAction, &QAction::triggered, this, &SystemMonitorWidget::showFromTray);

    if (FontBundle::available()) {
        auto *fonts = m_trayMenu->addAction(QStringLiteral("安装 Times New Roman（其他程序可用）…"));
        connect(fonts, &QAction::triggered, this, [this] { FontBundle::offerInstallation(this, false); });
    }

#if defined(Q_OS_LINUX)
    m_newsMenu = m_trayMenu->addMenu(QStringLiteral("科技新闻自动发布"));
    m_newsStatusAction = m_newsMenu->addAction(QStringLiteral("状态：未配置"));
    m_newsStatusAction->setEnabled(false);
    auto *schedule = m_newsMenu->addAction(QStringLiteral("北京时间 06、09、11、14、17、19、21 点"));
    schedule->setEnabled(false);
    m_newsMenu->addSeparator();
    m_newsEnabledAction = m_newsMenu->addAction(QStringLiteral("启用定时发布"));
    m_newsEnabledAction->setCheckable(true);
    connect(m_newsEnabledAction, &QAction::toggled, this, &SystemMonitorWidget::setNewsEnabled);
    auto *runNews = m_newsMenu->addAction(QStringLiteral("立即运行一次"));
    connect(runNews, &QAction::triggered, this, &SystemMonitorWidget::runNewsNow);
    auto *setupNews = m_newsMenu->addAction(QStringLiteral("查看启用方法…"));
    connect(setupNews, &QAction::triggered, this, &SystemMonitorWidget::showNewsSetup);
    connect(m_newsMenu, &QMenu::aboutToShow, this, &SystemMonitorWidget::updateNewsMenu);
#endif

    m_trayMenu->addSeparator();
    m_quitAction = m_trayMenu->addAction(QStringLiteral("退出"));
    m_quitAction->setIcon(ThemeIcons::icon(ThemeIcon::Power, QColor(QStringLiteral("#F99AAD"))));
    connect(m_quitAction, &QAction::triggered, this, &SystemMonitorWidget::quitFromTray);

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(ThemeIcons::applicationIcon());
    m_trayIcon->setToolTip(QStringLiteral("StellarVoyager · 本机实时遥测"));
    m_trayIcon->setContextMenu(m_trayMenu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            const QPoint anchor = QCursor::pos();
            QTimer::singleShot(0, this, [this, anchor] { toggleDashboardAt(anchor); });
        } else if (reason == QSystemTrayIcon::DoubleClick) {
            showFromTray();
        }
    });
    m_trayIcon->show();
}

void SystemMonitorWidget::placeAtTopRight()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;

    const QRect available = screen->availableGeometry();
    const int availableHeight = std::max(340, available.height() - 24);
    resize(width(), std::min(470, availableHeight));
    move(available.right() - width() - 22, available.top() + 12);
}

void SystemMonitorWidget::refresh()
{
    updateSnapshot(m_reader.sample());
}

void SystemMonitorWidget::updateSnapshot(const SystemSnapshot &snapshot)
{
    m_starMap->setSnapshot(snapshot);
    if (m_dashboard) m_dashboard->updateSnapshot(snapshot);
#if defined(Q_OS_MACOS)
    if (m_menuBarStatus)
        m_menuBarStatus->update(snapshot);
#endif
    const QString network = QStringLiteral("↑ %1   ↓ %2")
                                .arg(formatRate(snapshot.networkTransmitBytesPerSecond),
                                     formatRate(snapshot.networkReceiveBytesPerSecond));
    const QString fan = snapshot.fanRpm >= 0.0
                            ? QString::number(qRound(snapshot.fanRpm))
                            : QStringLiteral("N/A");
    const QString storage = snapshot.storageTotal > 0
                                ? formatPercent(snapshot.storageUsagePercent)
                                : QStringLiteral("N/A");
    const QString memory = snapshot.memoryTotal > 0
                               ? formatPercent(snapshot.memoryUsagePercent)
                               : QStringLiteral("N/A");
    const QString metrics = QStringLiteral("FAN %1   SSD %2   MEM %3   CPU %4")
                                .arg(fan,
                                     storage,
                                     memory,
                                     formatPercent(snapshot.cpuUsagePercent));
    if (m_networkAction) m_networkAction->setText(network);
    if (m_metricsAction) m_metricsAction->setText(metrics);
    if (m_trayIcon)
        m_trayIcon->setToolTip(QStringLiteral("StellarVoyager  %1  %2").arg(network, metrics));
}

void SystemMonitorWidget::toggleDashboardAt(const QPoint &globalAnchor)
{
    if (!m_dashboard) return;
    if (m_dashboard->isVisible()) {
        m_dashboard->hide();
    } else {
        m_dashboard->showAt(globalAnchor);
    }
}

void SystemMonitorWidget::toggleAlwaysOnTop(bool enabled)
{
    setWindowStayOnTop(enabled);
}

void SystemMonitorWidget::setWindowStayOnTop(bool enabled)
{
    Qt::WindowFlags flags = windowFlags();
    if (enabled)
        flags |= Qt::WindowStaysOnTopHint;
    else
        flags &= ~Qt::WindowStaysOnTopHint;

    setWindowFlags(flags);
    show();
}

void SystemMonitorWidget::showFromTray()
{
    show();
    raise();
    activateWindow();
}

void SystemMonitorWidget::quitFromTray()
{
    m_allowQuit = true;
    if (m_trayIcon)
        m_trayIcon->hide();
    qApp->quit();
}

void SystemMonitorWidget::updateNewsMenu()
{
    const QJsonObject settings = readJsonObject(newsConfigPath());
    const QJsonObject history = readJsonObject(newsStatePath());
    const bool configured = QFileInfo::exists(newsConfigPath());
    const bool enabled = settings.value(QStringLiteral("enabled")).toBool();
    m_newsEnabledAction->blockSignals(true);
    m_newsEnabledAction->setChecked(enabled);
    m_newsEnabledAction->blockSignals(false);
    const QString last = history.value(QStringLiteral("last_status")).toString();
    const QString finished = history.value(QStringLiteral("last_finished")).toString();
    const QHash<QString, QString> labels = {
        {QStringLiteral("running"), QStringLiteral("运行中")},
        {QStringLiteral("publishing"), QStringLiteral("发布中")},
        {QStringLiteral("skipped"), QStringLiteral("无新文章")},
        {QStringLiteral("published"), QStringLiteral("已发布")},
        {QStringLiteral("uncertain"), QStringLiteral("待核查")},
        {QStringLiteral("failed"), QStringLiteral("失败")}
    };
    m_newsStatusAction->setText(configured
        ? QStringLiteral("状态：%1%2").arg(last.isEmpty() ? QStringLiteral("待运行") : labels.value(last, last),
                                        finished.isEmpty() ? QString() : QStringLiteral(" · ") + finished.left(16))
        : QStringLiteral("状态：未配置"));
    m_newsStatusAction->setToolTip(history.value(QStringLiteral("last_message")).toString());
}

void SystemMonitorWidget::setNewsEnabled(bool enabled)
{
    const QString path = newsConfigPath();
    if (!QFileInfo::exists(path)) {
        showNewsSetup();
        updateNewsMenu();
        return;
    }
    QJsonObject settings = readJsonObject(path);
    settings.insert(QStringLiteral("enabled"), enabled);
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(QJsonDocument(settings).toJson(QJsonDocument::Indented)) < 0
        || !file.commit()) {
        QMessageBox::warning(this, QStringLiteral("科技新闻自动发布"),
                             QStringLiteral("无法保存自动发布设置。"));
    }
    updateNewsMenu();
}

void SystemMonitorWidget::runNewsNow()
{
    const QString stableScript = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/stellarvoyager/automation/news_publisher.py");
    const QString script = QFileInfo::exists(stableScript) ? stableScript : newsScriptPath();
    const QString python = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
        + QStringLiteral("/stellarvoyager/venv/bin/python");
    if (script.isEmpty() || !QFileInfo::exists(python)) {
        showNewsSetup();
        return;
    }
    if (m_newsProcess && m_newsProcess->state() != QProcess::NotRunning)
        return;
    auto *process = new QProcess(this);
    m_newsProcess = process;
    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError error) {
                if (error != QProcess::FailedToStart || m_newsProcess != process)
                    return;
                QMessageBox::warning(this, QStringLiteral("科技新闻自动发布"),
                                     QStringLiteral("无法启动任务程序。请重新运行设置脚本。"));
                m_newsProcess = nullptr;
                process->deleteLater();
            });
    connect(process, &QProcess::finished, this,
            [this, process](int exitCode, QProcess::ExitStatus) {
                if (m_newsProcess != process) {
                    process->deleteLater();
                    return;
                }
                updateNewsMenu();
                if (exitCode != 0) {
                    QString details = QString::fromUtf8(process->readAllStandardError()).trimmed();
                    if (details.isEmpty())
                        details = readJsonObject(newsStatePath())
                            .value(QStringLiteral("last_message")).toString();
                    QMessageBox::warning(this, QStringLiteral("科技新闻自动发布"),
                                         details.isEmpty() ? QStringLiteral("任务执行失败，请查看运行状态。") : details);
                }
                if (m_newsProcess == process)
                    m_newsProcess = nullptr;
                process->deleteLater();
            });
    process->start(python, {script, QStringLiteral("run"), QStringLiteral("--force")});
}

void SystemMonitorWidget::showNewsSetup()
{
    const QString script = newsScriptPath();
    const QString setup = script.isEmpty() ? QStringLiteral("automation/setup.sh")
        : QFileInfo(script).dir().filePath(QStringLiteral("setup.sh"));
    auto *box = new QMessageBox(QMessageBox::Information,
                                QStringLiteral("启用科技新闻自动发布"),
                                QStringLiteral("请在终端运行：\n\nbash '%1'\n\n"
                                               "请先在平常使用的 Edge 中登录 ChatGPT 和微博。"
                                               "请在 Edge 地址栏手动打开 edge://inspect，点“远程调试”并允许连接。"
                                               "然后启用七个北京时间定时点。发布状态可在此菜单查看。").arg(setup),
                                QMessageBox::Ok, this);
    box->setTextInteractionFlags(Qt::TextSelectableByMouse);
    box->exec();
    box->deleteLater();
}

bool SystemMonitorWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_header) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                m_dragOffset = mouseEvent->globalPosition().toPoint() - frameGeometry().topLeft();
#else
                m_dragOffset = mouseEvent->globalPos() - frameGeometry().topLeft();
#endif
                m_dragging = true;
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_dragging) {
            auto *mouseEvent = static_cast<QMouseEvent *>(event);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            move(mouseEvent->globalPosition().toPoint() - m_dragOffset);
#else
            move(mouseEvent->globalPos() - m_dragOffset);
#endif
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && m_dragging) {
            m_dragging = false;
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void SystemMonitorWidget::closeEvent(QCloseEvent *event)
{
    if (m_trayAvailable && !m_allowQuit) {
        hide();
        event->ignore();
        return;
    }

    m_allowQuit = true;
    if (m_trayIcon)
        m_trayIcon->hide();
    qApp->quit();
    QWidget::closeEvent(event);
}
