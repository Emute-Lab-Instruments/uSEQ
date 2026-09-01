#ifndef SIGNAL_ENGINE_BUILD_PROFILE_H
#define SIGNAL_ENGINE_BUILD_PROFILE_H

// The synth compiler and host artefact graph belong to the desktop/WASM
// profile. Firmware and the native firmware-capacity harness deliberately do
// not expose or retain that domain. Derive this capability from the target so
// an Arduino build cannot accidentally opt it back in with a stale flag.
#if defined(ARDUINO) || defined(USEQ_FIRMWARE_PROFILE)
#define USEQ_HAS_SYNTH_ENGINE 0
#else
#define USEQ_HAS_SYNTH_ENGINE 1
#endif

// Temporal queries are a host/WASM cold-path capability. Firmware keeps them
// out of both flash and SRAM; browser builds carry them through the ordinary
// ModuLisp eval surface, so no second JavaScript query API is required.
#if defined(ARDUINO) || defined(USEQ_FIRMWARE_PROFILE)
#define USEQ_HAS_TEMPORAL_QUERY 0
#else
#define USEQ_HAS_TEMPORAL_QUERY 1
#endif

#endif // SIGNAL_ENGINE_BUILD_PROFILE_H
