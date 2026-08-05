# Map

Firmware and generated-WASM implementation of the uSEQ ModuLisp
compiler/control runtime. The canonical language entry point is
`docs/specs/MAIN.md`; `ALIGNMENT.md` records mission-level gaps.

## Repository layout

- `uSEQ/src/signal_engine/` — tokenizer, cold evaluator, graph builder, node
  pool, executor, diagnostics, state/resource ownership, and builtin table.
- `uSEQ/src/firmware/` — RP2040/RP2350 composition root, live tick loop,
  hardware I/O, storage, serial protocol, build identity/capabilities
  (`build_info.h`), CRC-protected manufacturing identity
  (`factory_identity.{h,cpp}`), and I2C host/expander runtime; see
  `docs/specs/i2c-expander.md` for the output-expander wire contract.
- `uSEQ/src/ports/` — platform interfaces and test doubles, including the
  fixed-capacity fake I2C bus used by native host/expander simulation.
- `wasm/` — generated-runtime ABI wrapper and built interpreter artifacts;
  `useq-capabilities.json` is the compiler/interpreter profile record.
- `nodedef/` — separately built `osc/sine` NodeDef implementation and module
  descriptor.
- `test/signal_engine/` — native compiler/runtime, resource, projection,
  health, synth, and builtin suites.
- `test/firmware/` and `test/hardware/` — host-composed firmware, wire
  contract, and fake-bus output-expander tests; these are not physical-device
  observations.
- `test/scripts/` — structural contracts for acceptance runners and evidence
  evaluation without consuming hosted simulator quota.
- `test/conformance/` — data-driven language corpus shared by native and
  generated-WASM adapters.
- `bench/firmware-corpus/` — declared moderate, combined-high, and focused
  near-boundary workloads for the native RP2040 capacity profile;
  `manifest.json` makes the intended utilization bands executable.
- `wokwi/` — linted Music Thing RP2040 simulation circuit: two active-low
  gate inputs and a logic-analyzer view of `d1`, `d2`, `a4`, and `a3`.
- `scripts/` — native/WASM builds, conformance adapters, manifest generation,
  NodeDef inspection, benchmarks, PlatformIO helpers, and RP2040 acceptance
  orchestration (`run_rp2040_profile.py`, `run_wokwi_rp2040.py`,
  `run_rp2040_goal_gate.py`, `rp2040_memory_report.py`,
  `verify_firmware_no_synth.py`, `prepare_factory_identity.py`,
  `rp2040_budget.json`).
- `docs/specs/` — canonical language, runtime, firmware, diagnostics, state,
  synth, and protocol specifications; `docs/SEMANTICS.md` is a compatibility
  pointer only.
- `docs/testing/` — conformance and benchmark methodology.
- `hardware/`, `interfaces/`, `samples/` — hardware material, external
  interfaces, and example programs.
- `platformio.ini` — target firmware profiles; `meson.build` and
  `test/meson.build` — native build and suite registration. `musicthing` is
  the production profile; `musicthing-observe` adds structured telemetry for
  simulator and physical-device measurement. The RP2040 platform dependency
  is commit-pinned so target-size comparisons share one linker/tool package
  baseline.
- `shell.nix` — declared native/conformance/WASM/PlatformIO tool environment
  for local and unattended RP2040 candidate gates.

## Conventions and local boundaries

- Fixed capacities reject before publication; a rejected form must not alter
  live behavior or consume bounded resources.
- The synth compiler, NodeDef registry, and published synth artefact graph are
  host/WASM capabilities. Arduino and native firmware-profile builds exclude
  their translation units, state, symbols, and source strings; the linked ELF
  boundary is executable via `verify_firmware_no_synth.py`.
- Native and generated-WASM conformance use the same fixtures. A target build,
  firmware-capacity native process, simulator run, browser execution, and
  physical-device observation are separate evidence profiles.
- `scripts/build_wasm.sh` generates the interpreter and its compiler-owned
  capability record. Its tracked profile permits WASM memory growth but keeps
  fixed-length ArrayBuffer heap views because growable views are rejected by
  `TextDecoder` in supported Chromium releases. NodeDef, application-served,
  and firmware-build fields belong to separate records.
- Generated records are deterministic and unsigned. They support exact-byte
  identity and drift detection, not publisher authentication.
- The `useq-perform` superproject owns the authoritative application pin and
  served bundle. Promote compiler changes by advancing its `src-useq` gitlink
  and rebuilding assets.
