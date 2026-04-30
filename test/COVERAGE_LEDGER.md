# Signal Compiler Coverage Ledger

Maps `docs/specs/` sections to C++ test coverage. Updated alongside code changes.

## Legend
- **Strict**: deterministic C++ assertion exists with value checks
- **WARN**: test exists but is non-strict (e.g. checks compile-only, not semantics)
- **None**: no test coverage for an implemented feature
- **N/A**: not yet implemented (deferred)

## Test File Key
- **golden** = `test/signal_engine/test_signal_engine_golden.cpp` — user-visible semantic tests
- **node** = `test/signal_engine/test_signal_engine.cpp` — node-shape / internal tests
- **e2e** = `test/firmware/test_firmware_e2e.cpp` / `test_firmware_e2e_part2.cpp` — firmware E2E
- **fuzz** = `test/firmware/test_firmware_fuzz.cpp` — fuzz tests

---

## MAIN.md

| Section | Feature | Status | Test File | Test Name |
|---------|---------|--------|-----------|-----------|
| 1.1 | ModuLisp is the live-coding language | -- | -- | (framing, not testable) |
| 1.2 | Outputs keep producing across edits/errors | Strict | golden | "compile error leaves previous output running" |
| 1.3 | Not a general-purpose language | -- | -- | (framing, not testable) |
| 1.4 | Two dialects (Reactive/Imperative) | N/A | -- | Imperative dialect not implemented in signal engine |
| 1.5 | WASM and firmware interchangeable | -- | -- | (arch constraint, not unit-testable) |
| 2.1 | Compile error must not stop the music | Strict | golden | "compile error leaves previous output running" |
| 2.2 | Runtime error falls back to LKG | Strict | node | "LKG fallback: output with no graph uses last known good value" |
| 2.3 | LKG is observed safety | Strict | node | "LKG fallback" tests |
| 2.4 | Diagnostics are structured | Strict | golden | "unknown names produce a diagnostic with fuzzy suggestions" |
| 2.5 | Diagnostics survive across evals | None | -- | No test for per-output health persistence |
| 2.6 | Diagnostic framing is "doesn't support this here" | Strict | golden | "side effects are rejected inside output expressions" |
| 2.7 | Numerical errors not silently zeroed | Strict | node | "LKG fallback" tests check NaN/Inf |
| 3.1 | Hot path never allocates | -- | -- | (performance constraint, not unit-testable) |
| 3.2 | Recompilation is sub-tick | -- | -- | (performance, not unit-testable) |
| 3.3 | Invalidation proactive, recompilation lazy | Strict | node | "Dependency tracking: cell changes trigger recompilation" |
| 3.4 | Pervasive constant folding | Strict | node | "Constant folding: transitive chains", "all unary ops", "all binary ops" |
| 3.5 | Tick rate ~1 kHz | -- | -- | (deployment constraint, not unit-testable) |
| 4.1-4.4 | Stable compatibility surface | -- | -- | (policy, not testable) |
| 5.1-5.9 | Open/deferred cross-cutting | N/A | -- | Open questions, not testable yet |

## values-types.md

| Section | Feature | Status | Test File | Test Name |
|---------|---------|--------|-----------|-----------|
| 1.1 | Clojure-shaped value tower | -- | -- | (framing) |
| 1.2 | Parser-level first-class values | Strict | node | "Tokenizer: basic tokens" |
| 1.3 | Numbers are doubles | Strict | golden | "constant arithmetic" |
| 1.4 | Truthiness: non-zero is true | Strict | golden | "numeric truthiness treats any non-zero as true" |
| 1.4 | Comparison returns 1.0/0.0 | Strict | node | "Graph builder: comparisons" |
| 1.5 | nil/0 are false | Strict | golden | "if without else defaults to zero", "nil is falsy in if" (coverage) |
| 1.6 | Signal context type-restricted by reachability | -- | -- | (arch constraint) |
| 1.7 | Vectors are first-class signal data | Strict | golden | "step over literal vector", node: "Graph builder: step function" |
| 1.8 | Callables in signal context are pure | Strict | golden | "defn with one argument inlines at call site" |
| 1.9 | nil is a value, not an error; unbound is compile-time error | Strict | golden | "unknown names produce a diagnostic" |
| 2.1 | Keywords are first-class | N/A | -- | Keyword arguments not yet in signal engine |
| 2.2 | Trailing keyword arguments | N/A | -- | Not yet implemented |
| 2.3 | Keyword parsing rules | N/A | -- | Not yet implemented |
| 2.4 | Keyword args are compile-time contract | N/A | -- | Not yet implemented |
| 3.1-3.3 | Open/deferred (maps, non-numeric roots, integers) | N/A | -- | Open questions |

## signal-model.md

| Section | Feature | Status | Test File | Test Name |
|---------|---------|--------|-----------|-----------|
| 1.1 | Implicit lifting: every expression is f(t) | Strict | golden | "constant arithmetic", "raw t is seconds" |
| 1.2 | Concrete lifting examples | Strict | golden | "constant folding is time-invariant", "raw t is seconds" |
| 1.3 | No explicit lambda wrapper | Strict | golden | all output assignments are implicitly lifted |
| 1.4 | Operators combine signals pointwise | Strict | golden | "constant arithmetic", "fractional arithmetic" |
| 1.5 | Canonical time input is t | Strict | golden | "raw t is seconds" |
| 1.6 | Sampling boundaries are engine concern | -- | -- | (arch constraint) |
| 1.7 | No hidden state on hot path | Strict | node | node-shape tests verify no allocations |
| 1.8 | Deterministic pseudorandomness | Strict | node | "random: deterministic per-beat hash", "index-rand: deterministic hash" |

## time.md

| Section | Feature | Status | Test File | Test Name |
|---------|---------|--------|-----------|-----------|
| 1.1 | t is seconds, monotonic | Strict | golden | "raw t is seconds" |
| 1.1.1 | t0 unaffected by time mods | N/A | -- | t0 not implemented in signal engine |
| 1.1.2 | ground-time never resets | N/A | -- | ground-time not implemented in signal engine |
| 1.2 | Phasor: beat wraps 0->1 | Strict | golden | "beat phasor at 120 bpm", "beat phasor at 60 bpm" |
| 1.2 | Phasor: bar wraps 0->1 | Strict | golden | "bar phasor at 120 bpm 4/4" |
| 1.2 | Phasor: phrase | Strict | golden | "phrase phasor uses bars-per-phrase default" |
| 1.2 | Phasor: section | Strict | golden | "section phasor uses phrase defaults" |
| 1.3 | Beat counters: beat-num | Strict | golden | "beat-num is an unwrapped counter" |
| 1.3 | Beat counters: bar-num | Strict | golden | "bar-num is an unwrapped counter" |
| 1.3.1 | Phasors re-derived inside time-as | None | -- | No test for phasor re-derivation inside time-as |
| 1.4 | Durations: beat-dur | Strict | golden | "beat-dur follows bpm" |
| 1.4 | Durations: bar-dur | Strict | golden | "bar-dur follows metre" |
| 1.5 | Timing cells are ordinary cells | Strict | node | "set-bpm affects beat phasor" |
| 1.6 | Phasors bipolar-domain agnostic | Strict | golden | "usin is unipolar sine over phase" |
| 1.7 | Mathematical primitives bipolar by default | Strict | node | "Graph builder: waveforms" |

## time-warps.md

| Section | Feature | Status | Test File | Test Name |
|---------|---------|--------|-----------|-----------|
| 1.1-1.3 | Two ideas: substitution vs rate integration | -- | -- | (framing) |
| 2.1 | Profunctor: premap/postmap | None | -- | No direct premap test (fast/slow test it indirectly) |
| 2.2 | time-as user-facing substitution | None | -- | No direct time-as test in golden (only fast/slow sugars tested) |
| 2.3 | Substitution is pointwise and pure | Strict | golden | "fast doubles local time", "slow halves local time" |
| 2.4 | Dynamic substitution is valid but not phase-coherent | Strict | golden | "dynamic fast is pointwise substitution" |
| 3.1 | fast sugar | Strict | golden | "fast doubles local time" |
| 3.1 | slow sugar | Strict | golden | "slow halves local time" |
| 3.1 | offset sugar | Strict | golden | "offset uses seconds" |
| 3.1 | shift alias | Strict | golden | "shift aliases offset" |
| 3.2 | Dynamic args are stateless substitution | Strict | golden | "dynamic fast is pointwise substitution" |
| 3.3 | Hint for dynamic fast on oscillator-like | N/A | -- | Hint policy not implemented |
| 3.4 | Musical offsets via beat-dur | Strict | golden | "musical offset can use beat-dur" |
| 3.5 | Constant affine compose/flatten | Strict | golden | "nested constant warps compose" |
| 4.1-4.3 | Rate clocks are not warps | N/A | -- | rate-as not implemented |
| 5.1-5.2 | Open/deferred | N/A | -- | |

## cells.md

| Section | Feature | Status | Test File | Test Name |
|---------|---------|--------|-----------|-----------|
| 1.1 | define creates/replaces a cell | Strict | node | "Cold eval: define number", "Cold eval: redefine overwrites" |
| 1.2 | Cells are reactive bindings | Strict | golden | "redefining a depended-on cell recompiles output" |
| 1.3 | User model: changed cell, signals follow | Strict | golden | "redefining a depended-on cell recompiles output" |
| 1.4 | Recompilation is sub-tick | -- | -- | (performance, not unit-testable) |
| 1.5 | Dependency tracking by symbol identity | Strict | node | "Dependency tracking: cell changes trigger recompilation" |
| 1.6 | Diamond/cascading dependencies | Strict | golden | "diamond dependency: redefining root propagates through chain" |
| 1.7 | Redefining unused cell is no-op | None | -- | No explicit test |
| 1.8 | Cells not stateful in reactive mode | Strict | golden | "expression cell is time-varying" |
| 1.9 | Cell body is a signal expression | Strict | golden | "expression cell is time-varying" |
| 1.10 | Cell redefs from any source | -- | -- | (arch constraint) |

## functions.md

| Section | Feature | Status | Test File | Test Name |
|---------|---------|--------|-----------|-----------|
| 1.1 | defn, fn, lambda produce callables | Strict | golden | "defn with one argument inlines at call site"; lambda: golden "fn anonymous lambda in signal context is rejected" |
| 1.2 | Callables inlined at call site | Strict | golden | "defn with one argument inlines at call site" |
| 1.3 | Recursion is compile-time error | Strict | golden | "recursion is rejected instead of hanging" |
| 1.4 | Higher-order returning callables | None | -- | No test for nested callable returns |
| 1.5 | Callables in cells with dependency tracking | Strict | golden | "function redefinition recompiles callers" |
| 1.6 | Top-level callables are first-class | Strict | node | "Defn and user function calls" |
| 1.7 | Variadic arithmetic left-folded | Strict | golden | "variadic addition", "variadic subtraction left folds", "unary negation form" |
| 1.8 | Local time context is lexical through inlining | Strict | golden | "defn inherits caller time context" |
| 1.9 | Keyword arguments | N/A | -- | Not yet implemented |
| 2.1 | Open: closures escaping signal context | N/A | -- | |

## outputs.md

| Section | Feature | Status | Test File | Test Name |
|---------|---------|--------|-----------|-----------|
| 1.1 | Standard outputs a1-a8, d1-d8, s1-s8 | Strict | node | "Multiple outputs: a1 and d1 simultaneously" |
| 1.2 | Output assignment is top-level form | Strict | golden | all assign_ok calls |
| 1.3 | Numeric literal is constant signal | Strict | golden | "unassigned outputs use neutral zero" (0 case); node: constant output tests |
| 1.4 | Active / LKG / last sample slots | Strict | node | "commit_outputs updates prev_output_values and lkg" |
| 1.5 | Reassignment replaces active program | Strict | node | "Output reassignment silently replaces" |
| 1.6 | q0 scheduling callback | Strict | e2e | "G1: Queued command executes at bar boundary" |
| 1.7 | Unassigned outputs produce 0 | Strict | golden | "unassigned outputs use neutral zero" |
| 1.8 | Runtime errors fall back to LKG | Strict | node | "LKG fallback" tests |

## prev.md

| Section | Feature | Status | Test File | Test Name |
|---------|---------|--------|-----------|-----------|
| 1.1 | Current sample not readable from other outputs | Strict | golden | "bare output reference reads previous committed sample" |
| 1.2 | (prev a1) reads previous sample | Strict | golden | "explicit prev reads previous committed sample" |
| 1.3 | Bare output name is sugar for prev | Strict | golden | "bare output reference reads previous committed sample"; node: "Bare output reference" |
| 1.4 | prev within batch reads preceding sample | None | -- | Batch prev semantics not tested |
| 1.5 | First sample reads neutral default | Strict | golden | "bare output reference reads previous committed sample" (first tick is 0.0) |
| 1.6 | prev enables feedback loops | Strict | golden | "self-reference via prev for integration" |
| 1.7 | prev does not introduce hidden state | -- | -- | (design constraint, not testable) |
| 1.8 | prev of undeclared output rejected; self-ref allowed | Strict | node | "(prev non-output) is an error", "(prev a1) explicit form" |
| 2.1 | Open: prev window across batches | N/A | -- | |

## inputs.md

| Section | Feature | Status | Test File | Test Name |
|---------|---------|--------|-----------|-----------|
| 1.1 | External leaves besides t | Strict | golden | "hardware input leaves read injected input values" |
| 1.2 | in1, in2 digital gates | Strict | golden | "hardware input leaves read injected input values" (in1) |
| 1.3 | ain1, ain2 analog CV | Strict | golden | "hardware input leaves read injected input values" (ain1) |
| 1.4 | swm/swt switch values | None | -- | No test for switch inputs |
| 1.5 | swr encoder switch | None | -- | No test for encoder switch |
| 1.6 | rot encoder position | None | -- | No test for encoder position |
| 1.7 | Inputs sampled once per tick | -- | -- | (arch constraint) |
| 1.8 | WASM inputs default to neutral | Strict | golden | implicit: hw_inputs init to 0 |
| 1.9 | Unsupported input is compile-time error | None | -- | No test for variant-gated inputs |

## top-level.md

| Section | Feature | Status | Test File | Test Name |
|---------|---------|--------|-----------|-----------|
| 1.1 | Top level is eval surface | Strict | node | "Cold eval" tests |
| 1.2 | No @ prefix | -- | -- | (historical note) |
| 1.3 | Quantised eval is editor concern | -- | -- | (not testable here) |
| 1.4 | define | Strict | node | "Cold eval: define number" |
| 1.4 | defn | Strict | node | "Cold eval: defn creates callable" |
| 1.4 | let | Strict | golden | "let creates local scope"; node: "Graph builder: let" |
| 1.4 | do | Strict | node | "Graph builder: do returns last" |
| 1.4 | if | Strict | golden | "if without else defaults to zero"; node: "Graph builder: if" |
| 1.4 | Output assignment a1-s8 | Strict | golden | all assign_ok tests |
| 1.4 | Transport: useq-play etc | Strict | node | "Cold eval: play/pause/stop/rewind" |
| 1.4 | setbpm | Strict | node | "Cold eval: set-bpm changes bpm cell" |
| 1.4 | settimesig | Strict | node | "Cold eval: set-time-sig changes beats-per-bar" |
| 1.4 | schedule/unschedule | N/A | -- | Not implemented in signal engine |
| 1.4 | eval (dynamic) | None | -- | No test for dynamic eval |
| 1.4 | print, perf, timeit | None | -- | No test for diagnostic side-effects |
| 1.5 | Top-level forms eagerly evaluated | Strict | node | "Cold eval" tests |
| 1.6 | Compiler may compile top-level to graph | -- | -- | (implementation detail) |
| 2.1 | Open: schedule/unschedule | N/A | -- | |

## compilation.md

| Section | Feature | Status | Test File | Test Name |
|---------|---------|--------|-----------|-----------|
| 1.1 | Every signal is a node graph | Strict | node | All graph builder tests |
| 1.2 | Compilation passes | Strict | node | Constant folding, CSE tests verify pass results |
| 1.3 | Pervasive constant folding | Strict | node | "Constant folding: transitive chains", "all unary ops", "all binary ops", "all ternary ops" |
| 1.4 | Time-substitution flattening | Strict | golden | "nested constant warps compose" |
| 1.5 | Node graph is not always a signal | -- | -- | (arch observation) |
| 1.6 | Dependency set | Strict | node | "Dependency tracking: cell changes trigger recompilation" |
| 1.7 | Invalidation proactive, recompilation lazy | Strict | node | dependency tracking tests |
| 1.8 | for unrolled at compile time | Strict | node | "Graph builder: for with literal vector", "for with range" |
| 1.9 | Higher-order on constant collections | None | -- | No test for map/reduce/filter |
| 1.10 | Numerical health: NaN/Inf | Strict | node | LKG fallback tests |
| 1.11 | Sub-tick guarantees | -- | -- | (performance, not unit-testable) |
| 2.1 | State-slot allocation | N/A | -- | defstate not implemented |
| 2.2 | CSE for state-bearing nodes | N/A | -- | Not implemented |
| 2.3 | State migration across recompilation | N/A | -- | Not implemented |
| 2.4 | dt is per-tick input | N/A | -- | Not implemented |
| 2.5 | Constant folding across state | N/A | -- | Not implemented |
| 2.6 | No symbolic-integration pass | -- | -- | (design decision, not testable) |
| 2.7 | Keyword arguments affect lowering | N/A | -- | Not implemented |
| 3.1 | Signal context definition | -- | -- | (framing) |
| 3.2 | Side-effect forms rejected | Strict | golden | "side effects are rejected inside output expressions" |
| 3.3 | Side effects rejected in subtree | Strict | node | "Negative: nested side-effect" |
| 3.4 | Reachable callables checked transitively | None | -- | No test for transitive side-effect checking through callables |
| 3.5 | Recursion rejected | Strict | golden | "recursion is rejected instead of hanging" |
| 3.5 | Mutual recursion rejected | Strict | node | "Mutual recursion detected", "Three-way mutual recursion" |
| 3.6 | Dynamic eval rejected | None | -- | No test for eval-in-signal rejection |
| 3.7 | Unresolvable symbols with fuzzy match | Strict | golden | "unknown names produce a diagnostic with fuzzy suggestions" |
| 3.8 | Type errors compiler can prove | None | -- | No test for type error diagnostics |
| 3.9 | Dynamic affine sugar allowed | Strict | golden | "dynamic fast is pointwise substitution" |
| 3.10 | Plain language diagnostics | Strict | golden | message substring checks |
| 3.11 | Diagnostic framing | Strict | golden | "side effects are rejected" message check |
| 4.1-4.3 | Open: while, FP determinism, locally-scoped state | N/A | -- | |

## failure-model.md

| Section | Feature | Status | Test File | Test Name |
|---------|---------|--------|-----------|-----------|
| 1.1 | Compile-time vs runtime errors | Strict | golden | "compile error leaves previous output running" |
| 1.2 | Compile-time errors: parse, unresolved, arity, side-effect, recursion | Strict | golden+node | Multiple diagnostic tests |
| 1.3 | Structured diagnostics | Strict | golden | span, message, suggestion checks |
| 1.4 | Compile error does not stop music | Strict | golden | "compile error leaves previous output running" |
| 1.5 | Runtime errors: div/0, NaN/Inf | Strict | node | LKG fallback tests |
| 1.6 | rate-as rate follows numeric health | N/A | -- | rate-as not implemented |
| 2.1 | Whole-output LKG fallback | Strict | node | "LKG fallback" tests |
| 2.2 | LKG is most recent healthy program | Strict | node | "commit_outputs updates prev_output_values and lkg" |
| 2.3 | LKG bindings are frozen | None | -- | No test for LKG cell-value freezing |
| 2.4 | Bootstrap: no healthy program falls to neutral | Strict | node | "LKG fallback: invalid output stays at zero" |
| 2.5 | Cascading failures don't promote unhealthy | None | -- | No test for cascading LKG promotion |
| 2.6 | Compile errors don't consume LKG | Strict | golden | "compile error leaves previous output running" |
| 3.1 | Numerical errors not silently zeroed | Strict | node | LKG tests |
| 4.1 | Diagnostics survive across evals | None | -- | No test for diagnostic persistence across evals |

## state.md (NOT YET IMPLEMENTED)

| Section | Feature | Status | Test File | Test Name |
|---------|---------|--------|-----------|-----------|
| 1.1-1.8 | Frame: stateless by default, declared state | N/A | -- | |
| 2.1-2.11 | define-state / defstate foundation | N/A | -- | |
| 3.1-3.3 | What defstate lets users build | N/A | -- | |
| 4.1-4.7 | State identity and recompilation | N/A | -- | |
| 5.1-5.7 | integrate primitive | N/A | -- | |
| 6.1-6.9 | UGen catalogue | N/A | -- | |
| 7.1-7.5 | time-as and rate-as | N/A | -- | time-as partially implemented (via fast/slow) |
| 8.1-8.7 | Local dt, state, time contexts | N/A | -- | |
| 9.1-9.3 | Motivating example | N/A | -- | |
| 10.1-10.6 | Compile-time properties | N/A | -- | |
| 11.1-11.7 | Failure model and LKG for state | N/A | -- | |
| 12.1-12.2 | Imperative mode | N/A | -- | |
| 13.1-13.12 | Open/deferred | N/A | -- | |

## dialects.md (NOT YET IMPLEMENTED in signal engine)

| Section | Feature | Status | Test File | Test Name |
|---------|---------|--------|-----------|-----------|
| 1.1-1.5 | Reactive vs Imperative, switching | N/A | -- | Signal engine is Reactive-only |
| 2.1-2.7 | Imperative-mode semantics | N/A | -- | |
| 3.1-3.2 | Open: dialect selection, schedule | N/A | -- | |

## ERROR_HANDLING_SPEC.md

| Section | Feature | Status | Test File | Test Name |
|---------|---------|--------|-----------|-----------|
| 1.1 | Errors are a conversation | Strict | golden | Message content checks |
| 1.2 | Never stop the music | Strict | golden | "compile error leaves previous output running" |
| 1.3 | Show, don't tell (inline annotations) | -- | -- | (frontend concern) |
| 1.4 | Progressive disclosure | -- | -- | (frontend concern) |
| 1.5 | Temporal awareness | None | -- | No runtime temporal diagnostic test |
| 1.6 | Rate limiting | None | -- | No diagnostic rate-limiting test |
| 1.7 | Silence on success | -- | -- | (frontend concern) |
| 2.1 | Source span struct | Strict | golden | span_start/span_len checks |
| 2.2 | Diagnostic struct | Strict | golden | severity, message, suggestion checks |
| 2.3 | Compile result (revised) | Strict | golden | EvalResult kind checks |
| 2.4 | Runtime diagnostic | Strict | node | LKG fallback with diagnostics |
| 2.5 | WASM ABI extension | -- | -- | (WASM integration, tested at higher level) |
| 3.1-3.5 | Parser: source span tracking | Strict | golden | span accuracy in "expect_error_at" tests |
| 4 | Replacing ErrorManager | Strict | node | All diagnostic tests (no ErrorManager) |
| 5/4.1 | Error messages by category | Strict | golden+node | Multiple message content tests |
| 6/5.1-5.8 | Dataflow: compiler to editor | -- | -- | (integration, tested at app level) |
| 7/6.1-6.3 | Fuzzy name matching | Strict | golden | "unknown names produce a diagnostic with fuzzy suggestions"; node: "Fuzzy match suggests corrections" |
| 8/7.1 | Interaction with LKG fallback | Strict | node | LKG fallback tests |
| 9/8.1 | Diagnostic content tests | Strict | golden+node | Comprehensive diagnostic assertions |
| 9/8.2 | Span accuracy tests | Strict | golden | "expect_error_at" helper exercises span checks |
| 9/8.3 | Multi-diagnostic tests | None | -- | No test for multiple diagnostics in one eval |
| 9/8.4 | Suggestion validity tests | Strict | golden | suggestion != nullptr checks |
| 10 | Implementation phases | -- | -- | (historical, all implemented) |
| 11 | Error message table | Strict | golden+node | Message substring checks across test files |

---

## Summary

### By Spec File

| Spec | Total Sections | Strict | WARN | None | N/A |
|------|---------------|--------|------|------|-----|
| MAIN.md | 26 | 7 | 0 | 1 | 9 |
| values-types.md | 16 | 8 | 0 | 0 | 5 |
| signal-model.md | 8 | 6 | 0 | 0 | 0 |
| time.md | 14 | 11 | 0 | 1 | 2 |
| time-warps.md | 14 | 7 | 0 | 1 | 4 |
| cells.md | 10 | 7 | 0 | 1 | 0 |
| functions.md | 11 | 7 | 0 | 1 | 2 |
| outputs.md | 8 | 8 | 0 | 0 | 0 |
| prev.md | 9 | 6 | 0 | 1 | 1 |
| inputs.md | 9 | 4 | 0 | 3 | 0 |
| top-level.md | 17 | 9 | 0 | 3 | 2 |
| compilation.md | 27 | 12 | 0 | 4 | 8 |
| failure-model.md | 14 | 9 | 0 | 3 | 1 |
| state.md | ~50 | 0 | 0 | 0 | ~50 |
| dialects.md | 10 | 0 | 0 | 0 | 10 |
| ERROR_HANDLING_SPEC.md | 20 | 10 | 0 | 3 | 0 |

### Gap Priorities (implemented but untested)

1. **failure-model.md 2.3**: LKG bindings are frozen after promotion (no cell rebinding)
2. **failure-model.md 2.5**: Cascading failures don't promote unhealthy programs
3. **failure-model.md 4.1**: Diagnostics survive across evals (per-output health persistence)
4. **prev.md 1.4**: prev within batch reads preceding sample
5. **compilation.md 3.4**: Reachable callables checked transitively for side effects
6. **compilation.md 3.6**: Dynamic eval rejected in signal context
7. **compilation.md 3.8**: Type errors compiler can prove (string in arithmetic)
8. **inputs.md 1.4-1.6**: Switch and encoder inputs
9. **top-level.md 1.4**: print/perf/timeit diagnostic side-effects
10. **ERROR_HANDLING_SPEC.md 9/8.3**: Multiple diagnostics in one eval
11. **time-warps.md 2.1-2.2**: Direct premap/time-as tests
12. **time.md 1.3.1**: Phasor re-derivation inside time-as
13. **cells.md 1.7**: Redefining unused cell is a no-op
