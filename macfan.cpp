#include "macfan.h"

#include <IOKit/IOKitLib.h>
#include <mach/mach.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

namespace
{
// AppleSMC's user-client protocol is undocumented. Keep all access here
// read-only and treat unavailable or malformed values as unsupported.
constexpr uint32_t smcCode(char a, char b, char c, char d)
{
    return (static_cast<uint32_t>(static_cast<uint8_t>(a)) << 24)
           | (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 16)
           | (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 8)
           | static_cast<uint32_t>(static_cast<uint8_t>(d));
}

struct SmcVersion
{
    uint8_t major;
    uint8_t minor;
    uint8_t build;
    uint8_t reserved;
    uint16_t release;
};

struct SmcPowerLimit
{
    uint16_t version;
    uint16_t length;
    uint32_t cpu;
    uint32_t gpu;
    uint32_t memory;
};

struct SmcKeyInfo
{
    uint32_t size;
    uint32_t type;
    uint8_t attributes;
};

struct SmcKeyData
{
    uint32_t key;
    SmcVersion version;
    SmcPowerLimit powerLimit;
    SmcKeyInfo keyInfo;
    uint8_t result;
    uint8_t status;
    uint8_t command;
    uint32_t data32;
    uint8_t bytes[32];
};

static_assert(sizeof(SmcKeyData) == 80, "Unexpected AppleSMC request layout");

constexpr uint8_t readBytesCommand = 5;
constexpr uint8_t readKeyInfoCommand = 9;
constexpr uint32_t fanCountKey = smcCode('F', 'N', 'u', 'm');
constexpr uint32_t fixedPointType = smcCode('f', 'p', 'e', '2');
constexpr uint32_t floatType = smcCode('f', 'l', 't', ' ');
constexpr uint32_t temperatureType = smcCode('s', 'p', '7', '8');
constexpr uint32_t byteType = smcCode('u', 'i', '8', ' ');

class SmcConnection
{
public:
    SmcConnection()
    {
        CFMutableDictionaryRef matching = IOServiceMatching("AppleSMC");
        if (!matching)
            return;

        const io_service_t service = IOServiceGetMatchingService(0, matching);
        if (!service)
            return;

        const kern_return_t result = IOServiceOpen(service, mach_task_self(), 0, &m_connection);
        IOObjectRelease(service);
        if (result != KERN_SUCCESS)
            m_connection = IO_OBJECT_NULL;
    }

    ~SmcConnection()
    {
        if (m_connection)
            IOServiceClose(m_connection);
    }

    SmcConnection(const SmcConnection &) = delete;
    SmcConnection &operator=(const SmcConnection &) = delete;

    bool isOpen() const { return m_connection != IO_OBJECT_NULL; }

    bool read(uint32_t key, SmcKeyInfo &info, uint8_t (&bytes)[32]) const
    {
        if (!isOpen())
            return false;

        SmcKeyData request{};
        SmcKeyData response{};
        request.key = key;
        request.command = readKeyInfoCommand;
        if (!call(request, response)
            || response.keyInfo.size == 0
            || response.keyInfo.size > sizeof(response.bytes))
            return false;

        info = response.keyInfo;
        request = {};
        request.key = key;
        request.command = readBytesCommand;
        request.keyInfo.size = info.size;
        response = {};
        if (!call(request, response))
            return false;

        std::memcpy(bytes, response.bytes, info.size);
        return true;
    }

private:
    bool call(const SmcKeyData &request, SmcKeyData &response) const
    {
        size_t responseSize = sizeof(response);
        const kern_return_t result = IOConnectCallStructMethod(
            m_connection, 2, &request, sizeof(request), &response, &responseSize);
        return result == KERN_SUCCESS && responseSize == sizeof(response)
               && response.result == 0;
    }

    io_connect_t m_connection = IO_OBJECT_NULL;
};

bool decodeRpm(const SmcKeyInfo &info, const uint8_t (&bytes)[32], double &rpm)
{
    if (info.type == fixedPointType && info.size == 2) {
        rpm = static_cast<double>((static_cast<uint16_t>(bytes[0]) << 8) | bytes[1]) / 4.0;
    } else if (info.type == floatType && info.size == sizeof(float)) {
        float value = 0.0f;
        std::memcpy(&value, bytes, sizeof(value));
        rpm = static_cast<double>(value);
    } else {
        return false;
    }
    return std::isfinite(rpm) && rpm >= 0.0 && rpm <= 20000.0;
}

bool readFan(const SmcConnection &smc, int index, double &rpm)
{
    // AppleSMC fan keys use one hexadecimal digit: F0Ac, F1Ac, ...
    constexpr char digits[] = "0123456789ABCDEF";
    if (index < 0 || index >= 16)
        return false;

    SmcKeyInfo info{};
    uint8_t bytes[32]{};
    if (!smc.read(smcCode('F', digits[index], 'A', 'c'), info, bytes))
        return false;
    return decodeRpm(info, bytes, rpm);
}

bool decodeTemperature(const SmcKeyInfo &info, const uint8_t (&bytes)[32], double &celsius)
{
    if (info.type == temperatureType && info.size == 2) {
        celsius = double(static_cast<int8_t>(bytes[0])) + double(bytes[1]) / 256.0;
    } else if (info.type == floatType && info.size == sizeof(float)) {
        float value = 0.0f;
        std::memcpy(&value, bytes, sizeof(value));
        celsius = double(value);
    } else {
        return false;
    }
    return std::isfinite(celsius) && celsius > 0.0 && celsius < 130.0;
}
} // namespace

double readMacFanRpm()
{
    const SmcConnection smc;
    if (!smc.isOpen())
        return -1.0;

    SmcKeyInfo info{};
    uint8_t bytes[32]{};
    int fanCount = 1; // Some Macs omit FNum but still expose F0Ac.
    if (smc.read(fanCountKey, info, bytes) && info.type == byteType && info.size == 1)
        fanCount = bytes[0];
    if (fanCount <= 0)
        return -1.0;

    double fastestRpm = -1.0;
    for (int i = 0; i < std::min(fanCount, 16); ++i) {
        double rpm = 0.0;
        if (readFan(smc, i, rpm))
            fastestRpm = std::max(fastestRpm, rpm);
    }
    return fastestRpm;
}

double readMacCpuTemperatureCelsius()
{
    const SmcConnection smc;
    if (!smc.isOpen())
        return -1.0;

    // CPU proximity/die sensors are common on Intel Macs. Apple Silicon may
    // expose neither key through this user client, in which case the reading
    // remains unavailable rather than showing a guessed temperature.
    for (const uint32_t key : {smcCode('T', 'C', '0', 'P'), smcCode('T', 'C', '0', 'D')}) {
        SmcKeyInfo info{};
        uint8_t bytes[32]{};
        double celsius = -1.0;
        if (smc.read(key, info, bytes) && decodeTemperature(info, bytes, celsius))
            return celsius;
    }
    return -1.0;
}
