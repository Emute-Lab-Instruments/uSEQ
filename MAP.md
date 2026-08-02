# Map

Firmware and generated-WASM implementation of the uSEQ ModuLisp
compiler/control runtime. The canonical language entry point is
`docs/specs/MAIN.md`; `ALIGNMENT.md` records mission-level gaps.

## Repository layout

- `uSEQ/src/signal_engine/` — tokenizer, cold evaluator, graph builder, node
  pool, executor, diagnostics, state/resource ownership, and builtin table.
- `uSEQ/src/firmware/` — RP2040/RP2350 composition root, live tick loop,
  hardware I/O, storage, and serial protocol.
- `uSEQ/src/ports/` — platform interfaces and test doubles.
- `wasm/` — generated-runtime ABI wrapper and built interpreter artifacts;
  `useq-capabilities.json` is the compiler/interpreter profile record.
- `nodedef/` — separately built `osc/sine` NodeDef implementation and module
  descriptor.
- `test/signal_engine/` — native compiler/runtime, resource, projection,
  health, synth, and builtin suites.
- `test/firmware/` and `test/hardware/` — host-composed firmware and wire
  contract tests; these are not physical-device observations.
- `test/conformance/` — data-driven language corpus shared by native and
  generated-WASM adapters.
- `bench/firmware-corpus/` — declared moderate, combined-high, and focused
  near-boundary workloads for the native RP2040 capacity profile;
  `manifest.json` makes the intended utilization bands executable.
- `scripts/` — native/WASM builds, conformance adapters, manifest generation,
  NodeDef inspection, benchmarks, PlatformIO helpers, and RP2040 acceptance
  orchestration (`run_rp2040_profile.py`, `rp2040_memory_report.py`,
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
  simulator and physical-device measurement.

## Conventions and local boundaries

- Fixed capacities reject before publication; a rejected form must not alter
  live behavior or consume bounded resources.
- The RP2040 profile retains 64 synth declarations and 128 control rows, which
  covers every parameter combination in the shipped registry; host profiles
  retain the 512-row descriptor ceiling.
- Native and generated-WASM conformance use the same fixtures. A target build,
  firmware-capacity native process, simulator run, browser execution, and
  physical-device observation are separate evidence profiles.
- `scripts/build_wasm.sh` generates the interpreter and its compiler-owned
  capability record. NodeDef, application-served, and firmware-build fields
  belong to separate records.
- Generated records are deterministic and unsigned. They support exact-byte
  identity and drift detection, not publisher authentication.
- The `useq-perform` superproject owns the authoritative application pin and
  served bundle. Promote compiler changes by advancing its `src-useq` gitlink
  and rebuilding assets.
