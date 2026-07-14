# Diagnostics ABI and Wire Format

> Spec: the on-wire format for compile-time and runtime diagnostics, the
> WASM ABI surface that produces them, and the firmware serial JSON
> embedding. Counterpart to [MAIN.md](MAIN.md). For the *semantics* of
> failure (when does the output go to LKG, how diagnostics survive,
> per-output health states, REPL vs editor channels) see
> [failure-model.md](failure-model.md); this doc only specifies the
> data shapes and the ABI calls that produce them.

### Source Files

- `uSEQ/src/signal_engine/diagnostics.{h,cpp}` — `Diagnostic` struct, `DiagnosticSeverity`/`DiagnosticCategory` enums, `severity_to_cstr()`, `category_to_cstr()`, `find_fuzzy_match()`
- `uSEQ/src/signal_engine/graph_builder.{h,cpp}` — `report_error()`, `report_error_with_fuzzy_match()`, `report_warning()`, `GraphBuildResult.diagnostics`
- `uSEQ/src/signal_engine/cold_eval.{h,cpp}` — `EvalResult.diagnostics` (diagnostics from eval), diagnostic propagation
- `uSEQ/src/signal_engine/token.{h,cpp}` — `Token.span_start`/`span_len` (source span on every token), tokenizer error diagnostics
- `wasm/wasm_wrapper.cpp` — `useq_last_diagnostics()` (JSON array from last eval), `useq_active_diagnostics()` (per-output health state), JSON serialization via `JsonBuilder`
- `uSEQ/src/firmware/serial_protocol.{h,cpp}` — `send_eval_response()` (embeds diagnostics in JSON), `send_diagnostics()` (standalone diagnostic frames)
- `uSEQ/src/utils/json_builder.h` — `JsonBuilder`: lightweight JSON construction for diagnostic serialization
- `uSEQ/src/utils/error_messages.{h,cpp}` — static error message strings

## 1. Source Span

1.1 A **source span** locates a region of source text. Character offsets, not line/column — the editor (CodeMirror) works natively with character positions and can derive line/column for display.

```cpp
struct SourceSpan {
    uint16_t start;   // character offset from start of source text
    uint16_t end;     // exclusive end offset
};
```

1.2 **Limit.** `uint16_t` allows offsets up to 65535. Live-coding expressions are short; this is sufficient. Source strings exceeding the limit emit diagnostics with a clamped `{0, 0}` span — the diagnostic is still produced, it just can't point at a specific location. (See `uSEQ/src/signal_engine/diagnostics.h` — Diagnostic.span_start, span_len, both uint16_t; `uSEQ/src/signal_engine/token.h` — Token.span_start, span_len; `uSEQ/src/signal_engine/node_pool.h` — Node.span_start, span_len.)

1.3 **Two coordinate spaces.** Spans live in either of two spaces; the system must not conflate them.

- **Eval-relative spans** are offsets into the code string passed to `useq_eval()`. Direct top-level eval errors are reported in this space; the frontend has the eval string and maps offsets straight to editor positions.
- **Expression-relative spans** are offsets into the *inner expression* stored by an output assignment. When the user evals `(a1 (sin (* x beat)))`, the output stores `(sin (* x beat))` — a substring starting at offset 4. If this expression is later recompiled (cell-mutation triggered), spans report against the inner substring.

The runtime tracks the **eval-time offset** of each stored expression. The frontend reconstructs the original editor position with `editor_position = expression_span + stored_offset`. The offset is recorded at assignment time and does not change until the expression is reassigned.

1.4 **Span propagation through compilation.**

- **Inlining.** When a `define`d expression is inlined, the *reference site* span (where the user wrote the symbol) is used for error reporting, not the original definition's span. The user cares about what they wrote, not what it expanded to.
- **Constant folding.** When `(+ 1 2)` folds to `3`, the result carries the span of the original `(+ 1 2)`.
- **Synthesised nodes.** When the parser wraps multiple top-level forms in a `(do ...)` that wasn't in the source, the synthesised wrapper carries `{0, 0}`. Diagnostics on synthesised nodes point at the first child's span.

## 2. Diagnostic

2.1 A single diagnostic from the compiler:

```cpp
enum class DiagnosticSeverity : uint8_t {
    Info,       // style suggestion or non-blocking observation
    Warning,    // suspicious but not broken
    Error       // expression will not work
};

enum class DiagnosticCategory : uint8_t {
    Syntax,         // parse errors: unmatched parens, bad tokens
    UndefinedName,  // unknown function or variable
    Arity,          // wrong number of arguments
    Type,           // wrong type of argument
    Boundary,       // side-effect in signal context, eval in signal
    Arithmetic,     // division by zero, non-finite result
    Runtime,        // loop budget, call depth, intrinsic failure
    Overflow        // value out of expected range (MIDI, phasor, etc.)
};

struct Diagnostic {
    DiagnosticSeverity severity;
    DiagnosticCategory category;
    SourceSpan         span;          // {0,0} if not span-attributable
    String             message;       // one plain sentence, no jargon
    String             suggestion;    // concrete fix (may be empty)
    String             example;       // working code (may be empty)
    String             triggered_by;  // empty for direct eval; symbol name
                                      // for dependency-triggered recompiles
};
```

2.2 The **wire severity values** are lowercase strings: `"info"`, `"warning"`, `"error"`. Implementations may use any internal enum spelling but the JSON contract is normative. ("Hint" is a synonym for `info` in some external systems; it is not a separate level here.)

2.3 The **wire category values** are lowercase, underscore-separated: `"syntax"`, `"undefined_name"`, `"arity"`, `"type"`, `"boundary"`, `"arithmetic"`, `"runtime"`, `"overflow"`.

2.4 **Multiple diagnostics per compile.** A single `useq_eval()` may produce multiple diagnostics. The compiler runs report-and-continue on non-fatal errors (arity, type, undefined name) so the user sees all problems in one pass; fatal errors (malformed AST, structural failures) abort early. Cascade-noise mitigation (typed placeholders, the per-eval display cap) is documented in [failure-model.md §8](failure-model.md).

2.5 **Warnings are not errors.** A program that compiles with warnings runs normally. Warnings flag suspicious patterns: `(floor 3)` ("no effect on a whole number"), `(+ x 0)` ("simplifies to `x`"), `(if 1 a b)` ("else branch never runs"), `(clamp x 0.5 0.3)` ("min greater than max — values may be swapped").

2.6 **Plain language is normative.** Messages must read as if explaining to someone who just started using the system. Lead with what to do, show a working example whenever possible, never use jargon ("arity mismatch", "predicate", "lvalue") without immediately explaining it.

## 3. Runtime Diagnostic

3.1 Errors that occur during execution carry a different shape because the executor operates on compiled graphs, not source text:

```cpp
struct RuntimeDiagnostic {
    DiagnosticCategory category;
    String output;              // output slot name: "a1", "d3", ...
    String message;
    bool   transient;           // comes and goes with time phase?
    double last_occurrence;     // wall time (s) of most recent occurrence
};
```

3.2 **No source span by default.** The compiled graph has no direct line back to source text. The output slot name identifies *which expression* is failing; the frontend maps that to the editor pane.

3.3 **Future: instruction-level source maps.** A debug-info table mapping execution-graph nodes to source spans would let the runtime attach a span to the failing subexpression. The `Diagnostic` struct is already span-capable. This is a future enhancement, not part of the current ABI.

3.4 **Transience.** A diagnostic is transient when the failure is time-phase-dependent: the expression succeeded at some samples and failed at others (`(/ 1 (sin t))` produces a NaN once per cycle, finite values elsewhere). Persistent diagnostics are produced by failures that happen on every sample. The semantic significance of transience is in [failure-model.md §1.7](failure-model.md).

## 4. WASM ABI

The WASM build exports three diagnostic functions. All return JSON strings via `EMSCRIPTEN_KEEPALIVE`. The returned pointer is owned by the runtime and is invalidated by the next call to the same function.

### 4.1 `useq_last_diagnostics`

(See `wasm/wasm_wrapper.cpp` — useq_last_diagnostics function, JSON serialization via JsonBuilder.)

```cpp
EMSCRIPTEN_KEEPALIVE
const char* useq_last_diagnostics();
```

Returns a JSON array of `Diagnostic` objects from the most recent `useq_eval()`. Empty array when the eval succeeded clean. Always valid JSON.

Spans are eval-relative — they reference offsets into the code string passed to `useq_eval()`.

```json
[
  {
    "severity": "error",
    "category": "arity",
    "start": 5,
    "end": 12,
    "message": "sin needs exactly 1 value to work with",
    "suggestion": "Try: (sin beat)",
    "example": "(sin (* 6.28 beat))",
    "triggered_by": null
  }
]
```

### 4.2 `useq_active_diagnostics`

(See `wasm/wasm_wrapper.cpp` — useq_active_diagnostics function; reports per-output runtime LKG-fallback entries from `NodePool::runtime_fallback_mask`, see [failure-model.md §3.2](failure-model.md).)

```cpp
EMSCRIPTEN_KEEPALIVE
const char* useq_active_diagnostics();
```

Returns a JSON **array** of per-output diagnostics. Each entry is a `RuntimeDiagnostic` that carries its own output attribution (the output it belongs to), so a single flat array describes the active state of every output. Outputs with no active diagnostics contribute no entries.

```json
[
  {"output": "a1", "severity": "error", "category": "arithmetic", "message": "..."},
  {"output": "d3", "severity": "warning", "message": "..."}
]
```

The empty case (all outputs healthy) is `[]`.

This includes both compile errors from background recompilation (cell-mutation triggered) and runtime errors from the current frame. The editor maps each array entry to its corresponding output-health entry via the entry's output attribution.

### 4.3 `useq_output_diagnostics`

**Deferred — not yet built; the frontend currently polls `useq_active_diagnostics`, which is perf-negligible.** This export does not exist in `wasm/wasm_wrapper.cpp`, `build_wasm.sh`, or `src/contracts/wasmAbi.ts`. The design below is retained as rationale only — do not bind to this phantom export.

```cpp
EMSCRIPTEN_KEEPALIVE
const char* useq_output_diagnostics();
```

Returns runtime-only diagnostics (`RuntimeDiagnostic` shape) for every output in error or fallback state. Empty object when all outputs are healthy. The frontend uses this to drive output-health indicators without re-reading the larger `useq_active_diagnostics` payload every frame.

```json
{
  "a1": {
    "category": "arithmetic",
    "message": "dividing by zero — the result is undefined",
    "transient": true,
    "last_occurrence": 12.34
  }
}
```

### 4.4 Clearing policy

- `useq_last_diagnostics()` is cleared at the start of every `useq_eval()` call. It only reflects the most recent eval.
- Per-output **compile** diagnostics are cleared when the output is successfully recompiled (new expression assigned, or a dependency change triggers a clean recompile).
- Per-output **runtime** diagnostics are cleared when the output runs a full healthy sample batch.
- `useq_active_diagnostics()` is the union of all per-output state and is never cleared explicitly — it reflects the live state of the system.

### 4.5 Polling cadence

The frontend reads `useq_last_diagnostics()` immediately after every eval for responsive inline feedback. It polls `useq_active_diagnostics()` (or `useq_output_diagnostics()` when only health indicators are needed) once per animation frame to reflect background-recompilation errors and runtime state.

## 5. Firmware Serial Embedding

5.1 The firmware ships diagnostics inside its existing serial JSON eval response. The response gains an optional `diagnostics` array with the same per-object shape as the WASM ABI. (See `uSEQ/src/firmware/serial_protocol.cpp` — send_eval_response embeds diagnostics; `uSEQ/src/utils/json_builder.h` — JsonBuilder for JSON construction.)

```json
{
  "id": "req-42",
  "result": "...",
  "diagnostics": [
    {
      "severity": "error",
      "category": "arity",
      "start": 1,
      "end": 5,
      "message": "sin needs a value to work with",
      "suggestion": "Try: (sin beat)"
    }
  ]
}
```

5.2 Per-output runtime state on firmware is communicated through LED colour ([firmware.md §3.2](firmware.md)) — the firmware does not currently push periodic diagnostic updates over serial outside eval responses.

5.3 The data model is identical on both targets. Only the delivery channel differs.

## 6. Frontend Mapping

6.1 The diagnostic types map to CodeMirror's lint extension as follows:

| Diagnostic field | CodeMirror field |
|---|---|
| `start` / `end` (after eval-time-offset adjustment) | `from` / `to` |
| `severity` | `severity` (same enum, lowercase string) |
| `message` + `suggestion` + `example` | rendered into the diagnostic tooltip |

6.2 For each runtime-diagnostic output slot, the visualisation layer applies a subtle indicator (border colour, gutter icon) on the editor pane that owns the affected output. The indicator state machine (idle / running / fallback / error) is in [failure-model.md §5](failure-model.md).

## 7. Fuzzy Name Suggestions

7.1 Diagnostics in the `undefined_name` category include a `suggestion` field that points at the nearest known symbol when one is plausible. The matching algorithm is implementation-defined but must consider, at minimum. (See `uSEQ/src/signal_engine/diagnostics.cpp` — find_fuzzy_match, Levenshtein distance, prefix, case-insensitive; `uSEQ/src/signal_engine/graph_builder.cpp` — report_error_with_fuzzy_match invokes find_fuzzy_match on unresolved symbols.)

- Levenshtein distance on lowercased names, with a hard cutoff (typically `≤ 2`).
- Prefix matches (`si` → `sin`).
- Case-insensitive matches (`Sin` → `sin`).

7.2 The candidate pool is the union of all builtin function names, all currently-defined user symbols, and all temporal-leaf names (`t`, `beat`, `bar`, `phrase`, `section`, plus `-num`/`-dur` variants).

7.3 At most three candidates may be surfaced; if none has distance ≤ 2 (or its prefix/case-insensitive equivalent), the diagnostic ships with an empty `suggestion`.

## 8. Suggestion Validity

8.1 Every diagnostic that includes a non-empty `suggestion` field with example code must produce code that itself compiles successfully if pasted into a fresh eval. Bad suggestions are worse than no suggestions.

8.2 This is enforceable as a meta-test in the diagnostic test suite: for every fixture asserting a suggestion, compile the suggestion text and assert success. See `docs/specs/state.md` for the analogous golden-fixture pattern.

## 9. Out of Scope

9.1 **Console formatting** of REPL eval results (success cases) is not part of this spec — see [failure-model.md §6](failure-model.md) for the REPL-vs-output channel split.

9.2 **Test fixture YAML** for diagnostic content/span/multi-diagnostic assertions lives with the test harness, not in this spec.
