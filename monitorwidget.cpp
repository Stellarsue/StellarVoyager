#include "monitorwidget.h"

#include "starmapwidget.h"
#include "themeicons.h"
#include "themefonts.h"
#include "fontbundle.h"

#include <algorithm>
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QScreen>
#include <QScrollArea>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

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
    buildTray();

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(1000);
    connect(m_refreshTimer, &QTimer::timeout, this, &SystemMonitorWidget::refresh);
    m_refreshTimer->start();

    placeAtTopRight();
    refresh();
}

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
        " background: transparent;"
        " border: none;"
        " border-radius: 9px;"
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
        "QScrollBar::handle:vertical { background: rgba(255, 235, 239, 66); min-height: 32px; border-radius: 3px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
        "QToolButton {"
        " background: transparent;"
        " border: none;"
        " border-radius: 7px;"
        " padding: 4px;"
        " min-width: 16px;"
        " min-height: 16px;"
        "}"
        "QToolButton:hover { background: rgba(255, 255, 255, 20); }"
        "QToolButton:pressed { background: rgba(0, 0, 0, 34); }"
        "QToolButton:checked { background: rgba(255, 215, 122, 12); }"
        "QToolButton#closeButton:hover { background: rgba(248, 113, 133, 38); }"));

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

void SystemMonitorWidget::buildTray()
{
    m_trayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
    if (!m_trayAvailable)
        return;

    m_trayMenu = new QMenu(this);
    m_trayMenu->setStyleSheet(QStringLiteral(
        "QMenu { background: #2d1822; color: #f8e9ed; border: 1px solid #684050;"
        " border-radius: 8px; padding: 6px; }"
        "QMenu::item { padding: 7px 24px 7px 9px; border-radius: 5px; }"
        "QMenu::item:selected { background: #563044; }"
        "QMenu::separator { height: 1px; background: #573244; margin: 5px 7px; }"));
    m_showAction = m_trayMenu->addAction(QStringLiteral("显示 StellarVoyager"));
    m_showAction->setIcon(ThemeIcons::icon(ThemeIcon::Show));
    connect(m_showAction, &QAction::triggered, this, &SystemMonitorWidget::showFromTray);

    if (FontBundle::available()) {
        auto *fonts = m_trayMenu->addAction(QStringLiteral("安装 Times New Roman（其他程序可用）…"));
        connect(fonts, &QAction::triggered, this, [this] { FontBundle::offerInstallation(this, false); });
    }

    m_trayMenu->addSeparator();
    m_quitAction = m_trayMenu->addAction(QStringLiteral("退出"));
    m_quitAction->setIcon(ThemeIcons::icon(ThemeIcon::Power, QColor(QStringLiteral("#F99AAD"))));
    connect(m_quitAction, &QAction::triggered, this, &SystemMonitorWidget::quitFromTray);

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(ThemeIcons::applicationIcon());
    m_trayIcon->setToolTip(QStringLiteral("StellarVoyager · 本机实时遥测"));
    m_trayIcon->setContextMenu(m_trayMenu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            showFromTray();
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
