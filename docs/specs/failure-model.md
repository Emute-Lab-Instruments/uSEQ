# Failure Model

> Spec: compile-time vs runtime errors, LKG fallback, diagnostic survival,
> numerical hygiene. Counterpart to [MAIN.md](MAIN.md). See
> [compilation.md §3](compilation.md) for the compile-time rejection rules
> in signal context, and [ERROR_HANDLING_SPEC.md](ERROR_HANDLING_SPEC.md)
> for the on-wire diagnostic format.

1.1 ModuLisp distinguishes **compile-time errors** (program never produces a value) from **runtime errors** (program ran but a sample was unhealthy).

1.2 **Compile-time errors** include: parse errors, unresolved symbols (with fuzzy-match suggestions), arity mismatches, side-effect forms inside signal context, recursion in signal context, oversized `for` collections, type errors that the compiler can prove (e.g. arithmetic on a string with no type-promoting operator), unsupported builtins for the current target, and structurally invalid forms.

1.3 Compile-time errors are **structured diagnostics**: they carry severity, category, source span, human-readable message, and a suggestion (often a working example). They surface in the editor as inline annotations and in the firmware as serial-protocol messages.

1.4 **A compile-time error does not stop the music.** The previously active program for that output (if any) keeps running. Other outputs are unaffected.

1.5 **Runtime errors** are: division by zero, out-of-bounds vector access on an unbounded path, integer-modulo by zero, NaN/Inf propagation that survives to the output root, and invalid local-clock deltas in state-bearing contexts.

1.6 `rate-as` rates follow ordinary numeric health rules. A non-finite rate is
a runtime error. Zero rate is valid: the local clock stops. Negative rates are
valid only for primitives that explicitly document reverse-time state semantics;
otherwise they are compile-time errors when provable or runtime errors when
only discovered from inputs.

## 2. Last-Known-Good (LKG) Fallback

2.1 **Whole-output LKG fallback.** When an active output program errors at runtime — including non-finite values reaching the root — that output **switches to its LKG program** for the rest of the current sampling pass and remains on LKG until either (a) the user replaces the broken program with a working one, or (b) the user explicitly clears the output.

2.2 **What is "last-known-good"**: the most recent program for that output that has completed at least one full healthy sample batch. LKG is observed safety, not provable safety — a graph that succeeded once may fail later under different time / inputs.

2.3 **LKG bindings are frozen.** When a graph becomes LKG, its inlined cell values are baked at the moment of LKG promotion. Subsequent cell mutations do **not** rebind LKG. This keeps fallback deterministic and avoids a recompile inside an error path.

2.4 **Bootstrap.** If an output has never had a healthy program, runtime error falls back to the last valid sample if one exists; otherwise to the neutral default (`0`).

2.5 **Cascading failures don't promote unhealthy programs.** If A is healthy and becomes LKG, then B replaces A and immediately fails, the engine falls back to A — not to a "B → fall back" chain. Only programs that have completed a healthy batch are eligible to be LKG.

2.6 **Compile-time errors do not consume LKG.** A program that fails to compile is not promoted, demoted, or substituted; the active program is unchanged.

## 3. Numerical Hygiene

3.1 **Numerical errors are not silently zeroed at the output level.** Per-node NaN/Inf clamping (if any engine performs it) is an internal hygiene mechanism; the user-observable contract is "if the output goes unhealthy, you fall back to LKG, and you see a diagnostic." The engine must not silently produce subtly-wrong output instead of declaring failure.

## 4. Diagnostic Survival

4.1 **Diagnostics survive across evals.** Per-output health (`idle` / `running` / `fallback` / `error`) is queryable by the editor and shown to the user. A successful eval clears prior diagnostics for that output.
