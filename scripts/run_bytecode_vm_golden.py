#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import statistics
import subprocess
import sys
import time
from pathlib import Path

import yaml


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_FIXTURE_DIR = PROJECT_ROOT / "test" / "modulisp" / "bytecode_vm" / "golden"
DEFAULT_BUILD_DIR = PROJECT_ROOT / "build"
DEFAULT_PROBE_CANDIDATES = [
    DEFAULT_BUILD_DIR / "test" / "bytecode_vm_probe",
    DEFAULT_BUILD_DIR / "test" / "test_bytecode_vm_probe",
    DEFAULT_BUILD_DIR / "bytecode_vm_probe",
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run the bytecode VM golden fixtures against the interpreter probe."
    )
    parser.add_argument(
        "fixtures",
        nargs="*",
        help="Fixture files or directories. Defaults to the golden fixture directory.",
    )
    parser.add_argument(
        "--probe",
        help="Path to the bytecode_vm_probe executable.",
    )
    parser.add_argument(
        "--benchmark",
        action="store_true",
        help="Measure probe execution time instead of only validating semantics.",
    )
    parser.add_argument(
        "--iterations",
        type=int,
        default=25,
        help="Probe executions per sample when benchmarking.",
    )
    return parser.parse_args()


def expand_fixtures(paths: list[str]) -> list[Path]:
    if not paths:
        paths = [str(DEFAULT_FIXTURE_DIR)]

    files: list[Path] = []
    for raw_path in paths:
        path = Path(raw_path)
        if path.is_dir():
            files.extend(sorted(path.glob("*.yaml")))
            files.extend(sorted(path.glob("*.yml")))
        else:
            files.append(path)
    return files


def resolve_probe(explicit: str | None) -> Path:
    if explicit:
        probe = Path(explicit)
        if not probe.is_file():
            raise SystemExit(f"Missing probe executable: {probe}")
        return probe

    for candidate in DEFAULT_PROBE_CANDIDATES:
        if candidate.is_file():
            return candidate

    raise SystemExit(
        "Could not find bytecode_vm_probe. Pass --probe or build the test target."
    )


def load_fixture(path: Path) -> dict:
    with path.open("r", encoding="utf-8") as handle:
        data = yaml.safe_load(handle)
    if not isinstance(data, dict):
        raise SystemExit(f"{path}: top-level YAML document must be a mapping")
    return data


def build_code(output: str, expr: str, setup: list[str] | None) -> str:
    expr_form = f"({output} {expr})"
    if setup:
        forms = " ".join(setup + [expr_form])
        return f"(do {forms})"
    return expr_form


def run_probe(
    probe: Path,
    *,
    code: str,
    output: str,
    time_seconds: float,
    bpm: float,
    time_sig: list[float],
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            str(probe),
            "--code",
            code,
            "--output",
            output,
            "--time",
            str(time_seconds),
            "--bpm",
            str(bpm),
            "--time-sig",
            str(time_sig[0]),
            str(time_sig[1]),
        ],
        cwd=PROJECT_ROOT,
        capture_output=True,
        text=True,
        check=False,
    )


def parse_probe_output(raw: str) -> dict:
    payload = raw.strip().splitlines()[-1]
    return json.loads(payload)


def validate_fixture(probe: Path, fixture_path: Path, benchmark: bool, iterations: int) -> int:
    fixture = load_fixture(fixture_path)
    bpm = float(fixture.get("default_bpm", 120.0))
    time_sig = fixture.get("default_time_sig", [4, 4])
    tolerance = float(fixture.get("default_tolerance", 1e-9))

    cases = fixture.get("tests", [])
    if not isinstance(cases, list):
        raise SystemExit(f"{fixture_path}: tests must be a list")

    failures = 0
    timings: list[float] = []

    for case in cases:
        if not isinstance(case, dict):
            raise SystemExit(f"{fixture_path}: each test case must be a mapping")

        case_name = case.get("name", "<unnamed>")
        output = case.get("output", "a1")
        expr = case.get("expr")
        if not isinstance(expr, str):
            raise SystemExit(f"{fixture_path}: case {case_name!r} is missing expr")

        case_bpm = float(case.get("bpm", bpm))
        case_time_sig = case.get("time_sig", time_sig)
        case_tolerance = float(case.get("tolerance", tolerance))
        setup = case.get("setup", [])
        if not isinstance(setup, list):
            raise SystemExit(f"{fixture_path}: case {case_name!r} setup must be a list")

        code = build_code(output, expr, [str(item) for item in setup])
        samples = case.get("samples", [])
        if not isinstance(samples, list):
            raise SystemExit(f"{fixture_path}: case {case_name!r} samples must be a list")

        for sample in samples:
            if not isinstance(sample, dict):
                raise SystemExit(f"{fixture_path}: sample entries must be mappings")

            time_seconds = float(sample.get("t", sample.get("time", 0.0)))
            expects_error = "expect_error" in sample

            started = time.perf_counter()
            result = run_probe(
                probe,
                code=code,
                output=output,
                time_seconds=time_seconds,
                bpm=case_bpm,
                time_sig=case_time_sig,
            )
            if result.returncode != 0 and not expects_error:
                print(result.stdout, end="")
                print(result.stderr, end="", file=sys.stderr)
                print(
                    f"{fixture_path}: {case_name} @ t={time_seconds} failed with exit {result.returncode}",
                    file=sys.stderr,
                )
                failures += 1
                continue

            payload = parse_probe_output(result.stdout or result.stderr)
            if expects_error:
                expected_error = str(sample["expect_error"])
                if payload.get("ok", False) or expected_error not in str(payload.get("error", "")):
                    print(
                        f"{fixture_path}: {case_name} expected error containing {expected_error!r}",
                        file=sys.stderr,
                    )
                    failures += 1
            else:
                expected = float(sample["expect"])
                actual = float(payload["value"])
                if abs(actual - expected) > case_tolerance:
                    print(
                        f"{fixture_path}: {case_name} @ t={time_seconds} expected {expected}, got {actual}",
                        file=sys.stderr,
                    )
                    failures += 1

            timings.append(time.perf_counter() - started)

            if benchmark and iterations > 1:
                for _ in range(iterations - 1):
                    repeat_started = time.perf_counter()
                    repeat_result = run_probe(
                        probe,
                        code=code,
                        output=output,
                        time_seconds=time_seconds,
                        bpm=case_bpm,
                        time_sig=case_time_sig,
                    )
                    timings.append(time.perf_counter() - repeat_started)
                    if repeat_result.returncode != 0:
                        break

    if benchmark and timings:
        print(
            f"{fixture_path.name}: samples={len(timings)} mean_ms={statistics.mean(timings) * 1000:.3f} "
            f"min_ms={min(timings) * 1000:.3f} max_ms={max(timings) * 1000:.3f}"
        )
        if iterations > 1:
            print(f"benchmark iterations per sample: {iterations}")

    return failures


def main() -> int:
    args = parse_args()
    probe = resolve_probe(args.probe)
    fixture_files = expand_fixtures(args.fixtures)

    failures = 0
    for fixture_path in fixture_files:
        failures += validate_fixture(probe, fixture_path, args.benchmark, args.iterations)

    if failures:
        print(f"{failures} fixture check(s) failed", file=sys.stderr)
        return 1

    print(f"validated {len(fixture_files)} fixture file(s) with {probe}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
