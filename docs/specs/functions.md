# Functions

> Spec: callable cells, anonymous lambdas, signal-context inlining,
> recursion rules, variadic arithmetic. Counterpart to [MAIN.md](MAIN.md).

1.1 `(defn name [p1 p2 ...] body)` defines a callable cell. `(fn [p1 p2 ...] body)` and `(lambda [p1 p2 ...] body)` produce anonymous callables.

1.2 **In signal context, callables are inlined at every call site.** No closure object is allocated, no environment is captured at runtime. The function's body is re-compiled with parameter symbols bound to the argument node graphs.

1.3 **Recursion in signal context is a compile-time error.** A signal must compile to a finite, acyclic graph. Recursive functions are top-level only.

1.4 **Higher-order functions returning callables** (e.g. `((fn [x] (fn [y] (* x y))) 3)`) are valid where the outer call's result is fully resolvable at compile time. Returning a callable from a runtime branch in signal context is rejected.

1.5 **Callables stored in cells** are referenced by name at the call site. The graph builder resolves the cell, inlines the body, records a dependency. Redefining the function invalidates every signal that calls it (see [cells.md](cells.md)).

1.6 At top level (and in imperative mode), callables are first-class values: stored, passed, returned, called dynamically.

1.7 Variadic arithmetic operators (`+`, `*`, `-`, `/`, `min`, `max`) are left-folded into pairwise nodes during compilation. `(+)` is `0`, `(*)` is `1`, `(- x)` is `(- 0 x)`, `(/ x)` is `(/ 1 x)`.

1.8 **Local time/rate context is lexical through inlining.** If a callable is
called inside `time-as` or `rate-as`, its body is inlined under the caller's
current local time and local `dt` context. A callable does not capture a hidden
clock; it sees the context of the call site.

1.9 **Keyword arguments are ordinary call syntax for callees that declare
them.** Builtin primitives and UGens may declare keyword options (for example
`:phase`, `:id`, `:fresh`). User-defined functions do not accept arbitrary
keyword arguments unless and until a future function-signature extension defines
that surface.

## Open / Deferred

2.1 **Closures escaping signal context.** Returning a callable from a runtime branch is rejected today. Whether some restricted form (e.g. closures over compile-time constants only, transformed into inlined alternates) is allowed is open.
