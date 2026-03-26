# Error Handling Specification

## Status

**Draft** | March 2026 | Companion to `BYTECODE_VM_SPEC.md`

Supersedes `error_handling_design.md`, which described an aspirational `Response` class that was never implemented. This spec describes the error handling system we are building into the bytecode VM compiler, integrated with the browser-based editor in `useq-perform`.

---

## 1. Principles

### 1.1 Errors are a conversation, not a verdict

The user is a musician live-coding on stage or in a studio. They may not know what an "arity mismatch" is. Every error message must be written as if explaining to a friend who just started using the system.

- **Lead with what to do**, not what went wrong
- **Show a working example** whenever possible
- **Never use jargon** without immediately explaining it in plain terms

### 1.2 Never stop the music

A broken expression must never cause silence, glitches, or a frozen UI. The LKG (last-known-good) fallback system (see `BYTECODE_VM_SPEC.md` §6.3) guarantees audio continuity. Error reporting exists on top of this guarantee — it informs, it never interrupts.

### 1.3 Show, don't tell

Errors should appear **where they happened** — as inline annotations in the editor, not buried in a console log. The console is a secondary channel for history and detail; the primary feedback channel is the code itself.

### 1.4 Progressive disclosure

Every diagnostic has three layers:

1. **Indicator** — a coloured underline on the offending span (visible at a glance)
2. **Summary** — one sentence, shown on hover or as an inline label
3. **Detail** — suggestion, working example, and explanation (expandable)

The user chooses their depth. A beginner reads all three; an expert glances at the underline and knows what's wrong.

### 1.5 Temporal awareness

ModuLisp expressions are signals — pure functions of time. An expression can be valid at `beat = 0.3` and produce `NaN` at `beat = 0.95`. Runtime diagnostics must account for this: they describe *when* something goes wrong, not just *that* it went wrong.

### 1.6 Rate limiting

A signal sampled 100 times per frame that produces an error every sample must not produce 100 diagnostics. Runtime errors are **deduplicated by (output, error category)** and rate-limited to at most one report per output per frame. The diagnostic may note "occurring frequently" if the error persists across multiple frames.

### 1.7 Silence on success is also a bug

A beginner typing `(a1 (sin beat))` needs to know it *worked*. If nothing visibly changes, they don't know whether their code was received, compiled, or is running. Success feedback is as important as error feedback — it closes the loop.

Success should be communicated through the same channels as errors but more subtly:

- **Brief confirmation** — a transient flash or label ("a1 updated") that fades after ~1 second
- **Output indicator state change** — the output slot transitions from "idle" or "error" to "running"
- **Visualisation** — if the output is being visualised, seeing the waveform change is itself confirmation

Success feedback must not be noisy. It should be visible enough for a beginner to notice, quiet enough that an expert performing on stage never thinks about it.

---

## 2. Diagnostic Data Model

### 2.1 Source Span

The foundation of all error reporting. A span locates a region of source text.

```cpp
struct SourceSpan {
    uint16_t start;   // character offset from start of source text
    uint16_t end;     // exclusive end offset
};
```

Character offsets rather than line/column because the editor (CodeMirror) works natively with character positions. Line/column can be derived for display.

**Limit**: `uint16_t` gives a maximum offset of 65535 characters. This is sufficient for live-coding (individual expressions are short), but if a source string exceeds this limit, spans clamp to `{0, 0}` (no span information) rather than wrapping. The diagnostic is still produced; it just can't point at a specific location.

Every AST node (`Value`) must carry an optional `SourceSpan`. The parser populates spans during parsing; the compiler reads them when generating diagnostics.

#### Span coordinate spaces

Spans exist in two coordinate spaces and the system must not conflate them:

1. **Eval-relative spans**: offsets into the code string passed to `useq_eval()`. These are what the frontend receives for top-level eval errors. The frontend knows the full eval string and can map these directly to editor positions.

2. **Expression-relative spans**: offsets into the inner expression stored by an output assignment. When the user evals `(a1 (sin (* x beat)))`, the output stores `(sin (* x beat))` — a substring starting at offset 4 in the original eval. If this expression is later recompiled (due to a dependency change), its spans are relative to this inner substring, not the original eval string.

The interpreter must track the **eval-time offset** of each stored expression so the frontend can reconstruct the original position: `editor_position = expression_span + stored_offset`. This offset is recorded at assignment time and does not change until the expression is reassigned.

### 2.2 Diagnostic

A single diagnostic message from the compiler or runtime.

```cpp
enum class DiagnosticSeverity : uint8_t {
    Hint,       // style suggestion, not an error
    Warning,    // something suspicious but not broken
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
    SourceSpan span;                // where in the source text
    String message;                 // one plain sentence
    String suggestion;              // concrete fix (may be empty)
    String example;                 // working code example (may be empty)
};
```

### 2.3 Compile Result (revised)

Replaces the current `NumericVmCompileResult`:

```cpp
struct NumericVmCompileResult {
    bool ok = false;
    NumericVmProgram program;
    std::vector<Diagnostic> diagnostics;   // ALL diagnostics, not just the first
};
```

Key change: **multiple diagnostics**. The current compiler stops at the first `fail()` call. The revised compiler should continue past non-fatal errors (arity, type, undefined name) to collect as many diagnostics as possible in a single pass. Fatal errors (malformed AST, unrecoverable parse failure) still abort immediately.

An expression can compile successfully *and* carry warnings — e.g. "this value is always 0, did you mean something else?" or "using floor on an integer has no effect".

### 2.4 Runtime Diagnostic

For errors that occur during execution, not compilation:

```cpp
struct RuntimeDiagnostic {
    DiagnosticCategory category;
    String output;              // which output slot: "a1", "d3", etc.
    String message;
    bool transient;             // comes and goes with time phase?
    double lastOccurrence;      // time (seconds) of most recent occurrence
};
```

Runtime diagnostics are not attached to source spans in the initial implementation because the VM executes compiled bytecode, not source text. However, the output slot identifies *which expression* is failing, and the frontend can map that back to the editor pane.

**Future enhancement — instruction-level source maps**: The compiler could emit a debug info table mapping instruction indices to source spans (analogous to JavaScript source maps or DWARF debug info). When a runtime error occurs at instruction N, the VM would look up the corresponding source span and attach it to the diagnostic. This would let the frontend highlight the exact subexpression that caused a division by zero, not just the output slot. Deferred from the initial implementation to keep the first pass simple, but the `Diagnostic` struct is already span-capable so no data model changes would be needed.

### 2.5 WASM ABI Extension

The current WASM ABI (§6.4 of `BYTECODE_VM_SPEC.md`) needs one new export:

```cpp
// Returns JSON array of diagnostics from the last eval/compile.
// Empty array if no diagnostics. Always valid JSON.
const char* useq_last_diagnostics();
```

JSON schema for the wire format:

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

The `triggered_by` field is `null` for direct eval errors and a symbol name string for dependency-triggered recompilation errors (see §5.4).

This replaces the current `useq_last_error()` string export. The old export can remain as a compatibility shim that returns the first error's `message` field.

#### Diagnostic persistence and clearing

`useq_last_diagnostics()` returns diagnostics from the most recent `useq_eval()` call only. But the system must also track **persistent per-output diagnostics** — errors from background recompilation triggered by dependency changes, not by a user eval.

A third export provides this:

```cpp
// All currently active diagnostics across all outputs.
// Includes compile errors from dependency-triggered recompilation
// and runtime errors from the current frame.
// Returns JSON object keyed by output name:
// { "a1": [...diagnostics...], "d3": [...diagnostics...] }
// Outputs with no active diagnostics are omitted.
EMSCRIPTEN_KEEPALIVE
const char* useq_active_diagnostics();
```

**Clearing policy**:

- Per-output compile diagnostics are cleared when the output is successfully recompiled (new expression assigned, or dependency change triggers a clean recompile)
- Per-output runtime diagnostics are cleared when the output runs a full sample batch without errors
- `useq_last_diagnostics()` is cleared at the start of each `useq_eval()` call
- `useq_active_diagnostics()` is the union of all per-output state and is never explicitly cleared — it reflects the live state of the system

The frontend should poll `useq_active_diagnostics()` once per animation frame to keep the UI in sync, and read `useq_last_diagnostics()` immediately after each eval for responsive inline feedback.

---

## 3. Parser: Source Span Tracking

### 3.1 What changes

The parser must track character position as it consumes input and attach a `SourceSpan` to every `Value` node it produces.

```
Source text:  (sin (* 2 beat))
              ^              ^
              |              |
              start=0        end=16

Inner nodes:
  sin        → span {1, 4}
  (* 2 beat) → span {5, 15}
  *          → span {6, 7}
  2          → span {8, 9}
  beat       → span {10, 14}
```

### 3.2 Value extension

Add an optional span field to the `Value` class:

```cpp
// In value.h
SourceSpan span = {0, 0};  // {0,0} means "no span information"
```

This is a 4-byte addition per `Value`. On the firmware (RP2040), where typical programs have 10–50 AST nodes, this costs 40–200 bytes — negligible against the 264KB SRAM budget.

### 3.3 Span propagation

The compiler must propagate spans through transformations:

- **Inlining**: when a `define`'d expression is inlined, the span of the *reference site* (where the user wrote the symbol name) is used for error reporting, not the span of the original definition. The user cares about what they wrote, not what it expanded to.
- **Constant folding**: when `(+ 1 2)` is folded to `3`, the result carries the span of the original `(+ 1 2)` expression.
- **Macro/builtin expansion**: synthesised instructions carry the span of the source form that triggered the expansion.

---

## 4. Compiler Diagnostics

### 4.1 Error messages by category

Each category below shows the internal condition, the user-facing message, and the suggestion/example. Messages are written in plain language with no jargon.

#### 4.1.1 Undefined Name

**Condition**: Symbol not found in environment at compile time and cannot be late-bound.

| User wrote | Message | Suggestion |
|---|---|---|
| `(sni beat)` | *I don't recognise "sni"* | *Did you mean `sin`?* |
| `(flor beat)` | *I don't recognise "flor"* | *Did you mean `floor`?* |
| `(beet)` | *I don't recognise "beet"* | *Did you mean `beat`?* |

**Implementation**: Levenshtein distance ≤ 2 against all known symbols (builtins + user defines). Show up to 3 candidates, closest first.

#### 4.1.2 Arity

**Condition**: Wrong number of arguments to a known function.

| User wrote | Message | Suggestion | Example |
|---|---|---|---|
| `(sin)` | *sin needs a value to work with* | *Try: `(sin beat)`* | `(sin (* 6.28 beat))` |
| `(sin 1 2)` | *sin takes 1 value, but got 2* | *Try: `(sin beat)`* | |
| `(+ 1)` | *+ needs at least 2 values* | *Try: `(+ 1 2)`* | |
| `(seq [0 1])` | *seq needs a list and a timing signal* | *Try: `(seq [0 1] beat)`* | `(seq [0 0.25 0.5 0.75] beat)` |
| `(euclid 3)` | *euclid needs at least 3 values: hits, steps, and a timing signal* | *Try: `(euclid 3 8 beat)`* | |
| `(tri beat)` | *tri needs a duty cycle and a signal* | *Try: `(tri 0.5 beat)`* | `(tri 0.5 beat)` |

#### 4.1.3 Type

**Condition**: Argument is the wrong type for the operation.

| User wrote | Message | Suggestion |
|---|---|---|
| `(+ 1 "hello")` | *can't do maths with text — + needs numbers* | *If you meant a number, remove the quotes* |
| `(sin [1 2 3])` | *sin works on a single number, not a list* | *To apply sin to each value, use `(map sin [1 2 3])`* |
| `(seq 5 beat)` | *seq needs a list of values as its first argument* | *Try: `(seq [0 0.25 0.5] beat)`* |

#### 4.1.4 Signal Boundary

**Condition**: Side-effectful form used inside a signal context (see `BYTECODE_VM_SPEC.md` §2.5).

| User wrote | Message | Suggestion |
|---|---|---|
| `(a1 (define x 3))` | *can't use define inside an output — define changes state, but outputs are pure signals* | *Move the define outside: `(define x 3)` then `(a1 (sin (* x beat)))`* |
| `(a1 (eval "(sin beat)"))` | *can't use eval inside an output* | *eval runs code once at the top level — write the expression directly: `(a1 (sin beat))`* |
| `(a1 (schedule '(foo)))` | *can't use schedule inside an output — schedule is a one-time action* | *Use schedule at the top level, outside output assignments* |

These messages explain *why* the boundary exists — outputs are sampled hundreds of times per second; side-effects would execute hundreds of times, which is never what the user intended.

#### 4.1.5 Syntax

**Condition**: Malformed source text.

| User wrote | Message | Suggestion |
|---|---|---|
| `(sin (* 2 beat)` | *missing a closing parenthesis* | *Add `)` at the end* |
| `(sin beat))` | *extra closing parenthesis* | *Remove the last `)` — the expression is already complete* |
| `"hello` | *this string was never closed* | *Add `"` at the end* |

#### 4.1.6 Arithmetic (runtime)

| Condition | Message |
|---|---|
| Division by zero | *dividing by zero — the result is undefined* |
| Non-finite result (NaN/Inf) | *this expression produced an undefined number (NaN) — check for division by zero or sqrt of a negative* |
| Loop budget exceeded | *this loop ran too long and was stopped — check that the condition eventually becomes false* |

#### 4.1.7 Warnings (non-fatal)

Warnings compile successfully but flag suspicious patterns:

| Pattern | Message |
|---|---|
| `(floor 3)` | *floor on a whole number has no effect* |
| `(+ beat 0)` | *adding 0 doesn't change anything — you can simplify this to just `beat`* |
| `(if 1 a b)` | *this condition is always true — the else branch will never run* |
| `(clamp beat 0.5 0.3)` | *clamp min (0.5) is greater than max (0.3) — the values might be swapped* |

---

## 5. Dataflow: Compiler to Editor

### 5.1 Compilation path

```
User types code in editor
    │
    ▼
Editor sends code string via WASM ABI
    │  useq_eval(code)
    ▼
Parser
    │  parse(source) → Value tree with SourceSpans
    ▼
Bytecode VM Compiler
    │  compile_numeric_program(expr, env, signal_context)
    │  Produces: NumericVmCompileResult { ok, program, diagnostics[] }
    ▼
Diagnostics stored in interpreter state
    │  Accessible via useq_last_diagnostics()
    ▼
Frontend reads diagnostics after eval
    │  Calls useq_last_diagnostics() → JSON array
    ▼
Editor integration
    │  Maps SourceSpan → CodeMirror positions
    │  Creates inline decorations (underlines, gutters, tooltips)
    ▼
User sees annotated code
```

### 5.2 Compilation: collecting diagnostics

The compiler currently calls `fail(msg)` and returns `-1` on the first error. The revised approach:

1. **Non-fatal errors** (arity, type, undefined name): record a `Diagnostic`, emit a placeholder instruction (e.g. `LOAD_CONST nil` or `LOAD_CONST 0`), and **continue compiling**. This lets the compiler find multiple errors in one pass.

2. **Fatal errors** (malformed AST, unrecoverable structural problem): record a `Diagnostic` and stop. These are rare — mostly parse-level failures.

3. **Warnings**: always recorded, never stop compilation.

4. The final `NumericVmCompileResult.ok` is `false` if any diagnostic has severity `Error`. The program may still be partially compiled but should not be executed.

**Cascade noise policy**: The report-and-continue approach can produce misleading secondary errors. When `(sni beat)` is unknown and replaced with a placeholder `0`, downstream expressions that consume the placeholder may generate their own spurious diagnostics.

Mitigation rules:

- **Placeholder values are typed correctly**: use `0.0` (not `nil`) as the placeholder for numeric contexts so that downstream arithmetic doesn't produce additional type errors.
- **Mark placeholder registers**: the compiler tracks which registers hold placeholder values. Errors whose *only* failing input is a placeholder register are suppressed — they are consequences of the primary error, not independent problems.
- **Frontend cap**: even if the compiler produces many diagnostics, the frontend shows at most **5 errors per eval** to avoid overwhelming the user. Additional diagnostics are available via "show all" but are hidden by default. Warnings are shown separately and do not count against this cap.

```cpp
// Revised compiler internals (sketch)
class NumericVmCompiler {
    std::vector<Diagnostic> m_diagnostics;

    void report(DiagnosticSeverity severity,
                DiagnosticCategory category,
                SourceSpan span,
                const String& message,
                const String& suggestion = "",
                const String& example = "")
    {
        m_diagnostics.push_back({severity, category, span,
                                  message, suggestion, example});
    }

    // For non-fatal errors: report and return a safe placeholder register
    int report_and_continue(DiagnosticCategory category,
                            SourceSpan span,
                            const String& message,
                            const String& suggestion = "",
                            const String& example = "")
    {
        report(DiagnosticSeverity::Error, category, span,
               message, suggestion, example);
        // Emit a zero constant so compilation can continue
        int reg = allocate_register();
        emit_load_const(reg, 0.0);
        return reg;
    }
};
```

### 5.3 Runtime: per-output diagnostics

Runtime errors (division by zero, non-finite results, intrinsic failures) are produced by the VM executor, not the compiler. These cannot carry source spans because the VM operates on compiled bytecode.

The runtime diagnostic path:

```
VM executor returns TaggedVmExecutionResult { ok=false, error="..." }
    │
    ▼
modulisp_api.cpp: eval_output_internal()
    │  Stores diagnostic in slot->lastDiagnostic
    │  Falls back to LKG graph
    │  Deduplicates: only reports if error message changed or N frames elapsed
    ▼
Interpreter state: per-output diagnostic buffer
    │  Accessible via useq_output_diagnostics() → JSON
    ▼
Frontend polls periodically (once per animation frame)
    │  Maps output name ("a1") back to the editor pane
    ▼
Editor shows output-level indicator
    │  e.g. amber dot on the line where (a1 ...) was defined
    │  Tooltip: "a1 is falling back to the previous version — dividing by zero"
```

### 5.4 Dependency-triggered recompilation errors

When a binding changes (e.g. `(define x "hello")`), all output graphs that depend on `x` are recompiled. If recompilation fails, the error is *caused by* the define but *manifests in* the output expression.

The diagnostic must communicate the causal chain:

```
a1: compile error — "can't do maths with text — * needs numbers"
     caused by: x was redefined (previously a number, now text)
```

**Implementation**: When recompilation is triggered by dependency invalidation, the compiler records the invalidating symbol name and its new value type. The resulting diagnostic includes a `triggered_by` field:

```cpp
struct Diagnostic {
    // ... existing fields ...
    String triggered_by;  // symbol name that caused recompilation (empty for direct eval)
};
```

The frontend displays this as context: "this broke because *x* changed". This helps the user understand that the fix is to change `x`, not the output expression.

**Span attribution**: The diagnostic span points at the location where `x` is *used* in the output expression (the reference site), not where `x` was redefined. The user needs to see which part of their signal expression is affected.

### 5.5 REPL errors vs editor-pane errors

The spec so far focuses on output-assignment errors displayed as editor annotations. But the user also types arbitrary expressions in the REPL/console — things like `(+ 1 2)`, `(define foo 42)`, or exploratory `(sin 0.5)`.

REPL errors have different characteristics:

- **Synchronous**: the error is the direct response to the user's input, not a background recompilation
- **No output slot**: the expression isn't assigned to an output, so there's no persistent state
- **No LKG**: there's no fallback behaviour — the expression either works or doesn't
- **Inline in console**: errors appear in the console output stream, not as editor annotations

For REPL errors, `useq_last_diagnostics()` is the correct API — the frontend reads it immediately after `useq_eval()` and renders the diagnostics inline in the console. Source spans still apply (they reference the eval string) and can be used to underline the problematic part of the expression in the console output.

### 5.6 WASM boundary

Three new exports added to the WASM ABI (two described in §2.5, repeated here for the full picture):

```cpp
// Diagnostics from the most recent useq_eval() call.
// Returns JSON array of Diagnostic objects.
// Spans reference character offsets in the code string that was passed to useq_eval().
EMSCRIPTEN_KEEPALIVE
const char* useq_last_diagnostics();

// Per-output runtime diagnostics for all active outputs.
// Returns JSON object keyed by output name.
// { "a1": { "message": "...", "category": "arithmetic", "transient": true }, ... }
// Empty object if all outputs are healthy.
EMSCRIPTEN_KEEPALIVE
const char* useq_output_diagnostics();
```

```cpp
// All currently active diagnostics across all outputs (compile + runtime).
// See §2.5 for clearing policy.
EMSCRIPTEN_KEEPALIVE
const char* useq_active_diagnostics();
```

The frontend calls `useq_last_diagnostics()` immediately after each `useq_eval()` call for responsive inline feedback. It polls `useq_active_diagnostics()` once per animation frame to keep output health indicators in sync (this catches background recompilation errors and runtime errors that `useq_last_diagnostics()` would miss).

### 5.7 Frontend integration (CodeMirror)

The editor integration uses CodeMirror's diagnostic infrastructure:

```typescript
// In src/effects/editorEvaluation.ts (sketch)

interface UseqDiagnostic {
  severity: 'hint' | 'warning' | 'error';
  category: string;
  start: number;        // character offset
  end: number;
  message: string;
  suggestion?: string;
  example?: string;
}

// After eval, fetch diagnostics and push to CodeMirror
const diagnosticsJson = wasmExports.useq_last_diagnostics();
const diagnostics: UseqDiagnostic[] = JSON.parse(diagnosticsJson);

// Map to CodeMirror Diagnostic format
const cmDiagnostics = diagnostics.map(d => ({
  from: d.start,
  to: d.end,
  severity: d.severity,
  message: d.message,
  // Render suggestion and example in the tooltip
  renderMessage: () => buildDiagnosticTooltip(d)
}));

// Push to the editor via CodeMirror's lint/diagnostic extension
setDiagnostics(editorView, cmDiagnostics);
```

For runtime diagnostics (per-output health), the visualisation layer already knows which output is assigned to which editor pane. A subtle indicator (border colour change, gutter icon) shows when an output is in fallback mode.

### 5.8 Firmware path

On the RP2040 firmware (no browser, no editor), diagnostics are delivered differently:

- **LED colour**: amber flash on compile error, red flash on persistent runtime error
- **Serial JSON**: diagnostics are included in the JSON response to `eval` requests. The existing serial protocol (`uSEQ/src/utils/log.cpp`, `json_builder.h`) uses a `meta` field in eval responses for state changes. Diagnostics are added as a `diagnostics` array in the response:

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

The diagnostic data model is the same on both targets. Only the delivery mechanism differs. The serial protocol's existing `JsonBuilder` is used to construct the response — no external JSON library is needed.

---

## 6. Fuzzy Name Matching

### 6.1 Algorithm

When a symbol is not found, compute Levenshtein distance against all known names:

- **Distance 0**: exact match (shouldn't happen if we're in this path)
- **Distance 1**: almost certainly a typo — show as primary suggestion
- **Distance 2**: possible match — show as "did you mean?"
- **Distance ≥ 3**: too far, don't suggest

Additionally, check for **prefix matches** (user typed `si` and meant `sin`) and **case-insensitive matches** (`Sin` → `sin`).

### 6.2 Candidate pool

The candidate pool is the union of:

- All builtin function names
- All user-defined symbols in the current environment
- All temporal variable names (`beat`, `bar`, `phrase`, `section`, `t`, `time`, `beat-num`, `bar-num`)

### 6.3 Firmware constraints

Levenshtein on the full symbol table is O(n × m) where n is the number of candidates and m is the symbol length. With ~150 builtins and typical symbols under 20 characters, this is < 60,000 character comparisons — negligible even on the RP2040 at 250MHz. No need for approximate or probabilistic matching.

---

## 7. Interaction with LKG Fallback

The LKG system (§6.3 of `BYTECODE_VM_SPEC.md`) handles *continuity* — keeping the output alive when something breaks. The diagnostic system handles *communication* — telling the user what broke and how to fix it.

These are complementary, not competing:

| Situation | LKG behaviour | Diagnostic behaviour |
|---|---|---|
| Compile error | Keep current active graph | Show inline errors on the code that failed to compile |
| Runtime error (active) | Switch to LKG graph | Show output indicator: "a1 fell back to previous version" |
| Runtime error (LKG also fails) | Hold last valid sample or neutral default | Show output indicator: "a1 is stuck — both current and previous versions failed" |
| Compile warning | Use newly compiled graph normally | Show inline warnings (amber underline) |
| Recovery (new valid code) | New graph becomes active | Clear all diagnostics for that output |

### 7.1 "Using previous version" indicator

When LKG fallback is active, the user should see a clear but non-alarming indicator on the affected output. This is distinct from an error — it means "your new code had a problem, but the old version is still running."

Suggested UX: a small label or badge on the output's editor pane, something like:

> *using previous version — [view error]*

Clicking "view error" expands to show the compile diagnostic from the failed attempt.

---

## 8. Testing

### 8.1 Diagnostic content tests

Every error category in §4 should have at least one test that asserts:

1. The diagnostic is produced (not silently swallowed)
2. The `category` field is correct
3. The `span` points to the correct region of source text
4. The `message` is human-readable (no internal identifiers, no C++ type names)
5. The `suggestion` is valid ModuLisp code that compiles successfully

```yaml
# Example test cases for the golden suite
diagnostic_tests:
  - name: "sin with no args"
    expr: "(sin)"
    expect_diagnostic:
      severity: error
      category: arity
      span: {start: 0, end: 5}
      message_contains: "sin needs"
      suggestion_contains: "(sin beat)"

  - name: "unknown symbol with typo"
    expr: "(sni beat)"
    expect_diagnostic:
      severity: error
      category: undefined_name
      span: {start: 1, end: 4}
      message_contains: "sni"
      suggestion_contains: "sin"

  - name: "define in signal context"
    expr: "(define x 3)"
    signal_context: true
    expect_diagnostic:
      severity: error
      category: boundary
      message_contains: "define"

  - name: "floor on integer warns"
    expr: "(floor 3)"
    expect_diagnostic:
      severity: warning
      message_contains: "whole number"
```

### 8.2 Span accuracy tests

Dedicated tests that verify spans point to the *right* part of the source, not just *a* part:

```yaml
span_tests:
  - expr: "(+ (sin) 2)"
    #      0123456789
    expect_diagnostic:
      span: {start: 3, end: 8}   # points to (sin), not the whole expr

  - expr: "(let (x) (+ x 1))"
    #      0123456789...
    expect_diagnostic:
      span: {start: 5, end: 8}   # points to (x), the malformed binding

  - expr: "(+ 1 (flor beat))"
    #      01234567...
    expect_diagnostic:
      span: {start: 6, end: 10}  # points to flor, not the whole call
```

### 8.3 Multi-diagnostic tests

Verify the compiler reports multiple errors in one pass:

```yaml
  - name: "two errors in one expression"
    expr: "(+ (sni beat) (cso bar))"
    expect_diagnostics:
      - {category: undefined_name, message_contains: "sni"}
      - {category: undefined_name, message_contains: "cso"}
```

### 8.4 Suggestion validity tests

A meta-test: for every diagnostic that includes a `suggestion`, compile the suggestion itself and assert it succeeds. Bad suggestions are worse than no suggestions.

---

## 9. Implementation Phases

### Phase 1: Source spans in the parser
- Add `SourceSpan` to `Value`
- Track character position during parsing
- Populate spans for all node types
- Unit tests for span accuracy

### Phase 2: Structured diagnostics in the compiler
- Replace `m_error` string with `std::vector<Diagnostic>`
- Replace `fail()` calls with `report()` calls that include category, span, message, suggestion
- Continue past non-fatal errors (report-and-continue pattern)
- Revise `NumericVmCompileResult` to carry `diagnostics[]`
- Unit tests for each error category

### Phase 3: Human-readable messages
- Write message templates for every error path in the compiler
- Implement fuzzy name matching for undefined symbols
- Write suggestion text and examples for common errors
- Test that all suggestions compile successfully

### Phase 4: WASM ABI and frontend integration
- Add `useq_last_diagnostics()` export
- Add `useq_output_diagnostics()` export
- Frontend: parse diagnostic JSON after eval
- Frontend: push diagnostics to CodeMirror inline decorations
- Frontend: output health indicators for runtime errors

### Phase 5: Runtime diagnostics
- VM executor reports structured errors (category + message)
- Per-output diagnostic buffer with deduplication and rate limiting
- LKG fallback indicator in the editor
- Firmware: LED colour coding for error state

---

## 10. Appendix: Complete Error Message Table

Reference table of all compiler error paths and their user-facing messages. This table is the source of truth for message wording — implementations should match these strings exactly or improve upon them, but never use more technical language.

| Category | Condition | Message | Suggestion | Example |
|---|---|---|---|---|
| Syntax | Unmatched `(` | missing a closing parenthesis | Add `)` at the end | |
| Syntax | Extra `)` | extra closing parenthesis | Remove the last `)` | |
| Syntax | Unclosed string | this string was never closed | Add `"` at the end | |
| UndefinedName | Unknown symbol | I don't recognise "*name*" | Did you mean *closest*? | |
| Arity | Too few args | *fn* needs *n* values to work with | Try: *correct usage* | *working example* |
| Arity | Too many args | *fn* takes *n* values, but got *m* | Try: *correct usage* | |
| Type | Number expected | *fn* needs a number, not *actual type* | | |
| Type | List expected | *fn* needs a list of values | Try: `(seq [0 0.5 1] beat)` | |
| Boundary | `define` in signal | can't use define inside an output — define changes state, but outputs are pure signals | Move the define outside | `(define x 3)` then `(a1 ...)` |
| Boundary | `eval` in signal | can't use eval inside an output | Write the expression directly | |
| Boundary | side-effect in signal | can't use *form* inside an output — *form* is a one-time action | Use *form* at the top level | |
| Arithmetic | Division by zero | dividing by zero — the result is undefined | | |
| Arithmetic | NaN result | this produced an undefined number — check for division by zero or sqrt of a negative | | |
| Arithmetic | Infinite result | this produced infinity — check for division by a very small number | | |
| Runtime | Loop budget | this loop ran too long and was stopped | Check that the condition eventually becomes false | |
| Runtime | Call depth | too many nested function calls | Check for accidental recursion | |
| Runtime | Intrinsic error | a built-in function returned an error | | |
| Overflow | Clamp inverted | clamp min (*lo*) is greater than max (*hi*) — the values might be swapped | | |
| Warning | No-op operation | *operation* has no effect here | You can simplify this to just *simplified* | |
| Warning | Dead branch | this condition is always *true/false* — one branch will never run | | |
| Warning | Constant signal | this output is always *value* — it doesn't change with time | If you want a changing signal, try using `beat` or `bar` | |
