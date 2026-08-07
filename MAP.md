# Map

Firmware and generated-WASM implementation of the uSEQ ModuLisp
compiler/control runtime. The canonical language entry point is
`docs/specs/MAIN.md`; `ALIGNMENT.md` records mission-level gaps.

## Repository layout

- `uSEQ/src/signal_engine/` — tokenizer, cold evaluator, graph builder, node
  pool, executor, diagnostics, state/resource ownership, and builtin table;
  `compiler_pipeline.{h,cpp}` owns the fixed-capacity parse IR, source-write
  plans, and rollback transaction between graph construction and publication.
- `uSEQ/src/firmware/` — RP2040/RP2350 composition root, live tick loop,
  hardware I/O, storage, and serial protocol partitioned into framing,
  schema-table dispatch, encoding, and handlers; the checked-in generated
  protocol header comes from `protocol/useq-json-v1.json`; build identity and
  capabilities
  (`build_info.h`), CRC-protected manufacturing identity
  (`factory_identity.{h,cpp}`), and I2C host/expander runtime; see
  `docs/specs/i2c-expander.md` for the output-expander wire contract;
  `clock_sync.{h,cpp}` owns external-gate tempo estimation and phase lock.
- `uSEQ/src/ports/` — platform interfaces and test doubles, including the
  fixed-capacity fake I2C bus used by native host/expander simulation.
- `uSEQ/src/utils/json_cursor.h` — allocation-free bounded JSON field/object/
  array cursor shared by firmware serial commands, devtools, and WASM ABI
  readers; malformed-input and byte-fuzz coverage lives in `test/firmware/`.
- `wasm/` — stable C ABI exports over one host-session owner and separate
  evaluation, projection, and probe units, plus built interpreter artifacts;
  `useq-capabilities.json` is the compiler/interpreter profile record.
- `nodedef/` — separately built `osc/sine` NodeDef implementation and module
  descriptor.
- `test/signal_engine/` — native compiler/runtime, resource, projection,
  health, synth, and builtin suites.
- `test/firmware/` — host-composed firmware, wire contract, and fake-bus
  output-expander tests; these are not physical-device observations.
- `test/scripts/` — structural contracts for acceptance runners, generated
  records, and evidence evaluation without consuming hosted simulator quota.
- `test/COVERAGE_LEDGER.md` — spec-section to C++ test-case map with an
  explicit Strict/WARN/None/N-A grade per clause.
- `test/conformance/` — data-driven language corpus shared by native and
  generated-WASM adapters.
- `protocol/useq-json-v1.json` — canonical current JSON-v1 request, response,
  unsolicited-envelope, validation, support, and conformance-fixture catalog;
  pre-1.2 framed text is intentionally outside it.
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
  pointer only, and `docs/useq.md` is the non-normative pre-1.2 user document
  retained solely because `docs/specs/MAIN.md` §4.3 cites it for the removed
  `@`-prefix surface.
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
- `run_conformance.py` resolves exactly one native probe,
  `build/test/signal_engine_probe`; any other probe must be named with
  `--probe`. There is no candidate search, because a sibling build directory
  that shadowed `build/` would report conformance for stale source.
  `test/scripts/test_run_conformance.py` holds that contract.
- The native warning set is enabled, not suppressed: there are no `-Wno-*`
  flags, and a clean `ninja -C build` is expected to emit zero warnings, so a
  warm build directory cannot hide a new one. Both former suppressions were
  removed after measuring that each hid zero warnings —
  `-Wno-unused-parameter` and `-Wno-conversion`, the latter inert because
  `-Wconversion` is not implied by `-Wall`/`-Wextra`/`-Wpedantic`. Measure a
  warning-policy change from scratch; a warm build dir proves nothing.
- `scripts/build_wasm.sh` generates the interpreter and its compiler-owned
  capability record. Its tracked profile permits WASM memory growth but keeps
  fixed-length ArrayBuffer heap views because growable views are rejected by
  `TextDecoder` in supported Chromium releases. NodeDef, application-served,
  and firmware-build fields belong to separate records.
- Generated records are deterministic and unsigned. They support exact-byte
  identity and drift detection, not publisher authentication.
  `wasm/useq-capabilities.json` is gitignored build output, so it is only ever
  as current as the last `build_wasm.sh` run.
  `test/scripts/test_capability_provenance.py` fails when a clean worktree
  holds a record naming some other commit; the full toolchain-dependent check
  is the `verify` subcommand of `scripts/wasm_manifest.py`.
- `MAP.md`, `CLAUDE.md`, and `README.md` are the orientation docs, and every
  repo path they cite must exist, enforced by
  `test/scripts/test_doc_source_paths.py`. `docs/specs/` is deliberately
  outside that gate: a spec may name a path that does not exist yet, and
  checking those produces noise rather than drift signal.
- The `useq-perform` superproject owns the authoritative application pin and
  served bundle. Promote compiler changes by advancing its `src-useq` gitlink
  and rebuilding assets.
