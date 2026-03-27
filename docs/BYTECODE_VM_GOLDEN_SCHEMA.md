# Bytecode VM Golden Fixture Schema

This repository’s first bytecode-VM harness slice uses a compact YAML schema so the
semantic fixtures can be shared across runners and future targets without tying the
format to one test framework.

## File Shape

```yaml
schema_version: 1
name: "Arithmetic and time basics"
description: "Smoke coverage for the output sampling harness"
default_bpm: 120
default_time_sig: [4, 4]
default_tolerance: 1e-9

tests:
  - name: "constant arithmetic"
    output: a1
    expr: "(+ 1 2)"
    samples:
      - t: 0.0
        expect: 3.0
```

## Supported Fields

- `schema_version`: integer schema revision, currently `1`.
- `name`: fixture suite name.
- `description`: free-form human description.
- `default_bpm`: BPM used for `beat`/`bar` derivation unless overridden.
- `default_time_sig`: `[numerator, denominator]` time signature used by the probe.
- `default_tolerance`: fallback numeric comparison tolerance.
- `tests`: array of test cases.

Each test case supports:

- `name`: case name.
- `setup`: optional list of top-level forms evaluated before the case expression.
- `output`: output slot name to assign, typically `a1`.
- `expr`: signal expression to sample.
- `samples`: array of sample checks.
- `tolerance`: per-case tolerance override.
- `bpm`: per-case BPM override.
- `time_sig`: per-case time signature override.

Each sample supports:

- `t` or `time`: sample time in seconds.
- `expect`: expected numeric value.
- `expect_error`: optional substring that must appear in the error message.

## Runner

`scripts/run_bytecode_vm_golden.py` consumes one or more fixture files and executes
them through the `bytecode_vm_probe` helper. The same script can also benchmark the
fixtures by repeating the probe calls and reporting elapsed time.

The benchmark numbers produced by the VM harness are for the public output path
used by `eval_output_at_time()` and `eval_outputs(...)` plus the direct numeric VM
path, not a legacy tree-walker baseline.
