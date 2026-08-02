#!/usr/bin/env python3
"""Report and gate the linked RP2040 memory envelope from an exact ELF.

The generic GNU ``size`` summary classifies the linker-reserved ``.heap``
section as text, so it is not a reliable flash measurement for this target.
This script reads section load addresses and linker symbols directly instead.
It deliberately reports static/linker capacity only; runtime heap and stack
high-water observations belong to the instrumented serial runner.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ELF = PROJECT_ROOT / ".pio" / "build" / "musicthing" / "firmware.elf"
DEFAULT_BUDGET = PROJECT_ROOT / "scripts" / "rp2040_budget.json"

SECTION_RE = re.compile(
    r"^\s*\d+\s+(\S+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+"
    r"([0-9a-fA-F]+)\s+([0-9a-fA-F]+)"
)


def run(argv: list[str], cwd: Path | None = None) -> str:
    return subprocess.check_output(argv, cwd=cwd, text=True)


def resolve_tool(name: str, prefix: str | None) -> str:
    if prefix:
        candidate = Path(f"{prefix}{name}").expanduser()
        if candidate.is_file():
            return str(candidate)
        raise SystemExit(f"missing tool: {candidate}")

    found = shutil.which(f"arm-none-eabi-{name}")
    if found:
        return found

    candidate = (
        Path.home()
        / ".platformio/packages/toolchain-rp2040-earlephilhower/bin"
        / f"arm-none-eabi-{name}"
    )
    if candidate.is_file():
        return str(candidate)
    raise SystemExit(
        f"arm-none-eabi-{name} not found; pass --tool-prefix PATH/arm-none-eabi-"
    )


def parse_sections(objdump: str, elf: Path) -> dict[str, dict[str, object]]:
    lines = run([objdump, "-h", str(elf)]).splitlines()
    sections: dict[str, dict[str, object]] = {}
    for index, line in enumerate(lines):
        match = SECTION_RE.match(line)
        if not match:
            continue
        name, size, vma, lma, file_offset = match.groups()
        flags = []
        if index + 1 < len(lines):
            flags = [part.strip() for part in lines[index + 1].split(",")]
        sections[name] = {
            "size": int(size, 16),
            "vma": int(vma, 16),
            "lma": int(lma, 16),
            "file_offset": int(file_offset, 16),
            "flags": flags,
        }
    return sections


def parse_symbols(nm: str, elf: Path) -> tuple[dict[str, int], list[dict[str, object]]]:
    output = run(
        [nm, "-n", "-S", "--format=posix", "--radix=d", str(elf)]
    )
    values: dict[str, int] = {}
    objects: list[dict[str, object]] = []
    for line in output.splitlines():
        parts = line.split()
        if len(parts) < 3 or not parts[2].isdigit():
            continue
        name, kind, address = parts[:3]
        values.setdefault(name, int(address))
        if len(parts) >= 4 and parts[3].isdigit() and kind in "bBdD":
            size = int(parts[3])
            if size:
                objects.append(
                    {"name": name, "kind": kind, "address": int(address), "size": size}
                )
    objects.sort(key=lambda item: int(item["size"]), reverse=True)
    return values, objects


def require_symbol(symbols: dict[str, int], name: str) -> int:
    try:
        return symbols[name]
    except KeyError as error:
        raise SystemExit(f"ELF is missing required linker symbol {name}") from error


def git_identity(repo: Path) -> dict[str, object]:
    try:
        revision = run(["git", "rev-parse", "HEAD"], cwd=repo).strip()
        dirty = subprocess.run(
            ["git", "diff", "--quiet"], cwd=repo, check=False
        ).returncode != 0
        return {"revision": revision, "dirty": dirty}
    except (OSError, subprocess.CalledProcessError):
        return {"revision": "unknown", "dirty": None}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def build_report(
    elf: Path,
    objdump: str,
    nm: str,
    budget: dict[str, int] | None,
    source_revision: str | None = None,
    label: str | None = None,
) -> dict[str, object]:
    sections = parse_sections(objdump, elf)
    symbols, objects = parse_symbols(nm, elf)

    flash_start = require_symbol(symbols, "__flash_binary_start")
    filesystem_start = require_symbol(symbols, "_FS_start")
    filesystem_end = require_symbol(symbols, "_FS_end")
    static_end = require_symbol(symbols, "__end__")
    heap_limit = require_symbol(symbols, "__HeapLimit")

    ram_candidates = [
        int(section["vma"])
        for section in sections.values()
        if 0x20000000 <= int(section["vma"]) < heap_limit
    ]
    if not ram_candidates:
        raise SystemExit("could not derive the main-RAM origin from ELF sections")
    ram_origin = min(ram_candidates)

    loaded_flash_sections = [
        section
        for section in sections.values()
        if "LOAD" in section["flags"]
        and flash_start <= int(section["lma"]) < filesystem_start
        and int(section["size"]) > 0
    ]
    flash_image_end = max(
        int(section["lma"]) + int(section["size"])
        for section in loaded_flash_sections
    )
    flash_image_bytes = flash_image_end - flash_start
    reserved_sections = {".ota", ".partition"}
    flash_payload_bytes = sum(
        int(section["size"])
        for name, section in sections.items()
        if section in loaded_flash_sections and name not in reserved_sections
    )

    data_bytes = int(sections.get(".data", {}).get("size", 0))
    bss_bytes = int(sections.get(".bss", {}).get("size", 0))
    vector_bytes = int(sections.get(".ram_vector_table", {}).get("size", 0))
    linker_heap_bytes = heap_limit - static_end
    main_ram_capacity = heap_limit - ram_origin

    stack_one_bottom = require_symbol(symbols, "__StackOneBottom")
    stack_one_top = require_symbol(symbols, "__StackOneTop")
    stack_bottom = require_symbol(symbols, "__StackBottom")
    stack_top = require_symbol(symbols, "__StackTop")

    objdump_version = run([objdump, "--version"]).splitlines()[0]
    source = git_identity(PROJECT_ROOT)
    if source_revision:
        source = {"revision": source_revision, "dirty": None}
    report: dict[str, object] = {
        "schema_version": 1,
        "target": "rp2040-musicthing",
        "source": source,
        "elf": {
            "path": str(elf.resolve()),
            "sha256": sha256(elf),
            "toolchain": objdump_version,
        },
        "flash": {
            "partition_bytes": filesystem_start - flash_start,
            "image_bytes": flash_image_bytes,
            "payload_bytes": flash_payload_bytes,
            "headroom_bytes": filesystem_start - flash_image_end,
            "filesystem_bytes": filesystem_end - filesystem_start,
        },
        "main_ram": {
            "capacity_bytes": main_ram_capacity,
            "platformio_static_bytes": data_bytes + bss_bytes,
            "linked_static_span_bytes": static_end - ram_origin,
            "linker_heap_bytes": linker_heap_bytes,
            "data_bytes": data_bytes,
            "bss_bytes": bss_bytes,
            "ram_vector_table_bytes": vector_bytes,
        },
        "stacks": {
            "core0_reserved_bytes": stack_top - stack_bottom,
            "core1_reserved_bytes": stack_one_top - stack_one_bottom,
        },
        "largest_static_objects": objects[:12],
    }
    if label:
        report["label"] = label

    checks: list[dict[str, object]] = []
    if budget:
        if "min_linker_heap_bytes" in budget:
            actual = linker_heap_bytes
            expected = int(budget["min_linker_heap_bytes"])
            checks.append(
                {
                    "metric": "main_ram.linker_heap_bytes",
                    "operator": ">=",
                    "limit": expected,
                    "actual": actual,
                    "pass": actual >= expected,
                }
            )
        if "max_flash_image_bytes" in budget:
            actual = flash_image_bytes
            expected = int(budget["max_flash_image_bytes"])
            checks.append(
                {
                    "metric": "flash.image_bytes",
                    "operator": "<=",
                    "limit": expected,
                    "actual": actual,
                    "pass": actual <= expected,
                }
            )
        report["budget"] = budget
    report["checks"] = checks
    report["pass"] = all(bool(check["pass"]) for check in checks)
    return report


def comparison(current: dict[str, object], baseline: dict[str, object]) -> dict[str, int]:
    current_flash = current["flash"]
    baseline_flash = baseline["flash"]
    current_ram = current["main_ram"]
    baseline_ram = baseline["main_ram"]
    assert isinstance(current_flash, dict) and isinstance(baseline_flash, dict)
    assert isinstance(current_ram, dict) and isinstance(baseline_ram, dict)
    return {
        "flash_image_bytes": int(current_flash["image_bytes"])
        - int(baseline_flash["image_bytes"]),
        "platformio_static_bytes": int(current_ram["platformio_static_bytes"])
        - int(baseline_ram["platformio_static_bytes"]),
        "linker_heap_bytes": int(current_ram["linker_heap_bytes"])
        - int(baseline_ram["linker_heap_bytes"]),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--elf", type=Path, default=DEFAULT_ELF)
    budget_group = parser.add_mutually_exclusive_group()
    budget_group.add_argument("--budget", type=Path, default=DEFAULT_BUDGET)
    budget_group.add_argument("--no-budget", action="store_true")
    parser.add_argument("--compare", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--tool-prefix")
    parser.add_argument("--source-revision")
    parser.add_argument("--label")
    args = parser.parse_args()

    elf = args.elf.resolve()
    if not elf.is_file():
        parser.error(f"ELF not found: {elf}")
    budget = None
    if not args.no_budget:
        if not args.budget.is_file():
            parser.error(f"budget not found: {args.budget}")
        budget = json.loads(args.budget.read_text())

    report = build_report(
        elf,
        resolve_tool("objdump", args.tool_prefix),
        resolve_tool("nm", args.tool_prefix),
        budget,
        args.source_revision,
        args.label,
    )
    if args.compare:
        baseline = json.loads(args.compare.read_text())
        report["comparison"] = comparison(report, baseline)

    encoded = json.dumps(report, indent=2, sort_keys=True) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded)
        print(f"wrote {args.output}", file=sys.stderr)
    else:
        print(encoded, end="")

    ram = report["main_ram"]
    flash = report["flash"]
    assert isinstance(ram, dict) and isinstance(flash, dict)
    print(
        "RP2040 memory: "
        f"static={ram['platformio_static_bytes']}/{ram['capacity_bytes']} B, "
        f"linked-heap={ram['linker_heap_bytes']} B, "
        f"flash-image={flash['image_bytes']}/{flash['partition_bytes']} B, "
        f"gate={'PASS' if report['pass'] else 'FAIL'}",
        file=sys.stderr,
    )
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
