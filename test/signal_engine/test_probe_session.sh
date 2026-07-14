#!/bin/sh
# Smoke test for the signal_engine_probe JSONL session mode (Layer 0 of
# docs/testing/conformance-and-bench-design.md).
#
# Usage: test_probe_session.sh <path-to-signal_engine_probe>
set -eu

PROBE="${1:?usage: $0 <probe-binary>}"

OUT=$("$PROBE" --session <<'EOF'
{"op":"eval","code":"(a1 (+ 1 2))"}
{"op":"sample","output":"a1","times":[0,0.5,1.0]}
{"op":"tick","t":0.5}
{"op":"health","output":"a1"}
{"op":"eval","code":"(nonexistent-fn 1)"}
{"op":"config","opt_level":0}
{"op":"clear"}
{"op":"sample","output":"a1","times":[0]}
{"op":"bogus"}
EOF
)

echo "$OUT"

fail() { echo "FAIL: $1" >&2; exit 1; }

# One response line per request
NLINES=$(printf '%s\n' "$OUT" | wc -l)
[ "$NLINES" -eq 9 ] || fail "expected 9 response lines, got $NLINES"

line() { printf '%s\n' "$OUT" | sed -n "${1}p"; }

case "$(line 1)" in *'"ok":true'*) ;; *) fail "eval should succeed";; esac
case "$(line 2)" in *'"ok":true'*'"values":[3,3,3]'*) ;; *) fail "sample should return [3,3,3]";; esac
case "$(line 3)" in *'"ok":true'*) ;; *) fail "tick should succeed";; esac
case "$(line 4)" in *'"health":"running"'*) ;; *) fail "health should be running";; esac
case "$(line 5)" in *'"ok":false'*'"category":'*'"span":['*) ;; *) fail "bad eval should return diagnostics with category+span";; esac
case "$(line 6)" in *'"ok":false'*'"error":"unsupported"'*) ;; *) fail "config opt_level=0 should be unsupported";; esac
case "$(line 7)" in *'"ok":true'*) ;; *) fail "clear should succeed";; esac
case "$(line 8)" in *'"ok":false'*) ;; *) fail "sample after clear should report unassigned output";; esac
case "$(line 9)" in *'"ok":false'*) ;; *) fail "unknown op should error";; esac

# One-shot CLI mode still works (golden runner compatibility)
ONESHOT=$("$PROBE" --code "(a1 (+ 1 2))" --output a1 --time 0.0)
case "$ONESHOT" in *'"ok": true'*'"value": 3'*) ;; *) fail "one-shot mode broken: $ONESHOT";; esac

echo "PASS"
