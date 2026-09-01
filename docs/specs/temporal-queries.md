---
stability: evolving
layer: behavioural
---

# Temporal Signal Queries

> Spec: turn an analyzable `time -> value` signal into a Tidal-style
> `arc -> events` result by solving the inverse image of a value condition.
> The current implementation is a sound partial solver: it is exact on its
> declared fragment and reports unsupported input outside that fragment.

## Source files

- `uSEQ/src/signal_engine/temporal_query.{h,cpp}` — graph analysis, analytic
  roots, directed crossings, range spans, and explicit completeness status.
- `uSEQ/src/signal_engine/program_semantics.{h,cpp}` — compositional effects
  used to distinguish schedule, external-environment, and ground-time domains.
- `uSEQ/src/signal_engine/cold_eval.cpp` — `q/*` parsing, scratch compilation,
  and printable ModuLisp results.
- `test/signal_engine/test_temporal_query.cpp` — solver and language-boundary
  witnesses.
- `scripts/wasm_init_smoke.mjs` — generated-WASM `useq_eval` witness.

---

## 1. Model

1.1 A scalar signal denotes a function `Signal a = Time -> a`. A temporal
query adds an arc and a condition, producing a pattern-like result:

```text
solve : Signal a -> Condition a -> Arc -> [Event]
```

For equality and range conditions, the result is the connected components of
the inverse image `signal^-1(condition)` intersected with the requested arc.
For a crossing condition, the result is the oriented boundary points where
the signal passes through the requested level.

1.2 This transformation is not evaluation at a denser sample rate. The
analyzer interprets the compiled pure DAG into a finite partition of the
requested arc, derives a closed form on each partition, and solves each
condition analytically. It MUST NOT silently substitute sampled guesses.

1.3 No implementation can decide arbitrary predicates over arbitrary opaque
real functions in general. Temporal querying is therefore a family of sound
partial decision procedures. A complete result means complete for the stated
condition, arc, numerical tolerance, and supported signal fragment. An
unsupported result is distinct from a complete result containing no events.

---

## 2. ModuLisp surface

2.1 The host and generated-WASM profiles expose these exact qualified,
top-level-only forms:

```lisp
(q/zeros [start end] target)
(q/crossings level [start end] target)
(q/crossings level [start end] :rising target)
(q/crossings level [start end] :falling target)
(q/range [low high] [start end] target)
(q/explain [start end] target)
```

The firmware profile does not currently carry the temporal analyzer.

2.2 **The target signal is always the final argument.** This is true in every
signature, including directed crossings. A form with anything after its
target is an eval error; optional direction immediately precedes the target.

2.3 `target` may be an arbitrary signal expression or a bare published output
name such as `a1`. In a query, a final bare output name denotes that output's
published graph, not its previous-sample scalar meaning inside another signal.

2.4 `[start end]` is a finite, increasing, half-open logical-time arc
`[start,end)`, in the same seconds-based coordinate used by `t`. Value ranges
are finite, closed ranges `[low,high]` and require `low <= high`.

2.5 `q/zeros` returns every equality component at level zero, including
tangent instants and non-zero-duration spans. `q/crossings` returns only
sign-changing crossings; tangencies are not crossings. `:rising` and
`:falling` filter by orientation. `q/range` returns maximal components for
which the signal lies inclusively between its bounds.

---

## 3. Results and event identity

3.1 A complete condition query returns a printable vector. Instant and span
events have these shapes:

```lisp
[:instant :rising :whole [0 0] :part [0 0]]
[:span :whole [0.4 0.6] :part [0.4 0.6]]
[:span :whole nil :part [0 0.1]]
```

3.2 `part` is always the portion inside the requested query arc. `whole` is
the complete connected event when proven. It is `nil` when the component
touches a query edge and the analyzer did not inspect beyond that edge. The
runtime MUST NOT guess a whole from a clipped part.

3.3 Direction is `:rising`, `:falling`, `:tangent`, or `:any`. A directed
discontinuity crossing is an instant even if the signal never equals the
crossing level at that exact boundary.

3.4 An incomplete analysis returns a tagged map rather than an event vector:

```lisp
{:status :unsupported-signal
 :analysis :analytic-piecewise-affine-wave
 :segments 0
 :detail-node 17}
```

Other printable statuses are `:inactive-output`, `:segment-limit`, and
`:event-limit`. Malformed or non-finite ModuLisp arguments are ordinary eval
errors; the lower-level C++ query result also has `InvalidArgument`. A complete
empty vector `[]` alone means the analyzer proved that no matching event
exists.

3.5 `q/explain` always returns the status map. On success its status is
`:complete` and `:segments` reports the finite partition size.

---

## 4. Current complete fragment

4.1 The current analysis kind is `:analytic-piecewise-affine-wave`. It accepts
a sine or cosine root (bipolar or normalized unipolar), optionally followed by
constant affine output mapping. Its phase/angle must be piecewise affine in
`t` over the requested arc.

4.2 The phase analyzer currently closes over constants, timing cells,
`live-edit` values, `t`, affine `+`/`-`, multiplication or division by a
piecewise constant, `abs`, forward `floor`/`frac`, forward non-negative
remainder, and stepped vector indexing such as `from-list`.

4.3 State loads, hardware inputs, previous-output loads inside an inline
expression, opaque/non-wave roots, nonlinear products, division by a varying
signal, singularities, reverse discontinuous partitions, and other unlisted
nodes return `:unsupported-signal`. Supporting one of these requires another
sound abstract domain or solver; it MUST NOT broaden `:complete` by sampling.

4.4 Analysis uses finite bounds: at most 512 affine segments and 64 printable
events per query. Crossing and equality comparisons use the analyzer's stated
time/value tolerances. Exceeding a bound reports its limit status; narrowing
the query arc is a valid recovery.

---

## 5. Isolation and profile boundary

5.1 An inline final target compiles in the engine's scratch node pool. Query
compilation and analysis MUST NOT change published output roots, source,
dependencies, state ownership, live slot ownership, or the persistent data
table count. Literal vectors created for the scratch target are discarded
after the query.

5.2 A query is cold-path work. It may allocate host/WASM analysis storage and
MUST NOT add work or allocation to per-sample execution.

5.3 Browser callers use the ordinary `useq_eval` ABI with the ModuLisp forms
above. There is no parallel JavaScript-only query language or API surface.

## Open / Deferred

- Symbolic rational, polynomial, interval, and monotonicity domains for
  nonlinear phase expressions.
- Stateful projection from a snapshot/checkpoint without mutating live state.
- A first-class runtime event value replacing the current printable text
  representation.
- Condition combinators (`and`, `or`, complement) once their event-boundary
  and completeness algebra is specified.
