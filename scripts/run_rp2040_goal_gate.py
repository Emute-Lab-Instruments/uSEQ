#!/usr/bin/env python3
"""Run one complete, uniquely recorded RP2040 optimization candidate gate."""

from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
LABEL_PATTERN = re.compile(r"^[A-Za-z0-9._-]+$")


def git_output(*args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(argv: list[str], environment: dict[str, str]) -> None:
    print(f"[rp2040-goal] {' '.join(argv)}", file=sys.stderr, flush=True)
    subprocess.run(argv, cwd=ROOT, env=environment, check=True)


def evidence_ref(path: Path) -> dict[str, Any]:
    data = json.loads(path.read_text())
    return {
        "path": str(path),
        "sha256": sha256(path),
        "pass": data.get("pass") is True,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-root", type=Path, default=ROOT / "build/rp2040-goal")
    parser.add_argument("--label")
    parser.add_argument("--wokwi-cli", default=os.environ.get("WOKWI_CLI", "wokwi-cli"))
    parser.add_argument("--endurance-cycles", type=int, default=2000)
    parser.add_argument("--ticks-per-cycle", type=int, default=32)
    parser.add_argument("--workload-cycles", type=int, default=2)
    parser.add_argument("--tick-observation-ms", type=int, default=1000)
    args = parser.parse_args()

    cli = shutil.which(args.wokwi_cli)
    if not cli:
        parser.error(
            "wokwi-cli not found; install the official CLI or pass --wokwi-cli"
        )
    if not os.environ.get("WOKWI_CLI_TOKEN"):
        parser.error("WOKWI_CLI_TOKEN is not set")
    for name in ("endurance_cycles", "ticks_per_cycle", "workload_cycles"):
        if getattr(args, name) <= 0:
            parser.error(f"--{name.replace('_', '-')} must be positive")

    revision_short = git_output("rev-parse", "--short=12", "HEAD")
    revision = git_output("rev-parse", "HEAD")
    timestamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    label = args.label or f"{timestamp}-{revision_short}"
    if not LABEL_PATTERN.fullmatch(label):
        parser.error(
            "--label may contain only letters, digits, dot, underscore, hyphen"
        )
    candidate_dir = (args.output_root / label).resolve()
    if candidate_dir.exists():
        parser.error(f"candidate evidence directory already exists: {candidate_dir}")
    local_dir = candidate_dir / "local"
    simulator_dir = candidate_dir / "wokwi"
    candidate_dir.mkdir(parents=True)

    environment = os.environ.copy()
    run(
        [
            sys.executable,
            "scripts/run_rp2040_profile.py",
            "--output-dir",
            str(local_dir),
            "--endurance-cycles",
            str(args.endurance_cycles),
            "--ticks-per-cycle",
            str(args.ticks_per_cycle),
        ],
        environment,
    )
    run(
        [
            sys.executable,
            "scripts/run_wokwi_rp2040.py",
            "--output-dir",
            str(simulator_dir),
            "--wokwi-cli",
            cli,
            "--workload-cycles",
            str(args.workload_cycles),
            "--tick-observation-ms",
            str(args.tick_observation_ms),
            "--skip-build",
        ],
        environment,
    )

    local_summary = local_dir / "summary.json"
    simulator_summary = simulator_dir / "summary.json"
    local_data = json.loads(local_summary.read_text())
    simulator_data = json.loads(simulator_summary.read_text())
    local = evidence_ref(local_summary)
    simulator = evidence_ref(simulator_summary)
    summary = {
        "schema_version": 1,
        "grade": "rp2040-optimization-candidate",
        "candidate": label,
        "source": {
            "revision": revision,
            "dirty": bool(git_output("status", "--porcelain")),
        },
        "toolchain": local_data.get("toolchain"),
        "simulator_environment": simulator_data.get("simulator"),
        "local": local,
        "simulator": simulator,
        "pass": local["pass"] and simulator["pass"],
    }
    summary_path = candidate_dir / "summary.json"
    summary_path.write_text(json.dumps(summary, indent=2) + "\n")
    if not summary["pass"]:
        print(f"[rp2040-goal] FAIL; evidence: {summary_path}", file=sys.stderr)
        return 1
    print(f"[rp2040-goal] PASS; evidence: {summary_path}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
