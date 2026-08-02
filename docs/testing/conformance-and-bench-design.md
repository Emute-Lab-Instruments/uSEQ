# Conformance & Benchmark Suite Design

Status: native and generated-WASM fixture adapters and the native benchmark are
implemented; the serial adapter, cross-adapter diff reporting, fuzzing, and
WASM/RP2040 benchmark lanes remain proposed.
Companion to `docs/specs/MAIN.md`.

## Part 1 — Implementation-independent conformance suite

### Principles

- **Black-box**: every case interacts with the engine only through a small probe
  protocol — no C++ includes, no internal headers. The suite must run unchanged
  against the native build, the WASM build, a future rewrite, and (subset) real
  hardware over serial.
- **Spec-anchored**: every case cites the normative spec section it verifies
  (`spec: state-identity.md §6.6`). A failing case means either the
  implementation or the spec is wrong — never "stale test"; triage against the
  spec, same doctrine as useq-perform's YAML suite.
- **Data-driven**: cases are YAML fixtures, cheap for humans and agents to add.
  Every bug found (e.g. audit findings A1–A14, F-R1/R2) lands as a fixture
  before/alongside its fix.

### Layer 0: the probe contract (the only implementation-coupled piece)

Extend `test/signal_engine/signal_engine_probe.cpp` from one-shot eval into a
**session probe**: JSONL request/response over stdin/stdout.

Ops:

```jsonc
{"op":"eval",   "code":"(define x 5)"}                    // → {"ok":true} | {"ok":false,"diagnostics":[{"category":"Boundary","span":[5,7],"msg":"…"}]}
{"op":"sample", "output":"a1", "times":[0,0.25,0.5]}      // → {"ok":true,"values":[…]}
{"op":"tick",   "t":0.5}                                   // advance engine time (state commits)
{"op":"clear"}                                             // useq-clear
{"op":"health", "output":"a1"}                            // → running|fallback|error|idle
{"op":"config", "opt_level":0}                             // optional: disable compile optimizations (for differential soundness testing)
```

The native binary and generated-WASM Node adapter implement this contract.
The WASM adapter loads the freshly generated `wasm/useq.js` and
`wasm/useq.wasm` artifacts and translates their exported diagnostics into the
same JSONL shape. Hardware still requires a serial adapter speaking the wire
protocol (fw-safe subset only).

The generated WASM capability manifest enumerates the fixed compiler limits,
their cross-field relations, public-versus-internal output counts, and bounded
counter domains. A component maximum is not a promise that every maximum is
simultaneously realizable; combination witnesses remain separate.

### Layer 1: fixture corpus

`test/conformance/<spec-area>/*.yaml`, mirroring `docs/specs/`:

```yaml
- name: scratch-eval-does-not-read-live-state
  spec: state-identity.md §6.6
  steps:
    - eval: "(a1 (phasor 1))"
    - tick: 0.5
    - eval: "(one-pole 0 1)"
      expect_value: 0.0        # audit A5: was 0.5 (live accumulator leak)
  tags: [smoke, state, audit-regression]
```

Case schema: `name`, `spec` (mandatory), `steps` (eval / tick / sample / clear),
`expect_value|expect_values` (+ `tol`, default 1e-9), `expect_diagnostic`
(`category` mandatory, `span` where the spec pins it), `tags`
(`smoke` / `full` / `fw-safe` / `audit-regression`).

Coverage areas (one directory each): values-types, time & phasors, time-warps,
state & identity, prev/outputs, cells & cascade, functions & inlining,
top-level/dialects, diagnostics (category + span assertions), failure-model
(LKG, compile-fail window, health transitions).

**Resilience corpus** (`conformance/resilience/`): malformed input, deep
nesting, symbol-interner flood (A1), state-slot exhaustion, arena exhaustion,
>64-element vectors, >64 deps, oversized sources. Invariant: every resilience
case ends with a *recovery step* — a trivial eval that must still succeed and
sample correctly. "Fails cleanly and stays usable" is the assertion, not just
"doesn't crash".

### Layer 2: differential

The runner executes the same fixture set against **native and WASM** probes and
diffs values bit-for-tolerance. This mechanically catches the
firmware-vs-WASM-divergence class (F-R6, A11) with zero extra cases. Hardware
runs the `fw-safe` subset as a release gate, not per-PR.

### Layer 3: property-based / fuzz

Grammar-driven generator (weights seeded from `scripts/builtins.edn`) producing
well-formed programs; run under ASan. Oracle-free invariants:

1. **No crash / ASan-clean**, always a diagnostic or a value.
2. **Determinism**: same program + same times ⇒ same values (fresh sessions).
3. **Recompile idempotence**: re-evaluating identical source changes nothing.
4. **Clear-equivalence**: `useq-clear` + redefine ≡ fresh session (A4 class).
5. **Optimization soundness**: values at `opt_level=0` ≡ default (catches the
   whole `Div a a → 1` / bad-CSE class (A10) without hand-written oracles).
6. **Metamorphic time-warp identities**: `(fast 1 x) ≡ x`; for stateless `x`,
   `(fast 2 x)` at `t` ≡ `x` at `2t`; `offset 0` is identity.

Failing inputs are shrunk (delete-a-form loop is enough) and appended to the
fixture corpus as permanent regressions.

### Runner & CI

`scripts/run_conformance.py` (evolve `run_bytecode_vm_golden.py`, whose golden
fixture dir is gone): discovers fixtures, drives probes, reports per-spec-area
pass/fail, `--target native|wasm|serial --diff --tags smoke`. Wire into meson
(`conformance_smoke` per-PR; full + 5-minute fuzz nightly; hardware subset on
release tags).

## Part 2 — Benchmark suite

Two axes, measured separately: **(a) cost of compiling** (cold eval, recompile)
and **(b) cost of the compiled output** (per-tick / per-sample execution).

### Workload corpus — `bench/corpus/*.useq`

Task classes chosen to expose different optimizer/runtime behaviours:

| workload | exercises |
|---|---|
| `minimal` — 1 phasor, 1 output | fixed overhead floor |
| `typical-set` — 8 outputs: phasors, trig, euclid, vectors (audit bench patch) | realistic live set |
| `state-heavy` — 24+ UGens (slew/one-pole/env-follow/count) | state commit path |
| `feedback-mesh` — dense `prev` cross-output reads | prev/commit ordering cost |
| `cascade` — deep define chains + 1-cell recompile storm | recompile latency, dep plumbing |
| `table-heavy` — large vectors, interp/index | data-table path |
| `math-dense` — div/mod/trig saturated | softfloat worst case (M0+) |
| `compile-stress` — deep nesting, many defns, unroll at cap | compiler passes themselves |
| `live-edit` — max live slots + input churn | slot write / routing path |

### Metrics

Compile (per workload): median + p99 wall time of cold eval; recompile time for
a single-cell change; node count before/after optimization (CSE/fold
effectiveness as a number, not vibes); state slots; arena bytes.

Execute: native ns/tick (median over ≥1e6 ticks, pinned governor);
WASM ns/sample through the batch export (node harness) — the vis hot path;
**RP2040 on-target** µs/tick via a small bench firmware target
(`time_us_64()` deltas around `tick()`, printed over serial) — the only ground
truth for M0+; plus static proxies per commit: `.text` size and softfloat-call
counts from `objdump` (cheap, catches double-creep without hardware).

### Harness

- `bench/bench_probe.cpp` (seed: the audit's `/tmp/useq_bench/bench.cpp`):
  loads a corpus file, emits JSONL `{workload, phase, metric, value}`.
- `scripts/run_bench.py`: orchestrates native + WASM (+ `--serial` for
  hardware), writes `bench/results/<git-sha>.json`, `--compare <baseline-sha>`
  prints a delta table and exits non-zero on >10% regression in the smoke set.
- Discipline: every optimization commit cites its before/after JSON in the
  commit body. x86 numbers are a proxy; RP2040 numbers gate releases.

### Rollout

1. Session probe + fixture schema + runner; port existing golden expectations
   and the A1–A14 audit repros (≈60 cases) — immediate regression value.
2. Bench probe + corpus + native/WASM runners + baseline JSON for current tip.
3. Differential (native↔WASM) in CI; `opt_level=0` probe flag + soundness diff.
4. Fuzz generator nightly; shrinker feeding the fixture corpus.
5. RP2040 bench firmware target + serial conformance adapter (release gates).
