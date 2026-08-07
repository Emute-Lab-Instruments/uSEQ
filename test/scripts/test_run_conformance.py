#!/usr/bin/env python3
"""Regression contract for conformance probe resolution.

A conformance result only means something if the probe it ran was built from
the source under test.  `run_conformance.py` used to search a list of build
directories and returned the first that existed, so a stale sibling tree
(`build-probe/`) shadowed the canonical `build/` and reported conformance for
whatever source that tree happened to be built from.  Resolution must stay
single-valued: the canonical path, or an explicit --probe, or an error.
"""

from __future__ import annotations

import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = ROOT / "scripts/run_conformance.py"
SPEC = importlib.util.spec_from_file_location("run_conformance", MODULE_PATH)
assert SPEC and SPEC.loader
conformance = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(conformance)


def main() -> None:
    # Exactly one canonical native probe path, under the canonical build dir.
    assert not hasattr(conformance, "DEFAULT_PROBE_CANDIDATES"), (
        "probe resolution must not search a candidate list; a shadowing build "
        "directory silently reports conformance for stale source"
    )
    default = conformance.DEFAULT_PROBE
    assert default == ROOT / "build/test/signal_engine_probe", default

    # An explicit probe that does not exist must fail loudly rather than
    # falling back to any other tree.
    try:
        conformance.resolve_probe("does/not/exist/signal_engine_probe", "native")
    except SystemExit as exc:
        assert "Missing probe executable" in str(exc), exc
    else:
        raise AssertionError("missing explicit probe must raise SystemExit")


if __name__ == "__main__":
    main()
