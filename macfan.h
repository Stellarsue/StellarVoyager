#pragma once

// Returns the fastest available fan's actual speed in RPM, or -1 when the
// hardware/SMC interface does not expose a readable fan speed.
#if defined(__APPLE__)
double readMacFanRpm();
// Best-effort AppleSMC CPU sensor reading; unsupported hardware returns -1.
double readMacCpuTemperatureCelsius();
#else
inline double readMacFanRpm() { return -1.0; }
inline double readMacCpuTemperatureCelsius() { return -1.0; }
#endif
