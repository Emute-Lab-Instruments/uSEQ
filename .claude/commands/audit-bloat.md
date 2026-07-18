---
description: "Spawn subagents to audit the firmware codebase for AI-introduced bloat, unnecessary complexity, and over-engineering"
---

# Audit Bloat

Systematically scan this codebase for complexity that shouldn't exist — code that was likely introduced by AI agents interpreting vague feedback too literally, over-engineering simple requirements, or inventing structures that nobody asked for.

## Arguments

Parse `$ARGUMENTS` for these flags (combine freely):
- **`deep`** — skip the structural scan, do deep code reads everywhere
- **`shallow`** — structural signals only, no deep code reads
- **`thorough`** — spawn 6-8 focused agents instead of the default 3-4
- **No flags (default)** — tiered: fast structural scan to find suspect areas, then deep reads on the most suspicious modules

## What to look for

Every subagent should hunt for these anti-patterns. The common thread is **unnecessary complexity introduced by agents who took instructions too literally or invented requirements that were never stated**.

### Anti-pattern checklist

1. **Duplicate code paths for the same concern** — two implementations of the same thing, often with a config toggle or `#ifdef` to switch between them. Usually born from a user saying "X doesn't work right" and the agent creating a second path instead of fixing the first. In embedded C++, watch for parallel implementations guarded by preprocessor conditionals that do roughly the same thing.

2. **Over-abstracted wrappers** — classes or functions that exist only to delegate to another class or function, adding an indirection layer with no behavioural value. In embedded code, this is especially wasteful — every vtable, extra function call, and indirection has a real cost in flash and RAM.

3. **Unnecessary configuration surfaces** — config options, parameters, `#define` toggles, or template parameters that control behaviour that should just be one way. Especially suspicious: boolean flags that switch between two implementations, or numeric parameters with only one sensible value.

4. **Phantom features** — code that implements functionality not described in any spec under `docs/specs/` and not exercised by any test under `test/`. These are likely agent inventions from misunderstood requests. (Note: not all unspecced code is wrong — hardware drivers and low-level utilities don't need specs. But interpreter semantics and signal-engine features should trace back to something.)

5. **Taxonomic over-engineering** — classification systems, enum hierarchies, or type unions that subdivide a concept far more finely than any consumer actually distinguishes. Example: a 5-level error severity system where callers only ever check "error or not." Watch for `DiagnosticCategory`, `DiagnosticSeverity`, or similar enums with members nobody discriminates on.

6. **Dead parameters and unused branches** — function parameters that are always passed the same value, switch/if branches that are never taken, type union members that are never constructed. Signs of an agent building "flexible" infrastructure around a single concrete need. In C++, also check for unused template parameters and `#ifdef` branches for configurations that don't exist.

7. **Naming that reveals confusion** — names that are vague ("DataProcessor", "handleStuff"), contradictory (a "validator" that also transforms), or that contain hedging words ("maybeParse", "tryToProcess") often indicate the agent wasn't sure what the code should do.

8. **Premature abstraction over hardware variants** — abstraction layers meant to support hardware configurations that may never exist. Check whether `configure.h` / `pinmap.h` variant switches are actually exercised by PlatformIO environments in `platformio.ini`, or are speculative infrastructure.

### Calibration examples

These are **fabricated examples** — they don't exist in this codebase. They illustrate the *flavour* of problems you're looking for. Use your judgment for what qualifies in the actual code.

**Example 1: The branching fix**
> A user says "the output mapping is wrong for one pin." Instead of fixing the pin table, the agent creates `OutputMapper` and `InvertedOutputMapper` classes with a `OUTPUT_MAPPING_MODE` define that switches between them. The fix should have been one line in `pinmap.h`.

**Example 2: The taxonomy explosion**
> A user says "we should show better error messages." The agent creates a `DiagnosticSeverity` enum with 7 levels, a `DiagnosticCategory` type with 12 members, and separate handler classes per severity. The codebase only ever distinguishes "error" from "not error."

**Example 3: The premature abstraction**
> A user says "can we run the interpreter in WASM too?" The agent creates a `PlatformAbstractionLayer` with `ArduinoPlatform`, `DesktopPlatform`, and `WasmPlatform` implementations, each with virtual dispatch for I/O — when all that was needed was a few `#ifdef` guards on the hardware-specific calls.

These are the kinds of things you're hunting. The common thread: **an agent generated more structure than the problem required**.

## Agent grouping

### Default mode (3-4 agents)

Spawn these in parallel:

| Agent | Scope | Focus |
|-------|-------|-------|
| **interpreter** | `uSEQ/src/modulisp/` including `lisp/` | Interpreter bloat, unnecessary abstraction in parser/evaluator/environment, over-engineered value types or dispatch |
| **signal-engine** | `uSEQ/src/signal_engine/` | Node pool complexity, compilation overhead, unnecessary abstraction in the bytecode VM |
| **hardware-io** | `uSEQ/src/uSEQ/`, `uSEQ/src/io/`, `uSEQ/src/ports/`, `uSEQ/src/firmware/`, `uSEQ/src/dsp/` | Hardware abstraction bloat, unnecessary I/O layering, dead variant code, DSP over-engineering |
| **cross-cutting** | Entire `uSEQ/src/`, `wasm/`, `test/` | Duplicate implementations across modules, concepts named differently in different places, data flowing through unnecessary intermediate representations, WASM wrapper bloat |

### Thorough mode (6-8 agents)

Split the groups further — one agent per major directory. Same anti-pattern checklist.

## Execution

### Step 1: Structural scan (skip if `deep` flag is set)

Each agent does a fast pass:
- Count files, exports, types, classes per module
- Look at naming patterns — flag any that match the anti-pattern signatures
- Check for `#ifdef` / `#define` configuration toggles and their actual usage
- Cross-reference features against `docs/specs/` for phantom features
- Check whether test coverage in `test/` actually exercises the code
- Identify the 2-3 most suspicious areas in their scope

### Step 2: Deep reads (skip if `shallow` flag is set)

In default (tiered) mode, agents deep-read only the areas flagged in Step 1.
In `deep` mode, agents read everything in their scope.

For each suspicious area:
- Read the actual implementation
- Trace the data flow and call sites
- Determine: is this complexity *load-bearing* (actually needed) or *accidental* (agent over-engineering)?
- If accidental, describe what a simpler version would look like
- For embedded code, estimate flash/RAM cost of the unnecessary abstraction if possible

### Step 3: Lightweight spec check

For anything that looks like a domain feature (not infrastructure/utilities), check:
- Does it appear in any spec under `docs/specs/`?
- Is there a test for it under `test/`?
- If neither: flag as a potential phantom feature. Don't automatically condemn it — just surface it for review.

### Step 4: Report

Create `reports/audit-bloat/` if it doesn't exist. Write a timestamped report to `reports/audit-bloat/YYYY-MM-DD-HHmmss.md` with this structure:

```markdown
# Audit Bloat Report — [date]

**Mode**: [default/deep/shallow] | **Agents**: [count] | **Scope**: full sweep

## Summary
[2-3 sentence overview: how much bloat was found, which areas are worst]

## Findings

### [Severity: high/medium/low] — [Short title]
- **Location**: [file path(s)]
- **Anti-pattern**: [which pattern from the checklist]
- **What exists**: [brief description of the current code]
- **What it should be**: [what a parsimonious version looks like]
- **Confidence**: [high/medium/low that this is actually unnecessary]
- **Effort to fix**: [small/medium/large]
- **Embedded cost**: [estimated flash/RAM impact if applicable]

[...repeat for each finding, ordered by severity then confidence...]

## Phantom Features
[Things found in code with no spec or test backing, if any]

## Clean Areas
[Modules that looked well-structured — worth noting so we know what "good" looks like here]
```

### Step 5: Interactive triage (optional)

After writing the report, ask the user: **"Want to triage these findings now?"**

If yes, present each finding (high-severity first) and ask:
- **File as ergo task** — create an `ergo` task with appropriate priority and type
- **Dismiss** — this complexity is actually justified, skip it
- **Defer** — might be real but not worth addressing now

If no, just point them at the report file and move on.

## Important notes

- **Don't fix anything.** This command is diagnostic only. Surface problems, don't solve them.
- **Err on the side of reporting.** A false positive the user can dismiss is better than a missed real problem.
- **Be specific.** "This file is complex" is useless. "Lines 45-120 implement a FooBarStrategy pattern with 3 concrete strategies, but only `DefaultFoo` is ever instantiated" is useful.
- **Compare to simplest-possible implementation.** The question isn't "is this code bad?" — it's "is there a version of this code that does the same thing with less structure?"
- **Read CLAUDE.md first** for architectural context, so you don't flag intentional design patterns as bloat.
- **Respect real-time constraints.** Some complexity in `dsp/` and `signal_engine/` exists for performance reasons (avoiding allocations, cache locality, avoiding virtual dispatch on the hot path). Don't flag performance-motivated patterns unless they're clearly not on any hot path.
- **Distinguish platform abstraction from bloat.** This codebase legitimately targets Arduino, desktop (Meson), and WASM. Some `#ifdef` layering is necessary. Flag it only when the abstraction is more elaborate than the platform differences warrant.
