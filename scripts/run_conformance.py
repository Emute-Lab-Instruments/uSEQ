#!/usr/bin/env python3
"""Conformance suite runner (Layer 1 of docs/testing/conformance-and-bench-design.md).

Discovers YAML fixtures under test/conformance/<spec-area>/*.yaml and drives
the session probe (signal_engine_probe --session, JSONL over stdin/stdout).

Usage:
    scripts/run_conformance.py [fixtures...] [--tags smoke,audit-regression]
        [--target native|wasm] [--probe PATH] [--validate] [-v]

Fixture case schema (see the design doc):
    - name: case-name                       # mandatory, unique within file
      spec: state-identity.md §6.6          # mandatory spec citation
      tags: [smoke, state, audit-regression]
      steps:
        - eval: "(a1 (phasor 1))"           # expect success by default
          expect_value: 0.0                 # optional (scratch-eval result)
          tol: 1e-9                         # optional, default 1e-9
        - eval: "(a1 (bogus))"              # failing eval:
          expect_diagnostic:                #   presence of expect_diagnostic
            category: undefinedName         #   (or expect_error) flips the
            span: [4, 11]                   #   expectation to "must fail"
        - tick: 0.5                         # advance engine time (commits)
        - sample: {output: a1, times: [0, 0.25]}
          expect_values: [0.0, 0.5]
          tol: 1e-9
        - clear: true                       # useq-clear
        - health: {output: a1, expect: running}
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

import yaml

PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_FIXTURE_DIR = PROJECT_ROOT / "test" / "conformance"
DEFAULT_PROBE_CANDIDATES = [
    PROJECT_ROOT / "build-probe" / "test" / "signal_engine_probe",
    PROJECT_ROOT / "build" / "test" / "signal_engine_probe",
]

VALID_STEP_OPS = {"eval", "tick", "sample", "clear", "health"}
VALID_STEP_KEYS = VALID_STEP_OPS | {
    "expect_value", "expect_values", "tol", "expect_diagnostic", "expect_error",
    "comment",
}
VALID_CASE_KEYS = {"name", "spec", "steps", "tags", "comment"}
VALID_CATEGORIES = {
    "syntax", "undefinedname", "arity", "type", "boundary", "arithmetic",
    "runtime", "overflow",
}
DEFAULT_TOL = 1e-9


# ── Probe session ────────────────────────────────────────────────────────────

class ProbeSession:
    """One --session subprocess; fresh per case for isolation."""

    def __init__(self, probe_path: Path):
        self.proc = subprocess.Popen(
            [str(probe_path), "--session"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
        )

    def request(self, obj: dict) -> dict:
        assert self.proc.stdin and self.proc.stdout
        self.proc.stdin.write(json.dumps(obj) + "\n")
        self.proc.stdin.flush()
        line = self.proc.stdout.readline()
        if not line:
            raise ProbeDied(f"probe exited (rc={self.proc.poll()}) on: {obj}")
        return json.loads(line)

    def close(self) -> None:
        try:
            if self.proc.stdin:
                self.proc.stdin.close()
            self.proc.wait(timeout=5)
        except Exception:
            self.proc.kill()


class ProbeDied(Exception):
    pass


class CaseFailure(Exception):
    pass


# ── Fixture loading / validation ────────────────────────────────────────────

def expand_fixtures(paths: list[str]) -> list[Path]:
    if not paths:
        paths = [str(DEFAULT_FIXTURE_DIR)]
    files: list[Path] = []
    for raw in paths:
        p = Path(raw)
        if p.is_dir():
            files.extend(sorted(p.rglob("*.yaml")))
            files.extend(sorted(p.rglob("*.yml")))
        else:
            files.append(p)
    return files


def validate_case(case: dict, source: str) -> list[str]:
    errs: list[str] = []
    if not isinstance(case, dict):
        return [f"{source}: case is not a mapping"]
    name = case.get("name", "<unnamed>")
    loc = f"{source}::{name}"
    for key in case:
        if key not in VALID_CASE_KEYS:
            errs.append(f"{loc}: unknown case key '{key}'")
    if not case.get("name"):
        errs.append(f"{source}: case missing 'name'")
    if not case.get("spec"):
        errs.append(f"{loc}: missing mandatory 'spec' citation")
    tags = case.get("tags", [])
    if not isinstance(tags, list) or not all(isinstance(t, str) for t in tags):
        errs.append(f"{loc}: 'tags' must be a list of strings")
    steps = case.get("steps")
    if not isinstance(steps, list) or not steps:
        errs.append(f"{loc}: 'steps' must be a non-empty list")
        return errs
    for i, step in enumerate(steps):
        sloc = f"{loc}#step{i + 1}"
        if not isinstance(step, dict):
            errs.append(f"{sloc}: step is not a mapping")
            continue
        ops = [k for k in step if k in VALID_STEP_OPS]
        if len(ops) != 1:
            errs.append(f"{sloc}: step must contain exactly one op "
                        f"(eval/tick/sample/clear/health), got {ops}")
            continue
        op = ops[0]
        for key in step:
            if key not in VALID_STEP_KEYS:
                errs.append(f"{sloc}: unknown step key '{key}'")
        if op == "eval" and not isinstance(step["eval"], str):
            errs.append(f"{sloc}: 'eval' must be a code string")
        if op == "tick" and not isinstance(step["tick"], (int, float)):
            errs.append(f"{sloc}: 'tick' must be a number (time t)")
        if op == "sample":
            spec = step["sample"]
            if (not isinstance(spec, dict) or "output" not in spec
                    or not isinstance(spec.get("times"), list)):
                errs.append(f"{sloc}: 'sample' needs {{output, times: [...]}}")
            elif "expect_values" in step and \
                    len(step["expect_values"]) != len(spec["times"]):
                errs.append(f"{sloc}: expect_values length != times length")
        if op == "health":
            spec = step["health"]
            if (not isinstance(spec, dict) or "output" not in spec
                    or spec.get("expect") not in
                    {"running", "fallback", "error", "idle"}):
                errs.append(f"{sloc}: 'health' needs {{output, expect: "
                            f"running|fallback|error|idle}}")
        ee = step.get("expect_error")
        if ee is not None and ee not in (True, False, "allow"):
            errs.append(f"{sloc}: expect_error must be true/false/allow")
        ed = step.get("expect_diagnostic")
        if ed is not None:
            if op != "eval":
                errs.append(f"{sloc}: expect_diagnostic only valid on eval")
            elif not isinstance(ed, dict) or "category" not in ed:
                errs.append(f"{sloc}: expect_diagnostic needs 'category'")
            else:
                if ed["category"].lower() not in VALID_CATEGORIES:
                    errs.append(f"{sloc}: unknown diagnostic category "
                                f"'{ed['category']}'")
                span = ed.get("span")
                if span is not None and (not isinstance(span, list)
                                         or len(span) != 2):
                    errs.append(f"{sloc}: span must be [start, end]")
        if "expect_value" in step and op != "eval":
            errs.append(f"{sloc}: expect_value only valid on eval steps")
        if "expect_values" in step and op != "sample":
            errs.append(f"{sloc}: expect_values only valid on sample steps")
    # Resilience doctrine: cases tagged 'resilience' must end with a recovery
    # step that asserts a value (the engine must stay usable).
    if "resilience" in tags:
        last = steps[-1] if isinstance(steps[-1], dict) else {}
        if not ("expect_value" in last or "expect_values" in last
                or "health" in last):
            errs.append(f"{loc}: resilience case must end with a recovery "
                        f"step asserting a value or health")
    return errs


# ── Execution ────────────────────────────────────────────────────────────────

def approx_equal(a: float, b: float, tol: float) -> bool:
    return abs(a - b) <= tol


def check_values(got: list[float], want: list[float], tol: float,
                 what: str) -> None:
    if len(got) != len(want):
        raise CaseFailure(f"{what}: got {len(got)} values, want {len(want)}")
    for i, (g, w) in enumerate(zip(got, want)):
        if not approx_equal(g, w, tol):
            raise CaseFailure(
                f"{what}[{i}]: got {g!r}, want {w!r} (tol {tol})")


def check_diagnostic(resp: dict, expect: dict, what: str) -> None:
    diags = resp.get("diagnostics", [])
    want_cat = expect["category"].lower()
    want_span = expect.get("span")
    for d in diags:
        if d.get("category", "").lower() != want_cat:
            continue
        if want_span is not None and list(d.get("span", [])) != list(want_span):
            continue
        return  # matched
    raise CaseFailure(
        f"{what}: no diagnostic with category '{want_cat}'"
        + (f" span {want_span}" if want_span else "")
        + f"; got {json.dumps(diags)}")


def run_step(session: ProbeSession, step: dict, idx: int) -> None:
    what = f"step {idx + 1}"
    tol = float(step.get("tol", DEFAULT_TOL))

    if "eval" in step:
        resp = session.request({"op": "eval", "code": step["eval"]})
        if step.get("expect_error") == "allow":
            # Resilience mode: either outcome is fine as long as the probe
            # responded (no crash; always a diagnostic or a value).
            return
        expect_fail = "expect_diagnostic" in step or step.get("expect_error")
        if expect_fail:
            if resp.get("ok"):
                raise CaseFailure(f"{what}: eval succeeded, expected failure: "
                                  f"{step['eval']!r}")
            if "expect_diagnostic" in step:
                check_diagnostic(resp, step["expect_diagnostic"], what)
        else:
            if not resp.get("ok"):
                raise CaseFailure(f"{what}: eval failed: {step['eval']!r} -> "
                                  f"{json.dumps(resp)}")
            if "expect_value" in step:
                if "value" not in resp:
                    raise CaseFailure(f"{what}: eval returned no value; "
                                      f"expected {step['expect_value']}")
                check_values([resp["value"]], [float(step["expect_value"])],
                             tol, what)
    elif "tick" in step:
        resp = session.request({"op": "tick", "t": float(step["tick"])})
        if not resp.get("ok"):
            raise CaseFailure(f"{what}: tick failed: {json.dumps(resp)}")
    elif "sample" in step:
        spec = step["sample"]
        resp = session.request({"op": "sample", "output": spec["output"],
                                "times": [float(t) for t in spec["times"]]})
        if not resp.get("ok"):
            raise CaseFailure(f"{what}: sample failed: {json.dumps(resp)}")
        if "expect_values" in step:
            check_values(resp["values"],
                         [float(v) for v in step["expect_values"]], tol, what)
    elif "clear" in step:
        resp = session.request({"op": "clear"})
        if not resp.get("ok"):
            raise CaseFailure(f"{what}: clear failed: {json.dumps(resp)}")
    elif "health" in step:
        spec = step["health"]
        resp = session.request({"op": "health", "output": spec["output"]})
        if not resp.get("ok"):
            raise CaseFailure(f"{what}: health failed: {json.dumps(resp)}")
        if resp.get("health") != spec["expect"]:
            raise CaseFailure(f"{what}: health of {spec['output']} is "
                              f"{resp.get('health')!r}, want {spec['expect']!r}")


def run_case(probe_path: Path, case: dict) -> None:
    session = ProbeSession(probe_path)
    try:
        for i, step in enumerate(case["steps"]):
            run_step(session, step, i)
    finally:
        session.close()


# ── Main ─────────────────────────────────────────────────────────────────────

def resolve_probe(explicit: str | None) -> Path:
    if explicit:
        p = Path(explicit)
        if not p.is_file():
            raise SystemExit(f"Missing probe executable: {p}")
        return p
    for c in DEFAULT_PROBE_CANDIDATES:
        if c.is_file():
            return c
    raise SystemExit("Could not find signal_engine_probe. Pass --probe or "
                     "build it (meson setup build && ninja -C build -j4).")


def spec_area(fixture: Path) -> str:
    try:
        return fixture.relative_to(DEFAULT_FIXTURE_DIR).parts[0]
    except ValueError:
        return fixture.parent.name


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("fixtures", nargs="*",
                    help="Fixture files/dirs (default: test/conformance/)")
    ap.add_argument("--probe", help="Path to signal_engine_probe")
    ap.add_argument("--target", choices=["native", "wasm", "serial"],
                    default="native")
    ap.add_argument("--tags", help="Comma-separated tag filter (case runs if "
                                   "it has ANY of these tags)")
    ap.add_argument("--validate", action="store_true",
                    help="Validate fixture schema only (no probe needed)")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    if args.target == "wasm":
        raise SystemExit("--target wasm: not implemented yet (needs the node "
                         "wrapper over the WASM exports; see the design doc "
                         "Layer 0). Use --target native.")
    if args.target == "serial":
        raise SystemExit("--target serial: not implemented (release-gate "
                         "adapter, fw-safe subset).")

    fixture_files = expand_fixtures(args.fixtures)
    if not fixture_files:
        raise SystemExit("No fixture files found.")

    tag_filter = set(args.tags.split(",")) if args.tags else None

    # Load + validate everything first.
    all_cases: list[tuple[Path, dict]] = []
    schema_errors: list[str] = []
    names_seen: set[str] = set()
    for f in fixture_files:
        try:
            docs = yaml.safe_load(f.read_text())
        except yaml.YAMLError as e:
            schema_errors.append(f"{f}: YAML parse error: {e}")
            continue
        if docs is None:
            continue
        if not isinstance(docs, list):
            schema_errors.append(f"{f}: top level must be a list of cases")
            continue
        for case in docs:
            errs = validate_case(case, str(f.relative_to(PROJECT_ROOT)))
            schema_errors.extend(errs)
            if not errs:
                if case["name"] in names_seen:
                    schema_errors.append(f"{f}: duplicate case name "
                                         f"'{case['name']}'")
                names_seen.add(case["name"])
                all_cases.append((f, case))

    if schema_errors:
        print(f"Schema validation: {len(schema_errors)} error(s)")
        for e in schema_errors:
            print(f"  ✗ {e}")
        return 1
    print(f"Schema validation: OK ({len(all_cases)} cases in "
          f"{len(fixture_files)} files)")
    if args.validate:
        return 0

    probe = resolve_probe(args.probe)

    # Run.
    area_stats: dict[str, list[int]] = defaultdict(lambda: [0, 0])  # pass, fail
    failures: list[tuple[str, str, str]] = []
    skipped = 0
    for f, case in all_cases:
        tags = set(case.get("tags", []))
        if tag_filter and not (tags & tag_filter):
            skipped += 1
            continue
        area = spec_area(f)
        label = f"{area}/{case['name']}"
        try:
            run_case(probe, case)
            area_stats[area][0] += 1
            if args.verbose:
                print(f"  ✓ {label}")
        except (CaseFailure, ProbeDied, json.JSONDecodeError) as e:
            area_stats[area][1] += 1
            failures.append((label, case["spec"], str(e)))
            print(f"  ✗ {label} [{case['spec']}]: {e}")

    print()
    print(f"{'spec area':<22} {'pass':>5} {'fail':>5}")
    total_pass = total_fail = 0
    for area in sorted(area_stats):
        p, fl = area_stats[area]
        total_pass += p
        total_fail += fl
        print(f"{area:<22} {p:>5} {fl:>5}")
    print(f"{'TOTAL':<22} {total_pass:>5} {total_fail:>5}"
          + (f"   ({skipped} skipped by tag filter)" if skipped else ""))

    if failures:
        print(f"\n{len(failures)} failing case(s). A failing case means the "
              f"implementation OR the spec is wrong — triage against the "
              f"cited spec §; do not delete the case.")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
