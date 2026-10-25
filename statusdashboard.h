#pragma once

#include "systeminfo.h"

#include <QPoint>
#include <QWidget>

class DashboardCpuGraph;
class QLabel;
class QProgressBar;
class QScrollArea;
class QToolButton;

// A compact, platform-neutral detail popup for the menu-bar / tray status item.
// It owns no telemetry reader: callers feed it the same snapshot as the main UI.
class StatusDashboard final : public QWidget
{
public:
    explicit StatusDashboard(QWidget *parent = nullptr);

    void updateSnapshot(const SystemSnapshot &snapshot);
    void showAt(const QPoint &globalAnchor);
    QToolButton *settingsButton() const { return m_settingsButton; }

private:
    QScrollArea *m_scrollArea = nullptr;
    QWidget *m_scrollContent = nullptr;
    QToolButton *m_settingsButton = nullptr;
    DashboardCpuGraph *m_cpuGraph = nullptr;
    QLabel *m_cpuModel = nullptr;
    QLabel *m_cpuTemperature = nullptr;
    QLabel *m_cpuUser = nullptr;
    QLabel *m_cpuSystem = nullptr;
    QLabel *m_cpuIdle = nullptr;

    QLabel *m_storageName = nullptr;
    QLabel *m_storageCapacity = nullptr;
    QLabel *m_storageRead = nullptr;
    QLabel *m_storageWrite = nullptr;
    QProgressBar *m_storageBar = nullptr;

    QLabel *m_memoryCapacity = nullptr;
    QProgressBar *m_memoryBar = nullptr;

    QLabel *m_fanSpeed = nullptr;
    QLabel *m_networkDownload = nullptr;
    QLabel *m_networkUpload = nullptr;
    QLabel *m_networkAddress = nullptr;
};
