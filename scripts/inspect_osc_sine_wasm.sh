#!/usr/bin/env bash
#
# WASM binary inspection for the osc/sine NodeDef.
#
# VAL-DSP-005: the module imports host-owned shared WebAssembly.Memory and
#              defines / exports no private memory.
# VAL-DSP-013: the module imports no waiting, locking, or other blocking
#              operation; compute invokes no host function.
# VAL-DSP-015: the module is a separate build artefact from the ModuLisp
#              interpreter (its import/export table has no interpreter
#              symbols and no private memory export).
# VAL-DSP-016: the NodeDef target does not link the DSP implementation into
#              the interpreter module. This script asserts the artefact's
#              size is bounded (< 64 KB) and that it has no unexpected
#              symbols that would suggest accidental linkage.
#
# Usage: inspect_osc_sine_wasm.sh [path/to/osc_sine.wasm]
#
# Exits 0 on success, non-zero on any contract violation.

set -eu

# Resolve the WASM artefact path.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WASM="${1:-${SCRIPT_DIR}/../wasm/osc_sine.wasm}"

if [ ! -f "$WASM" ]; then
    echo "FAIL: osc_sine.wasm not found at $WASM" >&2
    echo "      Run nodedef/build_osc_sine_wasm.sh first." >&2
    exit 1
fi

if ! command -v wasm-dis >/dev/null 2>&1; then
    echo "FAIL: wasm-dis not available on PATH" >&2
    exit 1
fi

echo "Inspecting $WASM"

# Generate text-format disassembly once. Use a heredoc / here-string when
# feeding it to grep so we don't trip on `set -o pipefail` interactions with
# grep's early-exit SIGPIPE.
WAT_TEXT="$(wasm-dis "$WASM")"

fail() {
    echo "FAIL: $*" >&2
    exit 1
}

ok() {
    echo "  ok: $*"
}

# grep_in_wat: helper that returns 0 if the regex matches the WAT text,
# non-zero otherwise. Uses here-string redirection to avoid SIGPIPE under
# pipefail.
grep_in_wat() {
    grep -qE "$1" <<<"$WAT_TEXT"
}

count_in_wat() {
    # Print the count, never return non-zero (caller compares the number).
    grep -cE "$1" <<<"$WAT_TEXT" || true
}

# ── VAL-DSP-005: imported memory, no defined/exported private memory ─────────
#
# The text format must contain exactly one `(import ... memory ...)` and zero
# `(memory ...)` definitions outside of imports. The single memory import must
# come from the "env" module so the host adapter can supply its shared memory
# at instantiation.

memory_import_count=$(count_in_wat '^[[:space:]]*\(import "[^"]*" "[^"]*" \(memory')
if [ "$memory_import_count" -ne 1 ]; then
    fail "expected exactly 1 memory import, got $memory_import_count"
fi
ok "exactly one memory import"

if ! grep_in_wat '^[[:space:]]*\(import "env" "memory" \(memory'; then
    fail "memory import is not (import \"env\" \"memory\" ...)"
fi
ok 'memory imported from ("env" "memory")'

# A `(memory $name N M)` declaration OUTSIDE an import block indicates the
# module defines its own private memory. Reject any such definition. We look
# for memory definitions that are NOT imports: in the WAT syntax these appear
# at the top level as `(memory ...)`, not as part of an import.
memory_definitions=$(count_in_wat '^[[:space:]]*\(memory \$[a-zA-Z_$][0-9a-zA-Z_$]* [0-9]')
if [ "$memory_definitions" -gt 0 ]; then
    fail "module defines its own private memory ($memory_definitions declaration(s))"
fi
ok "no private memory definition"

# An exported memory is also forbidden: the host adapter must own the memory.
memory_exports=$(count_in_wat '^[[:space:]]*\(export "[^"]*" \(memory')
if [ "$memory_exports" -gt 0 ]; then
    fail "module exports a memory ($memory_exports export(s)) — host must own memory"
fi
ok "no memory export"

# ── VAL-DSP-013: no blocking or waiting imports ──────────────────────────────
#
# Every import must be a memory. The WAT text uses `(import "module" "name"
# (func ...))` for function imports and `(import "module" "name" (memory
# ...))` for memory imports. Compute must invoke no host function — so the
# function-import count must be zero. This is the binary-level evidence that
# compute cannot block on the host.
func_import_count=$(count_in_wat '^[[:space:]]*\(import "[^"]*" "[^"]*" \(func')
if [ "$func_import_count" -ne 0 ]; then
    echo "FAIL: module imports functions (compute could block):" >&2
    grep -E '^[[:space:]]*\(import "[^"]*" "[^"]*" \(func' <<<"$WAT_TEXT" >&2
    exit 1
fi
ok "zero function imports — compute cannot block on the host"

# A defensive denylist of well-known blocking/wait symbols. Even if the
# import count is zero, this catches a future regression where a different
# import shape (e.g. `(import "env" "wait" (tag ...))`) sneaks in.
if grep_in_wat '\(import "[^"]*" "(Atomics\.wait|wait|notify|futex|lock|mutex|cond)"'; then
    fail "module imports a known blocking/wait symbol"
fi
ok "no blocking/wait symbol imports"

# ── VAL-DSP-015 / VAL-DSP-016: separate artefact ─────────────────────────────
#
# The interpreter WASM (src-useq/wasm/useq.wasm) defines its own memory and
# exposes 25+ interpreter exports (useq_init, useq_eval, etc.). The NodeDef
# artefact must not overlap with that surface. We check:
#   1. The artefact is reasonably small (< 64 KB).
#   2. It does not export any useq_* or interpreter symbol.
#   3. It exposes the 20 declared osc_sine_* ABI symbols.

size_bytes=$(stat -c %s "$WASM")
if [ "$size_bytes" -ge 65536 ]; then
    fail "artefact size $size_bytes bytes >= 64 KB (likely accidental linkage with interpreter)"
fi
ok "artefact size $size_bytes bytes < 64 KB"

# Forbidden interpreter / firmware symbols.
if grep_in_wat '^[[:space:]]*\(export "(useq_|_useq_|fmod_|mod_|sin_|cos_|__stdio_)'; then
    fail "artefact exports interpreter/firmware symbols (useq_*, stdio, etc.)"
fi
ok "no interpreter / firmware / stdlib exports leaked"

# Required osc_sine ABI exports. The full list of 16 symbols.
REQUIRED_EXPORTS=(
    osc_sine_registry_json
    osc_sine_state_bytes
    osc_sine_state_align
    osc_sine_control_stride_bytes
    osc_sine_output_stride_bytes
    osc_sine_min_quantum
    osc_sine_max_quantum
    osc_sine_sample_rate_abi_version
    osc_sine_sample_rate
    osc_sine_fade_in_ms
    osc_sine_fade_out_ms
    osc_sine_validate_layout
    osc_sine_init
    osc_sine_compute
    osc_sine_compute_at_sample_rate
    osc_sine_compute_fm
    osc_sine_compute_fm_at_sample_rate
    osc_sine_get_phase
    osc_sine_get_smoothed_amp
    osc_sine_reset_phase
)
for sym in "${REQUIRED_EXPORTS[@]}"; do
    if ! grep_in_wat "^[[:space:]]*\(export \"${sym}\" \(func"; then
        fail "required export '${sym}' not found in module"
    fi
done
ok "all 20 osc_sine_* ABI exports present"

# Compute (`osc_sine_compute`) must be exported; this is what the host adapter
# calls per render quantum.
if ! grep_in_wat '^[[:space:]]*\(export "osc_sine_compute" \(func'; then
    fail "osc_sine_compute is not exported"
fi
ok "osc_sine_compute exported (host-adapter render entry)"

echo
echo "VAL-DSP-005/013/015/016 inspection PASSED for $WASM"
