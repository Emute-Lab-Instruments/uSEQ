#!/usr/bin/env python3
"""Exercise the exact Music Thing observation ELF in Wokwi.

This is a target-functional and target-timing evidence grade. Static capacity
still comes from the exact ELF report, and silicon remains authoritative for
properties outside Wokwi's RP2040 model.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import subprocess
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
WOKWI_ROOT = ROOT / "wokwi"
DEFAULT_ELF = ROOT / ".pio/build/musicthing-observe/firmware.elf"
DEFAULT_BUDGET = ROOT / "scripts/rp2040_budget.json"
WORKLOADS = ("moderate-mixed", "high-combined")
GPIO_SIGNALS = ("d1", "d2", "a4", "a3")
GPIO_CHANNELS = {"D0": "d1", "D1": "d2", "D2": "a4", "D3": "a3"}
TICK_STREAM_RATE_HZ = 20
DEFAULT_TICK_OBSERVATION_MS = 30_000


def git_output(*args: str) -> str:
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True).strip()


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_forms(path: Path) -> list[str]:
    forms: list[str] = []
    current: list[str] = []
    depth = 0
    in_comment = False
    in_string = False
    escaped = False
    for char in path.read_text():
        if in_comment:
            if char == "\n":
                in_comment = False
            continue
        if not in_string and char == ";":
            in_comment = True
            continue
        if depth == 0 and char.isspace():
            continue
        current.append(char)
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            continue
        if char == '"':
            in_string = True
        elif char in "([":
            depth += 1
        elif char in ")]":
            depth -= 1
            if depth < 0:
                raise SystemExit(f"unbalanced closing delimiter in {path}")
            if depth == 0:
                forms.append("".join(current))
                current.clear()
    if depth != 0 or in_string or current:
        raise SystemExit(f"incomplete top-level form in {path}")
    return forms


def percentile(values: list[int], quantile: float) -> int:
    if not values:
        raise ValueError("percentile of empty sample")
    ordered = sorted(values)
    index = max(0, math.ceil(quantile * len(ordered)) - 1)
    return ordered[index]


def request_bytes(payload: dict[str, Any]) -> list[int]:
    encoded = json.dumps(payload, separators=(",", ":")).encode() + b"\n"
    if len(encoded) > 2048:
        raise SystemExit(
            f"generated request exceeds firmware RX capacity: {len(encoded)} bytes"
        )
    return list(encoded)


class Scenario:
    def __init__(self, tick_observation_ms: int) -> None:
        self.steps: list[dict[str, Any]] = []
        self.expected_success: dict[str, bool] = {}
        self.timing_workload: dict[str, str] = {}
        self.tick_observation_ms = tick_observation_ms
        self.protocol_snapshot_request_count = 0

    def wait_ready(self) -> None:
        self.steps.append({"wait-serial": '"type":"ready"'})

    def request(self, payload: dict[str, Any], *, success: bool = True) -> str:
        request_id = payload.get("requestId")
        if not isinstance(request_id, str) or not request_id:
            raise ValueError("scenario requests require a requestId")
        if request_id in self.expected_success:
            raise ValueError(f"duplicate requestId: {request_id}")
        self.expected_success[request_id] = success
        self.steps.append({"write-serial": request_bytes(payload)})
        self.steps.append({"wait-serial": f'"requestId":"{request_id}"'})
        return request_id

    def eval(self, code: str, request_id: str, *, success: bool = True) -> None:
        self.request(
            {"type": "eval", "code": code, "requestId": request_id},
            success=success,
        )

    def timed_eval(self, code: str, request_id: str, workload: str) -> None:
        self.eval(code, request_id)
        timing_id = f"time-{request_id}"
        self.request(
            {
                "type": "debug",
                "action": "query",
                "channel": "eval",
                "output": "timing",
                "requestId": timing_id,
            }
        )
        self.timing_workload[timing_id] = workload

    def expect_pin(self, pin: str, expected: int) -> None:
        self.steps.append(
            {
                "expect-pin": {
                    "part-id": "pico",
                    "pin": pin,
                    "expected": expected,
                }
            }
        )

    def set_control(self, part: str, value: int) -> None:
        self.steps.append(
            {
                "set-control": {
                    "part-id": part,
                    "control": "pressed",
                    "value": value,
                }
            }
        )

    def delay(self, milliseconds: int) -> None:
        self.steps.append({"delay": f"{milliseconds}ms"})


def generate_scenario(cycles: int, tick_observation_ms: int) -> Scenario:
    scenario = Scenario(tick_observation_ms)
    scenario.wait_ready()
    scenario.request(
        {
            "type": "hello",
            "client": "rp2040-acceptance",
            "version": "1.2.0",
            "requestId": "hello-1",
        }
    )
    scenario.request(
        {
            "type": "debug",
            "action": "capabilities",
            "requestId": "debug-capabilities",
        }
    )

    # Digital gate inputs are active-low at the connector and the Music Thing
    # gate outputs are inverted. The scenario checks the physical pin level.
    scenario.eval("(d1 in1)", "gpio-d1-input")
    scenario.expect_pin("GP8", 1)
    scenario.set_control("gate1", 1)
    scenario.delay(5)
    scenario.expect_pin("GP8", 0)
    scenario.set_control("gate1", 0)
    scenario.delay(5)
    scenario.expect_pin("GP8", 1)

    scenario.eval("(d2 in2)", "gpio-d2-input")
    scenario.expect_pin("GP9", 1)
    scenario.set_control("gate2", 1)
    scenario.delay(5)
    scenario.expect_pin("GP9", 0)
    scenario.set_control("gate2", 0)
    scenario.delay(5)
    scenario.expect_pin("GP9", 1)

    # a3/a4 are inverted direct PWM outputs. Logical one is static low and
    # logical zero is full-scale high, making exact pin assertions possible.
    for output, pin in (("a3", "TP4"), ("a4", "GP22")):
        scenario.eval(f"({output} 1)", f"gpio-{output}-low")
        scenario.delay(2)
        scenario.expect_pin(pin, 0)
        scenario.eval(f"({output} 0)", f"gpio-{output}-high")
        scenario.delay(2)
        scenario.expect_pin(pin, 1)

    for workload in WORKLOADS:
        forms = parse_forms(ROOT / "bench/firmware-corpus" / f"{workload}.useq")
        for cycle in range(cycles):
            scenario.eval("(useq-clear)", f"clear-{workload[:4]}-{cycle:02d}")
            for index, form in enumerate(forms):
                request_id = f"eval-{workload[:4]}-{cycle:02d}-{index:02d}"
                scenario.timed_eval(form, request_id, workload)
            scenario.request(
                {
                    "type": "debug",
                    "action": "query",
                    "channel": "resources",
                    "requestId": f"resources-{workload}-{cycle:02d}",
                }
            )

    scenario.request(
        {
            "type": "debug",
            "action": "configure",
            "tick": "stream",
            "streamRateHz": TICK_STREAM_RATE_HZ,
            "requestId": "tick-stream-start",
        }
    )
    scenario.delay(tick_observation_ms)
    scenario.request(
        {
            "type": "debug",
            "action": "configure",
            "tick": "off",
            "requestId": "tick-stream-stop",
        }
    )
    scenario.request(
        {
            "type": "debug",
            "action": "query",
            "channel": "state",
            "requestId": "state-before-invalid",
        }
    )
    scenario.request(
        {
            "type": "debug",
            "action": "query",
            "channel": "resources",
            "requestId": "resources-before-invalid",
        }
    )
    scenario.eval("(a1 definitely-undefined)", "invalid-transaction", success=False)
    scenario.request(
        {
            "type": "debug",
            "action": "query",
            "channel": "state",
            "requestId": "state-after-invalid",
        }
    )
    scenario.request(
        {
            "type": "debug",
            "action": "query",
            "channel": "resources",
            "requestId": "resources-after-invalid",
        }
    )
    scenario.request(
        {
            "type": "debug",
            "action": "query",
            "channel": "eval",
            "output": "timing",
            "requestId": "eval-final",
        }
    )
    scenario.request({"type": "ping", "requestId": "recovery-ping"})
    scenario.request(
        {
            "type": "debug",
            "action": "query",
            "channel": "protocol",
            "requestId": "protocol-final",
        }
    )
    scenario.protocol_snapshot_request_count = len(scenario.expected_success)
    return scenario


def write_scenario(path: Path, scenario: Scenario) -> None:
    payload = {
        "name": "uSEQ RP2040 acceptance",
        "version": 1,
        "author": "uSEQ",
        "steps": scenario.steps,
    }
    path.write_text(json.dumps(payload, indent=2) + "\n")


def run_logged(
    name: str,
    argv: list[str],
    output_dir: Path,
    *,
    cwd: Path = ROOT,
    env: dict[str, str] | None = None,
    timeout: int | None = None,
) -> subprocess.CompletedProcess[str]:
    print(f"[rp2040-wokwi] {name}", file=sys.stderr, flush=True)
    result = subprocess.run(
        argv,
        cwd=cwd,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=timeout,
    )
    (output_dir / f"{name}.log").write_text(result.stdout)
    if result.stdout:
        print(result.stdout, end="")
    if result.returncode != 0:
        raise SystemExit(
            f"{name} failed with exit status {result.returncode}; "
            f"see {output_dir / f'{name}.log'}"
        )
    return result


def read_serial_messages(path: Path) -> list[dict[str, Any]]:
    messages: list[dict[str, Any]] = []
    for raw_line in path.read_bytes().splitlines():
        start = raw_line.find(b"{")
        if start < 0:
            continue
        try:
            value = json.loads(raw_line[start:].decode("utf-8", errors="strict"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            continue
        if isinstance(value, dict):
            messages.append(value)
    return messages


def parse_vcd_activity(path: Path) -> dict[str, list[int]]:
    identifier_names: dict[str, str] = {}
    levels: dict[str, set[int]] = {name: set() for name in GPIO_SIGNALS}
    for line in path.read_text(errors="replace").splitlines():
        fields = line.split()
        if len(fields) >= 5 and fields[0] == "$var":
            identifier = fields[3]
            raw_name = fields[4].split(".")[-1]
            name = GPIO_CHANNELS.get(raw_name, raw_name)
            identifier_names[identifier] = name
            continue
        if len(line) >= 2 and line[0] in "01xXzZ":
            identifier = line[1:].strip()
            name = identifier_names.get(identifier)
            if name in levels and line[0] in "01":
                levels[name].add(int(line[0]))
    return {name: sorted(values) for name, values in levels.items()}


def response_map(messages: list[dict[str, Any]]) -> dict[str, list[dict[str, Any]]]:
    result: dict[str, list[dict[str, Any]]] = {}
    for message in messages:
        request_id = message.get("requestId")
        if isinstance(request_id, str):
            result.setdefault(request_id, []).append(message)
    return result


def resource_ratios(data: dict[str, Any]) -> dict[str, float]:
    ratios: dict[str, float] = {}
    for name in (
        "nodes",
        "arena",
        "cells",
        "data_entries",
        "state_slots",
        "live_slots",
    ):
        value = data.get(name)
        if not isinstance(value, dict):
            continue
        used = value.get("used")
        capacity = value.get("capacity")
        if isinstance(used, int) and isinstance(capacity, int) and capacity > 0:
            ratios[name] = used / capacity
    return ratios


def evaluate(
    messages: list[dict[str, Any]],
    scenario: Scenario,
    budget: dict[str, Any],
    manifest: dict[str, Any],
    vcd_activity: dict[str, list[int]],
) -> tuple[list[dict[str, Any]], dict[str, Any]]:
    checks: list[dict[str, Any]] = []

    def record(name: str, passed: bool, actual: Any, limit: Any) -> None:
        checks.append(
            {"name": name, "pass": bool(passed), "actual": actual, "limit": limit}
        )

    responses = response_map(messages)
    for request_id, expected_success in scenario.expected_success.items():
        matching = responses.get(request_id, [])
        actual = matching[0].get("success") if len(matching) == 1 else None
        record(
            f"response:{request_id}",
            len(matching) == 1 and actual is expected_success,
            {"count": len(matching), "success": actual},
            {"count": 1, "success": expected_success},
        )

    ready_count = sum(message.get("type") == "ready" for message in messages)
    record("single-boot", ready_count == 1, ready_count, 1)

    compile_samples: dict[str, list[int]] = {name: [] for name in WORKLOADS}
    for request_id, workload in scenario.timing_workload.items():
        matching = responses.get(request_id, [])
        if len(matching) != 1:
            continue
        data = matching[0].get("data")
        if isinstance(data, dict) and isinstance(data.get("last_us"), int):
            compile_samples[workload].append(data["last_us"])

    runtime_budget = budget["runtime"]
    compile_summary: dict[str, Any] = {}
    all_compile_samples: list[int] = []
    for workload, samples in compile_samples.items():
        all_compile_samples.extend(samples)
        expected_count = sum(
            value == workload for value in scenario.timing_workload.values()
        )
        p99 = percentile(samples, 0.99) if samples else None
        maximum = max(samples) if samples else None
        compile_summary[workload] = {
            "samples": len(samples),
            "p99_us": p99,
            "max_us": maximum,
        }
        record(
            f"compile-samples:{workload}",
            len(samples) == expected_count,
            len(samples),
            expected_count,
        )
        p99_limit = int(runtime_budget["compile_p99_us"][workload])
        record(
            f"compile-p99:{workload}",
            p99 is not None and p99 < p99_limit,
            p99,
            f"<{p99_limit}",
        )
    compile_max = max(all_compile_samples) if all_compile_samples else None
    compile_max_limit = int(runtime_budget["max_compile_us"])
    record(
        "compile-maximum",
        compile_max is not None and compile_max < compile_max_limit,
        compile_max,
        f"<{compile_max_limit}",
    )

    tick_records = [
        message["data"]
        for message in messages
        if message.get("type") == "debug"
        and message.get("channel") == "tick"
        and isinstance(message.get("data"), dict)
        and isinstance(message["data"].get("last_total_us"), int)
        and isinstance(message["data"].get("tick_count"), int)
    ]
    tick_samples = [int(data["last_total_us"]) for data in tick_records]
    tick_counts = [int(data["tick_count"]) for data in tick_records]
    tick_p99 = percentile(tick_samples, 0.99) if tick_samples else None
    minimum_tick_samples = int(runtime_budget["min_tick_samples"])
    maximum_tick_p99 = int(runtime_budget["max_tick_p99_us"])
    minimum_observation_ms = int(
        runtime_budget["min_sustained_observation_ms"]
    )
    record(
        "sustained-observation-duration",
        scenario.tick_observation_ms >= minimum_observation_ms,
        scenario.tick_observation_ms,
        f">={minimum_observation_ms}",
    )
    record(
        "tick-samples",
        len(tick_samples) >= minimum_tick_samples,
        len(tick_samples),
        f">={minimum_tick_samples}",
    )
    record(
        "tick-p99",
        tick_p99 is not None and tick_p99 < maximum_tick_p99,
        tick_p99,
        f"<{maximum_tick_p99}",
    )
    tick_counts_monotonic = bool(tick_counts) and all(
        later > earlier for earlier, later in zip(tick_counts, tick_counts[1:])
    )
    tick_count_delta = (
        tick_counts[-1] - tick_counts[0] if len(tick_counts) >= 2 else None
    )
    tick_rate_hz = (
        tick_count_delta * 1000.0 / scenario.tick_observation_ms
        if tick_count_delta is not None
        else None
    )
    minimum_tick_rate_hz = int(runtime_budget["min_tick_rate_hz"])
    record(
        "tick-counter-monotonic",
        tick_counts_monotonic,
        tick_counts[-2:] if tick_counts else [],
        "strictly increasing",
    )
    record(
        "tick-throughput",
        tick_rate_hz is not None and tick_rate_hz >= minimum_tick_rate_hz,
        tick_rate_hz,
        f">={minimum_tick_rate_hz}",
    )

    resource_messages: dict[str, dict[str, Any]] = {}
    for request_id, matching in responses.items():
        if not request_id.startswith("resources-") or len(matching) != 1:
            continue
        data = matching[0].get("data")
        if isinstance(data, dict):
            resource_messages[request_id] = data
    resource_keys = (
        "nodes",
        "arena",
        "cells",
        "data_entries",
        "state_slots",
        "live_slots",
    )
    resource_high_water: dict[str, dict[str, int]] = {}
    for resource in resource_keys:
        samples = [
            data[resource]
            for data in resource_messages.values()
            if isinstance(data.get(resource), dict)
        ]
        used_values = [
            value.get("used") for value in samples
            if isinstance(value.get("used"), int)
        ]
        capacities = {
            value.get("capacity") for value in samples
            if isinstance(value.get("capacity"), int)
        }
        capacity = next(iter(capacities)) if len(capacities) == 1 else None
        used = max(used_values) if used_values else None
        record(
            f"resource-capacity-consistent:{resource}",
            capacity is not None and len(samples) == len(resource_messages),
            sorted(capacities),
            "one positive capacity in every resource sample",
        )
        record(
            f"resource-bounds:{resource}",
            used is not None and capacity is not None
            and capacity > 0 and all(0 <= value <= capacity for value in used_values),
            {"used_high_water": used, "capacity": capacity},
            "0 <= every used value <= capacity",
        )
        if used is not None and capacity is not None:
            resource_high_water[resource] = {
                "used": used,
                "capacity": capacity,
            }
    heap_values = [
        int(data["heap_min_free"])
        for data in resource_messages.values()
        if isinstance(data.get("heap_min_free"), int)
    ]
    min_heap = min(heap_values) if heap_values else None
    min_heap_limit = int(runtime_budget["min_heap_free_bytes"])
    record(
        "heap-low-water",
        min_heap is not None and min_heap >= min_heap_limit,
        min_heap,
        f">={min_heap_limit}",
    )

    final_resources = resource_messages.get("resources-before-invalid", {})
    stack = final_resources.get("core0_stack")
    stack_remaining = None
    stack_initialized = None
    stack_capacity = None
    if isinstance(stack, dict):
        used = stack.get("used")
        capacity = stack.get("capacity")
        stack_initialized = stack.get("initialized")
        if isinstance(used, int) and isinstance(capacity, int):
            stack_remaining = capacity - used
            stack_capacity = capacity
    min_stack = int(runtime_budget["min_core0_stack_remaining_bytes"])
    expected_stack_capacity = int(runtime_budget["core0_stack_capacity_bytes"])
    margin_intact = final_resources.get("core0_stack_margin_intact")
    record(
        "stack-watermark-initialized",
        stack_initialized is True,
        stack_initialized,
        True,
    )
    record(
        "stack-capacity",
        stack_capacity == expected_stack_capacity,
        stack_capacity,
        expected_stack_capacity,
    )
    record("stack-margin-intact", margin_intact is True, margin_intact, True)
    record(
        "stack-remaining",
        stack_remaining is not None and stack_remaining >= min_stack,
        stack_remaining,
        f">={min_stack}",
    )

    watchdog_values = [
        data.get("watchdog_reboot") for data in resource_messages.values()
    ]
    record(
        "watchdog-reboot",
        bool(watchdog_values) and all(value == 0 for value in watchdog_values),
        watchdog_values,
        "all zero",
    )
    cpu_hz_values = [data.get("cpu_hz") for data in resource_messages.values()]
    expected_cpu_hz = int(runtime_budget["expected_cpu_hz"])
    record(
        "target-clock",
        bool(cpu_hz_values)
        and all(value == expected_cpu_hz for value in cpu_hz_values),
        cpu_hz_values,
        f"all {expected_cpu_hz}",
    )

    for workload in WORKLOADS:
        resource_ids = sorted(
            request_id
            for request_id in resource_messages
            if request_id.startswith(f"resources-{workload}-")
        )
        data = resource_messages.get(resource_ids[-1], {}) if resource_ids else {}
        ratios = resource_ratios(data)
        requirements = manifest["workload_requirements"][workload]
        for resource, minimum in requirements.get("minimum_ratios", {}).items():
            actual = ratios.get(resource)
            record(
                f"resource-min:{workload}:{resource}",
                actual is not None and actual >= float(minimum),
                actual,
                f">={minimum}",
            )
        for resource, maximum in requirements.get("maximum_ratios", {}).items():
            actual = ratios.get(resource)
            record(
                f"resource-max:{workload}:{resource}",
                actual is not None and actual <= float(maximum),
                actual,
                f"<={maximum}",
            )

    before = resource_messages.get("resources-before-invalid", {})
    after = resource_messages.get("resources-after-invalid", {})
    retained_before = {key: before.get(key) for key in resource_keys}
    retained_after = {key: after.get(key) for key in resource_keys}
    record(
        "invalid-eval-retains-resources",
        retained_before == retained_after and bool(before),
        retained_after,
        retained_before,
    )

    state_after = responses.get("state-after-invalid", [{}])[0].get("data", {})
    health_values = []
    if isinstance(state_after, dict) and isinstance(state_after.get("outputs"), list):
        health_values = [
            output.get("health")
            for output in state_after["outputs"]
            if isinstance(output, dict)
        ]
    record(
        "runtime-output-health",
        bool(health_values) and not ({"fallback", "error"} & set(health_values)),
        sorted(set(health_values)),
        "no fallback or error output",
    )

    final_eval = responses.get("eval-final", [{}])[0].get("data", {})
    error_count = (
        final_eval.get("error_count") if isinstance(final_eval, dict) else None
    )
    record("controlled-eval-errors", error_count == 1, error_count, 1)

    protocol = responses.get("protocol-final", [{}])[0].get("data", {})
    if not isinstance(protocol, dict):
        protocol = {}
    msg_in = protocol.get("msg_in")
    msg_out = protocol.get("msg_out")
    rx_overflow = protocol.get("rx_overflow", 0)
    record(
        "protocol-input-count",
        msg_in == scenario.protocol_snapshot_request_count,
        msg_in,
        scenario.protocol_snapshot_request_count,
    )
    record(
        "protocol-output-count",
        isinstance(msg_out, int)
        and msg_out >= scenario.protocol_snapshot_request_count,
        msg_out,
        f">={scenario.protocol_snapshot_request_count}",
    )
    record("protocol-rx-overflow", rx_overflow == 0, rx_overflow, 0)

    for signal, levels in vcd_activity.items():
        record(f"vcd-activity:{signal}", levels == [0, 1], levels, [0, 1])

    observations = {
        "compile": compile_summary,
        "compile_max_us": compile_max,
        "tick": {
            "samples": len(tick_samples),
            "p99_us": tick_p99,
            "max_us": max(tick_samples) if tick_samples else None,
            "counter_delta": tick_count_delta,
            "throughput_hz": tick_rate_hz,
            "observation_ms": scenario.tick_observation_ms,
        },
        "heap_min_free_bytes": min_heap,
        "core0_stack_remaining_bytes": stack_remaining,
        "resources": resource_ratios(final_resources),
        "resource_high_water": resource_high_water,
        "protocol": protocol,
        "cpu_hz": cpu_hz_values[-1] if cpu_hz_values else None,
        "vcd_activity": vcd_activity,
        "ready_count": ready_count,
    }
    return checks, observations


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument("--budget", type=Path, default=DEFAULT_BUDGET)
    parser.add_argument("--wokwi-cli", default=os.environ.get("WOKWI_CLI", "wokwi-cli"))
    parser.add_argument("--elf", type=Path, default=DEFAULT_ELF)
    parser.add_argument("--workload-cycles", type=int, default=2)
    parser.add_argument(
        "--tick-observation-ms",
        type=int,
        default=DEFAULT_TICK_OBSERVATION_MS,
    )
    parser.add_argument("--simulation-timeout-ms", type=int, default=120000)
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument(
        "--generate-only",
        action="store_true",
        help="write and structurally validate the scenario without invoking Wokwi",
    )
    args = parser.parse_args()
    if args.workload_cycles <= 0:
        parser.error("--workload-cycles must be positive")
    if args.tick_observation_ms <= 0 or args.simulation_timeout_ms <= 0:
        parser.error("simulation durations must be positive")
    budget = json.loads(args.budget.read_text())
    runtime_budget = budget["runtime"]
    minimum_observation_ms = int(
        runtime_budget["min_sustained_observation_ms"]
    )
    if args.tick_observation_ms < minimum_observation_ms:
        parser.error(
            "--tick-observation-ms may not be shorter than the runtime budget "
            f"({minimum_observation_ms} ms)"
        )
    configured_rate = int(runtime_budget["tick_stream_rate_hz"])
    if configured_rate != TICK_STREAM_RATE_HZ:
        parser.error(
            "runtime budget tick_stream_rate_hz does not match the scenario "
            f"rate ({TICK_STREAM_RATE_HZ})"
        )

    revision = git_output("rev-parse", "--short=12", "HEAD")
    output_dir = (args.output_dir or ROOT / "build/rp2040-wokwi" / revision).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    scenario = generate_scenario(args.workload_cycles, args.tick_observation_ms)
    scenario_path = output_dir / "scenario.yaml"
    write_scenario(scenario_path, scenario)
    scenario_round_trip = json.loads(scenario_path.read_text())
    if len(scenario_round_trip.get("steps", [])) != len(scenario.steps):
        raise SystemExit("generated scenario did not round-trip")
    if args.generate_only:
        print(
            f"[rp2040-wokwi] generated {len(scenario.steps)} steps: {scenario_path}",
            file=sys.stderr,
        )
        return 0

    cli = shutil.which(args.wokwi_cli)
    if not cli:
        parser.error(
            "wokwi-cli not found; install the official CLI or pass --wokwi-cli"
        )
    if not os.environ.get("WOKWI_CLI_TOKEN"):
        parser.error("WOKWI_CLI_TOKEN is not set")

    environment = os.environ.copy()
    if not args.skip_build:
        run_logged(
            "platformio-observe",
            ["pio", "run", "-e", "musicthing-observe"],
            output_dir,
            env=environment,
        )
    elf = args.elf.resolve()
    if not elf.is_file():
        parser.error(f"observation ELF not found: {elf}")
    memory_path = output_dir / "musicthing-observe-memory.json"
    run_logged(
        "memory",
        [
            sys.executable,
            "scripts/rp2040_memory_report.py",
            "--elf",
            str(elf),
            "--label",
            "musicthing-observe",
            "--output",
            str(memory_path),
        ],
        output_dir,
        env=environment,
    )
    memory_report = json.loads(memory_path.read_text())
    if not memory_report.get("pass"):
        raise SystemExit("exact observation ELF memory gate did not pass")

    run_logged(
        "wokwi-lint",
        [cli, "lint", "--warnings-as-errors"],
        output_dir,
        cwd=WOKWI_ROOT,
        env=environment,
    )
    serial_path = output_dir / "serial.jsonl"
    vcd_path = output_dir / "gpio.vcd"
    run_logged(
        "wokwi",
        [
            cli,
            str(WOKWI_ROOT),
            "--elf",
            str(elf),
            "--scenario",
            str(scenario_path),
            "--serial-log-file",
            str(serial_path),
            "--vcd-file",
            str(vcd_path),
            "--timeout",
            str(args.simulation_timeout_ms),
            "--timeout-exit-code",
            "1",
            "--fail-text",
            "Message too long",
            "--quiet",
        ],
        output_dir,
        env=environment,
        timeout=600,
    )
    if not serial_path.is_file():
        raise SystemExit("Wokwi did not produce the serial log")
    if not vcd_path.is_file():
        raise SystemExit("Wokwi did not produce the GPIO VCD")

    messages = read_serial_messages(serial_path)
    vcd_activity = parse_vcd_activity(vcd_path)
    manifest = json.loads((ROOT / "bench/firmware-corpus/manifest.json").read_text())
    checks, observations = evaluate(messages, scenario, budget, manifest, vcd_activity)
    failures = [check for check in checks if not check["pass"]]
    version = subprocess.check_output([cli, "--version"], text=True).strip()
    summary = {
        "schema_version": 1,
        "grade": "rp2040-wokwi",
        "source": {
            "revision": git_output("rev-parse", "HEAD"),
            "dirty": bool(git_output("status", "--porcelain")),
        },
        "simulator": {
            "version": version,
            "model_boundary": "single-core RP2040 functional model",
        },
        "artifacts": {
            "elf": {"path": str(elf), "sha256": sha256(elf)},
            "serial": {"path": str(serial_path), "sha256": sha256(serial_path)},
            "vcd": {"path": str(vcd_path), "sha256": sha256(vcd_path)},
            "scenario": {"path": str(scenario_path), "sha256": sha256(scenario_path)},
        },
        "budget": budget["runtime"],
        "observations": observations,
        "checks": checks,
        "pass": not failures,
    }
    summary_path = output_dir / "summary.json"
    summary_path.write_text(json.dumps(summary, indent=2) + "\n")
    if failures:
        for failure in failures:
            print(
                f"FAIL {failure['name']}: {failure['actual']} "
                f"(limit {failure['limit']})",
                file=sys.stderr,
            )
        print(f"[rp2040-wokwi] FAIL; evidence: {summary_path}", file=sys.stderr)
        return 1
    print(f"[rp2040-wokwi] PASS; evidence: {summary_path}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
