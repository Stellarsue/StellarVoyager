#pragma once

#include <QString>
#include <QStringList>
#include <QtGlobal>

struct GpuSnapshot
{
    bool available = false;
    QString name;
    double usagePercent = -1.0;
    quint64 memoryUsed = 0;
    quint64 memoryTotal = 0;
    double temperatureCelsius = -1.0;
};

struct SystemSnapshot
{
    double cpuUsagePercent = 0.0;
    int cpuCoreCount = 0;

    quint64 memoryUsed = 0;
    quint64 memoryTotal = 0;
    double memoryUsagePercent = 0.0;

    quint64 storageUsed = 0;
    quint64 storageTotal = 0;
    double storageUsagePercent = 0.0;

    double networkReceiveBytesPerSecond = 0.0;
    double networkTransmitBytesPerSecond = 0.0;
    QString networkInterface;

    double uptimeSeconds = 0.0;
    GpuSnapshot gpu;
};

class SystemInfoReader
{
public:
    SystemInfoReader();

    SystemSnapshot sample();

private:
    struct CpuCounters
    {
        quint64 total = 0;
        quint64 idle = 0;
        bool valid = false;
    };

    struct NetworkCounters
    {
        quint64 receive = 0;
        quint64 transmit = 0;
        QStringList interfaces;
        bool valid = false;
    };

    CpuCounters readCpuCounters() const;
    NetworkCounters readNetworkCounters() const;
    void readMemory(SystemSnapshot &snapshot) const;
    void readStorage(SystemSnapshot &snapshot) const;
    void readUptime(SystemSnapshot &snapshot) const;
    GpuSnapshot readGpu() const;
    GpuSnapshot readDrmGpu() const;

    CpuCounters m_previousCpu;
    NetworkCounters m_previousNetwork;
    qint64 m_previousNetworkTimestampMs = 0;
};

QString formatBytes(quint64 bytes);
QString formatRate(double bytesPerSecond);
QString formatPercent(double percent);
QString formatUptime(double seconds);
