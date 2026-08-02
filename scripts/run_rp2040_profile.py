#!/usr/bin/env python3
"""Run the local acceptance surface for the constrained RP2040 profile.

This lane combines exact target linking with native semantic, capacity, and
endurance checks. Native timings are comparative host proxies. Simulator and
physical-device measurements are separate evidence profiles.
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[1]


def git_output(*args: str) -> str:
    return subprocess.check_output(
        ["git", *args], cwd=ROOT, text=True
    ).strip()


def run_logged(
    name: str,
    argv: list[str],
    output_dir: Path,
    *,
    env: dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    print(f"[rp2040-profile] {name}", file=sys.stderr, flush=True)
    result = subprocess.run(
        argv,
        cwd=ROOT,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    log_path = output_dir / f"{name}.log"
    log_path.write_text(result.stdout)
    if result.stdout:
        print(result.stdout, end="")
    if result.returncode != 0:
        raise SystemExit(
            f"{name} failed with exit status {result.returncode}; "
            f"see {log_path}"
        )
    return result


def read_json(path: Path) -> dict[str, Any]:
    with path.open() as stream:
        value = json.load(stream)
    if not isinstance(value, dict):
        raise SystemExit(f"expected a JSON object in {path}")
    return value


def parse_last_json_line(output: str, name: str) -> dict[str, Any]:
    for line in reversed(output.splitlines()):
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            value = json.loads(line)
        except json.JSONDecodeError:
            continue
        if isinstance(value, dict):
            return value
    raise SystemExit(f"{name} did not emit a JSON result")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output-dir",
        type=Path,
        help="evidence directory (default: build/rp2040-profile/<revision>)",
    )
    parser.add_argument(
        "--endurance-cycles", type=int, default=2000,
        help="replacement cycles for each endurance binary (default: 2000)",
    )
    parser.add_argument(
        "--ticks-per-cycle", type=int, default=32,
        help="execution ticks per endurance cycle (default: 32)",
    )
    parser.add_argument(
        "--skip-target-build", action="store_true",
        help="skip PlatformIO and exact ELF gates; result is not full local acceptance",
    )
    args = parser.parse_args()
    if args.endurance_cycles <= 0 or args.ticks_per_cycle <= 0:
        parser.error("endurance counts must be positive")

    revision = git_output("rev-parse", "--short=12", "HEAD")
    output_dir = args.output_dir or (
        ROOT / "build" / "rp2040-profile" / revision
    )
    output_dir = output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    environment = os.environ.copy()
    em_cache = output_dir / "em-cache"
    em_cache.mkdir(exist_ok=True)
    environment["EM_CACHE"] = str(em_cache)

    run_logged(
        "nodedef-artifact",
        ["bash", "nodedef/build_osc_sine_wasm.sh"],
        output_dir,
        env=environment,
    )
    if (ROOT / "build" / "build.ninja").exists():
        meson_setup = ["meson", "setup", "--reconfigure", "build"]
    else:
        meson_setup = ["meson", "setup", "build"]
    run_logged("meson-setup", meson_setup, output_dir, env=environment)
    run_logged(
        "meson-compile", ["meson", "compile", "-C", "build"],
        output_dir, env=environment,
    )
    run_logged(
        "meson-test",
        ["meson", "test", "-C", "build", "--print-errorlogs"],
        output_dir,
        env=environment,
    )
    run_logged(
        "desktop-conformance",
        [
            sys.executable,
            "scripts/run_conformance.py",
            "--probe",
            "build/test/signal_engine_probe",
            "--target",
            "native",
        ],
        output_dir,
        env=environment,
    )

    run_logged(
        "firmware-harness-build", ["bash", "bench/build.sh", "all"],
        output_dir, env=environment,
    )
    run_logged(
        "firmware-conformance",
        [
            sys.executable,
            "scripts/run_conformance.py",
            "--probe",
            "bench/signal_engine_probe_firmware",
            "--target",
            "native",
        ],
        output_dir,
        env=environment,
    )

    bench_path = output_dir / "firmware-bench.json"
    run_logged(
        "firmware-bench",
        [
            sys.executable,
            "scripts/run_bench.py",
            "--profile",
            "firmware",
            "--results",
            str(bench_path),
        ],
        output_dir,
        env=environment,
    )
    benchmark = read_json(bench_path)
    if not benchmark.get("capacity_gate", {}).get("pass"):
        raise SystemExit("firmware capacity gate did not pass")

    endurance_arguments = [
        "bench/firmware-corpus/high-combined.useq",
        str(args.endurance_cycles),
        str(args.ticks_per_cycle),
    ]
    endurance_result = run_logged(
        "firmware-endurance",
        ["bench/firmware_profile_endurance", *endurance_arguments],
        output_dir,
        env=environment,
    )
    sanitizer_environment = environment.copy()
    sanitizer_environment["ASAN_OPTIONS"] = "detect_leaks=1:halt_on_error=1"
    sanitizer_environment["UBSAN_OPTIONS"] = (
        "halt_on_error=1:print_stacktrace=1"
    )
    sanitizer_result = run_logged(
        "firmware-endurance-sanitized",
        ["bench/firmware_profile_endurance_asan", *endurance_arguments],
        output_dir,
        env=sanitizer_environment,
    )
    endurance = parse_last_json_line(endurance_result.stdout, "endurance")
    endurance_sanitized = parse_last_json_line(
        sanitizer_result.stdout, "sanitized endurance"
    )
    if not endurance.get("pass") or not endurance_sanitized.get("pass"):
        raise SystemExit("firmware endurance gate did not pass")
    if endurance.get("checksum") != endurance_sanitized.get("checksum"):
        raise SystemExit("sanitized and optimized endurance checksums differ")

    target_reports: dict[str, Any] = {}
    if not args.skip_target_build:
        run_logged(
            "platformio",
            ["pio", "run", "-e", "musicthing", "-e", "musicthing-observe"],
            output_dir,
            env=environment,
        )
        for profile in ("musicthing", "musicthing-observe"):
            report_path = output_dir / f"{profile}-memory.json"
            run_logged(
                f"{profile}-memory",
                [
                    sys.executable,
                    "scripts/rp2040_memory_report.py",
                    "--elf",
                    f".pio/build/{profile}/firmware.elf",
                    "--label",
                    profile,
                    "--output",
                    str(report_path),
                ],
                output_dir,
                env=environment,
            )
            target_reports[profile] = read_json(report_path)
            if not target_reports[profile].get("pass"):
                raise SystemExit(f"{profile} exact memory gate did not pass")

    summary = {
        "schema_version": 1,
        "source": {
            "revision": git_output("rev-parse", "HEAD"),
            "dirty": bool(git_output("status", "--porcelain")),
        },
        "pass": True,
        "full_local_acceptance": not args.skip_target_build,
        "target_memory": target_reports,
        "firmware_capacity": benchmark["capacity_gate"],
        "endurance": endurance,
        "endurance_sanitized": endurance_sanitized,
        "evidence_directory": str(output_dir),
    }
    summary_path = output_dir / "summary.json"
    summary_path.write_text(json.dumps(summary, indent=2) + "\n")
    print(
        f"[rp2040-profile] PASS; evidence: {summary_path}",
        file=sys.stderr,
    )


if __name__ == "__main__":
    main()
