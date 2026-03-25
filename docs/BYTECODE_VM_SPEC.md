# Bytecode VM for ModuLisp Interpreter

## Status

**Draft** | March 2026 | Relates to `useq-perform-q3q`

## 1. Motivation

The ModuLisp interpreter currently uses a tree-walking evaluator. For every sample point, `eval_output_internal()` traverses the full AST — at 100 samples x N active channels, this means 300+ complete tree-walks per frame. Each walk allocates temporary `Value` objects, creates environment frames, and performs string-keyed lookups.

A bytecode VM compiles expressions once and executes a flat instruction loop per sample. The expected speedup is 10-50x for tight arithmetic, driven by:

- No AST node allocation per sample
- No environment frame creation per sample
- No string-keyed symbol lookup per sample
- Cache-friendly linear instruction stream
- Register-based instruction format eliminates stack shuffling

## 2. Semantic Model

### 2.1 Signals as Pure Functions

All ModuLisp expressions are **signals** — pure functions from time (plus external inputs) to a scalar output:

```
output = f(time, knob1, knob2, ...) → double
```

Writing `(+ 1 t)` does not mean "add 1 to the current value of t". It creates a signal: a function that, when sampled at a specific time, evaluates to `1 + t`.

### 2.2 Profunctor Structure

Signals support two mapping operations:

- **postmap** (output mapping): `(postmap g signal)` = `g(signal(t))` — transform the output. Note: postmap is just normal function application — `(postmap sin expr)` ≡ `(sin expr)`. It requires no special compiler support; it's implicit in the language.
- **premap** (input mapping): `(premap f signal)` = `signal(f(t))` — warp the input time. Premap requires dedicated compiler support because it changes the time context for the inner subgraph.

The built-in time modifiers are affine premaps:
- `(fast k expr)` = `(premap (fn [t] (* k t)) expr)` — speed up
- `(slow k expr)` = `(premap (fn [t] (/ t k)) expr)` — slow down
- `(offset k expr)` = `(premap (fn [t] (+ t k)) expr)` — shift time linearly
- `(shift k expr)` is an alias of `(offset k expr)`

For V1 there are no non-linear time-warp semantics in the core language. `shift` does not wrap or phase-normalize; it exists only as a synonym for `offset`.

### 2.3 External Input Slots

The graph is not purely a function of time. A small number of named external inputs (physical knobs, MIDI CCs, sensor values) act as additional constant parameters per sample point. These are leaves in the signal graph alongside temporal variables (`t`, `beat`, `bar`, etc.).

### 2.4 No Hidden State

All built-in functions are pure functions of their inputs:
- `random` must be deterministically seeded (from time or a hash)
- `step` / `euclid` / `gates` / `trigs` derive their state purely from phasor position
- `schedule` is a side-effect that operates at the REPL level, not within the signal graph

### 2.5 Imperative / Signal Boundary

The language has two distinct evaluation contexts:

**Top-level (imperative)**: Forms that execute side-effects once — `define`, `defn`, `a1`-`a8`, `d1`-`d8`, `schedule`, `eval`, transport commands (`useq-play`, `useq-pause`, etc.). Top-level `do` blocks group multiple top-level forms for batch evaluation (e.g., user selects a block and hits eval-hotkey); each child of a top-level `do` is itself a top-level form.

**Signal context (pure)**: The expression subtrees stored by output assignments (`a1`, `a2`, etc.) and passed to sampling. These are pure functions of time + external inputs, compiled to bytecode, and sampled repeatedly.

Side-effectful forms inside signal expressions are a **compile-time error**. The tree-walker tolerated this (silently re-executing `define` 300 times per frame), but the bytecode VM enforces the boundary explicitly. This is a deliberate improvement over the tree-walker's semantics.

### 2.6 Data Types

The bytecode VM replaces the tree-walking interpreter entirely, so it must support a minimal tagged-value subset of the language rather than only scalar doubles. V1 supports at least:

- `number` (double)
- `string`
- `symbol`
- `keyword`
- `bool`
- `nil`
- `vector`
- `lambda` / closure

Signal hot paths remain numerically optimized. The compiler specializes arithmetic-heavy signal graphs into numeric fast paths whenever it can prove that an expression stays in the numeric/vector subset required for sampling. General REPL evaluation and mixed-type expressions use the tagged-value execution path.

Quoted lists (`'(...)`) remain part of the general language for metaprogramming, scheduling, and code-as-data. Signal-oriented data is still expected to use vectors (`[...]`) by default, following the Clojure convention: parens for code, brackets for data.

### 2.7 Loops and Comprehensions

The current language already contains looping and comprehension-like constructs, and the VM must be able to represent the supported subset rather than wish them away.

- `for` remains part of the general language. If we keep the current imperative semantics, the general VM must support them. If we later change `for` to comprehension-only semantics, that will be an explicit language change with dedicated migration and tests.
- `while` remains part of the general language and therefore part of the full-replacement VM scope, even though it is outside the numeric signal fast path.

The arithmetic/vector fast path does not need to optimize loops in V1, but the general VM must still execute them correctly.

### 2.8 Dynamic `eval` in Signal Context

`eval` (dynamic evaluation of strings) is **top-level only** for V1. Using `eval` inside a signal expression is a compile-time error.

**Revisit post-MVP**: Dynamic `eval` in signal context is a legitimate future use case (e.g., generative code, live-coding tools that construct expressions programmatically). Supporting it would require the VM to invoke the full compile+execute pipeline recursively at runtime. Deferred to avoid scope creep.

### 2.9 Collapsing the Dual-Map Architecture

The current tree-walker maintains two parallel lookup maps (`m_defs` for static bindings, `m_def_exprs` for expression bindings) with an `attempt_expr_eval_first` flag that flips lookup order during output sampling. This complexity exists because the tree-walker doesn't distinguish between imperative and signal contexts.

The bytecode VM eliminates this distinction. The compiler resolves all names at compile time by inlining bindings into the signal graph. There is no runtime symbol lookup during sampling (except for the rare `LOAD_VAR` escape hatch). The dual-map architecture is replaced by the compile-time symbol resolution + dependency invalidation system (section 6.2).

## 3. VM Architecture

### 3.1 Register-Based

The VM uses **numbered virtual registers** rather than a stack. Instructions operate directly on register operands:

```
ADD  r0, r1, r2    ; r0 = r1 + r2
SIN  r3, r0        ; r3 = sin(r0)
```

**Rationale**: Arithmetic-heavy signal code benefits from fewer instructions (no push/pop overhead). Register bytecode also maps nearly 1:1 to ARM register instructions, supporting the future JIT path.

### 3.2 Dispatch Abstraction

The dispatch mechanism is compile-time selectable:

- **Computed goto** (`goto *dispatch_table[opcode]`, GCC/Clang extension) on ARM firmware — eliminates branch prediction penalty of a central switch
- **Switch dispatch** on WASM — V8/SpiderMonkey JITs optimize the local variables (registers) into CPU registers anyway, so computed goto provides no benefit

Same bytecode format and compiler for both targets. The dispatch is behind a `#ifdef` or template parameter.

### 3.3 Compilation Unit

A **compiled graph** consists of:

- **Instruction buffer**: flat array of bytecode instructions
- **Data segment**: constant pool (tagged literals, scalars, vector data)
- **Register count**: number of registers used (for allocation)
- **Dependency set**: set of symbol names this graph transitively references (for invalidation)

### 3.4 Register Allocation

For V1, a simple linear-scan or tree-coloring allocator is sufficient. ModuLisp expressions are trees (or DAGs after CSE) with no complex control flow, making register allocation straightforward.

### 3.5 Instruction Encoding

**Deferred decision**: V1 uses a struct-based instruction representation (e.g., `{opcode, rd, rs1, rs2, imm}` — ~12-16 bytes per instruction) for clarity and debuggability. Compact binary encoding (16-bit or 32-bit fixed-width) will be designed after V1 is functional and we have real bytecode to profile.

This is acceptable because typical ModuLisp expressions compile to 10-50 instructions. Even at 16 bytes per instruction, a compiled graph is 160-800 bytes — negligible on both WASM and firmware (RP2040 has 264KB RAM).

## 4. Instruction Set

### 4.1 Core Instructions

```
; Arithmetic
ADD     rd, rs1, rs2       ; rd = rs1 + rs2
SUB     rd, rs1, rs2
MUL     rd, rs1, rs2
DIV     rd, rs1, rs2       ; division by zero → runtime error
MOD     rd, rs1, rs2
NEG     rd, rs              ; rd = -rs

; Comparison (result: 1.0 or 0.0 in numeric fast paths, tagged bool in general VM)
CMP_GT  rd, rs1, rs2
CMP_LT  rd, rs1, rs2
CMP_GE  rd, rs1, rs2
CMP_LE  rd, rs1, rs2
CMP_EQ  rd, rs1, rs2

; Math
FLOOR   rd, rs
CEIL    rd, rs
FRAC    rd, rs             ; rd = rs - floor(rs)
ABS     rd, rs
MIN     rd, rs1, rs2
MAX     rd, rs1, rs2
POW     rd, rs1, rs2       ; rd = rs1 ^ rs2
SQRT    rd, rs
CLAMP   rd, rs, rlo, rhi   ; rd = clamp(rs, rlo, rhi)

; Trigonometry (mathematical by default)
SIN     rd, rs             ; rd = sin(rs)
COS     rd, rs             ; rd = cos(rs)
TAN     rd, rs             ; rd = tan(rs)

; UGen variants (phasor-domain / unipolar convenience ops)
U_SIN   rd, rs             ; rd = sin(rs * 2π) * 0.5 + 0.5
U_COS   rd, rs             ; rd = cos(rs * 2π) * 0.5 + 0.5
U_SIN_BI rd, rs            ; rd = sin(rs * 2π)
U_COS_BI rd, rs            ; rd = cos(rs * 2π)

; Waveform generators
TRI     rd, ...            ; exact arity/shape follows language semantics
SQR     rd, rs             ; square wave from phasor
PULSE   rd, rs1, rs2       ; pulse wave, rs2 = duty cycle
```

The language default is bipolar/mathematical. UGen variants are explicit convenience operations for signal work; they do not replace the normal mathematical meaning of `SIN`, `COS`, or `TAN`.

### 4.2 Load / Store

```
LOAD_CONST  rd, #imm_idx   ; load from constant pool
                            ; constant pool entries may be tagged values, not just numbers
LOAD_TIME   rd, channel, scale, offset
                            ; rd = temporal_var(time * scale + offset)
                            ; channel: T, BEAT, BAR, PHRASE, SECTION, BEAT_NUM, BAR_NUM
LOAD_INPUT  rd, #input_id  ; load external input (knob, MIDI CC)
LOAD_VAR    rd, #sym_id    ; late-bound variable lookup (rare, for non-inlinable refs)
MOV         rd, rs          ; rd = rs
```

### 4.3 Vector Operations

```
VEC_INDEX   rd, #data_slot, rs_phasor
    ; rd = data[floor(rs_phasor * len)] with bounds clamping
    ; data_slot references the data segment

VEC_LERP    rd, #data_slot, rs_phasor
    ; rd = lerp(data[i], data[i+1], frac)
    ; linear interpolation between adjacent elements
```

Vector data lives in a **separate data segment**, not inline in the instruction stream. Benefits:
- Large vectors don't bloat instruction cache
- Shared vectors across expressions are deduplicated (same data slot)
- Data segment can be aligned for optimal ARM memory access
- JIT can emit vector base address as an immediate

### 4.4 Control Flow

```
BRANCH      #offset                ; unconditional jump
BRANCH_IF   rs, #offset            ; jump if rs != 0.0
BRANCH_UNLESS rs, #offset          ; jump if rs == 0.0
```

Real branches rather than branchless SELECT. This enables future symbolic interval analysis optimizations (section 8.2).

### 4.5 Function Calls

```
CALL        #func_id, rd, rs_first, rs_count
    ; call compiled function, result in rd
    ; arguments are in consecutive registers starting at rs_first (rs_count args)
RET         rs                        ; return value from function
CALL_INTRINSIC  #intrinsic_id, rd, rs_first, rs_count
    ; call opaque C++ intrinsic (for complex builtins with signal args)
    ; same argument convention as CALL
```

Arguments are passed in a contiguous register range. The compiler allocates argument registers consecutively before emitting `CALL`/`CALL_INTRINSIC`. This keeps instructions fixed-width while supporting variable arity.

The compiler **prefers inlining** lambda bodies at call sites. `CALL` is emitted only when inlining is not possible (recursion, very large bodies, explicit opt-out). `CALL_INTRINSIC` is the escape hatch for builtins that can't be expressed as primitives (or whose args are signals, preventing compile-time pre-computation).

### 4.6 Error Handling

```
; No explicit error instructions — errors are handled by the VM dispatch loop.
; On runtime error (div/0, out-of-bounds), the VM:
;   1. Sets an error flag with diagnostic info
;   2. Aborts execution of the current graph
;   3. Falls back to the last-known-good graph (see section 6.3)
```

## 5. Compiler

### 5.1 Pipeline

```
Source text → Parser → AST (Value tree)
    → Compiler frontend:
        1. Symbol resolution (inline known bindings)
        2. Constness analysis (propagate which nodes are compile-time constant)
        3. Constant folding (evaluate constant subexpressions)
        4. Builtin expansion (expand builtins with constant args to pre-computed results)
        5. Time-warp flattening (compose affine transforms, bake into LOAD_TIME)
        6. CSE (common subexpression elimination on the DAG)
    → Register allocation
    → Bytecode emission
    → Compiled graph (instruction buffer + data segment + dependency set)
```

### 5.2 Symbol Resolution and Inlining

The compiler walks the AST and resolves symbols to their current bindings:

- **Builtin functions**: inlined as dedicated instructions or intrinsic calls
- **User-defined functions** (`defn`, `lambda`): body is inlined at the call site with parameter substitution
- **User-defined variables** (`define`): binding expression is inlined (transitively)
- **Unresolvable symbols**: emit `LOAD_VAR` for late-bound runtime lookup (rare)

A **dependency set** is recorded: the set of all user-defined symbols that were inlined. This set drives invalidation (section 6.2).

### 5.3 Constness Analysis

Each node in the AST is classified as:

- **Constant**: all inputs are constant → evaluate at compile time
- **Runtime**: depends on time or external inputs → must execute at runtime

(For V1, external inputs and temporal variables are both "runtime" — neither can be constant-folded. A future SIMD pass may distinguish them: external inputs are *uniform* across a sample batch while temporal variables differ per sample point. This distinction is deferred.)

This classification drives:
- **Constant folding**: constant nodes are evaluated once and replaced with `LOAD_CONST`
- **Builtin specialization**: `(euclid 3 8 beat)` — args `3` and `8` are constant → pre-compute the bit pattern, emit only the phasor lookup at runtime
- **Time-warp flattening**: affine scale/offset are constant → compose at compile time

### 5.4 Time-Warp Flattening

The compiler accumulates an **affine time transform** `(scale, offset)` as it descends through `fast`, `slow`, `offset`, and `shift` nodes:

```
(fast k expr)   → scale *= k
(slow k expr)   → scale /= k
(offset k expr) → offset += k
(shift k expr)  → offset += k      ; alias of offset
```

When the compiler reaches a temporal leaf (`beat`, `bar`, `t`, etc.), it emits:

```
LOAD_TIME rd, channel, accumulated_scale, accumulated_offset
```

The `fast`/`slow`/`offset`/`shift` nodes **vanish from the numeric signal bytecode**. Nested affine warps compose directly: `(fast 2 (slow 0.5 (shift 0.1 expr)))` → `scale=1.0, offset=0.1`.

There is no segmented or non-linear premap chain in V1. If we later add genuinely non-linear time-warp operators, they will require a separate design because they cannot be represented as simple affine composition.

### 5.5 Builtin Expansion

Builtins fall into three tiers:

1. **Primitive instructions**: `+`, `-`, `*`, `/`, `sin`, `cos`, `floor`, etc. → direct bytecode instructions
2. **Expandable builtins**: `euclid`, `gates`, `trigs`, `seq`/`from-list`, etc. → if all structural args are constant, pre-compute at compile time and emit simple runtime lookups. If structural args are signals, fall back to `CALL_INTRINSIC`
3. **Opaque intrinsics**: complex builtins with loop-heavy or stateful C++ implementations → always `CALL_INTRINSIC`

The decision boundary is: **can the result be reduced to a constant data structure at compile time?** If yes, expand. If no, call intrinsic.

## 6. Runtime Integration

### 6.1 Compilation Trigger

**Debounced eager compilation**:

1. When `eval()` modifies a binding (via `define`, `defn`, output assignment, etc.), affected output graphs are marked **dirty**
2. At the next frame boundary (or after a short debounce window), all dirty graphs are recompiled in a single pass
3. Multiple rapid evals (e.g., pasting a code block) coalesce into one compilation

This ensures the sampling hot path never hits a cold compile.

### 6.2 Dependency Tracking and Invalidation

Each compiled graph carries a **dependency set**: the symbols it inlined during compilation.

When a symbol is redefined (via `define`, `defn`):
1. Walk all compiled output graphs
2. Any graph whose dependency set contains the redefined symbol is marked dirty
3. Dirty graphs are recompiled at the next frame boundary (debounced)

This gives full inlining optimization with correct live-coding semantics — redefining `foo` propagates to all outputs that use `foo`, with minimal recompilation.

**Note**: Redefinition can come from multiple sources — user REPL eval, `schedule`d code executing at bar boundaries, or programmatic `eval`. All sources trigger the same invalidation path.

### 6.3 Last-Known-Good Fallback

Each output slot maintains **two compiled graphs**:

- **Active**: the graph currently used for sampling.
- **Last-known-good (LKG)**: the most recently **observed-good** graph, meaning it completed at least one full sample batch without error.

We cannot prove in general that a graph which succeeded once will never fail later. Time, external inputs, and dynamic values make that undecidable in the general case. So LKG is based on observed behavior, not proof of future safety.

**Recommended runtime model**:

1. **New expression compiled** → new graph becomes the active graph candidate.
2. **First healthy batch** from that graph → it becomes eligible for promotion into the LKG chain.
3. **If a newer graph supersedes a healthy active graph** → the superseded healthy graph becomes LKG.
4. **Active graph errors at runtime** → switch immediately to LKG for the rest of the current batch and subsequent sampling.
5. **Compilation fails** → keep the current active graph unchanged.
6. **No LKG exists** → hold the last valid sample for the remainder of the current batch if one exists; otherwise fall back to neutral defaults.

**Neutral defaults**:

- Analog outputs: `0.0` by default because the new VM semantics are bipolar by default and `0.0` represents 0V.
- Digital outputs: `0.0`
- Serial outputs: `0.0`

**LKG binding semantics**: LKG graphs use frozen bindings from the moment they were compiled. This keeps fallback deterministic and avoids recompiling during an error path.

**Result**: no hard drop to silence unless there has never been any valid graph or valid sample to fall back to. In the common case, broken code gracefully holds the most recent working behavior.

### 6.4 WASM ABI

The existing WASM ABI (`useq_eval_output`, `useq_eval_outputs_time_window`, etc.) remains unchanged from the frontend's perspective. The bytecode VM is an internal implementation detail of the C++ interpreter. The frontend continues to call the same C functions; those functions now execute compiled bytecode instead of tree-walking.

No WASM ABI changes are needed for V1.

### 6.5 Replacing the Tree-Walker

The bytecode VM replaces the tree-walker for **all evaluation**, including REPL, top-level forms, and signal sampling. There is no separate "signal-only VM" beside a retained general interpreter.

When `useq_eval(code)` is called:

1. Parse the code into an AST
2. Compile the form into bytecode for the appropriate execution path
3. Execute it in the tagged-value VM or the specialized numeric fast path
4. Persist compiled artifacts for outputs and other long-lived bindings when appropriate

For REPL expressions, compiled bytecode may be ephemeral. For output assignments (`a1`, `a2`, etc.), the compiled graph is stored and reused for sampling.

**Top-level `do` handling**: `(do form1 form2 ... formN)` at the top level is still desugared into sequential top-level evaluation of each child form.

**Return values**: `useq_eval()` still returns a string representation at the API boundary, but that representation may come from any supported tagged value type, not only numbers. Strings, symbols, keywords, vectors, booleans, and `nil` must all round-trip through the general VM correctly.

This design preserves one execution engine with two optimization tiers: a general tagged-value VM for language completeness and a specialized numeric fast path for arithmetic/vector-heavy signal evaluation.

## 7. Testing Strategy

### 7.1 Guiding Principle

The test suite validates the **mathematical semantics of the language**, not parity with the current tree-walking interpreter. The tree-walker is not gospel — it may have its own semantic bugs. The bytecode VM is an opportunity to establish a canonical, well-tested definition of ModuLisp semantics. There are no external users yet, so there is no backwards-compatibility obligation.

### 7.2 Test Infrastructure

**Two complementary systems:**

- **YAML golden test suite** — declarative, cross-platform, canonical source of truth for language semantics. Parsed by both C++ (Catch2) and TS (Vitest) test runners. Lives in a shared test fixture directory.
- **Catch2 C++ tests** — platform-specific tests for VM internals, compiler passes, instruction behavior, and anything that requires inspecting bytecode or internal state.

**Property-based testing** uses both:
- **RapidCheck** (C++) for fast-feedback property tests during development
- **Generated YAML** via a script (Python or JS) that computes expected values for random inputs analytically and emits test cases. Generated corpus is committed and becomes part of the golden suite.

### 7.3 YAML Test Schema

```yaml
# Top-level test file structure
name: "Arithmetic basics"
description: "Tests for core arithmetic operations"
default_tolerance: 1e-9          # overridable per test
default_bpm: 120                 # optional, for deriving beat/bar from t
default_time_sig: [4, 4]         # optional

tests:
  - name: "addition of constants"
    expr: "(+ 1 2)"
    samples:
      - {t: 0.0, expect: 3.0}   # t is ground truth; beat/bar derived from t + bpm

  - name: "u-sin at zero phasor is 0.5 (unipolar)"
    expr: "(u-sin beat)"
    samples:
      - {t: 0.0, expect: 0.5}
      - {t: 0.25, expect: 1.0}  # quarter beat at 120bpm → beat phasor 0.25

  - name: "vector indexing"
    expr: "([10 20 30] beat)"
    samples:
      - {t: 0.0, expect: 10.0}
      - {t: 0.166, expect: 10.0}      # still in first third
      - {t: 0.334, expect: 20.0}      # second third
      - {t: 0.499, expect: 30.0}      # last third (just before wrap)
    tolerance: 1e-6                     # per-test override

  - name: "time warp flattening"
    setup:                               # optional: top-level forms executed before sampling
      - "(define base (sin beat))"
    expr: "(fast 2 (slow 2 base))"       # should equal (sin beat)
    equivalent_to: "(sin beat)"          # assert outputs match at all sample points
    sample_range: {start: 0.0, end: 2.0, points: 100}
```

**Schema features:**
- `t` is the only required temporal input; `beat`, `bar`, `phrase`, etc. are derived from `t` + `bpm` + `time_sig`
- `tolerance` is per-test configurable, defaults to `1e-9`
- `setup` allows top-level definitions before the expression under test
- `equivalent_to` tests signal equivalence: two expressions must produce identical outputs across a sample range
- `sample_range` for sweep-style tests (sample N points across a time window)
- `expect_error` for rejection tests (see 7.5)

### 7.4 Test Categories

#### Category 1: Instruction-level (Catch2)

Each bytecode instruction tested in isolation. These are C++ tests that construct bytecode directly (no compiler involved) and execute it.

- Every instruction: `ADD`, `SUB`, `MUL`, `DIV`, `SIN`, `COS`, `VEC_INDEX`, `VEC_LERP`, `BRANCH_IF`, etc.
- Edge cases: division by zero behavior, NaN propagation, `CLAMP` at boundaries
- Phasor boundaries: `VEC_INDEX` at 0.0 (first element), at 0.999... (last element), at exactly 1.0 (wrap behavior)
- `FRAC` instruction: negative inputs, values > 1.0, exact integers
- `LOAD_TIME` with scale/offset: verify temporal context injection

#### Category 2: Compiler correctness (Catch2 + YAML)

**Bytecode inspection tests** (Catch2 only) — assert on emitted bytecode for stable optimizations:
- Constant folding: `(+ 1 2)` → single `LOAD_CONST 3`
- Time-warp flattening: `(fast 2 (sin beat))` → `LOAD_TIME` with `scale=2`, no `FAST` instruction in output
- Dead code: `(if 1 (sin beat) (cos beat))` → only `SIN` emitted, no branch
- Affine composition: `(fast 2 (slow 0.5 (offset 0.1 expr)))` → `LOAD_TIME` with `scale=1.0, offset=0.1`
- `shift` aliasing: `(shift 0.25 expr)` emits the same affine transform as `(offset 0.25 expr)`

**Instruction count tests** (Catch2 only) — assert upper bounds for complex optimizations:
- CSE: `(+ (sin beat) (sin beat))` → at most 1 `SIN` instruction
- Inlining: `(define f (fn [x] (sin x))) (f beat)` → no `CALL` instruction, inlined
- Builtin expansion: `(euclid 3 8 beat)` with constant args → no `CALL_INTRINSIC`

**Semantic correctness** (YAML golden suite):
- Constant folding produces correct values
- Time-warp flattening: `(fast k (slow k expr))` ≡ `expr` for various `k`
- Inlined symbols resolve correctly: `(define x 3) (+ x 1)` → `4.0`

#### Category 3: Semantic property tests (RapidCheck + generated YAML)

**Algebraic properties** (RapidCheck in Catch2):
- Arithmetic: commutativity `(+ a b) ≡ (+ b a)`, associativity, identity elements
- Time-warp inverses: `(fast k (slow k expr))` ≡ `expr` for random `k > 0`
- Offset aliasing: `(shift k expr)` ≡ `(offset k expr)` for all `k`
- Postmap composition: `(postmap f (postmap g expr))` ≡ `(postmap (compose f g) expr)`
- Premap composition: `(fast a (fast b expr))` ≡ `(fast (* a b) expr)`
- Vector indexing monotonicity: for sorted vector, output at phasor `p1 < p2` → `result1 <= result2`

**Trig and waveform conventions** (generated YAML):
- Mathematical trig: `(sin 0.0)` = `0.0`, `(cos 0.0)` = `1.0`, `(tan 0.0)` = `0.0`
- UGen trig: `(u-sin 0.0)` = `0.5`, `(u-sin 0.25)` = `1.0`, `(u-sin 0.5)` = `0.5`, `(u-sin 0.75)` = `0.0`
- UGen trig: `(u-cos 0.0)` = `1.0`, `(u-cos 0.25)` = `0.5`, `(u-cos 0.5)` = `0.0`
- `tri`, `sqr`, and `pulse` examples must match the actual chosen language-level arity and waveform definitions rather than assuming old unipolar defaults

**Phasor derivation** (generated YAML):
- At 120 BPM: `t=0.0` → `beat=0.0`, `t=0.25` → `beat=0.5`, `t=0.5` → `beat=0.0` (wrapped)
- `bar`, `phrase`, `section` derivation at known time points
- `beatNum`, `barNum` integer values at boundaries

#### Category 4: Imperative/signal boundary rejection tests (Catch2 + YAML)

Comprehensive tests that the compiler correctly **rejects** side-effectful forms in signal context:

**Must reject** (compile error with clear message):
```yaml
rejection_tests:
  - expr: "(a1 (do (define x 3) (sin x)))"
    error_contains: "define"         # error message must mention the offending form
  - expr: "(a1 (eval \"(sin beat)\"))"
    error_contains: "eval"
  - expr: "(a1 (schedule '(foo)))"
    error_contains: "schedule"
  - expr: "(a1 (useq-play))"
    error_contains: "useq-play"
  - expr: "(a1 (if (> beat 0.5) (define x 1) 0))"
    error_contains: "define"         # define nested inside if inside signal
```

**Must accept** (valid programs that look similar):
```yaml
acceptance_tests:
  - expr: "(do (define x 3) (a1 (sin (* x beat))))"
    description: "top-level do with define and a1"
  - expr: "(a1 (let ((x 3)) (sin (* x beat))))"
    description: "let is pure, not a side-effect"
  - expr: "(do (defn f [x] (sin x)) (a1 (f beat)))"
    description: "defn at top level, referenced in signal"
```

#### Category 5: Dependency invalidation tests (Catch2)

Dedicated test category for the dependency tracking and invalidation system:

**Direct dependency:**
```
(define x 3) → (a1 (* x beat)) → redefine x=5 → a1 recompiles with x=5
```

**Transitive dependency:**
```
(define a 1) → (define b (+ a 2)) → (a1 (* b beat))
→ redefine a=10 → a1 must recompile (transitively depends on a through b)
```

**Diamond dependency:**
```
(define a 1)
(define b (+ a 1))
(define c (+ a 2))
(a1 (+ (* b beat) (* c beat)))
→ redefine a → a1 recompiles once (not twice)
```

**Orphan redefinition:**
```
(define unused 42) → redefine unused=99 → no graphs invalidated
```

**Cascading redefinition:**
```
(define a 1) → (define b (+ a 1)) → (define c (+ b 1))
→ (a1 (* c beat)) → redefine a → entire chain recompiles
```

**Rapid successive redefinitions:**
```
(define x 1) (a1 (* x beat))
→ rapid: (define x 2) (define x 3) (define x 4)
→ debounced compilation: a1 should compile once with x=4 (not three times)
```

**Schedule-triggered invalidation:**
```
(define pattern [1 0 1 0]) → (a1 (seq pattern beat))
→ schedule fires: (define pattern [1 1 0 0])
→ a1 must recompile with new pattern
```

#### Category 6: Last-known-good fallback tests (Catch2)

Full state machine coverage for the error recovery system:

**State machine invariants:**
- LKG is the most recent graph that ran at least one full sample batch without error
- LKG is based on observed healthy execution, not proof of future safety
- Only graphs that have completed at least one healthy batch are eligible to become LKG
- When no LKG exists, fallback uses the last valid sample if possible, otherwise neutral defaults

**Test scenarios:**

1. **Bootstrap (no LKG):** First expression for output errors → output holds the last valid sample if available, otherwise uses the neutral default
2. **Normal flow:** Working expr A → working expr B → B is active, A is LKG
3. **Error fallback:** Working A (becomes LKG) → broken B → falls back to A, output matches A's signal
4. **Cascading errors:** Working A → broken B → broken C → still falls back to A (not B)
5. **Recovery after error:** Working A → broken B (fallback to A) → working C → C is active, A is still LKG until C proves healthy
6. **Promotion after healthy run:** Working A → working B (A becomes LKG) → working C (B becomes LKG) → error in C → falls back to B (not A)
7. **Compile-time error:** Working A → expression that fails to compile → A stays active and LKG unchanged, output uninterrupted
8. **Runtime error on specific time values:** Graph that errors only at `beat > 0.9` → fallback triggers mid-batch, remaining samples come from LKG
9. **Stale LKG bindings:** Working with `freq=3` (LKG inlines freq=3) → redefine `freq=5` → new graph errors → LKG executes with frozen `freq=3` (**Note: revisit this decision — see section 6.3**)

#### Category 7: Cross-target validation (YAML)

The same YAML golden test suite runs on both targets:
- **Desktop/WASM**: C++ compiled with Emscripten, tests run via Node.js or browser
- **ARM firmware**: C++ cross-compiled for RP2040/RP2350, tests run on hardware or QEMU

Cross-target tests verify:
- Identical outputs within configured tolerance (default 1e-9, loosened per-test for transcendentals)
- Computed-goto dispatch (ARM) vs switch dispatch (WASM) produce identical results
- Struct-based instruction representation works correctly on both architectures

### 7.5 Floating-Point Tolerance

Default tolerance is `1e-9` (tight enough to catch real bugs). Per-test override via the `tolerance` field in YAML.

**Guidance for test authors:**
- Exact arithmetic (`+`, `-`, `*` with integer-valued doubles): use `1e-15` or `0.0`
- Single transcendental (`sin`, `cos`): `1e-9` is safe cross-platform
- Compound expressions with multiple transcendentals: consider `1e-6`
- Cross-target tests (ARM soft-float vs x86): may need `1e-6` for transcendental chains

### 7.6 Test Phasing

Tests are introduced alongside the implementation phases (section 9):

| Phase | Test focus |
|-------|-----------|
| 1 (Core VM) | Category 1: instruction-level tests |
| 2 (Compiler) | Category 2: compiler correctness, begin YAML golden suite |
| 3 (Vectors, control flow) | Category 1 additions (VEC_INDEX, branches), YAML vector tests |
| 4 (Builtins, time warps) | Category 3: property tests, YAML builtin tests, time-warp flattening bytecode inspection |
| 5 (Functions, inlining) | Category 5: dependency invalidation tests |
| 6 (Error handling) | Category 4: rejection tests, Category 6: LKG fallback tests |
| 7 (Optimization) | Instruction count assertions, benchmark suite |
| 8 (Cross-target) | Category 7: cross-target YAML validation |

## 8. Future Optimization Paths

These are explicitly **not in V1 scope** but the architecture is designed to accommodate them.

### 8.1 JIT to Native ARM

The register-based bytecode is designed to be a natural IR for JIT compilation:

- Register numbering maps to ARM VFP/FPU registers
- `VEC_INDEX` maps to indexed load instructions
- Affine time transforms are already folded into `LOAD_TIME` immediates
- `CALL_INTRINSIC` becomes a function pointer call in native code

Target: RP2040 (soft-float, Cortex-M0+) and RP2350 (hard-float, Cortex-M33). Both can execute from RAM.

### 8.2 Symbolic Interval Analysis

For conditions like `(if (> beat 0.5) expr_a expr_b)`:

Since `beat` is a phasor (monotonically 0→1 within each beat period), the flip point can be computed symbolically: `t_flip = beat_period * 0.5`. Instead of evaluating the condition 100 times per beat:

```
if (sample_time < t_flip) → execute branch A
else → execute branch B
```

One comparison per sample instead of full condition evaluation. Constant conditions fold away entirely at compile time.

### 8.3 SIMD Vectorization

Batch sampling evaluates the same bytecode at many time points. A future pass could vectorize the instruction stream to process 4 samples simultaneously (ARM NEON on RP2350, or WASM SIMD).

### 8.4 Web Worker Offloading

Move WASM evaluation to a Web Worker with SharedArrayBuffer for batch results. The bytecode VM makes this easier — the compiled graph is a flat data structure that can be transferred or shared between threads.

## 9. Implementation Phases

### Phase 1: Core VM and instruction set
- Define bytecode instruction encoding (opcode + register operands)
- Implement VM dispatch loop (switch-based initially)
- Implement core instruction set: arithmetic, comparisons, math, transcendentals, tagged constant loading, LOAD_TIME, MOV
- Unit tests for each instruction

### Phase 2: Compiler frontend
- AST → bytecode compiler for simple expressions (arithmetic, temporals)
- Establish the tagged-value execution path for strings, symbols, keywords, booleans, nil, vectors, and closures
- Register allocation (linear scan)
- Constant folding pass
- Integration with `eval_in()` — compile then execute instead of tree-walk

### Phase 3: Vector and control flow
- Data segment allocation and management
- VEC_INDEX and VEC_LERP instructions
- Branch instructions (BRANCH, BRANCH_IF, BRANCH_UNLESS)
- Compile `if`, `do`, `let` forms

### Phase 4: Builtin expansion and time warps
- Time-warp flattening (affine transform composition)
- Constness analysis pass
- Builtin expansion for constant-arg cases (euclid, gates, etc.)
- CALL_INTRINSIC for signal-arg cases

### Phase 5: Functions and inlining
- Lambda/defn compilation with inlining
- CALL/RET for non-inlinable cases
- Symbol resolution and dependency tracking
- Invalidation on rebinding

### Phase 6: Error handling and live-coding integration
- Last-known-good graph management
- Debounced eager compilation trigger
- Error reporting (web console, firmware LED)
- Integration with output assignment (a1-a8, d1-d8, s1-s8)

### Phase 7: Optimization and dispatch
- CSE pass
- Computed-goto dispatch for ARM (behind #ifdef)
- Additional time-warp operators beyond affine composition (if we choose to add them later)
- Benchmark vs tree-walking baseline

### Phase 8: Cross-target validation
- WASM build integration and testing
- ARM firmware build integration and testing
- Golden test suite across both targets
- Performance benchmarks on both targets
