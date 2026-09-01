---
stability: evolving
layer: behavioural
---

# Qualified Symbols

> Spec: `/` as exact qualified binding identity in ModuLisp. Callable
> specialization belongs to
> [operator-modifiers.md](operator-modifiers.md), not to qualification.

## Source files

- `uSEQ/src/signal_engine/token.cpp` — symbol tokenization and the bare `/`
  exception.
- `uSEQ/src/signal_engine/graph_builder.cpp` — lexical and callable binding
  resolution.
- `uSEQ/src/signal_engine/synth_registry.cpp` — registry-supplied qualified
  callables such as `osc/sine`.
- `test/signal_engine/test_builtin_conformance.cpp` — exact-resolution and
  malformed-symbol witnesses.

---

## 1. Frame

1.1 A **qualified symbol** is one symbol token with two non-empty identity
segments separated by one `/`, such as `voice/gain` or `osc/sine`.
Qualification is part of the binding's exact name; it is not an adapter,
fallback search path, import hint, or modifier.

1.2 The symbol whose entire text is `/` remains the division callable. It is
not a qualified symbol.

&nbsp;&nbsp;&nbsp;&nbsp;**Why:** division is established language surface, while
non-empty segments make the qualified grammar unambiguous.

---

## 2. Identity and Resolution

2.1 Resolution looks up the complete token exactly. `left/gain`, `right/gain`,
and `gain` are three distinct binding identities; none falls back to another.

2.2 The same local segment may exist under multiple qualifiers:

```lisp
(define left/gain 0.25)
(define right/gain 0.75)
(+ left/gain right/gain)
```

The two definitions coexist and reads resolve to their exact bindings.

2.3 Registry names such as `osc/sine` obey the same exact-identity rule. When
registered as callables, `(osc/sine :freq 440)` resolves that exact registry
binding; `osc` is not interpreted as modifier policy.

2.4 An attached modifier block is parsed only after exact base resolution:
`osc/sine[raw]` means the exact base `osc/sine` specialized by `raw` under
[operator-modifiers.md](operator-modifiers.md).

&nbsp;&nbsp;&nbsp;&nbsp;**Why:** base-first resolution keeps one callable
inventory and makes qualification orthogonal to specialization.

---

## 3. Formation and Failures

3.1 A qualified symbol contains exactly one `/` between two non-empty symbol
segments. `/name`, `name/`, and `a/b/c` are malformed qualified symbols.

3.2 A malformed or unknown qualified symbol is rejected as a binding error
before call lowering. The compiler MUST NOT reinterpret any segment as an
operator modifier.

3.3 Removed adapter spellings MAY receive targeted migration diagnostics, but
there is no dual resolution. For example, an unknown `lfo/sin` may suggest
`sin[lfo]`; it never lowers as that modifier implicitly.

---

## 4. Normative Witnesses

4.1 Conformance MUST distinguish:

```lisp
(/ 8 2)                    ; division
left/gain                  ; exact qualified read
right/gain                 ; different exact qualified read
(osc/sine :freq 440)       ; exact registry callable
(osc/sine[raw] :freq 440)  ; qualification plus orthogonal modifier
```

4.2 Conformance MUST reject empty segments, multiple separators, unknown exact
bindings, and any attempt to obtain modifier semantics through `/`.

## Open / Deferred

- **Deeper qualification.** More than two segments is not admitted by this
  contract; no demonstrated binding hierarchy requires it.
