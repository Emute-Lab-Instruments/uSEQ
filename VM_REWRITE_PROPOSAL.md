# VM Rewrite Proposal

## Summary

The current bytecode VM migration improved performance and correctness, but it did not fully remove the architectural mismatch between:

- cold-path Lisp evaluation for REPL / top-level forms
- hot-path sampled output execution for signals

That mismatch still shows up as:

- runtime bridges from compiled output graphs back into AST-driven evaluation
- `Environment`-copy churn on bridge-heavy paths
- hybrid semantics where some forms are fully compiled, some are partly lowered, and some fall back dynamically
- extra complexity around dependency invalidation, closure capture, and late-bound globals

This document proposes a rewrite direction that keeps the working parts of the current system, but replaces the hybrid execution model with a cleaner architecture.

## Recommendation

Do not rewrite this as "the same VM, but cleaner."

Rewrite it as a two-tier execution system with a hard boundary:

- a cold general evaluator/VM for full Lisp semantics
- a hot signal compiler/runtime for outputs and sampled execution

The hot signal path should never call back into AST evaluation on the sample path.

## Why

The current implementation has already shown three distinct categories of issues:

1. Straight defects

- semantic mismatches like `time`
- unnecessary invalidation/stringification work
- wasteful batch fallback behavior

2. Local inefficiencies

- temporary argument allocation
- repeated environment copying
- bridge cache misses

3. Structural friction

- sampled graphs still use runtime bridge callbacks for unsupported forms
- the environment model is still string-keyed and interpreter-oriented
- output execution still depends on a hybrid of compile-time inlining and runtime lookup

Category 1 should be fixed in place.

Category 2 can be improved incrementally, and we already did some of that.

Category 3 is where the architecture is carrying debt. Further work there is mostly compensation, not real simplification.

## Architectural Position

### 1. Separate Cold and Hot Execution

The system should have two execution modes with different constraints and internal representations.

Cold path:

- REPL evaluation
- top-level `define`, `defn`, transport commands, scheduling
- general Lisp semantics
- can allocate freely
- can use tagged values everywhere
- latency matters less than completeness and diagnostics

Hot path:

- output sampling
- time-window evaluation
- WASM render loops
- RP2040/RP2350 per-sample execution
- allocation-free during execution
- no AST callbacks
- no string-keyed lookups
- predictable failure and recovery behavior

This boundary should be explicit in the implementation and in the language contract.

### 2. Make Output Programs Closed Over Explicit Inputs

Compiled output programs should not depend on an `Environment` object at runtime.

Instead, a compiled output graph should read from explicit runtime inputs:

- temporal channels: `t`, `time`, `beat`, `bar`, `phrase`, `section`, counts, durations
- hardware inputs
- global cells referenced by the graph
- closure captures lowered to slots/cells

A sampled program should be executable from:

- bytecode/IR
- a runtime context struct
- a compact global-cell table

and nothing else.

### 3. Replace Environment-Based Lookup With Slot/Cell-Based Lookup

The current `Environment` is a good interpreter data structure. It is a poor hot-path runtime substrate.

A rewrite should distinguish:

- lexical local slots
- closure capture slots
- global cell indices
- builtins/opcodes
- temporal/input channels

Name lookup should happen during lowering, not repeatedly during execution.

Suggested model:

- locals: integer slot indices
- globals: interned symbol ID -> cell index
- captures: explicit capture vector or cell references
- builtins: direct opcode lowering or builtin ID dispatch

This removes the need for sampled execution to copy or walk chained `Environment` objects.

### 4. Remove Runtime AST Bridges From the Sample Path

The current fallback model is convenient but expensive:

- some forms compile natively
- some partially compile
- some fall back through bridge intrinsics

That is exactly the boundary where performance work keeps piling up.

For a rewrite, choose one of these models explicitly:

Option A: Restricted output language

- outputs may only use the signal-safe subset
- unsupported forms are compile errors
- no runtime bridge exists for sampled graphs

Option B: General output VM

- outputs may use broader Lisp semantics
- the general VM is real and self-contained
- no fallback to AST interpreter from compiled execution

What should not survive is the current middle ground where sampled graphs may bounce back into the interpreter machinery.

### 5. Use an Explicit IR Instead of Bytecode Plus Escape Hatches

The rewrite should introduce an explicit intermediate representation between AST and executable bytecode.

Suggested pipeline:

1. Parse Lisp AST
2. Lower to typed-ish IR with explicit value categories
3. Resolve names to slots/cells
4. Run transforms
5. Validate hot-path safety
6. Lower to execution form

IR-level distinctions that matter:

- numeric scalar
- tagged value
- vector data segment
- temporal/input load
- global cell load
- closure/call node
- effectful op
- bounded loop / reducible loop

This gives a clean place for:

- constant folding
- common-subexpression elimination
- closure lowering
- loop lowering
- time-transform composition
- future WASM SIMD or RP2350-specific specialization

## Language and Semantics Policy

### Output Language

A rewrite should define a hard policy for outputs.

Recommended default:

- outputs are expression graphs
- top-level effects are forbidden inside outputs
- mutable/general control-flow support is explicit and bounded
- `eval` is not allowed on the sampled path

`for` and `while` should not be "maybe compiled, maybe bridged." They should either:

- lower to native hot-path constructs
- or be rejected in output context

### Globals

Globals referenced by outputs should be represented as versioned cells.

Each cell should have:

- current value
- current expression, if lazily derived
- revision number
- hot-path compatibility metadata

Compiled outputs should depend on cell IDs and revision stamps, not on serialized snapshots.

### Closures

Closures should not capture `Environment` objects.

They should capture:

- values by slot
- or references to global cells

If a closure is allowed in output context, it must lower to a representation the hot runtime can execute without re-entering the cold evaluator.

## Runtime Design

### Hot Runtime Requirements

The hot runtime should guarantee:

- no heap allocation during steady-state execution
- no string lookup
- no AST traversal
- no `Environment` cloning
- no symbol-changed callbacks during local slot mutation
- bounded branches / bounded loops
- direct numeric fast path when possible

### Failure Model

The current LKG and batch-fallback design is reasonable and should mostly be kept.

Hot runtime errors should:

- abort the current graph evaluation
- preserve already-valid batch prefix results
- switch to last-known-good or neutral fallback
- emit diagnostics on the cold side, not allocate per sample

## WASM and Microcontroller Implications

### WASM

The rewrite should optimize for:

- compact transfer/storage of compiled graphs
- worker-friendly execution
- easy future SIMD backend
- deterministic hot loop with no JS-interpreter crossover

### RP2040 / RP2350

The rewrite should optimize for:

- low heap pressure
- fixed-size or amortized-stable runtime storage
- predictable instruction dispatch
- minimal pointer chasing
- data locality for globals and registers

This argues strongly against keeping `std::function`-heavy bridge callbacks on the hot path.

## What To Keep From The Current Work

These ideas are still good and should be retained:

- unboxed numeric fast path
- structured diagnostics
- dependency tracking by symbol ID / revision
- batch execution APIs
- LKG fallback strategy
- compile-time time-transform lowering
- direct lowering for common math / vector operations

The rewrite should preserve those gains, not start from zero conceptually.

## Proposed Rewrite Shape

### Phase 0. Freeze the Contract

Before implementation, define:

- what is legal in output context
- what closure semantics are supported in output context
- whether dynamic `for`/`while` are hot-path features or cold-path-only features
- whether a general tagged VM will fully replace cold interpreter execution

### Phase 1. Introduce Global Cells

Create a runtime global-cell table with:

- stable cell IDs
- revision counters
- current value / expression metadata

Move output invalidation and compiled dependencies onto cells directly.

### Phase 2. Introduce a Real IR

Lower AST to IR that has:

- explicit slot/cell references
- explicit effectful nodes
- explicit hot-path legality markers

Use the IR to decide:

- hot signal compilation
- cold tagged execution
- rejection with diagnostics

### Phase 3. Build the Signal Runtime

Implement a signal runtime that consumes only:

- compiled graph
- temporal/input context
- global-cell table

No runtime AST bridge.

### Phase 4. Migrate Outputs First

Do not rewrite everything at once.

Migrate output execution first:

- `a*`, `d*`, `s*`
- batch sampling
- time-window rendering

This is where the performance constraints are strictest and the architecture matters most.

### Phase 5. Decide the Fate of the General VM

At that point choose:

- keep cold evaluator as interpreter
- or finish a full tagged VM for cold execution too

Either choice is acceptable.

What matters is that the hot path no longer depends on the cold evaluator.

## Non-Goals

This proposal does not require:

- JIT compilation
- LLVM
- immediate SIMD work
- type inference beyond what is useful for hot-path validation
- eliminating tagged values from cold execution

Those can come later.

## Tradeoffs

### Pros

- cleaner hot/cold separation
- less bridge complexity
- less environment churn
- easier reasoning about correctness
- easier future specialization for WASM / RP2350

### Cons

- stricter output language unless a real general VM is built
- larger up-front rewrite cost
- migration complexity around closures and global state
- some existing permissive behaviors may become explicit compile errors

## Recommended Direction

Recommended practical choice:

- keep a permissive cold evaluator
- build a strict compiled signal runtime for outputs
- reject unsupported output forms instead of bridging them

This is the best fit for:

- live-coding reliability
- microcontroller constraints
- WASM execution
- simpler long-term maintenance

If full generality in outputs is a hard product requirement, then the right answer is not more bridge work. The right answer is a proper general VM that does not delegate back to the interpreter.

## Questions For The AI Council

1. Should output context become a formally restricted language, or is full general Lisp semantics in outputs a hard requirement?
2. Should dynamic `for` and `while` be supported natively in sampled graphs, or moved to cold-path-only semantics?
3. Should closures in output context capture by value, by cell reference, or be restricted further?
4. Is a full tagged general VM worth the complexity, or should the cold evaluator remain interpreter-based?
5. What is the minimal IR needed to support the signal runtime cleanly without overengineering?
6. What migration plan best preserves live-coding behavior while removing the runtime bridge from sampled outputs?

## Bottom Line

The recent fixes were valuable, but they also clarified the boundary:

- correctness fixes were necessary
- bridge optimization work was mostly debt service

If we rewrite from scratch, the goal should be to eliminate the need for that debt service, not become better at paying it.
