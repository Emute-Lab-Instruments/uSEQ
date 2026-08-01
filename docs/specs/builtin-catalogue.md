# Builtin and Form Catalogue

> Normative catalogue for the compiler-recognised language surface. This
> document states the families and their contracts; the row-complete,
> executable inventory is `test/signal_engine/test_builtin_conformance.cpp`,
> which is checked against every entry in
> `uSEQ/src/signal_engine/symbols.def`.

## Source files

- `uSEQ/src/signal_engine/symbols.def` — interned builtin names and dispatch
  categories.
- `uSEQ/src/signal_engine/graph_builder.cpp` — signal-context dispatch and
  lowering.
- `uSEQ/src/signal_engine/cold_eval.cpp` — top-level form dispatch.
- `test/signal_engine/test_builtin_conformance.cpp` — exactly one disposition for
  every symbol, positive witnesses, bounded-arity adversaries, signal-context
  boundary checks, type/keyword rejection, and rejection non-mutation.

## 1. Numeric and pure signal forms

| Family | Names | Arity |
|---|---|---|
| Arithmetic | `+`, `*` | 0 or more |
| Arithmetic | `-`, `/`, `min`, `max` | 1 or more |
| Remainder | `%` | 2 or more, left-folded |
| Comparison | `>`, `<`, `>=`, `<=`, `=` | exactly 2 |
| Logic | `not` | exactly 1 |
| Logic | `and`, `or` | exactly 2 |
| Unary maths | `sin`, `cos`, `tan`, `abs`, `floor`, `ceil`, `sqrt`, `neg`, `frac`, `usin`, `ucos`, `bi-to-uni`, `b>u`, `uni-to-bi`, `u>b` | exactly 1 |
| Binary maths | `pow`, `expt`, `mod`, `pulse` | exactly 2 |
| Ternary maths | `clamp`, `lerp`, `scale` | exactly 3 |
| Phase waveforms | `tri`, `sqr` | exactly 1 |

`tri` and `sqr` are pure phase-to-value functions. They are not oscillators,
do not allocate state, and have no pivot/frequency argument. The historical
two-argument compiler path discarded its first argument, so that spelling is
rejected. Use `tri-osc`/`sqr-osc` for phase-coherent oscillators and `pulse`
for a square wave with an explicit width.

Unary `%` is rejected: remainder has no coherent unary identity. `mod` is
the exactly-two-argument spelling; `%` permits a left fold of two or more
arguments.

## 2. Time, control, sequence, and state forms

The executable catalogue covers the complete named surface:

- time substitution: `time-as`, `fast`, `slow`, `offset`, `shift`,
  `loop-at`, `eval-at-time`;
- control: `if`, `let`, `do`, `scope`, `for`, `while`, and immediately-called
  `fn`/`lambda`;
- pattern/data: `step`, `gates`, `trigs`, `euclid`/`eu`, `seq`/`from-list`,
  `interp`/`flatseq`, `dm`, `range`, `gatesw`, `random`, `index-rand`,
  `rpulse`, `rstep`, `ridx`, `rwarp`;
- state: `integrate`, `phasor`, `lfo`, `slew`, `one-pole`, `env-follow`,
  `sah`, `noise`, `toggle`, `count`, and the aliases `osc`, `tri-osc`, `saw`,
  `sqr-osc`, `envelope-follower`, `latch`;
- external/special reads: `live-edit`, `prev`, and `input`.

State-bearing forms accept only their documented keywords. Unknown or
duplicate keywords, keyword values of the wrong type, missing values, and extra
positional arguments are compile errors. A rejected output candidate leaves
the active root/source/dependencies, state and live-slot resources, data
tables, source arena, and synth revision unchanged.

### 2.1 Leaf and runtime-generated names

| Disposition | Names | Meaning |
|---|---|---|
| Time leaf | `t`, `beat`, `bar`, `phrase`, `section`, `beat-num`, `bar-num`, `dt`, `beat-dur`, `bar-dur` | Current logical time or a derived timing value |
| Timing cell | `bpm`, `beats-per-bar`, `bars-per-phrase`, `phrases-per-section` | Read the current numeric cell value |
| Hardware-input leaf | `in1`, `in2`, `ain1`, `ain2` | Read channels 0, 1, 8, and 9 respectively |
| Previous-output leaf | `a1`..`a8`, `d1`..`d8`, `s1`..`s8` | Read that sink's previous committed sample |
| Explicit previous-output form | `(prev OUTPUT)` | Same previous-sample read, with output-name validation |
| Indexed hardware-input form | `(input INDEX)` | Read a literal hardware-input index |
| Syntax-only | `quote` | Quoted-data handling; not a runtime callable |

The 28 hardware/output leaf names are generated from their spelling rather
than stored in `symbols.def`. The conformance suite executes all 28 mappings;
they are part of the language inventory despite that implementation detail.

### 2.2 Keyword dispositions

| Context | Keywords | Disposition |
|---|---|---|
| Stateful UGens | `:id`; plus `:phase`, `:wave`, `:pw`, or `:reset` on the forms that declare them | Typed option; each may occur once |
| Wave selection | `:sin`, `:tri`, `:saw`, `:sqr` | Values of LFO `:wave`, not standalone forms |
| Reserved | `:fresh`, `:attack`, `:release` | Recognised names but rejected: no implemented option uses them |
| Synth metadata | `:name`, `:id`, `:version` | Typed identity/version fields; each may occur once |
| `osc/sine` synth ports | `:freq`, `:amp`, `:fm` | NodeDef-declared control/audio bindings; duplicates reject |
| Live-edit required | `:id`; numeric `:min`, `:max` | String identity and finite numeric bounds |
| Live-edit optional | `:name`, `:options`, `:step`, `:precision` | Display name, keyword value space, positive step, non-negative integer precision |

`:min`, `:max`, `:options`, `:step`, and `:precision` are explicit symbol
inventory entries. Other NodeDef parameter keywords, if future registered
NodeDefs add them, are registry-derived runtime surfaces and must acquire a
NodeDef descriptor plus generated positive/negative witnesses.

### 2.3 Live-edit variants

`live-edit` accepts exactly one literal seed category:

- finite number: requires finite `:min` and `:max` with `min < max`;
- `true` or `false`: stored as a boolean slot and coerced to `0`/`1` on writes;
- keyword: stored as an index into a non-empty, unique `:options` vector. The
  seed must occur in the vector. Omitting `:options` repairs it to the singleton
  seed vector and emits a warning.

The runtime slot preserves variant, step, precision, and keyword options;
WASM and firmware metadata/state surfaces expose those fields. `:name` is
validated by the compiler and consumed from source by the editor, not copied
into the runtime slot. Reordering a recompiled keyword option vector preserves
the selected keyword by spelling rather than silently reinterpreting its old
numeric index.

## 3. Top-level-only forms

The top-level forms are `define`/`def`, `defn`/`defun`, `defs`, `defstate`,
`set`, `unassign`, `zeros`, `get-expr`, `set-bpm`, `set-time-sig`,
`useq-clear`, `useq-set-time-offset`, `useq-nudge-time`, `useq-play`,
`useq-pause`, `useq-stop`, `useq-rewind`, `synth`, and the editor-generated
`with-state-id` wrapper. Their appearance anywhere in a signal expression is
a compile-time boundary error.

`(set-time-sig beats subdivision)` requires a positive whole beat count and
currently accepts only subdivision `4`. The clock model measures `bpm`,
`beat`, and `beats-per-bar` in quarter-note units; accepting another
subdivision without representing it would calculate the wrong bar duration.
Other denominators are therefore explicitly unsupported, not ignored.

`(unassign output)` is the sole per-output removal spelling. `output` must be
one of `a1`..`a8`, `d1`..`d8`, or `s1`..`s8`. Successful unassignment removes
that output's active graph, source, dependencies, scalar history, and
owner-scoped state/live resources. `(a1 0)` remains a running constant-zero
program and is not unassignment. Invalid `unassign` forms validate fully
before mutation.

## 4. Inventory rule

Every `symbols.def` entry must have exactly one disposition in the executable
catalogue: signal form, top-level form, leaf/special form, or keyword. Adding
a new name without a catalogue row, adding a duplicate row, or duplicating an
inventory name fails `builtin_conformance_test`. Bounded
forms also carry too-few/too-many witnesses; top-level forms carry a valid
witness, an invalid witness, and a nested signal-context rejection witness.
