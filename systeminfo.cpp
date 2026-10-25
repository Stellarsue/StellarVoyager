#include "systeminfo.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QProcess>
#include <QRegularExpression>
#include <QStorageInfo>
#include <QStringList>
#include <QThread>

#include <algorithm>
#include <vector>

#if defined(Q_OS_MACOS)
#include "macfan.h"
#include <CoreFoundation/CFNumber.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
#include <mach/host_info.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <net/if.h>
#include <net/route.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#elif defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <iphlpapi.h>
#include <winioctl.h>
#endif

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

SystemInfoReader::SystemInfoReader()
    : m_cpuModel(readCpuModel())
{
}

SystemSnapshot SystemInfoReader::sample()
{
    SystemSnapshot snapshot;
    snapshot.cpuCoreCount = std::max(1, QThread::idealThreadCount());
    snapshot.cpuModel = m_cpuModel;
    snapshot.cpuTemperatureCelsius = readCpuTemperature();

    const CpuCounters currentCpu = readCpuCounters();
    if (currentCpu.valid && m_previousCpu.valid
        && currentCpu.total >= m_previousCpu.total
        && currentCpu.idle >= m_previousCpu.idle) {
        const quint64 totalDelta = currentCpu.total - m_previousCpu.total;
        const quint64 idleDelta = currentCpu.idle - m_previousCpu.idle;
        if (totalDelta > 0) {
            const double busy = static_cast<double>(totalDelta - std::min(idleDelta, totalDelta));
            snapshot.cpuUsagePercent = std::clamp((busy / static_cast<double>(totalDelta)) * 100.0,
                                                  0.0,
                                                  100.0);
            if (currentCpu.user >= m_previousCpu.user
                && currentCpu.system >= m_previousCpu.system) {
                const double scale = 100.0 / static_cast<double>(totalDelta);
                snapshot.cpuUserPercent = std::clamp(
                    double(currentCpu.user - m_previousCpu.user) * scale, 0.0, 100.0);
                snapshot.cpuSystemPercent = std::clamp(
                    double(currentCpu.system - m_previousCpu.system) * scale, 0.0, 100.0);
                snapshot.cpuIdlePercent = std::clamp(double(idleDelta) * scale, 0.0, 100.0);
            }
        }
    }
    m_previousCpu = currentCpu;

    readMemory(snapshot);
    readStorage(snapshot);
    const StorageCounters currentStorage = readStorageCounters();
    const qint64 storageNowMs = QDateTime::currentMSecsSinceEpoch();
    if (currentStorage.valid && m_previousStorage.valid && m_previousStorageTimestampMs > 0
        && currentStorage.readBytes >= m_previousStorage.readBytes
        && currentStorage.writeBytes >= m_previousStorage.writeBytes) {
        const double seconds = double(std::max<qint64>(1, storageNowMs - m_previousStorageTimestampMs))
                               / 1000.0;
        snapshot.storageReadBytesPerSecond =
            double(currentStorage.readBytes - m_previousStorage.readBytes) / seconds;
        snapshot.storageWriteBytesPerSecond =
            double(currentStorage.writeBytes - m_previousStorage.writeBytes) / seconds;
    }
    m_previousStorage = currentStorage;
    m_previousStorageTimestampMs = storageNowMs;
    readUptime(snapshot);
    snapshot.fanRpm = readFanRpm();

    const NetworkCounters currentNetwork = readNetworkCounters();
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (currentNetwork.interfaces.size() == 1) {
        snapshot.networkInterface = currentNetwork.interfaces.first();
    } else if (currentNetwork.interfaces.size() > 1) {
        snapshot.networkInterface =
            QStringLiteral("%1 +%2").arg(currentNetwork.interfaces.first())
                .arg(currentNetwork.interfaces.size() - 1);
    }
    snapshot.localIpv4Address = readLocalIpv4Address(currentNetwork.interfaces);
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
#if defined(Q_OS_MACOS)
    host_cpu_load_info_data_t load{};
    mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
    const mach_port_t host = mach_host_self();
    const kern_return_t status = host_statistics(host, HOST_CPU_LOAD_INFO,
                                                 reinterpret_cast<host_info_t>(&load), &count);
    mach_port_deallocate(mach_task_self(), host);
    if (status != KERN_SUCCESS) return {};
    CpuCounters result;
    for (int state = 0; state < CPU_STATE_MAX; ++state)
        result.total += static_cast<quint32>(load.cpu_ticks[state]);
    result.user = static_cast<quint32>(load.cpu_ticks[CPU_STATE_USER])
                  + static_cast<quint32>(load.cpu_ticks[CPU_STATE_NICE]);
    result.system = static_cast<quint32>(load.cpu_ticks[CPU_STATE_SYSTEM]);
    result.idle = static_cast<quint32>(load.cpu_ticks[CPU_STATE_IDLE]);
    result.valid = result.total > 0;
    return result;
#elif defined(Q_OS_WIN)
    FILETIME idle{}, kernel{}, user{};
    if (!GetSystemTimes(&idle, &kernel, &user)) return {};
    const auto ticks = [](const FILETIME &value) -> quint64 {
        return (quint64(value.dwHighDateTime) << 32) | value.dwLowDateTime;
    };
    CpuCounters result;
    result.idle = ticks(idle);
    result.user = ticks(user);
    result.system = ticks(kernel) >= result.idle ? ticks(kernel) - result.idle : 0;
    result.total = ticks(kernel) + ticks(user);
    result.valid = result.total > 0;
    return result;
#else
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
        result.user = user + nice;
        result.system = system + irq + softirq + steal;
        result.total = user + nice + system + idle + iowait + irq + softirq + steal;
        result.valid = result.total > 0;
        return result;
    }

    return {};
#endif
}

QString SystemInfoReader::readCpuModel() const
{
#if defined(Q_OS_MACOS)
    for (const char *key : {"machdep.cpu.brand_string", "hw.chip_name"}) {
        size_t size = 0;
        if (sysctlbyname(key, nullptr, &size, nullptr, 0) != 0 || size < 2 || size > 512)
            continue;
        std::vector<char> value(size, 0);
        if (sysctlbyname(key, value.data(), &size, nullptr, 0) == 0) {
            const QString model = QString::fromLocal8Bit(value.data()).simplified();
            if (!model.isEmpty())
                return model;
        }
    }
#elif defined(Q_OS_WIN)
    wchar_t value[256]{};
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_LOCAL_MACHINE,
                     L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                     L"ProcessorNameString", RRF_RT_REG_SZ, nullptr, value, &size) == ERROR_SUCCESS)
        return QString::fromWCharArray(value).simplified();
#else
    QFile file(QStringLiteral("/proc/cpuinfo"));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QStringList lines = QString::fromLocal8Bit(file.readAll()).split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            const int separator = line.indexOf(QLatin1Char(':'));
            if (separator < 0)
                continue;
            const QString key = line.left(separator).trimmed();
            if (key == QStringLiteral("model name") || key == QStringLiteral("Hardware")
                || key == QStringLiteral("Processor")) {
                const QString value = line.mid(separator + 1).simplified();
                if (!value.isEmpty())
                    return value;
            }
        }
    }
#endif
    return {};
}

double SystemInfoReader::readCpuTemperature() const
{
#if defined(Q_OS_MACOS)
    return readMacCpuTemperatureCelsius();
#elif defined(Q_OS_LINUX)
    const QDir hwmon(QStringLiteral("/sys/class/hwmon"));
    const QStringList devices = hwmon.entryList({QStringLiteral("hwmon*")}, QDir::Dirs, QDir::Name);
    for (const QString &device : devices) {
        const QDir sensors(hwmon.filePath(device));
        const QString sensorName = readTextFile(sensors.filePath(QStringLiteral("name"))).toLower();
        if (sensorName != QStringLiteral("coretemp") && sensorName != QStringLiteral("k10temp")
            && sensorName != QStringLiteral("zenpower")
            && sensorName != QStringLiteral("cpu_thermal"))
            continue;
        const QStringList inputs = sensors.entryList({QStringLiteral("temp*_input")}, QDir::Files,
                                                      QDir::Name);
        // Prefer a package/control reading to a single core's temperature.
        for (int pass = 0; pass < 2; ++pass) {
            for (const QString &input : inputs) {
                const QString prefix = input.left(input.size() - QStringLiteral("_input").size());
                const QString label = readTextFile(sensors.filePath(prefix + QStringLiteral("_label")))
                                          .toLower();
                const bool preferred = label.contains(QStringLiteral("package"))
                                       || label.contains(QStringLiteral("tctl"));
                if ((pass == 0) != preferred)
                    continue;
                double millidegrees = 0.0;
                if (readDoubleFile(sensors.filePath(input), millidegrees)) {
                    const double celsius = millidegrees / 1000.0;
                    if (celsius > 0.0 && celsius < 130.0)
                        return celsius;
                }
            }
        }
    }
#endif
    return -1.0;
}

SystemInfoReader::NetworkCounters SystemInfoReader::readNetworkCounters() const
{
#if defined(Q_OS_MACOS)
    int query[] = {CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST2, 0};
    size_t size = 0;
    if (sysctl(query, 6, nullptr, &size, nullptr, 0) != 0 || size == 0) return {};
    std::vector<char> data(size);
    if (sysctl(query, 6, data.data(), &size, nullptr, 0) != 0) return {};

    NetworkCounters result;
    for (size_t offset = 0; offset + sizeof(if_msghdr) <= size;) {
        const auto *message = reinterpret_cast<const if_msghdr *>(data.data() + offset);
        if (message->ifm_msglen == 0 || offset + message->ifm_msglen > size) break;
        if (message->ifm_type == RTM_IFINFO2 && message->ifm_msglen >= sizeof(if_msghdr2)) {
            const auto *interface = reinterpret_cast<const if_msghdr2 *>(message);
            if ((interface->ifm_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING)
                && !(interface->ifm_flags & IFF_LOOPBACK)) {
                char name[IF_NAMESIZE]{};
                if (if_indextoname(interface->ifm_index, name)) {
                    result.receive += interface->ifm_data.ifi_ibytes;
                    result.transmit += interface->ifm_data.ifi_obytes;
                    result.interfaces.append(QString::fromLocal8Bit(name));
                    result.valid = true;
                }
            }
        }
        offset += message->ifm_msglen;
    }
    return result;
#elif defined(Q_OS_WIN)
    MIB_IF_TABLE2 *table = nullptr;
    if (GetIfTable2(&table) != NO_ERROR || !table) return {};
    NetworkCounters result;
    for (ULONG index = 0; index < table->NumEntries; ++index) {
        const MIB_IF_ROW2 &interface = table->Table[index];
        if (interface.Type == IF_TYPE_SOFTWARE_LOOPBACK
            || interface.OperStatus != IfOperStatusUp) continue;
        result.receive += interface.InOctets;
        result.transmit += interface.OutOctets;
        result.interfaces.append(QString::fromWCharArray(interface.Alias));
        result.valid = true;
    }
    FreeMibTable(table);
    return result;
#else
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
#endif
}

QString SystemInfoReader::readLocalIpv4Address(const QStringList &interfaces) const
{
    const QList<QNetworkInterface> all = QNetworkInterface::allInterfaces();
    // Follow the active interfaces used for throughput, so the IP and rate
    // normally refer to the same connection.
    for (int pass = 0; pass < 2; ++pass) {
        for (const QNetworkInterface &interface : all) {
            const auto flags = interface.flags();
            if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning)
                || (flags & QNetworkInterface::IsLoopBack))
                continue;
            if (pass == 0 && !interfaces.contains(interface.name()))
                continue;
            for (const QNetworkAddressEntry &entry : interface.addressEntries()) {
                const QHostAddress ip = entry.ip();
                if (ip.protocol() != QAbstractSocket::IPv4Protocol
                    || ip.isNull() || ip.isLoopback())
                    continue;
                const QString address = ip.toString();
                if (!address.startsWith(QStringLiteral("169.254.")))
                    return address;
            }
        }
    }
    return {};
}

SystemInfoReader::StorageCounters SystemInfoReader::readStorageCounters() const
{
#if defined(Q_OS_MACOS)
    io_iterator_t iterator = IO_OBJECT_NULL;
    CFMutableDictionaryRef matching = IOServiceMatching("IOBlockStorageDriver");
    if (!matching || IOServiceGetMatchingServices(0, matching, &iterator) != KERN_SUCCESS)
        return {};

    StorageCounters result;
    io_service_t service = IO_OBJECT_NULL;
    while ((service = IOIteratorNext(iterator))) {
        CFTypeRef property = IORegistryEntryCreateCFProperty(
            service, CFSTR("Statistics"), kCFAllocatorDefault, 0);
        if (property && CFGetTypeID(property) == CFDictionaryGetTypeID()) {
            const CFDictionaryRef statistics = static_cast<CFDictionaryRef>(property);
            const auto readNumber = static_cast<CFNumberRef>(
                CFDictionaryGetValue(statistics, CFSTR("Bytes (Read)")));
            const auto writeNumber = static_cast<CFNumberRef>(
                CFDictionaryGetValue(statistics, CFSTR("Bytes (Write)")));
            qint64 readBytes = 0;
            qint64 writeBytes = 0;
            if (readNumber && writeNumber && CFGetTypeID(readNumber) == CFNumberGetTypeID()
                && CFGetTypeID(writeNumber) == CFNumberGetTypeID()
                && CFNumberGetValue(readNumber, kCFNumberSInt64Type, &readBytes)
                && CFNumberGetValue(writeNumber, kCFNumberSInt64Type, &writeBytes)
                && readBytes >= 0 && writeBytes >= 0) {
                result.readBytes += quint64(readBytes);
                result.writeBytes += quint64(writeBytes);
                result.valid = true;
            }
        }
        if (property)
            CFRelease(property);
        IOObjectRelease(service);
    }
    IOObjectRelease(iterator);
    return result;
#elif defined(Q_OS_WIN)
    StorageCounters result;
    for (unsigned index = 0; index < 16; ++index) {
        const QString path = QStringLiteral("\\\\.\\PhysicalDrive%1").arg(index);
        HANDLE handle = CreateFileW(reinterpret_cast<const wchar_t *>(path.utf16()), 0,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            continue;
        DISK_PERFORMANCE performance{};
        DWORD returned = 0;
        if (DeviceIoControl(handle, IOCTL_DISK_PERFORMANCE, nullptr, 0,
                            &performance, sizeof(performance), &returned, nullptr)
            && returned >= sizeof(performance)
            && performance.BytesRead.QuadPart >= 0
            && performance.BytesWritten.QuadPart >= 0) {
            result.readBytes += quint64(performance.BytesRead.QuadPart);
            result.writeBytes += quint64(performance.BytesWritten.QuadPart);
            result.valid = true;
        }
        CloseHandle(handle);
    }
    return result;
#elif defined(Q_OS_LINUX)
    const QByteArray device = QStorageInfo::root().device();
    if (!device.startsWith("/dev/"))
        return {};
    const QString devicePath = QFileInfo(QString::fromLocal8Bit(device)).canonicalFilePath();
    const QString blockName = QFileInfo(devicePath).fileName();
    if (blockName.isEmpty())
        return {};
    const QStringList fields = readTextFile(QStringLiteral("/sys/class/block/%1/stat").arg(blockName))
                                   .simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (fields.size() < 7)
        return {};
    bool readOk = false;
    bool writeOk = false;
    const quint64 readSectors = fields.at(2).toULongLong(&readOk);
    const quint64 writeSectors = fields.at(6).toULongLong(&writeOk);
    if (!readOk || !writeOk)
        return {};
    StorageCounters result;
    // Linux block accounting reports sectors in 512-byte units.
    result.readBytes = readSectors * 512ULL;
    result.writeBytes = writeSectors * 512ULL;
    result.valid = true;
    return result;
#else
    return {};
#endif
}

void SystemInfoReader::readMemory(SystemSnapshot &snapshot) const
{
#if defined(Q_OS_MACOS)
    quint64 totalBytes = 0;
    size_t size = sizeof(totalBytes);
    if (sysctlbyname("hw.memsize", &totalBytes, &size, nullptr, 0) != 0) return;
    vm_statistics64_data_t stats{};
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    const mach_port_t host = mach_host_self();
    const kern_return_t status = host_statistics64(host, HOST_VM_INFO64,
                                                   reinterpret_cast<host_info64_t>(&stats), &count);
    vm_size_t pageSize = 0;
    const kern_return_t pageStatus = host_page_size(host, &pageSize);
    mach_port_deallocate(mach_task_self(), host);
    if (status != KERN_SUCCESS || pageStatus != KERN_SUCCESS) return;
    snapshot.memoryTotal = totalBytes;
    // Inactive and purgeable pages can be reclaimed; active, wired and
    // compressed pages represent the current working memory load.
    const quint64 usedPages = quint64(stats.active_count) + stats.wire_count
                              + stats.compressor_page_count;
    snapshot.memoryUsed = std::min(totalBytes, usedPages * quint64(pageSize));
    if (totalBytes > 0)
        snapshot.memoryUsagePercent = double(snapshot.memoryUsed) / double(totalBytes) * 100.0;
#elif defined(Q_OS_WIN)
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    if (!GlobalMemoryStatusEx(&memory)) return;
    snapshot.memoryTotal = memory.ullTotalPhys;
    snapshot.memoryUsed = memory.ullTotalPhys - memory.ullAvailPhys;
    snapshot.memoryUsagePercent = memory.dwMemoryLoad;
#else
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
#endif
}

void SystemInfoReader::readStorage(SystemSnapshot &snapshot) const
{
    const QStorageInfo storage = QStorageInfo::root();
    if (!storage.isValid() || !storage.isReady())
        return;

    snapshot.storageName = storage.name();
    if (snapshot.storageName.isEmpty())
        snapshot.storageName = storage.displayName();

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
#if defined(Q_OS_MACOS)
    int query[] = {CTL_KERN, KERN_BOOTTIME};
    timeval boot{};
    size_t size = sizeof(boot);
    if (sysctl(query, 2, &boot, &size, nullptr, 0) != 0) return;
    snapshot.uptimeSeconds = std::max(0.0, QDateTime::currentSecsSinceEpoch()
                                              - double(boot.tv_sec) - double(boot.tv_usec) / 1e6);
#elif defined(Q_OS_WIN)
    snapshot.uptimeSeconds = double(GetTickCount64()) / 1000.0;
#else
    bool ok = false;
    const double uptime = readTextFile(QStringLiteral("/proc/uptime")).split(QLatin1Char(' '),
                                       Qt::SkipEmptyParts)
                              .value(0)
                              .toDouble(&ok);
    if (ok)
        snapshot.uptimeSeconds = std::max(0.0, uptime);
#endif
}

double SystemInfoReader::readFanRpm() const
{
#if defined(Q_OS_MACOS)
    return readMacFanRpm();
#elif defined(Q_OS_LINUX)
    const QDir hwmon(QStringLiteral("/sys/class/hwmon"));
    for (const QString &device : hwmon.entryList({QStringLiteral("hwmon*")}, QDir::Dirs, QDir::Name)) {
        const QDir sensors(hwmon.filePath(device));
        for (const QString &fan : sensors.entryList({QStringLiteral("fan*_input")}, QDir::Files, QDir::Name)) {
            double rpm = 0.0;
            if (readDoubleFile(sensors.filePath(fan), rpm) && rpm > 0.0)
                return rpm;
        }
    }
#endif
    return -1.0;
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
