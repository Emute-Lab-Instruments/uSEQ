#ifndef USEQ_FIRMWARE_BUILD_INFO_H
#define USEQ_FIRMWARE_BUILD_INFO_H

// One source of truth for firmware identity. Beta increments are ordinary
// SemVer prereleases: 1.2.0-beta.1, 1.2.0-beta.2, ...; the public release is
// 1.2.0. Release tooling may override USEQ_FIRMWARE_VERSION at compile time.
#ifndef USEQ_FIRMWARE_VERSION
#define USEQ_FIRMWARE_VERSION "1.2.0-beta.1"
#endif

namespace firmware::build_info {

inline constexpr int PROTOCOL_VERSION = 1;
inline constexpr const char* VERSION = USEQ_FIRMWARE_VERSION;

#if defined(MUSICTHING)
inline constexpr const char* HARDWARE_TARGET = "musicthing";
#elif defined(USEQHARDWARE_0_2)
inline constexpr const char* HARDWARE_TARGET = "hardware_v0_2";
#elif defined(USEQHARDWARE_1_0)
inline constexpr const char* HARDWARE_TARGET = "hardware_v1_0";
#else
inline constexpr const char* HARDWARE_TARGET = "unknown";
#endif

// Kept as a JSON fragment to avoid allocating a second dynamic builder during
// the boot handshake. Capability names are additive; editors ignore unknowns.
inline constexpr const char* CAPABILITIES_JSON =
    "[\"json-v1\",\"stream-v1\",\"diagnostics-v1\","
    "\"state-snapshot-v1\",\"live-inputs-v1\",\"calibration-v1\"]";

} // namespace firmware::build_info

#endif
