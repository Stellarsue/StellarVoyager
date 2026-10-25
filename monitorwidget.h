#pragma once

#include "systeminfo.h"

#include <QPoint>
#include <QWidget>
#include <memory>

class QAction;
class QCloseEvent;
class QFrame;
class QMenu;
class QMouseEvent;
class QSystemTrayIcon;
class QTimer;
class QToolButton;
class StarMapWidget;
class StatusDashboard;
#if defined(Q_OS_MACOS)
class MacMenuBarStatus;
#endif

class SystemMonitorWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit SystemMonitorWidget(QWidget *parent = nullptr);
    ~SystemMonitorWidget() override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void refresh();
    void toggleAlwaysOnTop(bool enabled);
    void showFromTray();
    void quitFromTray();

private:
    void buildUi();
    void buildSettingsMenus();
    void populateSettingsMenu(QMenu *menu, bool fromDashboard);
    void buildTray();
    void placeAtTopRight();
    void updateSnapshot(const SystemSnapshot &snapshot);
    void toggleDashboardAt(const QPoint &globalAnchor);
    void setWindowStayOnTop(bool enabled);

    QFrame *m_header = nullptr;
    QToolButton *m_pinButton = nullptr;
    QToolButton *m_refreshButton = nullptr;
    QToolButton *m_settingsButton = nullptr;

    StarMapWidget *m_starMap = nullptr;

    QTimer *m_refreshTimer = nullptr;
    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_trayMenu = nullptr;
    QMenu *m_settingsMenu = nullptr;
    QAction *m_networkAction = nullptr;
    QAction *m_metricsAction = nullptr;
    QAction *m_showAction = nullptr;
    QAction *m_dashboardAction = nullptr;
    QAction *m_quitAction = nullptr;
    std::unique_ptr<StatusDashboard> m_dashboard;
#if defined(Q_OS_MACOS)
    std::unique_ptr<MacMenuBarStatus> m_menuBarStatus;
#endif

    SystemInfoReader m_reader;
    QPoint m_dragOffset;
    bool m_dragging = false;
    bool m_allowQuit = false;
    bool m_trayAvailable = false;
};
