#!/usr/bin/env python3
"""Contract: repo paths cited by the orientation docs must exist.

Scope is deliberately narrow. Only `MAP.md`, `CLAUDE.md`, and `README.md` are
checked, because only those three claim to be a current inventory of the tree.
`docs/specs/` is excluded on purpose: a spec may name a path that is not built
yet, and treating that as inventory drift produces noise instead of signal.

Two further noise controls:

  * fenced code blocks are skipped -- CLAUDE.md draws a tree diagram whose
    entries are relative to `uSEQ/src/`, not to the repo root;
  * a backticked token is resolved only when its first segment is a real
    top-level entry of this repo. That skips prose and non-paths that merely
    contain a slash (`osc/sine` is a NodeDef name, `log.cpp/h` is shorthand,
    `.pio/build/{env}/…` is a build artifact path) without needing to enumerate
    them.

Observed 2026-08-07: CLAUDE.md claimed test binaries land in `test_build/` and
MAP.md indexed a `test/hardware/` directory. Neither existed.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DOCS = ("MAP.md", "CLAUDE.md", "README.md")

BACKTICKED = re.compile(r"`([^`\n]+)`")
FENCE = re.compile(r"^\s*```")
# foo.{h,cpp} -> foo.h, foo.cpp
BRACES = re.compile(r"^(.*)\{([^}]*)\}(.*)$")


def top_level_entries() -> set[str]:
    return {p.name for p in ROOT.iterdir()}


def expand(token: str) -> list[str]:
    """Expand `foo.{h,cpp}` alternation; drop `{env}`-style placeholders.

    A brace group with no comma is a fill-in-the-blank placeholder, not a set of
    real alternatives. Expanding it would invent a literal path like
    `.pio/build/env/firmware.elf` that is never supposed to exist.
    """
    match = BRACES.match(token)
    if not match:
        return [token]
    head, options, tail = match.groups()
    if "," not in options:
        return []
    return [f"{head}{opt.strip()}{tail}" for opt in options.split(",") if opt.strip()]


def candidate_paths(text: str, tops: set[str]) -> list[str]:
    found: list[str] = []
    in_fence = False
    for line in text.splitlines():
        if FENCE.match(line):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        for raw in BACKTICKED.findall(line):
            token = raw.strip()
            if "/" not in token or token.startswith(("http://", "https://")):
                continue
            # A backticked run of words is a command line (`script.py verify`),
            # never a path.
            if any(ch.isspace() for ch in token):
                continue
            for expanded in expand(token):
                # A leading slash in these docs means repo-root, not filesystem
                # root.
                normalised = expanded.lstrip("/")
                if not normalised:
                    continue
                if normalised.split("/", 1)[0] not in tops:
                    continue
                found.append(normalised)
    return found


def main() -> int:
    tops = top_level_entries()
    failures: list[str] = []
    checked = 0

    for name in DOCS:
        doc = ROOT / name
        if not doc.exists():
            failures.append(f"{name}: orientation doc is missing")
            continue
        for path in candidate_paths(doc.read_text(encoding="utf-8"), tops):
            checked += 1
            if not (ROOT / path).exists():
                failures.append(f"{name}: cites `{path}`, which does not exist")

    if failures:
        for line in sorted(set(failures)):
            print(f"error: {line}")
        return 1

    print(f"orientation docs: {checked} cited repo path(s) all resolve")
    return 0


if __name__ == "__main__":
    sys.exit(main())
