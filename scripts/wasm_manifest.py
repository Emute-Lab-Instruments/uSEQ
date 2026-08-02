#!/usr/bin/env python3
"""Generate and verify the deterministic uSEQ WASM capability manifest."""

from __future__ import annotations

import argparse
import ast
import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parent.parent
PROFILE_PATH = ROOT / "scripts" / "wasm_build_profile.json"
DEFAULT_MANIFEST = ROOT / "wasm" / "useq-capabilities.json"
JS_PATH = ROOT / "wasm" / "useq.js"
WASM_PATH = ROOT / "wasm" / "useq.wasm"


class ManifestError(RuntimeError):
    pass


def load_profile() -> dict[str, Any]:
    try:
        profile = json.loads(PROFILE_PATH.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ManifestError(f"cannot read build profile: {exc}") from exc
    if profile.get("schema") != "useq.wasm-build-profile/v1":
        raise ManifestError("unsupported or missing WASM build-profile schema")
    return profile


def run_text(command: list[str]) -> str:
    try:
        result = subprocess.run(
            command, cwd=ROOT, check=True, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        )
    except (OSError, subprocess.CalledProcessError) as exc:
        raise ManifestError(f"command failed: {' '.join(command)}: {exc}") from exc
    return result.stdout.rstrip()


def first_version_line(program: str) -> str | None:
    path = shutil.which(program)
    if path is None:
        return None
    text = run_text([path, "--version"])
    return text.splitlines()[0] if text else ""


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def source_inputs(profile: dict[str, Any]) -> list[Path]:
    paths = {ROOT / str(path) for path in profile["sources"]}
    paths.update({
        ROOT / "scripts" / "build_wasm.sh",
        ROOT / "scripts" / "wasm_init_smoke.mjs",
        ROOT / "scripts" / "wasm_manifest.py",
        PROFILE_PATH,
        ROOT / "wasm" / "emscripten-post.js",
    })
    for subtree in (ROOT / "uSEQ" / "src" / "signal_engine",
                    ROOT / "uSEQ" / "src" / "utils",
                    ROOT / "uSEQ" / "src" / "modulisp"):
        paths.update(path for path in subtree.rglob("*") if path.is_file())
    missing = [path for path in paths if not path.is_file()]
    if missing:
        raise ManifestError(
            "missing manifest input(s): " +
            ", ".join(str(path.relative_to(ROOT)) for path in missing)
        )
    return sorted(paths, key=lambda path: path.relative_to(ROOT).as_posix())


def source_tree_digest(profile: dict[str, Any]) -> str:
    digest = hashlib.sha256()
    for path in source_inputs(profile):
        relative = path.relative_to(ROOT).as_posix().encode("utf-8")
        digest.update(len(relative).to_bytes(4, "little"))
        digest.update(relative)
        contents = path.read_bytes()
        digest.update(len(contents).to_bytes(8, "little"))
        digest.update(contents)
    return digest.hexdigest()


def git_source_state(profile: dict[str, Any]) -> dict[str, Any]:
    commit = run_text(["git", "rev-parse", "HEAD"])
    status = run_text([
        "git", "status", "--porcelain=v1", "--untracked-files=all",
    ])
    dirty_entries = status.splitlines() if status else []
    return {
        "git_commit": commit,
        "git_dirty": bool(dirty_entries),
        "git_dirty_entries": dirty_entries,
        "input_tree_sha256": source_tree_digest(profile),
    }


def safe_cpp_expr(expression: str, values: dict[str, int]) -> int:
    expression = re.sub(r"\((?:u?int(?:8|16|32|64)_t|size_t)\)", "", expression)
    expression = re.sub(r"(?<=\d)[uUlL]+\b", "", expression)
    tree = ast.parse(expression.strip(), mode="eval")

    def evaluate(node: ast.AST) -> int:
        if isinstance(node, ast.Expression):
            return evaluate(node.body)
        if isinstance(node, ast.Constant) and isinstance(node.value, int):
            return node.value
        if isinstance(node, ast.Name) and node.id in values:
            return values[node.id]
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.USub):
            return -evaluate(node.operand)
        if isinstance(node, ast.BinOp):
            left = evaluate(node.left)
            right = evaluate(node.right)
            if isinstance(node.op, ast.Add):
                return left + right
            if isinstance(node.op, ast.Sub):
                return left - right
            if isinstance(node.op, ast.Mult):
                return left * right
            if isinstance(node.op, ast.FloorDiv):
                return left // right
        raise ManifestError(f"unsupported C++ constant expression: {expression}")

    return evaluate(tree)


def cpp_limits() -> dict[str, int]:
    files = [
        ROOT / "uSEQ" / "src" / "signal_engine" / "types.h",
        ROOT / "uSEQ" / "src" / "signal_engine" / "graph_builder.h",
        ROOT / "uSEQ" / "src" / "signal_engine" / "node_pool.h",
        ROOT / "uSEQ" / "src" / "signal_engine" / "cold_eval.cpp",
        ROOT / "uSEQ" / "src" / "signal_engine" / "synth_registry.h",
        ROOT / "uSEQ" / "src" / "signal_engine" / "synth_graph.h",
        ROOT / "uSEQ" / "src" / "firmware" / "hardware_io.h",
        ROOT / "wasm" / "wasm_wrapper.cpp",
    ]
    wanted = {
        "MAX_CELLS", "MAX_TOTAL_NODES", "MAX_DATA_ENTRIES",
        "MAX_DATA_TABLES", "MAX_LIVE_SLOTS", "MAX_LIVE_SLOT_OPTIONS",
        "SOURCE_ARENA_SIZE", "CSE_TABLE_SIZE", "MAX_OUTPUT_DEPS",
        "MAX_STATE_SLOTS", "MAX_OUTPUTS", "MAX_TOKENS",
        "MAX_CALLABLE_PARAMS", "MAX_SCOPE_DEPTH", "MAX_LOCAL_BINDINGS",
        "MAX_DIAGNOSTICS", "MAX_INLINE_DEPTH", "MAX_COMPILE_DEPTH",
        "MAX_LIVE_SLOT_ID", "MAX_LIVE_SLOT_OPTION_LEN",
        "MAX_IDS_PER_BUILD", "MAX_EXTERNAL_ROOTS", "MAX_DEFS_BINDINGS",
        "MAX_HW_INPUTS", "MAX_PROBE_SLOTS",
        "MAX_NODEDEF_PARAMS", "MAX_NODEDEF_AUDIO_INPUTS", "SYNTH_MAX_NODES",
        "MAX_NODEDEF_ENTRIES", "MAX_NODEDEF_NAME", "MAX_SYNTH_IDENTITY",
        "MAX_SYNTH_DECLARATIONS", "MAX_SYNTH_CONTROLS",
        "MAX_SYNTH_CONNECTIONS", "MAX_SYNTH_CONTROL_ROOTS",
        "MAX_SYNTH_NESTING",
        "SYNTH_ARTIFACT_JSON_CAP", "SYNTH_ARTIFACT_ABI_VERSION",
    }
    expressions: dict[str, str] = {}
    pattern = re.compile(
        r"(?:static\s+)?constexpr\s+(?:int|size_t|u?int(?:8|16|32|64)_t)\s+"
        r"([A-Z][A-Z0-9_]*)\s*=\s*([^;]+);"
    )
    for path in files:
        text = path.read_text(encoding="utf-8")
        for name, expression in pattern.findall(text):
            if name in wanted:
                # The later types.h duplicate is its non-ARDUINO/WASM value.
                expressions[name] = expression

    values: dict[str, int] = {}
    unresolved = dict(expressions)
    while unresolved:
        progress = False
        for name, expression in list(unresolved.items()):
            try:
                values[name] = safe_cpp_expr(expression, values)
            except (ManifestError, KeyError):
                continue
            del unresolved[name]
            progress = True
        if not progress:
            raise ManifestError(
                "cannot resolve compiler limit(s): " + ", ".join(sorted(unresolved))
            )
    missing = wanted - values.keys()
    if missing:
        raise ManifestError("missing compiler limit(s): " + ", ".join(sorted(missing)))
    limits = {
        name.lower(): values[name]
        for name in sorted(wanted)
        if name != "MAX_SYNTH_CONTROL_ROOTS"
    }
    # Token spans are uint16_t and the accepted source length is checked
    # against UINT16_MAX before tokenization. Public CV/output names are the
    # 24 a/d/s channels; MAX_OUTPUTS also includes internal execution slots.
    limits["max_source_bytes"] = (1 << 16) - 1
    limits["public_output_names"] = 24
    return limits


def cross_field_constraints(limits: dict[str, int]) -> list[dict[str, Any]]:
    constraints = [
        {
            "id": "synth-controls-by-declarations-and-params",
            "relation": "max_synth_controls == max_synth_declarations * max_nodedef_params",
            "value": limits["max_synth_declarations"] * limits["max_nodedef_params"],
        },
        {
            "id": "synth-connections-by-declarations",
            "relation": "max_synth_connections == max_synth_declarations",
            "value": limits["max_synth_declarations"],
        },
        {
            "id": "cse-capacity-by-node-profile",
            "relation": "cse_table_size == 2 * max_total_nodes",
            "value": 2 * limits["max_total_nodes"],
        },
        {
            "id": "public-output-subset",
            "relation": "public_output_names <= max_outputs",
            "value": limits["public_output_names"],
        },
        {
            "id": "reactive-diagnostic-subject-capacity",
            "relation": "max_reactive_diagnostic_subjects == max_outputs + max_state_slots + max_synth_controls",
            "value": (limits["max_outputs"] + limits["max_state_slots"] +
                      limits["max_synth_controls"]),
        },
    ]
    for constraint in constraints[:3]:
        field = {
            "synth-controls-by-declarations-and-params": "max_synth_controls",
            "synth-connections-by-declarations": "max_synth_connections",
            "cse-capacity-by-node-profile": "cse_table_size",
        }[constraint["id"]]
        if limits[field] != constraint["value"]:
            raise ManifestError(
                f"cross-field constraint {constraint['id']} is false: "
                f"{limits[field]} != {constraint['value']}"
            )
    return constraints


def public_js_exports(js_text: str) -> list[str]:
    names = re.findall(
        r'Module\["_([A-Za-z][A-Za-z0-9_]*)"\]=wasmExports\[', js_text
    )
    return sorted(set(names))


def validate_artifacts(profile: dict[str, Any]) -> None:
    for path in (JS_PATH, WASM_PATH):
        if not path.is_file():
            raise ManifestError(f"missing generated artifact: {path.relative_to(ROOT)}")
        if path.stat().st_size == 0:
            raise ManifestError(f"empty generated artifact: {path.relative_to(ROOT)}")
    with WASM_PATH.open("rb") as wasm_file:
        if wasm_file.read(8) != b"\x00asm\x01\x00\x00\x00":
            raise ManifestError("useq.wasm is not a WebAssembly v1 module")
    if WASM_PATH.stat().st_size > int(profile["max_wasm_bytes"]):
        raise ManifestError(
            f"useq.wasm is {WASM_PATH.stat().st_size} bytes; "
            f"limit is {profile['max_wasm_bytes']}"
        )
    expected = sorted(profile["public_function_exports"])
    observed = public_js_exports(JS_PATH.read_text(encoding="utf-8"))
    if observed != expected:
        missing = sorted(set(expected) - set(observed))
        unexpected = sorted(set(observed) - set(expected))
        raise ManifestError(
            f"generated JS export binding mismatch; missing={missing}, "
            f"unexpected={unexpected}"
        )


def artifact_record(path: Path) -> dict[str, Any]:
    return {"bytes": path.stat().st_size, "sha256": sha256_file(path)}


def build_manifest(postprocess: str) -> dict[str, Any]:
    profile = load_profile()
    validate_artifacts(profile)
    limits = cpp_limits()
    emcc_version = first_version_line("emcc")
    if emcc_version is None:
        raise ManifestError("emcc is required to attest the compiler build")
    wasm_opt_version = first_version_line("wasm-opt")
    if postprocess == "wasm-opt" and wasm_opt_version is None:
        raise ManifestError("build claims wasm-opt post-processing but wasm-opt is absent")
    return {
        "schema": "useq.compiler-capabilities/v1",
        "profile_ownership": {
            "profile": "compiler-interpreter",
            "owns": [
                "source_identity",
                "compiler_build_profile",
                "compiler_capabilities",
                "interpreter_exports",
                "interpreter_artifacts",
                "interpreter_smoke_gates",
            ],
            "field_dispositions": {
                "node_def_descriptors": {
                    "status": "not-applicable",
                    "owner_profile": "application-served-bundle",
                },
                "sample_rate_and_render_quantum": {
                    "status": "not-applicable",
                    "owner_profile": "application-served-bundle",
                },
                "audio_activation_and_worklet": {
                    "status": "not-applicable",
                    "owner_profile": "application-served-bundle",
                },
                "target_firmware_artifacts_and_resources": {
                    "status": "not-applicable",
                    "owner_profile": "firmware-build",
                },
            },
        },
        "source": git_source_state(profile),
        "build": {
            "profile": profile["profile"],
            "compile_flags": profile["compile_flags"],
            "emcc_version": emcc_version,
            "wasm_opt_version": wasm_opt_version,
            "postprocess": postprocess,
            "stack_bytes": profile["stack_bytes"],
            "memory_growth": profile["memory_growth"],
            "environment": profile["environment"],
            "modularize": profile["modularize"],
            "module_factory": profile["module_factory"],
        },
        "capabilities": {
            "hard_limits": limits,
            "cross_field_constraints": cross_field_constraints(limits),
            "counter_domains": {
                "cell_store_revision": {
                    "storage": "uint32",
                    "maximum": (1 << 32) - 1,
                    "exhaustion_policy": "session must end before aliasing",
                },
                "session_generation": {
                    "storage": "uint32",
                    "maximum": (1 << 32) - 1,
                    "exhaustion_policy": "session must end before aliasing",
                },
                "synth_revision": {
                    "storage": "uint32",
                    "maximum": (1 << 32) - 1,
                    "exhaustion_policy": "session must end before aliasing",
                },
                "wasm_cache_revision": {
                    "storage": "uint32",
                    "maximum": (1 << 32) - 1,
                    "exhaustion_policy": "session must end before aliasing",
                },
            },
            "public_function_exports": profile["public_function_exports"],
            "runtime_methods": profile["runtime_methods"],
        },
        "artifacts": {
            "wasm/useq.js": artifact_record(JS_PATH),
            "wasm/useq.wasm": artifact_record(WASM_PATH),
        },
        "gates": {
            "max_wasm_bytes": profile["max_wasm_bytes"],
            "init_and_synth_smoke_required": True,
        },
    }


def canonical_json(value: dict[str, Any]) -> str:
    return json.dumps(value, indent=2, sort_keys=True) + "\n"


def command_profile_lines(field: str) -> int:
    profile = load_profile()
    value = profile.get(field)
    if not isinstance(value, list) or not all(isinstance(item, str) for item in value):
        raise ManifestError(f"profile field {field!r} is not a string list")
    for item in value:
        print(item)
    return 0


def command_emcc_exports() -> int:
    profile = load_profile()
    print(json.dumps(["_" + name for name in profile["public_function_exports"]],
                     separators=(",", ":")))
    return 0


def command_size_limit() -> int:
    print(int(load_profile()["max_wasm_bytes"]))
    return 0


def command_profile_value(field: str) -> int:
    profile = load_profile()
    value = profile.get(field)
    if not isinstance(value, (str, int, bool)):
        raise ManifestError(f"profile field {field!r} is not a scalar")
    if isinstance(value, bool):
        print("1" if value else "0")
    else:
        print(value)
    return 0


def command_generate(path: Path, postprocess: str) -> int:
    manifest = build_manifest(postprocess)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(canonical_json(manifest), encoding="utf-8")
    temporary.replace(path)
    print(f"generated {path.relative_to(ROOT)}")
    return 0


def command_verify(path: Path) -> int:
    try:
        raw = path.read_text(encoding="utf-8")
        observed = json.loads(raw)
    except (OSError, json.JSONDecodeError) as exc:
        raise ManifestError(f"cannot read capability manifest: {exc}") from exc
    postprocess = observed.get("build", {}).get("postprocess")
    if postprocess not in {"none", "wasm-opt"}:
        raise ManifestError("manifest has an invalid postprocess value")
    expected = build_manifest(postprocess)
    if observed != expected:
        raise ManifestError(
            "capability manifest does not match source, toolchain, profile, "
            "exports, limits, or artifact digests; rebuild it"
        )
    if raw != canonical_json(observed):
        raise ManifestError("capability manifest is not canonical JSON")
    print(f"verified {path.relative_to(ROOT)}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    lines = subparsers.add_parser("profile-lines")
    lines.add_argument("field", choices=["sources", "compile_flags"])
    value = subparsers.add_parser("profile-value")
    value.add_argument("field", choices=["stack_bytes"])
    subparsers.add_parser("emcc-exports")
    subparsers.add_parser("size-limit")
    generate = subparsers.add_parser("generate")
    generate.add_argument("--output", type=Path, default=DEFAULT_MANIFEST)
    generate.add_argument("--postprocess", choices=["none", "wasm-opt"], required=True)
    verify = subparsers.add_parser("verify")
    verify.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    args = parser.parse_args()

    if args.command == "profile-lines":
        return command_profile_lines(args.field)
    if args.command == "emcc-exports":
        return command_emcc_exports()
    if args.command == "size-limit":
        return command_size_limit()
    if args.command == "profile-value":
        return command_profile_value(args.field)
    if args.command == "generate":
        return command_generate(args.output.resolve(), args.postprocess)
    if args.command == "verify":
        return command_verify(args.manifest.resolve())
    raise ManifestError("unknown command")


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except ManifestError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
