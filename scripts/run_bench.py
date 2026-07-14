#!/usr/bin/env python3
"""run_bench.py — orchestrate bench/bench_probe over bench/corpus/*.useq.

Writes bench/results/<git-sha>.json and optionally compares against a
baseline: --compare <baseline.json> prints a delta table and exits non-zero
on >10% regression (in a "bigger is worse" metric) for smoke-tagged
workloads.

Usage:
  scripts/run_bench.py                 # run, write bench/results/<sha>.json
  scripts/run_bench.py --compare bench/results/abc123.json
  scripts/run_bench.py --no-run --results X.json --compare Y.json
"""

import argparse
import json
import os
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROBE = os.path.join(REPO, "bench", "bench_probe")
CORPUS = os.path.join(REPO, "bench", "corpus")
RESULTS_DIR = os.path.join(REPO, "bench", "results")

# Workloads in the smoke regression gate.
SMOKE = {"minimal", "typical-set", "state-heavy", "cascade"}

# Metrics compared for regressions (bigger is worse).
REGRESSION_METRICS = {
    "cold_eval_median": "ms",
    "recompile_median": "us",
    "ns_per_tick_median": "ns",
}
THRESHOLD = 0.10  # 10%


def git_sha():
    try:
        return subprocess.check_output(
            ["git", "rev-parse", "--short", "HEAD"], cwd=REPO, text=True
        ).strip()
    except Exception:
        return "unknown"


def build_probe():
    if not os.path.exists(PROBE):
        print("bench_probe not built; running bench/build.sh ...", file=sys.stderr)
        subprocess.check_call(["bash", os.path.join(REPO, "bench", "build.sh")])


def run_workload(path):
    """Run bench_probe on one corpus file, return list of metric dicts."""
    out = subprocess.run([PROBE, path], capture_output=True, text=True)
    rows = []
    for line in out.stdout.splitlines():
        line = line.strip()
        if line.startswith("{"):
            try:
                rows.append(json.loads(line))
            except json.JSONDecodeError:
                pass
    if out.returncode != 0:
        sys.stderr.write(out.stderr)
        print(f"WARNING: probe exited {out.returncode} for {path}", file=sys.stderr)
    return rows


def run_all():
    build_probe()
    results = {"git_sha": git_sha(), "workloads": {}}
    files = sorted(
        f for f in os.listdir(CORPUS) if f.endswith(".useq")
    )
    for fname in files:
        wl = fname[: -len(".useq")]
        print(f"running {wl} ...", file=sys.stderr)
        rows = run_workload(os.path.join(CORPUS, fname))
        metrics = {}
        for r in rows:
            metrics[r["metric"]] = {"value": r["value"], "unit": r["unit"],
                                    "phase": r["phase"]}
        results["workloads"][wl] = metrics
    return results


def compare(current, baseline):
    """Print delta table; return list of (workload, metric, pct) regressions."""
    regressions = []
    hdr = f"{'workload':<16} {'metric':<20} {'baseline':>12} {'current':>12} {'delta':>8}"
    print(hdr)
    print("-" * len(hdr))
    for wl, metrics in sorted(current["workloads"].items()):
        base_wl = baseline.get("workloads", {}).get(wl, {})
        for metric, unit in REGRESSION_METRICS.items():
            cur = metrics.get(metric, {}).get("value")
            base = base_wl.get(metric, {}).get("value")
            if cur is None or base is None:
                continue
            pct = (cur - base) / base * 100 if base else 0.0
            flag = ""
            if pct > THRESHOLD * 100:
                flag = " REGRESSION" if wl in SMOKE else " (non-smoke)"
                if wl in SMOKE:
                    regressions.append((wl, metric, pct))
            print(f"{wl:<16} {metric:<20} {base:>10.3f}{unit:<2} "
                  f"{cur:>10.3f}{unit:<2} {pct:>+7.1f}%{flag}")
    return regressions


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--compare", metavar="BASELINE_JSON",
                    help="baseline results JSON to compare against")
    ap.add_argument("--results", metavar="JSON",
                    help="use an existing results JSON instead of default path")
    ap.add_argument("--no-run", action="store_true",
                    help="skip running the bench; requires --results")
    args = ap.parse_args()

    if args.no_run:
        if not args.results:
            ap.error("--no-run requires --results")
        with open(args.results) as f:
            current = json.load(f)
    else:
        current = run_all()
        os.makedirs(RESULTS_DIR, exist_ok=True)
        out_path = args.results or os.path.join(
            RESULTS_DIR, f"{current['git_sha']}.json")
        with open(out_path, "w") as f:
            json.dump(current, f, indent=2)
        print(f"wrote {out_path}", file=sys.stderr)

    if args.compare:
        with open(args.compare) as f:
            baseline = json.load(f)
        regressions = compare(current, baseline)
        if regressions:
            print(f"\nFAIL: {len(regressions)} smoke regression(s) > "
                  f"{THRESHOLD:.0%}:", file=sys.stderr)
            for wl, metric, pct in regressions:
                print(f"  {wl}/{metric}: +{pct:.1f}%", file=sys.stderr)
            sys.exit(1)
        print("\nno smoke regressions > 10%")


if __name__ == "__main__":
    main()
