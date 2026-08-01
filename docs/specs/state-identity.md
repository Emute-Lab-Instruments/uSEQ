# Stateful Expression Identity

> Runtime/language spec for stable identity of anonymous stateful expressions.
> Counterpart to [state.md](state.md), [compilation.md](compilation.md),
> [top-level.md](top-level.md), [visualisation-projection.md](visualisation-projection.md),
> and the app-side spec
> [../../../docs/specs/state-identity.md](../../../docs/specs/state-identity.md).
>
> This document is the runtime source of truth. The app spec owns editor UX,
> hidden metadata, and source decorations; this spec owns how evaluated code
> maps state IDs to runtime state resources.

### Source Files

- `uSEQ/src/signal_engine/node_pool.h` - `state_values[]`,
  `state_update_roots[]`, `state_slot_count`, `NodeOp::LoadState`,
  `NodeOp::LoadDt`.
- `uSEQ/src/signal_engine/graph_builder.{h,cpp}` - UGen and stateful primitive
  compilation, keyword argument parsing, anonymous structural identity
  (output-context + ordinal keys routed through the registry).
- `uSEQ/src/signal_engine/executor.{h,cpp}` - dense-slot execution and
  `commit_state()`.
- `uSEQ/src/signal_engine/cold_eval.{h,cpp}` - top-level eval, output
  assignment, dependency-triggered recompilation.
- `wasm/wasm_wrapper.cpp` - WASM eval, time injection, output sampling,
  projection fork save/restore.
- `test/signal_engine/test_signal_engine*.cpp` - signal-engine semantic tests.

---

## 1. Frame

1.1 Named state already has identity: a top-level `defstate` cell is identified
by its symbol and survives recompilation according to [state.md section 4](state.md).

1.2 Anonymous stateful expressions do not have enough identity today. A UGen or
stateful primitive inside an output currently allocates slots according to the
compiler walk. Reordering, inserting, or swapping forms can reset or misthread
state even when the user intends continuity.

1.3 Runtime identity must be based on a stable state ID plus a resource schema,
not on node index, allocation order, or full structural hash alone.

1.4 The hot path does not change. State ID lookup is a compile/cold-path
operation. The executor still reads and writes dense integer state slots.

---

## 2. State IDs

2.1 A **state ID** is an opaque compile-time-resolvable value attached to a
stateful expression. It can be written by the user or injected by the editor.

2.2 The runtime must accept at least one explicit surface for state IDs:

```lisp
(phasor 1 :id "phase-A")
```

or:

```lisp
(with-state-id "phase-A" (phasor 1))
```

The implementation may support both. If both are supported, they normalise to
the same internal identity annotation.

2.3 State IDs are not cell names. They live in their own namespace. A state ID
string equal to a cell symbol is not ambiguous.

2.4 A state ID value must be compile-time-resolvable. String literals and
keywords are valid. Dynamic signal values are invalid because they would change
state identity at sample time.

2.5 If no explicit state ID is supplied, the compiler uses an anonymous
identity derived from structural context: the program being compiled (output
index, or state slot for `defstate` update graphs) plus the ordinal position
of the allocation within that compile. Recompiling the same program therefore
reuses its slots instead of leaking one per compile. Anonymous identities are
best-effort and may not survive arbitrary source edits (inserting a stateful
form shifts the ordinals after it). Editors should inject explicit IDs for
stable live-coding behaviour.

---

## 3. State Resources

3.1 A state ID does not directly name one raw slot. It names a family of
resources. Runtime state is keyed by:

```text
StateResourceKey = state_id + resource_kind + role
```

3.2 Examples:

```text
phase-A / oscillator-phase / phase
hold-B  / sample-hold      / held-value
hold-B  / trigger-memory   / previous-trigger
cnt-C   / counter          / count
cnt-C   / trigger-memory   / previous-trigger
cnt-C   / trigger-memory   / reset-latch
```

3.3 A stateful primitive declares a resource schema. The schema lists which
resources it reads/updates and which roles those resources play.

3.4 Compatible operators may share a resource. Oscillator-like forms share the
`oscillator-phase` resource:

```lisp
(phasor 1 :id "phase-A")
(saw 1 :id "phase-A")
(tri-osc 1 :id "phase-A")
(sqr-osc 1 :id "phase-A")
(lfo 1 :wave :tri :id "phase-A")
```

Changing waveform or frequency continues the same phase unless the user resets
or forks the identity.

3.5 Incompatible operators do not reinterpret values just because the user ID
matches:

```lisp
(saw 1 :id "x")
(count gate :id "x")
```

The oscillator phase and counter value are different resources under the same
user-facing ID.

3.6 Init values apply only when a resource is first created or explicitly
reset. Recompiling a form with the same resource key must not replay the init
value and reset state.

---

## 4. Registry and Slot Allocation

4.1 The runtime holds a persistent state-resource registry. The compiler asks
the registry for dense slot indices while compiling a graph.

4.2 The compiled graph stores dense slot indices. The registry is not consulted
during per-sample execution.

4.3 Recompilation is scoped by an owning compiler context (an output, a
`defstate` update source, or a synth-control source). Before building a
candidate, the context's existing entries are marked unseen. Resolving a
matching key reactivates its slot and preserves its value. A reachable state
load also keeps its transition graph recursively reachable.

4.4 On successful publication, the runtime computes reachability from every
published output root, synth-control root, and named `defstate` cell. Unseen
resources formerly owned by a replaced context are retired; unreachable slots
are removed; the remaining slots are compacted; and graph loads, registry
entries, nested owners, live-edit owners, values, update roots, and update
sources are remapped atomically. A failed reactive recompile retains its LKG
root, so that graph's resources stay reachable and continue evolving. On
rejection, registry entries, values, roots, owners, and allocation metadata are
restored exactly.

4.5 `useq-clear` clears every session-owned state resource, anonymous or
named, together with the registry, free list, slot values, update roots,
stored update sources, owners, and state-cell markers. A later declaration
starts from its declared init value and may reuse slot zero. Transport play,
pause, rewind, and stop do not reset state; any future persistent-state feature
must define a separate explicit operation.

---

## 5. Duplicate Active IDs

5.1 The same state ID may appear multiple times in stored source text or in
different top-level variants. That is not a runtime error by itself.

5.2 Each resource key has exactly one active update-writer context. The same
state ID may appear multiple times only for disjoint resource kinds/roles or
as pure reads of one separately declared state source.

5.3 Ambiguous duplicate updates to the same resource are errors:

```lisp
(a1 (+ (saw 1 :id "phase-A")
       (tri-osc 2 :id "phase-A")))
```

Both forms would update the same oscillator phase with different rates. The
runtime must reject this instead of choosing one silently.

The same rule applies across outputs and other published programs. Compiling
`a2` with an explicit key already written by `a1` is a boundary error; it
cannot steal the update law from the running owner. Recompiling the owning
context is permitted and preserves matching state.

5.4 A coherent equivalent separates state source from pure views:

```lisp
(a1 (let [p (phasor 1 :id "phase-A")]
      (+ (saw-shape p) (tri-shape p))))
```

The shape function names are illustrative; the semantic requirement is one
state source feeding multiple pure views.

5.5 Duplicate IDs with disjoint resource kinds may compile, but the compiler
may warn if the relationship is likely accidental.

---

## 6. Cold Eval and Top-Level Expressions

6.1 `eval_cold()` is not only a side-effect dispatcher. It is also the
top-level value evaluator used by REPL results, editor inline results, probes,
and firmware/WASM eval responses.

6.2 Top-level signal expressions evaluate at the runtime's current time and
return their current value:

```lisp
bar
(* bar 0.5)
(eval-at-time 2 bar)
(from-list [1 2 3] bar)
```

These forms should return numeric values when they compile successfully.
Unknown side-effect-free signal forms should be compiled and executed rather
than skipped as `ok`.

6.3 Top-level vector forms evaluate each element and return a vector result:

```lisp
[(eval-at-time 0 bar) (eval-at-time 0.5 bar)]
```

returns a printable numeric vector such as:

```lisp
[0 0.25]
```

6.4 Cold eval expression execution must be isolated from live output programs.
Compiling a REPL/probe expression must not append unreachable nodes to the live
`NodePool`, corrupt CSE entries, or leak literal vector tables into the live
`CellStore`.

6.5 Isolation therefore requires at least:

- scratch `NodePool` for the compiled expression graph,
- scratch or copy-on-write `CellStore` for literal vector/data-table
  allocation,
- live read access to cells, data, inputs, previous outputs, and named state,
- caller-provided workspace for execution.

6.6 A pure scratch expression has no persistent anonymous state. A stateful
scratch expression without a state ID may initialise fresh local state for the
one eval. A stateful scratch expression with a state ID may use the persistent
state-resource registry.

6.7 `eval-at-time` substitutes the expression's local time. It does not
advance live runtime state unless the expression explicitly uses identity-backed
state in a mode that is documented to mirror/update resources.

6.8 `dt` in a one-off top-level expression is the runtime's current eval delta
if available; otherwise it is `0`. Stateful projection windows use per-step
sequential `dt`.

---

## 7. Probes and Projection

7.1 State IDs solve identity. Projection solves read-only advancement through
time.

7.2 A stateful probe over a time window must evaluate sequentially over sample
times when it needs state continuity. Vectorised batch execution is valid only
for pure expressions or for specialised projection machinery that preserves
per-sample state transitions.

7.3 Projection forks clone live state resources, previous outputs, cells,
hardware inputs, and timing anchors, then advance the fork without mutating
live state. See [visualisation-projection.md](visualisation-projection.md).

7.4 A probe expression with an explicit/editor-injected state ID should use
identity-backed state. A probe expression without an ID may evaluate from fresh
scratch state or be marked as not faithfully stateful.

---

## 8. Diagnostics

8.1 Duplicate active incompatible state IDs are compile-time errors.
Diagnostics point at the duplicate occurrence and suggest forking the ID or
using one explicit state source with pure views.

8.2 Malformed or dynamic `:id` values are compile-time errors.

8.3 Unknown state-ID keywords or duplicate keyword args in a primitive call are
compile-time errors following [values-types.md section 2](values-types.md).

8.4 Hidden editor-injected syntax must not leak confusing spans. Runtime spans
are still eval-relative; the editor is responsible for remapping rewritten
payload diagnostics to visible source.

---

## 9. Open Questions

9.1 Which explicit syntax is canonical: `:id`, `with-state-id`, or both?
(Partially resolved for the `synth-node` resource kind: both are
supported and normalise to the same identity annotation, with
`:name`/`:id` taking precedence over a surrounding `with-state-id`
wrapper — synth-nodes.md §5.1.1. Still open for other stateful forms.)

9.2 Should there be a separate `(reset-state-id ...)` command for resetting a
still-live resource without replacing its owning program?

9.3 Should the wire protocol expose a state-resource introspection command for
debugging editor UI?

9.4 Should state IDs be serialised into flash snapshots, or are they strictly
session/editor metadata until the user evaluates source that contains them?
