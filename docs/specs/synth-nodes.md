# Synth Nodes

> Spec (accepted v1): the `(synth ...)` top-level form — declarative instantiation of
> audio-rate DSP nodes whose parameters are ModuLisp control signals.
> Counterpart to [MAIN.md](MAIN.md). The language stays control-rate
> ([signal-model.md](signal-model.md)); audio-rate DSP lives in a fixed,
> curated set of **NodeDefs** hosted by the runtime environment. The
> engine/host behaviour is specified app-side in
> `../../../docs/specs/synthesis.md` (in `useq-perform`).
>
> This spec owns the *language semantics*: form syntax, NodeDef contract,
> identity, lifecycle, vectors, failure behaviour. The app spec owns engine
> mechanics (worklet host, control transport, clocks, limits) and cites the
> semantics from here.
>
> Mental model (SuperCollider analogy): NodeDefs are SynthDefs — some
> low-level (an oscillator, a filter), some high-level (a full curated
> voice). The program instantiates arbitrary numbers of each and patches
> them at runtime; every parameter is driven by a ModuLisp signal
> expression, with omitted parameters falling back to sane static defaults.

### Status

Implemented and adversarially hardened (2026-08). The executable proof set is
`osc/sine` version 2, synth artefact ABI 2, up to 64 declarations, named or
nested acyclic routing through the node's `fm` input, and persistent
block-rate `freq`/`amp` control programs. Vector voice fan-out, explicit free,
document-sync freeing, latch/fast channels, and runtime-registered NodeDefs
remain specified future surfaces and must not be inferred from the proof set.

---

## 1. Frame

1.1 ModuLisp semantics are unchanged by this feature. Signal expressions
remain pure control-rate functions of time per [signal-model.md](signal-model.md).
The `(synth ...)` form adds a new *sink* for those signals — analogous to
the hardware output sinks in [outputs.md](outputs.md) — whose destination
is a parameter of an audio-rate DSP node rather than a CV/gate channel.

1.2 The language does not express audio-rate DSP. Audio-rate processing is
the province of NodeDefs: precompiled, host-provided DSP units. The
language's job is to *instantiate* nodes, *control* their parameters with
signals, and *describe routing* between them.

1.3 Synth nodes are orthogonal to hardware outputs. A program may freely
mix `(synth ...)` forms with `(a1 ...)`, `(d1 ...)`, etc., and share
sub-expressions between them. Neither surface depends on the other.

1.4 Synth nodes require an audio-capable host with a running WASM executor
(see §6). On hosts without audio capability the forms compile (all
diagnostics still apply) but produce no sound, and a single capability
diagnostic (severity `info`) is emitted.

---

## 2. NodeDefs

2.1 A **NodeDef** is a named, versioned description of an audio-rate DSP
unit, drawn from a registry owned by the host. The language sees only the
NodeDef *contract*, never its implementation (Faust, hand-written, or
otherwise — an app-side concern, `synthesis.md` §2).

2.2 The contract per NodeDef declares:
- **name** — namespaced identifier used in `(synth ...)` forms (§2.7);
- **audio inputs/outputs** — count and channel layout of audio-rate ports;
- **params** — for each parameter: name, static default value, nominal
  range, **rate class**, and **smoothing class**;
- **voice fan-out** — whether the def responds to vector-valued params
  (§5.6), and any voice-mix normalisation it applies;
- **runtime layout and render limits** — state/control/output byte layout,
  supported quantum range, nominal sample rate, and lifecycle fades.

2.2.1 The executable module is authoritative for this metadata: it exports a
complete registry descriptor which the host validates against the selected
name/version before installation. Missing, malformed, or mismatched module
metadata is an incompatibility, not permission to substitute the editor's
registry entry.

2.2.2 A module whose DSP depends on sample rate either renders only at its
exported nominal rate or advertises the paired, versioned sample-rate
capability `sample_rate_abi_version = 1` plus `compute_at_sample_rate`. Under
that capability the host passes the actual render rate on every compute call;
phase increments, Nyquist limits, and other rate-derived behavior use that
value. Missing halves or unknown versions fail closed. The legacy `compute`
entry remains defined at the nominal rate.

2.2.3 A NodeDef with audio inputs exports an input-capable render entry. The
shipped `osc/sine` version 2 names input zero `fm` and exposes
`compute_fm_at_sample_rate`; each FM sample is a signed frequency offset in
hertz. Non-finite FM means zero offset and the post-addition frequency is
clamped to `[0, Nyquist]`. Registry port names and order are executable ABI,
not documentation: compiler, artefact consumer, and module must agree.

2.3 **Rate class** declares how densely the host samples the controlling
signal: `block` (once per audio block) or `fast` (a declared higher
control rate, an integer number of points per block). Params that must be
audio-rate modulatable are not params at all — they are declared as audio
*inputs* (§4.1).

2.4 **Smoothing class** declares how sampled values behave between control
points:
- `step` — apply at the control point, hold until the next; no
  interpolation. **The default for pitch/frequency-class params**:
  melodic changes are abrupt unless the def opts into glide —
  portamento is a def's declared character, never an engine side
  effect;
- `linear` — ramp to the next value;
- `slew` — exponential approach with a declared time constant. Defs
  wanting glide declare `slew` on the pitch param and conventionally
  expose the time constant as a param (e.g. `:glide`);
- `latch` — stepped *and event-like*: **required** for gates and
  triggers, which the host transports as timestamped edges applied
  sample-accurately within the block (`synthesis.md` §4.6), never as
  block-rate level samples.

2.5 Smoothing ownership: `linear`/`slew` behaviour is implemented *inside*
the def (for Faust defs, baked into the source); the declared class and
time constant are a conformance requirement on the def author, and the
host delivers raw control points. `step` and `latch` are implemented by
the host (`step` is plain hold; `latch` via edge synthesis). Users never
annotate rates or smoothing in v1 (per-binding override is deferred,
§8.9); correctness-by-default is the def author's responsibility.

2.6 NodeDefs split informally into **low-level** defs (single oscillators,
filters, VCAs — the modular rack primitives) and **high-level** defs
(complete curated voices and effect chains). The contract is identical;
the distinction is curatorial.

2.7 **Naming.** NodeDef names are namespaced strings (`"osc/saw"`,
`"filt/svf"`, `"voice/fm"`) and must not collide with control-rate UGen
names from [state.md](state.md) §6 — `(synth "osc/saw" ...)` and the UGen
`(saw 1)` are different worlds and must not share a bare name. `name` and
`id` are reserved and illegal as param names (they occupy form-level
keyword positions, §3.1).

---

## 3. The `synth` Form

3.1 `(synth <def-name> [:name <string>] :<param> <expr> ...)` is a
top-level form ([top-level.md](top-level.md)) that declares one node
instance of the named NodeDef. In value position (e.g. as the last form of
a `do`, or echoed at the REPL), a `synth` form's value is its **node
reference** — an opaque handle usable in audio-input position (§4).

3.2 Each `:<param> <expr>` pair binds a signal expression to a parameter.
Expressions are ordinary signal-position expressions with full implicit
lifting — everything in [signal-model.md](signal-model.md),
[time.md](time.md), [state.md](state.md), and [cells.md](cells.md)
applies unchanged. Probes and visualisation attach to them like any other
signal expression; probing a param expression uses scratch compilation and
never touches the live patch graph (cf. [state-identity.md](state-identity.md)
§6).

3.3 Parameters not bound in the form take the NodeDef's static default.
Defaults are constants, not signals; a defaulted parameter costs nothing
on the control transport.

3.4 Binding a parameter name the NodeDef does not declare is a
compile-time diagnostic (with fuzzy-name suggestion per
[diagnostics.md](diagnostics.md)). Binding the same parameter twice is a
compile-time error. Referencing an unknown def name or an unavailable def
version is a compile-time capability diagnostic.

3.5 A `synth` form in signal position is an error, with one exception:
audio-input positions of an enclosing `synth` form (§4.3).

3.6 Dialects and dynamic eval: `synth` forms behave identically in
reactive and imperative dialects ([dialects.md](dialects.md)) — the
lifecycle (§5) is declarative regardless of dialect. Forms produced via
dynamic `eval` follow the same commit semantics as directly evaluated
forms (§4.5, §5.3).

---

## 4. Routing

4.1 Audio-rate routing is expressed **inline**: audio-input parameters of
a node accept node references rather than control signals. Which
parameters are audio inputs is declared by the NodeDef (§2.2).

4.2 `(node <name>)` references the audio output of the node instance named
`<name>` (its `:name`). One node's output may be referenced by any number
of inputs (fan-out).

4.3 A nested `(synth ...)` form may appear directly in an audio-input
position, declaring an anonymous node patched inline:

```clojure
(synth "filt/svf"
  :in     (synth "osc/saw" :freq (* 55 (pow 2 (floor (* 4 bar)))))
  :cutoff (+ 1500 (* 1000 (sin (* 2 pi bar)))))
```

Nested forms are sugar: semantically identical to a top-level `synth` with
a derived identity (§5.2) referenced by the parent.

4.4 Cycles through audio inputs are an error in v1 (feedback requires an
explicit delay def; relaxing this is deferred, §8).

4.5 **Resolution time.** Name resolution (§4.2) and cycle detection (§4.4)
are validated at *eval-commit time* against the post-diff live patch
graph, not against the compile unit alone — `(node "bass")` may
legitimately reference a node declared by an earlier eval, and two
individually-acyclic evals can jointly form a cycle. For quantised eval
([top-level.md](top-level.md)), validation runs when the queued eval
commits. A reference to a name absent from the post-diff graph is an
eval-commit error: the eval is rejected as a unit (no partial
application), per the compile-failure no-op rule (§5.8).

4.6 Deferred routing surfaces (§8): explicit patch statements
(`(patch "a" "b")`) and named busses. The inline form is the v1 surface;
the compiled representation (a patch graph over node ports) is designed to
admit all three.

---

## 5. Identity, Lifecycle, Vectors

### Identity

5.1 Node identity **is** state identity: each node instance is a state
resource of resource kind `synth-node` under the machinery of
[state-identity.md](state-identity.md). `:name <string>` is sugar that
normalises to the explicit-ID surface (`:id`); explicit identities contain
from 1 to 31 bytes, so the empty string is rejected transactionally. A `synth` form with neither
receives an editor-generated hidden ID exactly as anonymous stateful
expressions do.

5.1.1 **Hidden-ID delivery channel.** The editor delivers the hidden ID
by wrapping the form as `(with-state-id "<id>" <form>)` in the eval
payload. Per state-identity.md §2.2 the wrapper normalises to the same
internal identity annotation as `:id`: the runtime stashes the wrapper
id as a pending state identity, and the **first** synth declaration
evaluated under the wrapper consumes it — using it as the node identity
only when the form carries no explicit `:name`/`:id` (explicit identity
always supersedes and also consumes the pending id). The pending id is
scoped to the wrapped form: nested wrappers shadow it, and it can never
leak past the wrapper's closing paren. Resolution order:
`:name`/`:id` > wrapper id > anonymous fallback.

5.1.2 **Anonymous fallback.** A synth form with no explicit identity
and no wrapper (raw REPL eval without editor sidecar) receives an
identity keyed by its ordinal position within the eval unit
(state-identity.md §2.5), so re-evaluating the same program updates in
place rather than minting a fresh identity per eval. This fallback is
best-effort — reordering anonymous synth forms shifts their ordinals;
editors must inject wrapper ids for stable live-coding behaviour. Two *active* instances with the same identity is a
duplicate-active error for the `synth-node` kind (one DSP instance per
identity — resource usage is never coherent across two instances).
Inactive **document variants** sharing an identity are allowed and
useful (app-side state-identity §2.5): alternate versions of the same
node kept in the buffer and switched between by evaluating either —
evaluating one takes over the instance in place.

5.2 **Provenance, not structure.** Hidden IDs are sidecar metadata
mapped through editor transactions (app-side state-identity §7, §13.3):
identity follows the *document object*, not its shape. Rewrapping,
reformatting, moving, or editing a form — any change through which the
editor can trace the range back to the original — preserves its ID and
therefore its DSP state. Structural context keys
(`(parent-identity, input-port)` for nested anonymous forms;
`(node-identity, param [, voice])` for anonymous state inside param
expressions) are a fallback used only where provenance is absent: fresh
text, programmatic insertion, or recovery after reload.

5.3 **Naming and renaming.** Giving a `:name` to a previously anonymous
node *migrates* its hidden ID to the explicit name — identity and DSP
state are preserved (the editor has provenance; runtime identity
transfer per app-side state-identity §14). Changing an existing
explicit `:name` is a deliberate identity change: old freed, new
instantiated (§5.5), standard fades. An identity-migrating rename
affordance for explicit names is deferred (§8.6).

5.3.1 **Paste gestures.** Copy/paste follows app-side state-identity §8,
with explicit keystrokes for synth forms: plain paste (`Ctrl+V`)
**forks** — the pasted form receives a fresh hidden ID ("another node
like this one"); variant paste (`Ctrl+Shift+V`) **links** — the pasted
form is a document variant of the *same* identity, for keeping
alternate versions of one node and switching between them (§5.1). The
editor must visibly indicate which occurred on every paste containing
synth forms (`synthesis.md` §7.6).

### Lifecycle

5.4 **Eval unit semantics — upsert.** Evaluating a form (or `do` of
forms) *upserts* exactly the node identities it declares; it never frees
nodes it doesn't mention. Deleting or commenting out a synth form in the
document has, by itself, no audible effect.

5.5 Per-identity diff on upsert:
- identity not currently active → **instantiate**, entering with a
  fade-in from silence (host constant `SYNTH_FADE_IN`, default 10 ms
  linear; per-NodeDef overridable);
- identity active with the **same def (name and version)** → **update in
  place**: parameter signal graphs are swapped without re-instantiating
  DSP state (no click, no retrigger). Newly bound params smooth from the
  current effective value per their smoothing class; newly *unbound*
  params transition from their last sampled value to the static default
  the same way (latch params step);
- identity active with a **different def or version** → free + instantiate
  with overlapping fades (release fade of the old overlaps fade-in of the
  new).

5.6 **Freeing.** A node is freed only by: (a) a **document-sync eval** —
a distinct whole-document evaluation action whose semantics are
whole-truth: identities absent from the document are freed; (b) an
explicit free (`(free <name-or-ref>)` top-level form, or the app's
per-node free affordance); (c) `(useq-clear)`, which frees all declarations,
control roots, and their graph references as part of the full in-memory
session reset.
Freeing exits with a release fade (host constant `SYNTH_FADE_OUT`,
default 30 ms; per-NodeDef overridable).

5.7 **Racing fades.** A fading-out identity is not yet free: re-declaring
it before the release fade completes cancels the release and fades back
in *on the same DSP instance* (state preserved). A free arriving during a
fade-in reverses into a release fade from the current gain.

5.8 **Failed evals.** A compile-time or eval-commit error is a no-op for
the patch graph, per [failure-model.md](failure-model.md) §2.6
(compile-time errors do not consume LKG): the previously running nodes
keep sounding.

5.9 **Runtime failure.** Each control channel is a root with the standard
non-finite policy ([failure-model.md](failure-model.md) §3): a param
whose expression yields NaN/∞ substitutes its last finite value, is
marked unhealthy, and emits a diagnostic — a NaN never reaches DSP state.
Node health states: `running` (all channels healthy), `fallback` (≥ 1
channel substituting), `error` (DSP trap or engine-level failure —
containment app-side, `synthesis.md` §3.6). Chain-of-blame attribution
([failure-model.md](failure-model.md) §9) extends to
`(node, param)` roots.

5.10 **Cell-driven changes.** Redefining a cell recompiles dependent
param graphs ([cells.md](cells.md)) *without* an eval of the synth form;
such recompilation feeds the same diff as §5.5 (including voice-width
changes, §5.6a below). Dependency indexing extends from per-output to
per-`(node, param)` roots.

### Vectors (polyphony)

5.11 A parameter bound to a vector-valued signal fans out to internal
**voices** on defs that declare voice fan-out: `:freq [110 165 220]`
yields three voices, element *i* driving voice *i*. Fan-out follows the
`for` regime of [values-types.md](values-types.md): per-element node
binding, time-varying elements permitted (`[110 (sin beat) 220]` is
legal), width = static element count. Dynamic-width collections are
rejected per [compilation.md](compilation.md) §1.8/§1.9.

5.12 All vector-valued params on one node must agree in width;
scalar-bound params broadcast to all voices. Width disagreement is a
compile-time error. Defs without voice fan-out reject vector-valued
params with a compile-time diagnostic.

5.13 Voice identity is **positional**: changing width re-instantiates
only added/removed voices (with fades); surviving positions keep DSP
state across value changes (shrinking `[110 165 220]` → `[165 220]`
means voice 0 keeps its phase/filter memory while moving to 165 Hz —
intended). Per §2.4, the move is an abrupt jump under the default
`step` smoothing; it glides only if the def declares `slew`. Voice
outputs are plainly summed unless the def declares a normalisation
(§2.2).

---

## 6. Capability

6.1 Audio is a host **capability** that additionally requires the WASM
executor: compiling synth forms and producing control samples is the WASM
runtime's job in every mode. "Hardware + audio" therefore implies a WASM
instance acting as compiler/control-producer alongside the hardware
connection (see `synthesis.md` §6). Firmware hosts do not implement
`synth`.

6.2 On an audio-incapable host, `synth` forms are compiled and checked but
inert; a single capability diagnostic (severity `info`) reports that audio
output is unavailable in the current mode. (This spec owns that
diagnostic's severity; the app spec cites it.)

---

## 7. Compilation Surface

7.1 Compilation ([compilation.md](compilation.md)) treats each bound
parameter as an additional root of the signal DAG, keyed by
`(node-identity, param [, voice])`. CSE applies across params and nodes
for *stateless* subgraphs; **state slots are never shared across roots
sampled at different cadences** — a stateful subgraph feeding both a
per-tick output and a `fast` param root gets distinct state slots (the
single-`dt` executor invariant of [state.md](state.md) is per-root-class,
not per-program). The control sampling pass and its relation to live
ticks and projection ([visualisation-projection.md](visualisation-projection.md))
are specified app-side (`synthesis.md` §4).

7.2 The compiled artefact exposed to the host comprises: the **patch
graph** (node instances, def names/versions, audio-port connections,
fades) and the **control channel table** — one entry per bound
`(node, param [, voice])` with rate class and smoothing class copied from
the NodeDef contract; `latch` channels are flagged as event channels
(edge records with sub-block offsets, `synthesis.md` §4.6).

7.2.1 The current public payload is synth artefact ABI 2. It always carries
`connections`, including `[]` for an unrouted graph. Connections identify
source/destination by stable identity and destination port by both name and
index. ABI 1 consumers must reject ABI 2 rather than silently omit routing.
Internal node indices and state-owner contexts are never serialised.

7.2.2 Every control row owns persisted source/dependency metadata and a stable
compiler context keyed by `(node identity, parameter)`. Reordering parameter
bindings does not change state ownership. Cell changes recompile dependent
control roots failure-atomically. External roots participate in both GC and
execution order.

7.2.3 `useq_tick_synth_controls` is an authoritative WASM sampling boundary.
One call executes the live graph once, advances state once, and returns values
in the exact order of the published control table. It shares a single strict
wall-time frontier with `useq_tick_and_project`; an app chooses one of those
APIs as the live-VM owner for an instant, and a duplicate or decreasing call
fails before execution. A non-finite control root holds its last finite value
(or zero before the first finite sample) and state owned by that failed control
does not advance invisibly. See [visualisation-projection.md](visualisation-projection.md)
§2.7 and §7.6.

7.3 `prev`: param roots do not join the `prev` namespace ([prev.md](prev.md))
— there is no `(prev <node> <param>)`. `(prev a1)` etc. *inside* a param
expression is allowed and reads the previous value of that output as
observed at the param channel's sampling cadence.

7.4 One top-level `synth` form publishes its declaration, nested declarations,
incoming connections, control rows/sources, compiled roots, state resources,
and shared revision atomically. Nested forms do not advance the revision
independently. Failure in any later parameter or whole-graph endpoint/port/
cycle validation restores the preceding graph and every bounded resource.
In a multi-form submission, an earlier successful synth form remains
committed if a later sibling fails, following [compilation.md](compilation.md)
§1.13.

7.5 Artifact serialization is part of publication evidence, not an implicit
graph-clear command. Every graph accepted by the shipped registry fits the
bounded ABI buffer. If serialization nevertheless fails, the WASM surface
returns an explicit error envelope without declaration/control arrays and
retains the published graph and revision; it never substitutes revision zero
with empty arrays.

---

## 8. Open / Deferred

8.1 Patch statements and named busses as alternative routing surfaces
(§4.6).

8.2 Audio-rate parameter modulation beyond declared audio inputs.

8.3 Feedback routing without an explicit delay def (§4.4).

8.4 Poly NodeDefs with internal voice allocation driven by discrete
events — deliberately excluded to keep the language purely continuous;
vectors (§5.11) are the polyphony surface.

8.5 Cross-node `prev` (audio-rate feedback into control) — currently no
(§7.3).

8.6 Editor-side identity-migrating rename for *explicit* names (§5.3).

8.7 Required corpus edits listed under **Status** (top-level.md,
state-identity.md, GLOSSARY).

8.8 **ModuLisp-embedded DSP DSL** (power users): a ModuLisp surface that
compiles to Faust — a Lisp frontend for authoring low-level defs
in-app. User-authored defs enter the same registry through the same
contract and conformance checks (`synthesis.md` §2.3); requires runtime
libfaust or a compile service. Explicitly out of v1, but the NodeDef
contract must not preclude runtime-registered defs.

8.9 **Per-binding rate/smoothing override** (power users): a user-side
surface (e.g. a binding modifier) overriding a param's declared rate or
smoothing class at the use site. Explicitly out of v1: def declarations
are the only surface (§2.5). The control channel table (§7.2) carries
class per channel, so the transport does not preclude this.
