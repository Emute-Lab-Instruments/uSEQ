# Live-Edit Inputs

> Spec: compiler and runtime treatment of `live-edit` — the surface form by which the editor declares a literal in source as an externally-driven input slot. Counterpart to [MAIN.md](MAIN.md).
> See also [inputs.md](inputs.md) (hardware input leaves; this spec adds a new class of input alongside them), [signal-model.md](signal-model.md) (implicit lifting and external leaves), [compilation.md](compilation.md) (compile pipeline, slot allocation, dependency tracking), [diagnostics.md](diagnostics.md) (diagnostic shape), [failure-model.md](failure-model.md) (LKG and health).
> Editor-side counterpart: [../../../docs/specs/live-edit.md](../../../docs/specs/live-edit.md). Wire protocol: [wire-protocol.md](wire-protocol.md) (`set-live-inputs` message).

## Source files

**Note:** The `live-edit` feature is partially implemented. The wire-protocol dispatch and stub handler exist, but the compiler-side slot table, `SlotLoad`-based compilation, and WASM ABI export are not yet landed. The source files below reflect the current state.

- `uSEQ/src/firmware/serial_protocol.cpp` — `handle_set_live_inputs()` dispatches `set-live-inputs` messages; currently a stub awaiting slot-table integration (see TODO at line ~611).
- `uSEQ/src/firmware/serial_protocol.h` — declares `set-live-inputs` as a recognised message type.
- `uSEQ/src/signal_engine/node_pool.h` — `NodeOp::SlotLoad` is the dedicated node type for live-edit slot reads (separate from `InputLoad` for hardware inputs — see spec section 3.1 for rationale).
- `uSEQ/src/signal_engine/graph_builder.cpp` — future home of `live-edit` form recognition during builtin lowering.
- `wasm/wasm_wrapper.cpp` — future home of `useq_set_live_inputs()` WASM ABI export (spec section 5.10).
- `test/firmware/test_wire_protocol_contract.cpp` — F6/F6b tests for `set-live-inputs` protocol dispatch and ack shape.

---

## 1. Frame

1.1 `live-edit` is a **compiler-known special form** that declares an externally-driven input slot. From the language's signal-model perspective, a `live-edit` value is an external leaf of the signal graph alongside `t`, `ain1`, `in1`, etc. ([signal-model.md §1.1](signal-model.md), [inputs.md](inputs.md)).

1.2 The motivating user case is hands-on tweaking of a literal value during performance without per-knob-turn recompilation. The runtime exposes a slot table; the host (editor) writes new values via the wire protocol or the WASM ABI; the signal graph reads the slot once per tick.

1.3 The form is identical on firmware and WASM. Both compilers recognise it; both runtimes maintain a slot table; both consume the same wire-protocol message ([wire-protocol.md](wire-protocol.md)) or its WASM-side equivalent (§5.10).

1.4 **`live-edit` is a declaration, not a function.** It does not appear in the runtime call table. Constant folding, CSE, and slot allocation all special-case it.

---

## 2. Surface Syntax

2.1 The form is:

```lisp
(live-edit <seed> :id <string> :min <num> :max <num>
                  [:name <string>] [:options [<keyword> ...]]
                  [:step <num>] [:precision <integer>])
```

2.2 **`<seed>`** is a numeric, boolean, or keyword literal. It must be a literal — not an expression, not another `live-edit` form. The compiler does not evaluate the seed beyond reading its constant value.

2.3 **`:id`** is a string identifier, unique within a compilation unit (the user's whole document). The string itself is the wire-level identity of the slot — neither compiler nor runtime hashes it (§3.2). Strings are case-sensitive and treated as opaque tokens.

2.4 **`:min` / `:max`** are finite numeric literals. For numeric seeds, both
are required, the compiler validates `:min < :max`, and incoming slot writes
are clamped to `[:min, :max]` at the runtime boundary. For boolean and keyword
seeds they are optional and ignored; `:options` governs a keyword's value
space.

2.5 **`:name`** is an optional display string. The compiler validates that it
is a string but does not copy it into the runtime slot: the editor preserves
and consumes the display name from the source wrapper. It has no effect on
slot identity or execution.

2.6 **`:options`** defines the value space for keyword seeds and is rejected for numeric/boolean. For keyword seeds it should be present in editor-authored source; if omitted, the compiler repairs it to a singleton vector containing the seed and emits a warning (§4.2.2). When present, it must be a vector of keyword literals and the seed must appear in the vector.

2.7 **`:step`** is an optional positive finite numeric literal — the slider
step granularity used by the editor. It is preserved in slot metadata; the
runtime does not enforce step alignment on incoming writes.

2.8 **`:precision`** is an optional non-negative integer hint for numeric
display. It is preserved in slot metadata.

2.9 **Form is canonical at compile time.** Implementations may not introduce alternative shorthand surfaces in v1; the editor is the sole writer of `live-edit` forms in normal use.

---

## 3. Compilation

3.1 **`live-edit` is recognised during builtin lowering** ([compilation.md §1.2](compilation.md)). The compiler does not lower it to a function call; it lowers it to a **`SlotLoad` node** parameterised by the slot index. `SlotLoad` is a **separate node type from `InputLoad`** (the node type used for hardware input leaves like `ain1`, `in1`, etc. — [inputs.md](inputs.md)).

**Design decision (SlotLoad vs InputLoad):** An earlier draft of this spec proposed reusing `InputLoad` for both hardware inputs and live-edit slots. The implementation uses a distinct `SlotLoad` op because the two input classes have different lifecycles:
- **Hardware inputs** (`InputLoad`) are permanent, hardware-sampled per tick, and never invalidated by recompilation.
- **Live-edit slots** (`SlotLoad`) are editor-driven, temporary (allocated/freed on recompile), and index into a separate slot table that is rebuilt each eval.

Sharing a single node type would conflate these lifecycles in the executor, the output classifier, and any tooling that reasons about graph structure (e.g. determining whether an output depends on editor-driven state). The performance contract is preserved: `SlotLoad` is a single indexed array read from the slot table — O(1) per sample, identical hot-path cost to `InputLoad`.

3.2 **Slot identity is the `:id` string plus its persistent compiler owner.** The public wire identity remains the string, while the compiler records which output, state-update declaration, or synth-control context declared it. Recompiling that same owner may reuse the slot and preserve its value; a different owner may not silently alias it, even when the conflicting declarations arrive in separate eval calls. Per allocation, the compiler builds a string→index map (id → slot index in the slot table). Wire-protocol and WASM-ABI messages carry the `:id` string; the runtime resolves it to an index at receive time (see [wire-protocol.md](wire-protocol.md) and §5). No hashing; no collision risk; in-flight messages crossing a recompile resolve against the post-recompile id table — if the slot still exists, the write lands; if not, the write is silently dropped per §5.4.

3.3 **Slot metadata table.** Alongside the compiled graph, the compiler emits a slot metadata table containing, per slot:
- The original `:id` string (for diagnostic display and id-resolution).
- `:min` / `:max` for runtime clamping.
- The seed value (for the runtime to initialise the slot if no host write has arrived).
- The variant tag (`numeric` / `boolean` / `keyword`) for type-correct slot reads.
- For keywords, the `:options` vector (for runtime validation of host writes).
- `:step` and `:precision` (preserved as metadata for the host).

3.4 **Constant folding skips `live-edit`.** A `live-edit` node is opaque to constant folding ([compilation.md §1.3](compilation.md)) even when its bounds are constant. The whole point is that the value varies at runtime.

3.5 **Duplicate ids are errors.** A `:id` names exactly one active compiler-owned slot in v1. If a second output/state/control owner uses an ID that is already active, compilation fails whether the declarations were submitted together or in separate eval calls (§4.1.2). Repeated expansion of one textual declaration within the same owner may share its slot. The editor's paste handler ([../../../docs/specs/live-edit.md §3.9](../../../docs/specs/live-edit.md)) prevents accidental duplicates from routine paste; the compiler rule remains authoritative.

3.6 **Dependency tracking.** A compiled graph carries the slot ids it reads alongside the cell symbols it inlined ([compilation.md §1.6](compilation.md)). The runtime indexes outputs by their slot dependencies for future selective notification (e.g., panel highlight on slot change). Slot writes do **not** dirty the graph (§3.7).

3.7 **Slot writes never invalidate compiled graphs.** This is the load-bearing performance contract. A host write to a slot updates the slot value; the next sample tick reads the new value via the existing `SlotLoad` node. No recompilation, no graph invalidation, no allocation. Distinguishes live-edit slot writes from cell mutations ([compilation.md §1.7](compilation.md)) which *do* invalidate.

3.8 **Recompilation triggers.** Slot allocation only changes during eval. Adding/removing a `live-edit` form (or changing its `:id`/`:min`/`:max`/`:options`/seed/variant) triggers normal recompilation of the enclosing form. A surviving `(owner, :id)` pair preserves its runtime value. After successful publication, slots unreachable from every published output, state-update graph, and synth-control graph are reclaimed and the remaining dense indices are remapped atomically. A rejected candidate restores the prior slot table and metadata. Changes to compiler-irrelevant metadata (`:name`, `:step`, `:precision`) do not require recompilation but are picked up at the next eval that runs.

3.9 **Sub-tick guarantees preserved.** A `SlotLoad` node is a single indexed array read from the slot table — no allocation, no string lookup. The string→index resolution happens at slot-write receive time, not on the per-sample hot path. Slot table is sized at compile time and indexed by integer slot index. Matches the existing input-leaf cost.

3.10 **`(define x (live-edit …))` lifts the bound name to an input load.** When the compiler sees a `define` (or `let` binding) whose value is a `live-edit` form:
- The bound name is associated with the slot directly.
- Every signal-context reference to the name lowers to the same `SlotLoad` node (subject to CSE).
- The `live-edit` is the binding's value; not a wrapper around the value.
- Redefining the name to a non-`live-edit` value is allowed and triggers normal cell-mutation recompilation; the slot is freed.

3.11 **`(defn f [...] ... (live-edit …) ...)` shares the slot across calls within one owner.** A `live-edit` inside a function body allocates one slot per textual occurrence for the enclosing output/state/control owner. Repeated calls in that graph read the same slot. A call from a different persistent owner must use a distinct `:id`; otherwise the compiler rejects the cross-owner alias under §3.5.

3.12 **Eager-consume heads (authoritative list).** `live-edit` is rejected as a direct argument of any of the following heads, where the value is consumed eagerly with no surviving binding:

- Transport: `useq-play`, `useq-stop`, `useq-restart`, `useq-pause`, `useq-resume`.
- Tempo and timing: `setbpm`, `settimesig`.
- Side-effect / IO: `print`, `perf`, `timeit`, `eval`.
- Scheduler: `schedule`, `unschedule`.
- (List grows alongside the language; the editor mirrors this list for client-side rejection — see [../../../docs/specs/live-edit.md §3.5](../../../docs/specs/live-edit.md). The runtime is authoritative; if the editor's mirror drifts, the compiler still rejects.)

3.13 **Slots vs cells vs hardware inputs.** All three are external leaves of the signal graph. Distinctions:

| Aspect | `defstate` cell | Hardware input (`ain1`/`in1`/...) | Live-edit slot |
| ------ | --------------- | --------------------------------- | -------------- |
| Source of value | Self-recursive update body | Sampled from hardware per tick | Written by host via wire protocol or WASM ABI |
| Mutation triggers recompile? | If body redefined: yes | Never | Never (slot-write only) |
| Seed/init | `:initial` expression | Variant-defined neutral value | Wrapper's `<seed>` |
| Identity | User-named symbol | Builtin symbol | `:id` string |
| Per-variant availability | All variants | Variant-defined ([inputs.md §1.9](inputs.md)) | All variants (host writes are universal) |

---

## 4. Diagnostics

4.1 **Errors** (block the eval; LKG applies to affected outputs per [failure-model.md](failure-model.md)):

4.1.1 **Missing `:id`** — `error: live-edit requires :id`. Suggestion: editor-supplied id template.
4.1.2 **Duplicate `:id`** — `error: live-edit :id "<id>" appears more than once in this document`. Source span points at both occurrences if the diagnostic format supports multi-span.
4.1.3 **`:min` ≥ `:max`** for numeric seeds — `error: live-edit :min (<m>) must be less than :max (<M>)`. Suggestion: swap.
4.1.4 **Non-numeric `:min`/`:max`** for numeric seed — `error: live-edit :min and :max must be numbers, got <type>`.
4.1.5 **Reserved.** Missing `:options` for keyword seeds is repaired with a warning in v1 (§4.2.2), not an error.
4.1.6 **`:options` present** for non-keyword seed — `error: live-edit :options is only valid for keyword seeds`.
4.1.7 **Seed not in `:options`** for keyword seed — `error: live-edit seed <:foo> is not in :options [<...>]`.
4.1.8 **Reserved.** Duplicate ids are covered by §4.1.2.
4.1.9 **`live-edit` in a rejected position** — `error: live-edit is not valid here`. The editor enforces this client-side too ([../../../docs/specs/live-edit.md §3.5](../../../docs/specs/live-edit.md)); the compiler rejects hand-typed wrappers in those positions:
  - Inside `defstate :initial` body.
  - Inside a quoted/syntax-quoted/unquoted form.
  - As a direct argument of an eager-consume head (§3.12).
4.1.10 **Non-literal seed** — `error: live-edit seed must be a number, boolean, or keyword literal — not <kind>`. Catches `(live-edit (* 0.5 1) …)`, nested `(live-edit (live-edit …) …)`, etc.
4.1.11 **Unknown keyword arg** — `error: live-edit does not accept keyword <:k>`. The keyword set is closed at v1 (no forwards-compat extension surface); unrecognised keywords are an error to surface typos early.

4.2 **Warnings** (eval succeeds; widget renders; clamp/repair applied):

4.2.1 **Seed outside `[:min, :max]`** for numeric seed — `warning: live-edit seed <s> is outside [<m>, <M>] — will be clamped on push`. Slot is initialised to `clamp(seed, min, max)`.
4.2.2 **`:options` missing** for keyword seed — `warning: live-edit on keyword <:foo> has no :options — defaulting to [<:foo>] (single-option enum)`. Slot is allocated with the singleton options vector. The user can edit the wrapper to add more options.
4.2.3 **Slot declaration discarded by its graph** — `warning: live-edit slot allocated but never read in this signal graph`. The candidate may allocate the slot while compiling, but successful publication immediately reclaims it when no published graph reaches it.

4.3 **Diagnostic framing** matches [MAIN.md §2.6](MAIN.md): "the compiler doesn't support this here", plain-language messages, working suggestions where applicable.

---

## 5. Runtime Behaviour

5.1 **Slot table.** The runtime holds an array of slot values, indexed by integer slot index. Allocated/resized at eval time; constant for the duration of one compiled program. Maximum slot count is implementation-bounded; firmware ships with a cap (initial proposal: 256) and emits a compile error if exceeded. WASM is bounded only by available memory.

5.2 **Slot value type.** Each slot stores a single `IEEE 754 double`. Boolean slots store `0.0` or `1.0`. Keyword slots store the integer index into the `:options` vector (cast to double). The `SlotLoad` node returns the slot value as a double; type-aware consumers (e.g. boolean test in a conditional) interpret accordingly.

5.3 **Slot write path.** A wire-protocol `set-live-inputs` message ([wire-protocol.md](wire-protocol.md)) or a WASM ABI call (§5.10) carries one or more `(id_string, value)` pairs. The runtime:
1. Resolves each `id_string` to a slot index via the id→index map built at the most recent eval.
2. Validates the value against the slot metadata: type matches variant; numeric clamped to `[:min, :max]`; keyword present in `:options`; boolean cast to `0.0`/`1.0`.
3. Writes the validated value into the slot table at the resolved index.
4. Returns nothing meaningful. Fire-and-forget.

5.4 **Unknown id** in a write — silently ignored. (The host is racing the eval; the slot may not exist yet, or may have been freed by a recent recompile.) An optional debug log is acceptable; no diagnostic, no error frame.

5.5 **Slot read path.** During sampling, a `SlotLoad` node reads the slot value from the slot table. Cost: one indexed array read. Equivalent to a hardware input read.

5.6 **Slot value persistence across recompilation.** When eval recompiles the same owner and its new graph includes the same `:id`, the slot value carries over. When a slot disappears from that owner and no published graph reaches it, its runtime value is dropped. A different owner cannot inherit it merely by reusing the string. Editor-side persistence is independent ([../../../docs/specs/live-edit.md §7](../../../docs/specs/live-edit.md)).

5.7 **Initial value.** When a slot is freshly allocated and the host has not yet written to it, the slot value is the seed (clamped per §4.2.1). The host typically writes its persisted value (if any) within one frame of eval success; until then the seed governs.

5.8 **Per-tick atomicity.** All slot writes from one `set-live-inputs` message (or one batched WASM call) are visible together to the next sample tick. Within the message, latest-wins per id. Across messages, the runtime applies them in arrival order; there is no batching delay on the runtime side.

5.9 **Numerical health.** A slot value is always a finite double after validation (§5.3). The runtime never produces NaN/Inf from a slot read. If the host sends a non-finite number, the slot retains its previous value and a runtime warning is emitted on the diagnostic channel ([failure-model.md](failure-model.md)) but never fatal:
- Non-finite number for a numeric slot.
- Type mismatch (e.g., string for a numeric slot).
- Keyword not in `:options`.

5.10 **WASM ABI.** The WASM build exposes a single batched export mirroring the wire-protocol message:

```c
extern "C" int useq_set_live_inputs(const char* json_str);
// Parses JSON object {"id1": value1, "id2": value2, ...}
// Validates and writes each pair via the same slot-write path as §5.3.
// Returns the count of slots successfully written.
// Unknown ids count as zero (silent drop, matches §5.4).
```

The editor calls this once per UI tick with a coalesced JSON batch — same payload as the wire-protocol message body ([wire-protocol.md](wire-protocol.md)). One ABI surface, one mental model, identical semantics on hardware and WASM.

5.11 **Slot enumeration.** After a successful eval, the host can read the current slot metadata table via the existing diagnostics/state ABIs (specific shape defined alongside the wire protocol). The host does not strictly require this — the editor scans its own AST to know what slots exist — but the query enables verification that runtime allocation matches editor expectations and surfaces firmware-specific allocation failures (e.g. cap exceeded per §5.1).

---

## 6. Performance Targets

6.1 **Per-sample cost is O(1) per `SlotLoad`** — equivalent to a hardware input read. Tens of slots per program is well within budget.

6.2 **Slot-write receive cost** is O(N) per message in N pairs, dominated by JSON parse on the WASM/firmware side. At typical N (≤ 20) and message rate (≤ 60 Hz), this stays under ~5% of one core's budget on RP2040; on WASM it is negligible.

6.3 **Slot allocation cost** is paid only at eval time (between ticks per [MAIN.md §3.1](MAIN.md)). Adding/removing slots does not affect the per-sample hot path.

6.4 **Maximum slot count** ships at 256 on firmware (configurable in firmware build); WASM is bounded by memory only. Exceeding the cap is a compile-time error with a plain-language diagnostic.

---

## 7. Stable Compatibility Surface Notes

7.1 The `live-edit` special form is **part of the stable language surface** once shipped. The wrapper shape (positional seed, keyword args, allowed seed types) is committed.

7.2 Adding `live-edit` is the **first new compiler-known special form since the language stabilised.** Implementation requires coordinated changes to:
- The EDN-driven builtin generator (`scripts/builtins.edn`) — new entry.
- Both compilers (firmware and WASM) — recognise the form, build the slot metadata table.
- Both runtimes — slot table data structure, write path, id→index resolution.
- The wire protocol ([wire-protocol.md](wire-protocol.md)) — new message type.
- The WASM ABI export (§5.10).
- Editor: AST recognition, widget rendering, persistence, panel.

These land together, not piecemeal. A coordinated cross-repo change.

7.3 The slot table representation, id→index resolution, wire protocol message shape, and WASM ABI are **runtime-internal but jointly stable** with the wire protocol spec ([wire-protocol.md](wire-protocol.md)).

7.4 The editor-side widget vocabulary, panel layout, and gamepad bindings are **app surface** ([../../../docs/specs/live-edit.md](../../../docs/specs/live-edit.md)) and may evolve independently provided the language semantics here are honoured.

---

## 8. Open / Deferred

8.1 **Slot read in non-signal contexts.** What `(live-edit X :id k …)` means at the top level outside any binding (e.g. as a direct argument to a side-effect head like `setbpm`) is currently rejected (§3.12 / §4.1.9). An alternative interpretation — "snapshot the current slot value for this single command" — has use cases (e.g. `(setbpm (live-edit 120 …))` would let the user knob-tweak the BPM seed before locking in). Deferred.

8.2 **Hardware-driven slots.** Whether some slots should be sampled by the hardware itself (e.g. an additional analog input multiplexed into a slot, allowing a physical knob to drive a `live-edit`) rather than written by the host. Out of v1; would require a slot-source mechanism.

8.3 **Rate limits / curves.** Whether the runtime should slew slot writes (linear interpolation over N samples) to avoid zipper noise on audible parameters. Currently the host is responsible for any smoothing. A future `:slew <ms>` or `:curve <fn>` keyword would push that into the runtime.

8.4 **Cross-variant `:options` portability.** A keyword slot's `:options` is a vector of arbitrary keyword literals. Whether some "standard" option vocabularies should exist (e.g. `:up`/`:down`/`:both` as a built-in trichotomy) is open and not language-essential.

8.5 **Slot-as-cell sugar.** Whether `(define x (live-edit 0.5 :id k))` should be writeable as `(define-knob x 0.5 :min 0 :max 1)` (compiler-side sugar that emits the live-edit form) is an ergonomics question. Editor-side: the editor already writes the verbose form. Compiler-side sugar is deferred.

8.6 **Slot-cap raise.** 256 is a starting bound on firmware. If real use shows users wanting more, the cap is configurable in the firmware build; raising the default ships in a future firmware version.
