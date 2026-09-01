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
| Unary maths | `tan`, `abs`, `floor`, `ceil`, `sqrt`, `neg`, `frac`, `bi-to-uni`, `b>u`, `uni-to-bi`, `u>b` | exactly 1 |
| Phase waveforms | `sin`, `cos`, `tri`, `sqr` | exactly 1 phasor |
| Width waveform | `pulse` | exactly 1 phasor; `:duty` is an attached modifier option |
| Binary maths | `pow`, `expt`, `mod` | exactly 2 |
| Ternary maths | `clamp`, `lerp`, `scale` | exactly 3 |

`sin`, `cos`, `tri`, `sqr`, and `pulse` are pure phase-to-value functions.
They are not oscillators, do not allocate state, and have no frequency
argument. Domain, range, and free-running behavior are explicit attached
facets: for example `sin[uni norm]`, `sin[bi norm]`, and `sin[lfo bi]`.
`pulse` reads `(pulse[:duty value] phasor)`; `:duty` defaults to the
signal-rate value `0.5`. The historical positional-width and two-argument
`tri`/`sqr` paths are rejected.

Rejected adapter spellings such as `b/sin`, `lfo/tri`, and `blfo/sin` have no
dual lowering. They are exact qualified bindings if declared, otherwise
unknown bindings; targeted diagnostics may suggest `sin[bi]`, `tri[lfo]`, or
`sin[lfo bi]`. See [operator-modifiers.md](operator-modifiers.md) §5.5.

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
- state: `integrate`, `phasor`, modifier-specialized waveform callables such as
  `sin[lfo]` and `tri[lfo]`, `slew`, `one-pole`, `env-follow`, `sah`, `noise`,
  `toggle`, `count`, and the aliases `envelope-follower`, `latch`;
- external/special reads: `live-edit`, `prev`, and `input`.

State-bearing forms accept only their documented keywords. Unknown or
duplicate keywords, keyword values of the wrong type, missing values, and extra
positional arguments are compile errors. A rejected output candidate leaves
the active root/source/dependencies, state and live-slot resources, data
tables, source arena, and synth revision unchanged.

2.1 **Driving signals are last.** `pulse` takes its phasor as its sole
positional argument; `:duty` lives in the attached modifier block under
[operator-modifiers.md](operator-modifiers.md) §4. The exact Euclidean
signatures remain:

```text
euclid hits total phasor
euclid hits total pulse-width phasor
euclid hits total pulse-width rotation phasor
```

`eu` has the same signatures. Hits come before total steps; optional values
stack immediately before the driving phasor, with rotation nearest it.

### 2.2 Leaf and runtime-generated names

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

### 2.3 Keyword dispositions

| Context | Keywords | Disposition |
|---|---|---|
| Stateful UGens | `:id`; plus `:phase` or `:reset` on the forms that declare them | Typed call option; each may occur once |
| Operator modifier | `:duty` on `pulse` | Attached keyword plus exactly one signal-rate expression; defaults to `0.5` |
| Reserved | `:fresh`, `:attack`, `:release` | Recognised names but rejected: no implemented option uses them |
| Synth metadata | `:name`, `:id`, `:version` | Typed identity/version fields; each may occur once |
| `osc/sine` synth ports | `:freq`, `:amp`, `:fm` | NodeDef-declared control/audio bindings; duplicates reject |
| Live-edit required | `:id`; numeric `:min`, `:max` | String identity and finite numeric bounds |
| Live-edit optional | `:name`, `:options`, `:step`, `:precision` | Display name, keyword value space, positive step, non-negative integer precision |

`:min`, `:max`, `:options`, `:step`, and `:precision` are explicit symbol
inventory entries. Other NodeDef parameter keywords, if future registered
NodeDefs add them, are registry-derived runtime surfaces and must acquire a
NodeDef descriptor plus generated positive/negative witnesses.

### 2.4 Live-edit variants

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
`useq-pause`, `useq-stop`, `useq-rewind`, `q/zeros`, `q/crossings`, `q/range`,
`q/explain`, `synth`, and the editor-generated `with-state-id` wrapper. Their
appearance anywhere in a signal expression is a compile-time boundary error.

The `q/*` family is host/WASM-only and follows
[temporal-queries.md](temporal-queries.md). Every query signature places its
target signal last; `:rising` and `:falling` are the only direction keywords
and occur immediately before the final target.

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

4.1 Every callable declares the modifier facts required by
[operator-modifiers.md](operator-modifiers.md) §5.2: intrinsic raw and bare
default `OperatorSpec`s, applicable facets, conflicts, options, option rates,
and one lowering target. Generated reference data projects this inventory; it
is not a second hand-maintained list.

4.2 Conformance covers each declared default, applicable modifier,
permutation-equivalent modifier set, and rejected duplicate, conflict, unknown,
or inapplicable entry. Rejected adapter spellings have no signal disposition
or alias row; their only permitted special handling is a targeted migration
diagnostic.
