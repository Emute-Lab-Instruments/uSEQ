---
stability: evolving
layer: behavioural
---

# Operator Modifiers

> Spec: the attached `operator[...]` syntax that specializes one callable from
> the canonical callable inventory. Modifiers are orthogonal to qualified
> binding identity, which is specified in
> [qualified-symbols.md](qualified-symbols.md). Editor presentation is specified
> in
> [../../../docs/specs/operator-modifier-ui.md](../../../docs/specs/operator-modifier-ui.md).

## Source files

- `uSEQ/src/signal_engine/symbols.def` — canonical builtin callable inventory
  and each callable's declared modifier capabilities.
- `uSEQ/src/signal_engine/graph_builder.cpp` — call-head resolution,
  validation, and lowering.
- `uSEQ/src/signal_engine/synth_registry.cpp` — registry-supplied qualified
  callables.
- `test/signal_engine/test_builtin_conformance.cpp` — exhaustive callable and
  modifier conformance surface.
- `../../../scripts/generate-reference-data.mjs` — generated projection of the
  canonical callable inventory for the app.

---

## 1. Frame

1.1 An **operator modifier** specializes an already-resolved callable along a
declared facet or option. It is written in an attached bracket block on a call
head: `(sin[uni norm] phase)`.

1.2 Modifiers are not namespaces, categories, aliases, or alternate callable
names. `/` never denotes a modifier; it remains part of exact qualified binding
identity ([qualified-symbols.md](qualified-symbols.md)).

&nbsp;&nbsp;&nbsp;&nbsp;**Why:** qualification answers *which binding?* while a
modifier answers *which declared specialization of that binding?* Combining
the two creates parallel resolution paths and makes local binding identity
depend on compiler policy.

1.3 Compiler and editor policy MUST derive from the same canonical callable
inventory. A second adapter table, editor list, or legacy lowering path is not
permitted.

&nbsp;&nbsp;&nbsp;&nbsp;**Why:** the editor must never confidently offer source
that the engine rejects, and the compiler must not assign a second meaning to a
token the binding environment already owns.

---

## 2. Syntax

2.1 A modifier block is recognized only when `[` begins exactly where the base
call-head token ends. `sin[uni]` is one modified call head; `sin [uni]` is the
ordinary symbol `sin` followed by an ordinary vector argument.

```lisp
(sin[uni] phase)  ; modified callable
(sin [uni] phase) ; bare callable receiving a vector and another argument
```

&nbsp;&nbsp;&nbsp;&nbsp;**Why:** adjacency gives modification a visible lexical
boundary without changing the meaning of vectors in argument position.

2.2 The base before `[` is one exact callable token. It may be bare (`sin`) or
qualified (`osc/sine`); qualification is resolved as part of that base before
the modifier block is interpreted.

2.3 A modifier block contains one or more **entries** separated by whitespace.
An entry is either:

- a facet symbol; or
- a keyword followed by exactly one ModuLisp expression.

2.4 Facet order is semantically irrelevant. These heads are equivalent and
MUST produce the same fixed operator specification:

```lisp
sin[uni norm]
sin[norm uni]
```

2.5 Keyword values are expressions, not facet tokens. In
`pulse[bi :duty (+ 0.1 amount)]`, the entire list after `:duty` is its one
value. A keyword without a following expression is malformed.

2.6 Empty blocks, nested modifier blocks, non-symbol/non-keyword entries,
trailing material after the closing `]` in the same head, and more than one
attached block are malformed call heads.

---

## 3. Canonical Facets

3.1 The initial facet vocabulary is closed:

| facet | axis | meaning |
|---|---|---|
| `norm` | input domain | principal input is a normalized phase; one unit is one cycle |
| `rad` | input domain | principal input is an angle in radians |
| `uni` | output range | output is mapped to `[0,1]` |
| `bi` | output range | output is mapped to `[-1,1]` |
| `lfo` | evaluation regime | a stateful, phase-coherent free-running form whose principal input is Hz |
| `raw` | baseline | begin from the callable's declared intrinsic engine specification rather than its bare-name defaults |

3.2 `norm` conflicts with `rad`; `uni` conflicts with `bi`. A facet repeated
verbatim is a duplicate. Facets on distinct axes compose, so `sin[lfo bi]` is
the bipolar free-running specialization of `sin`.

3.3 `raw` selects the intrinsic baseline before other explicit entries are
applied. It does not create another callable and does not bypass applicability
validation.

3.4 `once` is a form, not a modifier. `(once expression)` evaluates its
expression once for the compiled graph. `op[once]` is an unknown facet and MUST
be rejected.

&nbsp;&nbsp;&nbsp;&nbsp;**Why:** evaluation time scopes an expression, not merely an
operator's input or output facet; representing it as a form preserves that
boundary.

---

## 4. Options and `pulse`

4.1 Keyword options belong to the resolved base callable's declaration. The
same inventory declares the option name, value contract, default, rate, and
applicability.

4.2 `pulse` is a phase shaper with this exact call surface:

```lisp
(pulse[:duty duty-expression] phasor)
```

The `:duty` option defaults to `0.5` when omitted and is signal-rate: its value
may vary per sample. It is not a positional width argument.

4.3 Facets and options compose without order significance. This witness is a
bipolar pulse whose signal-rate duty is `0.2`:

```lisp
(pulse[bi :duty 0.2] phasor)
```

4.4 An option may occur at most once. Unknown, duplicate, or inapplicable
options are compile-time errors before lowering.

---

## 5. Resolution and Lowering

5.1 Compilation MUST resolve the exact base token first. It MUST NOT split `/`
and reinterpret a qualifier as adapter policy. For example, `osc/sine[raw]`
first resolves the exact callable `osc/sine`.

5.2 Each callable inventory entry provides:

- its exact binding identity;
- an intrinsic raw `OperatorSpec`;
- its bare-name default facet values;
- applicable facet values and declared conflicts;
- options with defaults, value contracts, and evaluation rates; and
- the single lowering target selected by the resulting fixed `OperatorSpec`.

5.3 Resolution constructs one fixed `OperatorSpec` in this order:

1. resolve the exact base callable;
2. select its declared bare default baseline, or its intrinsic baseline when
   `raw` is present;
3. overlay explicit facet values and option values, independent of source
   order;
4. reject malformed, duplicate, conflicting, unknown, and inapplicable entries;
5. lower the validated fixed `OperatorSpec` once.

&nbsp;&nbsp;&nbsp;&nbsp;**Why:** validation before lowering prevents source order
from selecting a path and prevents partially-created graph or state resources
from surviving a rejected call.

5.4 Defaults and the intrinsic raw baseline are declarations in the canonical
inventory, not hidden branches in the compiler. An explicit facet may replace a
default on the same axis; two explicit values on that axis conflict.

5.5 There is no legacy dual lowering. Removed adapter spellings such as
`n/sin`, `b/sin`, `lfo/sin`, and `blfo/sin` do not retain modifier meaning.
They are exact qualified symbols if such a binding exists, otherwise unknown
bindings. A targeted diagnostic MAY suggest `sin[norm]`, `sin[bi]`,
`sin[lfo]`, or `sin[lfo bi]`; it MUST NOT emulate the rejected spelling.

---

## 6. Failure Contract

6.1 The compiler rejects all modifier-shape errors before graph lowering:

| source | rejection |
|---|---|
| `sin[]` | empty modifier block |
| `sin[norm norm]` | duplicate facet |
| `sin[norm rad]` | conflicting input-domain facets |
| `sin[uni bi]` | conflicting output-range facets |
| `+[uni]` | facet is inapplicable to the resolved base |
| `sin[once]` | unknown facet; `once` is a form |
| `pulse[:duty]` | missing option expression |
| `pulse[:duty 0.2 :duty 0.3]` | duplicate option |
| `sin[:duty 0.2]` | option is inapplicable to the resolved base |
| `sin[unknown]` | unknown facet |
| `sin[[uni]]` | malformed non-entry content |

6.2 A failure identifies the exact base and offending entry and leaves the
active graph, state resources, and source identity unchanged.

6.3 `sin [uni]` is not a malformed modifier. It follows ordinary call and
vector rules and may subsequently fail the bare callable's arity/type contract.

---

## 7. Normative Witnesses

7.1 Conformance MUST cover at least:

```lisp
(sin[uni norm] phase)
(sin[norm uni] phase)       ; same OperatorSpec and graph as the prior call
(pulse[bi :duty 0.2] phase)
(sin[lfo bi] 2)
(sin[uni] phase)            ; attached modifier
(sin [uni] phase)           ; spaced ordinary vector argument
```

7.2 Conformance MUST cover every malformed, duplicate, conflicting, unknown,
and inapplicable row in §6.1, plus every declared facet and option on every
callable for which it applies.

7.3 Generated reference data MUST project the callable inventory's exact base
identity, defaults, intrinsic baseline, applicable facets, conflicts, options,
option rates, and fixed display descriptions. It MUST NOT be hand-maintained.

## Open / Deferred

- **Additional facets.** A new facet requires one uniform semantic axis and a
  declared applicability rule; no additional facet is approved yet.
