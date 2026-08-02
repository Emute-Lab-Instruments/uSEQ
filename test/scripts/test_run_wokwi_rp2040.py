#!/usr/bin/env python3
"""Structural contract test for the Wokwi RP2040 acceptance runner."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import tempfile


ROOT = Path(__file__).resolve().parents[2]
MODULE_PATH = ROOT / "scripts/run_wokwi_rp2040.py"
SPEC = importlib.util.spec_from_file_location("run_wokwi_rp2040", MODULE_PATH)
assert SPEC and SPEC.loader
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


def resource_data() -> dict[str, object]:
    return {
        "heap_free": 70000,
        "heap_min_free": 60000,
        "core0_stack_margin_intact": True,
        "core0_stack": {"used": 900, "capacity": 2048},
        "nodes": {"used": 512, "capacity": 1024},
        "arena": {"used": 8192, "capacity": 16384},
        "cells": {"used": 12, "capacity": 64},
        "data_entries": {"used": 256, "capacity": 512},
        "state_slots": {"used": 8, "capacity": 16},
        "live_slots": {"used": 16, "capacity": 32},
        "synth_declarations": {"used": 16, "capacity": 64},
        "synth_controls": {"used": 32, "capacity": 128},
        "watchdog_reboot": 0,
    }


def main() -> None:
    scenario = runner.generate_scenario(1, 1000)
    assert scenario.steps
    assert scenario.steps[0] == {"wait-serial": '"type":"ready"'}
    assert runner.parse_forms(ROOT / "bench/firmware-corpus/high-combined.useq")

    messages: list[dict[str, object]] = [{"type": "ready", "version": "1.2.0"}]
    resources = resource_data()
    for request_id, expected_success in scenario.expected_success.items():
        response: dict[str, object] = {
            "type": "response",
            "requestId": request_id,
            "success": expected_success,
        }
        if request_id in scenario.timing_workload:
            response["data"] = {
                "last_us": 100,
                "max_us": 100,
                "count": 1,
                "error_count": 0,
            }
        elif request_id.startswith("resources-"):
            response["data"] = resources
        elif request_id.startswith("state-"):
            response["data"] = {
                "outputs": [
                    {"name": "a1", "health": "running"},
                    {"name": "a2", "health": "idle"},
                ]
            }
        elif request_id == "eval-final":
            response["data"] = {
                "last_us": 20,
                "max_us": 100,
                "count": 50,
                "error_count": 1,
            }
        else:
            response["data"] = {}
        messages.append(response)

    for index in range(25):
        messages.append(
            {
                "type": "debug",
                "channel": "tick",
                "data": {"last_total_us": 100 + index},
            }
        )

    with tempfile.TemporaryDirectory() as directory:
        vcd = Path(directory) / "gpio.vcd"
        identifiers = ("!", '"', "#", "$")
        lines = ["$scope module logic $end"]
        for identifier, channel in zip(identifiers, runner.GPIO_CHANNELS):
            lines.append(f"$var wire 1 {identifier} {channel} $end")
        lines.append("$enddefinitions $end")
        for identifier in identifiers:
            lines.extend((f"0{identifier}", "#1", f"1{identifier}"))
        vcd.write_text("\n".join(lines) + "\n")
        activity = runner.parse_vcd_activity(vcd)

    budget = json.loads((ROOT / "scripts/rp2040_budget.json").read_text())
    manifest = json.loads((ROOT / "bench/firmware-corpus/manifest.json").read_text())
    checks, observations = runner.evaluate(
        messages, scenario, budget, manifest, activity
    )
    failures = [check for check in checks if not check["pass"]]
    assert not failures, failures
    assert observations["ready_count"] == 1
    assert observations["vcd_activity"] == {
        "d1": [0, 1],
        "d2": [0, 1],
        "a4": [0, 1],
        "a3": [0, 1],
    }


if __name__ == "__main__":
    main()
