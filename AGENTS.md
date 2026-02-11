# Agent Instructions

## Issue Tracking

This project uses **bd (beads)** for issue tracking.
Run `bd prime` for workflow context and `bd ready` for unblocked work.

Quick reference:
- `bd ready` - find available work
- `bd show <id>` - inspect issue details
- `bd update <id> --status in_progress` - claim work
- `bd close <id>` - complete work
- `bd sync` - sync issue JSONL with git

## Skills

A skill is a local instruction set in a `SKILL.md` file.
Use a skill when the user names it (`$SkillName` or plain text), or when the task clearly matches the skill description.

### Available skills

- `dev-browser`: Browser automation with persistent page state. Use for navigation, form filling, screenshots, web extraction, and browser workflow testing.  
  Path: `/home/w1n5t0n/.codex/skills/dev-browser/SKILL.md`
- `skill-creator`: Instructions for creating or updating skills.  
  Path: `/home/w1n5t0n/.codex/skills/.system/skill-creator/SKILL.md`
- `skill-installer`: Install curated skills or skills from GitHub repo paths.  
  Path: `/home/w1n5t0n/.codex/skills/.system/skill-installer/SKILL.md`

### Skill usage rules

- Discovery: Start from the available-skill list above; do not assume unavailable skills.
- Triggering: If a named skill is present or the request clearly matches, use it for that turn.
- Missing skill: If requested skill cannot be read, state that briefly and use the best fallback.
- Progressive disclosure:
  1. Open the selected `SKILL.md` and read only what is needed.
  2. Resolve skill-relative paths against the skill directory first.
  3. Load only required reference files; avoid bulk-loading.
  4. Prefer existing skill scripts/templates/assets over re-implementing.
- Coordination: Use the minimal set of skills needed; if multiple skills are used, state order briefly.
- Context hygiene: Keep context tight and avoid deep reference chasing unless blocked.
- Safety fallback: If a skill is unclear/broken, note the issue and continue with the next-best approach.
