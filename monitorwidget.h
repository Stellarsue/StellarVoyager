#pragma once

#include "systeminfo.h"

#include <QPoint>
#include <QWidget>

class QAction;
class QCloseEvent;
class QFrame;
class QMenu;
class QMouseEvent;
class QSystemTrayIcon;
class QTimer;
class QToolButton;
class StarMapWidget;

class SystemMonitorWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit SystemMonitorWidget(QWidget *parent = nullptr);
    ~SystemMonitorWidget() override = default;

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
    void buildTray();
    void placeAtTopRight();
    void updateSnapshot(const SystemSnapshot &snapshot);
    void setWindowStayOnTop(bool enabled);

    QFrame *m_header = nullptr;
    QToolButton *m_pinButton = nullptr;
    QToolButton *m_refreshButton = nullptr;

    StarMapWidget *m_starMap = nullptr;

    QTimer *m_refreshTimer = nullptr;
    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_trayMenu = nullptr;
    QAction *m_showAction = nullptr;
    QAction *m_quitAction = nullptr;

    SystemInfoReader m_reader;
    QPoint m_dragOffset;
    bool m_dragging = false;
    bool m_allowQuit = false;
    bool m_trayAvailable = false;
};
