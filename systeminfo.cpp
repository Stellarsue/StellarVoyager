#include "systeminfo.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QStringList>
#include <QThread>

#include <algorithm>

namespace
{
QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    return QString::fromLocal8Bit(file.readAll()).trimmed();
}

bool readUnsignedFile(const QString &path, quint64 &value)
{
    bool ok = false;
    const QString text = readTextFile(path);
    const quint64 parsed = text.toULongLong(&ok);
    if (ok)
        value = parsed;
    return ok;
}

bool readDoubleFile(const QString &path, double &value)
{
    bool ok = false;
    const QString text = readTextFile(path);
    const double parsed = text.toDouble(&ok);
    if (ok)
        value = parsed;
    return ok;
}

QString trimCsvValue(const QString &value)
{
    return value.trimmed();
}

QString shortenModelName(const QString &name)
{
    QString result = name.simplified();
    if (result.size() > 28)
        result = result.left(27) + QChar(0x2026);
    return result;
}
} // namespace

SystemInfoReader::SystemInfoReader() = default;

SystemSnapshot SystemInfoReader::sample()
{
    SystemSnapshot snapshot;
    snapshot.cpuCoreCount = std::max(1, QThread::idealThreadCount());

    const CpuCounters currentCpu = readCpuCounters();
    if (currentCpu.valid && m_previousCpu.valid) {
        const quint64 totalDelta = currentCpu.total - m_previousCpu.total;
        const quint64 idleDelta = currentCpu.idle - m_previousCpu.idle;
        if (totalDelta > 0) {
            const double busy = static_cast<double>(totalDelta - std::min(idleDelta, totalDelta));
            snapshot.cpuUsagePercent = std::clamp((busy / static_cast<double>(totalDelta)) * 100.0,
                                                  0.0,
                                                  100.0);
        }
    }
    m_previousCpu = currentCpu;

    readMemory(snapshot);
    readStorage(snapshot);
    readUptime(snapshot);

    const NetworkCounters currentNetwork = readNetworkCounters();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (currentNetwork.interfaces.size() == 1) {
        snapshot.networkInterface = currentNetwork.interfaces.first();
    } else if (currentNetwork.interfaces.size() > 1) {
        snapshot.networkInterface =
            QStringLiteral("%1 +%2").arg(currentNetwork.interfaces.first())
                .arg(currentNetwork.interfaces.size() - 1);
    }
    if (currentNetwork.valid && m_previousNetwork.valid && m_previousNetworkTimestampMs > 0) {
        const qint64 elapsedMs = std::max<qint64>(1, nowMs - m_previousNetworkTimestampMs);
        const double seconds = static_cast<double>(elapsedMs) / 1000.0;
        const quint64 receiveDelta = currentNetwork.receive >= m_previousNetwork.receive
                                         ? currentNetwork.receive - m_previousNetwork.receive
                                         : 0;
        const quint64 transmitDelta = currentNetwork.transmit >= m_previousNetwork.transmit
                                          ? currentNetwork.transmit - m_previousNetwork.transmit
                                          : 0;
        snapshot.networkReceiveBytesPerSecond =
            static_cast<double>(receiveDelta) / seconds;
        snapshot.networkTransmitBytesPerSecond =
            static_cast<double>(transmitDelta) / seconds;
        snapshot.networkReceiveBytesPerSecond =
            std::max(0.0, snapshot.networkReceiveBytesPerSecond);
        snapshot.networkTransmitBytesPerSecond =
            std::max(0.0, snapshot.networkTransmitBytesPerSecond);
    }
    m_previousNetwork = currentNetwork;
    m_previousNetworkTimestampMs = nowMs;

    snapshot.gpu = readGpu();
    return snapshot;
}

SystemInfoReader::CpuCounters SystemInfoReader::readCpuCounters() const
{
    QFile file(QStringLiteral("/proc/stat"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    const QStringList lines =
        QString::fromLocal8Bit(file.readAll()).split(QRegularExpression(QStringLiteral("[\r\n]+")),
                                                     Qt::SkipEmptyParts);
    for (const QString &rawLine : lines) {
        const QString line = rawLine.simplified();
        if (!line.startsWith(QStringLiteral("cpu ")))
            continue;

        const QStringList fields = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (fields.size() < 5)
            return {};

        quint64 user = 0;
        quint64 nice = 0;
        quint64 system = 0;
        quint64 idle = 0;
        quint64 iowait = 0;
        quint64 irq = 0;
        quint64 softirq = 0;
        quint64 steal = 0;
        bool ok = true;

        const QList<quint64 *> counters{&user, &nice, &system, &idle,
                                        &iowait, &irq, &softirq, &steal};
        for (int index = 0; index < counters.size(); ++index) {
            if (fields.size() <= index + 1)
                break;
            bool fieldOk = false;
            *counters.at(index) = fields.at(index + 1).toULongLong(&fieldOk);
            ok = ok && fieldOk;
        }

        if (!ok)
            return {};

        CpuCounters result;
        result.idle = idle + iowait;
        result.total = user + nice + system + idle + iowait + irq + softirq + steal;
        result.valid = result.total > 0;
        return result;
    }

    return {};
}

SystemInfoReader::NetworkCounters SystemInfoReader::readNetworkCounters() const
{
    QFile file(QStringLiteral("/proc/net/dev"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    NetworkCounters result;
    const QStringList lines =
        QString::fromLocal8Bit(file.readAll()).split(QRegularExpression(QStringLiteral("[\r\n]+")),
                                                     Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString rawLine = line.trimmed();
        if (!rawLine.contains(QLatin1Char(':')))
            continue;

        const int separator = rawLine.indexOf(QLatin1Char(':'));
        const QString interfaceName = rawLine.left(separator).trimmed();
        if (interfaceName.isEmpty() || interfaceName == QStringLiteral("lo"))
            continue;

        const QStringList values =
            rawLine.mid(separator + 1).simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (values.size() < 9)
            continue;

        bool receiveOk = false;
        bool transmitOk = false;
        const quint64 receive = values.at(0).toULongLong(&receiveOk);
        const quint64 transmit = values.at(8).toULongLong(&transmitOk);
        if (!receiveOk || !transmitOk)
            continue;

        result.receive += receive;
        result.transmit += transmit;
        result.interfaces.append(interfaceName);
        result.valid = true;
    }

    return result;
}

void SystemInfoReader::readMemory(SystemSnapshot &snapshot) const
{
    QFile file(QStringLiteral("/proc/meminfo"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    quint64 totalKb = 0;
    quint64 availableKb = 0;
    const QStringList lines =
        QString::fromLocal8Bit(file.readAll()).split(QRegularExpression(QStringLiteral("[\r\n]+")),
                                                     Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList fields = line.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (fields.size() < 2)
            continue;

        bool ok = false;
        const quint64 valueKb = fields.at(1).toULongLong(&ok);
        if (!ok)
            continue;

        if (fields.at(0) == QStringLiteral("MemTotal:"))
            totalKb = valueKb;
        else if (fields.at(0) == QStringLiteral("MemAvailable:"))
            availableKb = valueKb;
    }

    snapshot.memoryTotal = totalKb * 1024;
    snapshot.memoryUsed = snapshot.memoryTotal > availableKb * 1024
                              ? snapshot.memoryTotal - availableKb * 1024
                              : 0;
    if (snapshot.memoryTotal > 0) {
        snapshot.memoryUsagePercent =
            static_cast<double>(snapshot.memoryUsed) / static_cast<double>(snapshot.memoryTotal) * 100.0;
    }
}

void SystemInfoReader::readStorage(SystemSnapshot &snapshot) const
{
    const QStorageInfo storage(QStringLiteral("/"));
    if (!storage.isValid() || !storage.isReady())
        return;

    snapshot.storageTotal = static_cast<quint64>(std::max<qint64>(0, storage.bytesTotal()));
    const quint64 available = static_cast<quint64>(std::max<qint64>(0, storage.bytesAvailable()));
    snapshot.storageUsed = snapshot.storageTotal > available ? snapshot.storageTotal - available : 0;
    if (snapshot.storageTotal > 0) {
        snapshot.storageUsagePercent =
            static_cast<double>(snapshot.storageUsed) / static_cast<double>(snapshot.storageTotal) * 100.0;
    }
}

void SystemInfoReader::readUptime(SystemSnapshot &snapshot) const
{
    bool ok = false;
    const double uptime = readTextFile(QStringLiteral("/proc/uptime")).split(QLatin1Char(' '),
                                       Qt::SkipEmptyParts)
                              .value(0)
                              .toDouble(&ok);
    if (ok)
        snapshot.uptimeSeconds = std::max(0.0, uptime);
}

GpuSnapshot SystemInfoReader::readGpu() const
{
    QProcess process;
    process.setProgram(QStringLiteral("nvidia-smi"));
    process.setArguments({QStringLiteral("--query-gpu=name,utilization.gpu,memory.used,memory.total,temperature.gpu"),
                          QStringLiteral("--format=csv,noheader,nounits")});
    process.start();

    if (process.waitForStarted(250) && process.waitForFinished(800)) {
        const QString output = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
        const QStringList lines = output.split(QRegularExpression(QStringLiteral("[\r\n]+")),
                                               Qt::SkipEmptyParts);
        if (!lines.isEmpty()) {
            const QStringList values = lines.first().split(QLatin1Char(','), Qt::KeepEmptyParts);
            if (values.size() >= 5) {
                GpuSnapshot gpu;
                gpu.available = true;
                gpu.name = shortenModelName(trimCsvValue(values.at(0)));

                bool usageOk = false;
                bool usedOk = false;
                bool totalOk = false;
                bool temperatureOk = false;
                gpu.usagePercent = trimCsvValue(values.at(1)).toDouble(&usageOk);
                gpu.memoryUsed = trimCsvValue(values.at(2)).toULongLong(&usedOk) * 1024ULL * 1024ULL;
                gpu.memoryTotal = trimCsvValue(values.at(3)).toULongLong(&totalOk) * 1024ULL * 1024ULL;
                gpu.temperatureCelsius = trimCsvValue(values.at(4)).toDouble(&temperatureOk);

                if (!usageOk)
                    gpu.usagePercent = -1.0;
                if (!usedOk)
                    gpu.memoryUsed = 0;
                if (!totalOk)
                    gpu.memoryTotal = 0;
                if (!temperatureOk)
                    gpu.temperatureCelsius = -1.0;
                return gpu;
            }
        }
    }

    return readDrmGpu();
}

GpuSnapshot SystemInfoReader::readDrmGpu() const
{
    const QDir drmDirectory(QStringLiteral("/sys/class/drm"));
    const QStringList entries =
        drmDirectory.entryList({QStringLiteral("card*")}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

    const QRegularExpression cardPattern(QStringLiteral("^card\\d+$"));
    for (const QString &entry : entries) {
        if (!cardPattern.match(entry).hasMatch())
            continue;

        const QString devicePath = drmDirectory.filePath(entry + QStringLiteral("/device"));
        if (!QFileInfo::exists(devicePath))
            continue;

        GpuSnapshot gpu;
        gpu.available = true;

        const QString vendor = readTextFile(devicePath + QStringLiteral("/vendor")).toLower();
        if (vendor == QStringLiteral("0x1002"))
            gpu.name = QStringLiteral("AMD Radeon");
        else if (vendor == QStringLiteral("0x8086"))
            gpu.name = QStringLiteral("Intel Graphics");
        else if (vendor == QStringLiteral("0x10de"))
            gpu.name = QStringLiteral("NVIDIA GPU");
        else
            gpu.name = QStringLiteral("GPU (%1)").arg(entry);

        double usage = -1.0;
        if (readDoubleFile(devicePath + QStringLiteral("/gpu_busy_percent"), usage))
            gpu.usagePercent = std::clamp(usage, 0.0, 100.0);

        readUnsignedFile(devicePath + QStringLiteral("/mem_info_vram_used"), gpu.memoryUsed);
        readUnsignedFile(devicePath + QStringLiteral("/mem_info_vram_total"), gpu.memoryTotal);

        const QDir hwmonDirectory(devicePath + QStringLiteral("/hwmon"));
        const QStringList hwmonEntries =
            hwmonDirectory.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &hwmon : hwmonEntries) {
            const QDir sensorDirectory(hwmonDirectory.filePath(hwmon));
            const QStringList temperatures =
                sensorDirectory.entryList({QStringLiteral("temp*_input")}, QDir::Files, QDir::Name);
            if (temperatures.isEmpty())
                continue;

            quint64 millidegrees = 0;
            if (readUnsignedFile(sensorDirectory.filePath(temperatures.first()), millidegrees)) {
                gpu.temperatureCelsius = static_cast<double>(millidegrees) / 1000.0;
                break;
            }
        }

        return gpu;
    }

    return {};
}

QString formatBytes(quint64 bytes)
{
    const QStringList units{QStringLiteral("B"),
                            QStringLiteral("KB"),
                            QStringLiteral("MB"),
                            QStringLiteral("GB"),
                            QStringLiteral("TB")};
    double value = static_cast<double>(bytes);
    int unitIndex = 0;
    while (value >= 1024.0 && unitIndex < units.size() - 1) {
        value /= 1024.0;
        ++unitIndex;
    }

    const int decimals = unitIndex == 0 ? 0 : (value >= 10.0 ? 0 : 1);
    return QStringLiteral("%1 %2").arg(QString::number(value, 'f', decimals), units.at(unitIndex));
}

QString formatRate(double bytesPerSecond)
{
    return formatBytes(static_cast<quint64>(std::max(0.0, bytesPerSecond))) + QStringLiteral("/s");
}

QString formatPercent(double percent)
{
    return QStringLiteral("%1%").arg(QString::number(std::clamp(percent, 0.0, 100.0), 'f', 0));
}

QString formatUptime(double seconds)
{
    const qint64 totalSeconds = std::max<qint64>(0, static_cast<qint64>(seconds));
    const qint64 days = totalSeconds / 86400;
    const qint64 hours = (totalSeconds % 86400) / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;

    if (days > 0)
        return QStringLiteral("%1天 %2小时 %3分钟").arg(days).arg(hours).arg(minutes);
    if (hours > 0)
        return QStringLiteral("%1小时 %2分钟").arg(hours).arg(minutes);
    return QStringLiteral("%1分钟").arg(minutes);
}
