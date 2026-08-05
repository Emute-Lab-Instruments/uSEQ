#!/usr/bin/env python3
"""Regression contract for derived compiler limits in the WASM manifest."""

from __future__ import annotations

import importlib.util
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = ROOT / "scripts/wasm_manifest.py"
SPEC = importlib.util.spec_from_file_location("wasm_manifest", MODULE_PATH)
assert SPEC and SPEC.loader
manifest = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(manifest)


def main() -> None:
    profile = manifest.load_profile()
    assert profile["growable_arraybuffers"] is False
    assert "-sGROWABLE_ARRAYBUFFERS=0" in profile["compile_flags"]
    limits = manifest.cpp_limits()
    assert limits["max_external_roots"] == 512
    assert limits["max_synth_controls"] == 512
    assert "max_synth_control_roots" not in limits


if __name__ == "__main__":
    main()
