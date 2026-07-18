---
description: "Audit all normative specs in docs/specs/ against the codebase implementation, producing a queryable JSON at .local/spec-audit.json"
---

# Audit Spec Completeness

Systematically compare every normative spec in `docs/specs/` against the actual codebase. For each requirement in each spec, determine whether it's implemented, partially implemented, or not started — then write a single queryable JSON file at `.local/spec-audit.json`.

## Arguments

Parse `$ARGUMENTS` for these flags (combine freely):
- **`fresh`** — delete any existing `.local/spec-audit.json` and re-audit every spec from scratch
- **`<spec-name>`** — re-audit only the named spec (e.g. `state`, `live-edit`, `wire-protocol`), updating its entry in the existing audit file
- **`deep`** — each subagent does exhaustive file reads rather than targeted greps (slower but more thorough)
- **No flags (default)** — re-audit specs whose `audit_commit` doesn't match HEAD, leave up-to-date entries untouched

## Methodology

The full methodology is defined in `.local/specs/spec-completeness-auditing.md`. Key points:

### Evidence hierarchy (strongest to weakest)

1. **Code + tests found** → `implemented`, confidence: `high`
2. **Code found, no tests** → `implemented`, confidence: `medium`
3. **Partial code (stubs, TODOs)** → `partial`, confidence varies
4. **No code, spec only** → `not-started`, confidence: `high`
5. **Uncertain** → `partial`, confidence: `low`

### Confidence-weighted scoring

```
weight = { high: 1.0, medium: 0.7, low: 0.4 }
confidence_weighted_score = sum(weight * score) / sum(weight)
```

### Per-requirement fields

Each requirement gets: `id` (prefix + sequence), `section`, `description`, `status`, `critical`, `score` (1–10), `confidence`, `status_reasoning`, `evidence_files`, `test_files`, `relevant_commits`, `beads_issues`.

## Execution

### Step 1: Determine scope

- If `fresh`: audit all 21 specs
- If a spec name is given: audit only that spec
- Default: read `.local/spec-audit.json`, compare `audit_commit` to `git rev-parse --short HEAD`. Re-audit specs where commits differ (check `git log --oneline <old_commit>..HEAD -- <spec-related paths>`)

### Step 2: Spawn subagents

Spawn one subagent per spec in scope, all in parallel. Each subagent:

1. **Read the spec** — extract all numbered normative requirements (not notes or rationale)
2. **Enumerate requirements** — assign IDs using prefix convention (first letters uppercased + sequence: `WP-1`, `STATE-12`, `DIAG-5`)
3. **Gather evidence** — `grep`/`find` for implementation, scan `test/`, `git log --oneline -10 -- <paths>`, `ergo list --json` (and search ergo task bodies via `ergo show <id>`)
4. **Assign status and confidence** — apply evidence hierarchy, write `status_reasoning`
5. **Compute rollups** — `overall_score`, `confidence_weighted_score`, `untested_requirements`, `critical_gaps`
6. **Emit JSON** — return per-spec object matching the schema

Key search paths per spec:
- `uSEQ/src/signal_engine/` — compiler, executor, cold eval, node pool, diagnostics, state registry
- `uSEQ/src/firmware/` — firmware tick loop, serial protocol, hardware I/O, flash storage
- `wasm/wasm_wrapper.cpp` — WASM bindings and ABI
- `test/` — test coverage
- `docs/specs/` — the specs themselves

### Step 3: Assemble `.local/spec-audit.json`

Top-level structure:
```json
{
  "audit_date": "ISO date",
  "audit_commit": "short SHA",
  "audit_branch": "branch name",
  "auditor": "claude-opus-4-7",
  "methodology": "per-spec subagent reads spec, extracts requirements, greps codebase...",
  "specs": [ ... ]
}
```

### Step 4: Print summary table

Show every spec with its score, confidence-weighted score, critical gaps, and untested count. Highlight specs below 5/10 or with critical gaps.

### Step 5: Suggest next actions

Based on the results, suggest:
- Which critical gaps to prioritise
- Which specs need the most work
- Which requirements have low confidence (may need manual verification)

## Subagent constraints

- **Read-only.** Subagents must not modify any source files.
- **Specific evidence.** Every requirement must cite file paths and line numbers. Vague references like "in the firmware" are not acceptable.
- **Informational sections are not requirements.** Spec sections describing rationale, background, or alternatives should be noted but not create requirement entries.
- **Stable IDs.** When re-auditing, reuse existing IDs where the requirement text hasn't changed.
- **Keep prompts focused.** Subagents that read entire files stall. Tell them to `grep` and read specific line ranges.

## Known pitfalls

- **Rate limiting.** Spawning 21 agents simultaneously can hit API rate limits. If agents fail with 429 errors, re-spawn failed ones individually with tighter prompts.
- **Agent stalls.** Agents that read too many large files can stall (600s timeout). Use focused grep-based prompts rather than "read everything" prompts.
- **Score scale inconsistency.** Some agents return 0-100 scores instead of 0-10. Normalize when assembling.
- **Status enum drift.** Some agents use "unimplemented" or "not-implemented" instead of "not-started", or "partially_implemented" instead of "partial". Normalize when assembling.

## Example queries

```bash
# Which specs are in good shape?
jq '.specs[] | select(.overall_score >= 8) | .name' .local/spec-audit.json

# How many critical things haven't been implemented?
jq '[.specs[].requirements[] | select(.status != "implemented" and .critical)] | length' .local/spec-audit.json

# Show what remains in a specific spec
jq '.specs[] | select(.name == "state") | .requirements[] | select(.status != "implemented") | .description' .local/spec-audit.json

# Which requirements have low confidence?
jq '.specs[].requirements[] | select(.confidence == "low") | {id, description, status}' .local/spec-audit.json

# List all untested implemented requirements
jq '.specs[].requirements[] | select(.status == "implemented" and (.test_files | length == 0)) | {id, description}' .local/spec-audit.json
```
