# uSEQ TP1 Refactor Checklist

- Epic: `useq-tp1`
- Contract ADR: `docs/architecture/adr-0001-modular-refactor-boundaries.md`
- Baseline metrics: `docs/architecture/useq-tp1-baseline-metrics.csv`
- TP1.2 test matrix: `docs/architecture/useq-tp1.2-test-matrix.md`
- Last updated: 2026-02-11

## Epic-Level Gates

- [x] Baseline boundary contract captured (this task: `useq-tp1.1`).
- [x] Baseline hotspot metrics captured for before/after comparison.
- [ ] Each subtask preserves or improves desktop test coverage for touched areas.
- [ ] Each subtask documents boundary changes against ADR-0001.
- [ ] Final epic closeout includes metric deltas and dead-path cleanup evidence.

## Subtask Tracking

- [x] `useq-tp1.1` Create baseline architecture map and dependency contract.
- [x] `useq-tp1.2` Expand refactor safety test harnesses before major extraction.
- [ ] `useq-tp1.3` Introduce runtime interfaces and delegation seams (no behavior change).
- [ ] `useq-tp1.4` Extract CommandRouter from `uSEQ.cpp` (serial/JSON dispatch isolation).
- [ ] `useq-tp1.5` Extract output evaluation service from `uSEQ_update.cpp`.
- [ ] `useq-tp1.6` Refactor persistence into ProgramStore adapters and resolve flash-path inconsistency.
- [ ] `useq-tp1.7` Unify Serial/JSON/I2C transport adapters and pure protocol codecs.
- [ ] `useq-tp1.8` Split IOManager into board profiles and focused input/output modules.
- [ ] `useq-tp1.9` Add shared builtin/API validation helpers (arity/type/eval/error).
- [ ] `useq-tp1.10` Split `builtins.cpp` into domain modules with stable registration.
- [ ] `useq-tp1.11` Split `modulisp_api.cpp` by domain and remove duplicated implementations.
- [ ] `useq-tp1.12` Decompose `value.cpp` into core + signal metadata policies.
- [ ] `useq-tp1.13` Clean parser/environment APIs and remove duplicate wrapper paths.
- [ ] `useq-tp1.14` Remove global/static interpreter state via explicit dependency injection.
- [ ] `useq-tp1.15` Unify timing/phasor ownership across `modulisp_time` and `phasor_manager`.
- [ ] `useq-tp1.16` Refactor I2C host/client layer to remove extern-global state.
- [ ] `useq-tp1.17` Targeted builtin correctness audit (`ard_lerp` + diagnostics consistency).
- [ ] `useq-tp1.18` Integration hardening: remove dead paths, update docs, and closeout metrics.

## Definition of Done per Subtask

- [ ] Scope matches one bounded context or one explicit seam extraction.
- [ ] No forbidden dependency direction is introduced (see ADR-0001).
- [ ] New interfaces are covered by tests with mocks/fakes on desktop.
- [ ] Runtime behavior remains unchanged unless task explicitly declares behavior change.
- [ ] Docs updated if ownership or dependency rules shift.
