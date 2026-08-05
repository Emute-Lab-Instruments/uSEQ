#!/usr/bin/env python3
"""Fail if an RP2040 firmware image contains the host/WASM synth engine."""

from __future__ import annotations

import argparse
import re
import shutil
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
FORBIDDEN_UNITS = ("synth_graph.cpp", "synth_registry.cpp")
FORBIDDEN_SYMBOL = re.compile(r"synth", re.IGNORECASE)


def resolve_nm() -> str:
    found = shutil.which("arm-none-eabi-nm")
    if found:
        return found
    candidate = (
        Path.home()
        / ".platformio/packages/toolchain-rp2040-earlephilhower/bin"
        / "arm-none-eabi-nm"
    )
    if candidate.is_file():
        return str(candidate)
    raise SystemExit("arm-none-eabi-nm is unavailable")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--elf",
        type=Path,
        default=ROOT / ".pio/build/musicthing/firmware.elf",
    )
    args = parser.parse_args()
    elf = args.elf.resolve()
    if not elf.is_file():
        parser.error(f"ELF not found: {elf}")

    platformio = (ROOT / "platformio.ini").read_text()
    missing_exclusions = [
        unit
        for unit in FORBIDDEN_UNITS
        if f"-<src/signal_engine/{unit}>" not in platformio
    ]
    if missing_exclusions:
        raise SystemExit(
            "PlatformIO does not exclude synth units: "
            + ", ".join(missing_exclusions)
        )

    build_dir = elf.parent
    compiled_units = [
        path
        for path in build_dir.rglob("*")
        if path.is_file() and any(unit in path.name for unit in FORBIDDEN_UNITS)
    ]
    if compiled_units:
        paths = ", ".join(str(path.relative_to(ROOT)) for path in compiled_units)
        raise SystemExit(f"firmware build contains synth objects: {paths}")

    symbols = subprocess.check_output(
        [resolve_nm(), "-C", "--defined-only", str(elf)],
        text=True,
        errors="replace",
    )
    forbidden = [line for line in symbols.splitlines() if FORBIDDEN_SYMBOL.search(line)]
    if forbidden:
        preview = "\n".join(forbidden[:20])
        raise SystemExit(f"firmware ELF contains synth symbols:\n{preview}")

    if b"synth" in elf.read_bytes().lower():
        raise SystemExit("firmware ELF contains synth text/data")

    print(f"firmware synth boundary: PASS ({elf})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
