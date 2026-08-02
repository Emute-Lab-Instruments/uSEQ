# RP2040 optimization and acceptance

This document defines the evidence model for reducing the uSEQ compiler and
runtime while preserving language and failure semantics. It covers the Music
Thing RP2040 profile: 264 KiB of SRAM, with 256 KiB main RAM plus two 4 KiB
scratch banks used by the linked core stacks, and a 2 MiB sketch partition in
the configured 16 MiB external flash.

## Acceptance order

Optimization proceeds through four distinct grades. A later grade supplements
rather than replaces the earlier ones.

1. **Exact link:** build the production and observation ELFs with the pinned
   PlatformIO toolchain. `rp2040_memory_report.py` reads sections and linker
   symbols rather than relying on the generic size summary.
2. **Native firmware-capacity profile:** compile the real compiler and executor
   with `USEQ_FIRMWARE_PROFILE`. Run the full 85-case conformance corpus, the
   declared capacity corpus, and optimized plus ASan/UBSan endurance binaries.
   This grade tests retained limits and recovery on the host; its timing is not
   RP2040 timing evidence.
3. **Automated simulator:** boot the exact `musicthing-observe` ELF, exercise
   the production serial protocol, drive GPIO inputs, capture output pins, and
   collect runtime heap, stack, tick, resource, and reset telemetry.
4. **Physical device:** repeat the release subset on the program card. This is
   the authority for USB behavior, actual timing, clock configuration, PIO,
   DMA, multicore behavior, electrical I/O, and allocator behavior on silicon.

The full local grades run with:

```bash
python3 scripts/run_rp2040_profile.py
```

The hosted simulator grade runs with:

```bash
python3 scripts/run_wokwi_rp2040.py
```

For an optimization candidate, run all non-physical grades through the
non-overwriting composition command:

```bash
python3 scripts/run_rp2040_goal_gate.py
```

## Current budgets

Static gates are encoded in `scripts/rp2040_budget.json`:

- at least 96 KiB of linked main-RAM heap remains after static allocation;
- the flash image occupies at most 1 MiB of the 2 MiB sketch partition.

The firmware workload manifest defines complexity as measured utilization,
not as an informal program label. The combined-high workload simultaneously
uses at least 40% of nodes and 50% each of data entries, state slots, and live
slots while retaining at least 15% node and arena headroom. Focused workloads
exercise 70–100% of individual retained capacities. Every workload must
compile without diagnostics, execute one million finite-output ticks, and
survive 200 same-session recompiles.

The initial target-runtime acceptance thresholds are:

- `heap_min_free >= 49152` bytes during the complete combined workload;
- `core0_stack_margin_intact == true` and at least 512 bytes of the linked
  2048-byte core-0 stack reserve remain outside the observed high-water mark;
- no allocation failure, protocol overflow affecting required responses,
  non-finite output, watchdog reboot, or unexpected reset;
- combined-high tick p99 below 1 ms, consistent with the current approximately
  1 kHz control-rate contract;
- every individual compilation below the 200 ms watchdog interval, with
  moderate-workload compile p99 below 10 ms and combined/high compile p99
  below 100 ms.

These are safety and responsiveness gates, not measured claims. They become
claims only when the observation image records them. A threshold may be made
stricter from evidence; weakening one requires an explicit change to this
document and its executable budget.

## Simulator boundary

Wokwi is the intended automated peripheral lane because its CLI can boot an
ELF, expose serial through RFC2217, run input scenarios, and record GPIO as
VCD. The test runner should use the normal wire protocol for program eval,
live inputs, and devtools queries. GPIO results should be observed externally
through connected components or VCD rather than inferred from firmware state.

Wokwi is not the memory authority. Capacity is enforced by the exact linker
and measured by firmware telemetry. Its RP2040 model currently executes one
core and does not represent every USB, PIO, DMA, flash, or timing property.
The Wokwi lane is therefore bounded to serial/GPIO functional checks and
repeatable workload automation; endurance and target timing remain mandatory
on a physical card before release-level hardware claims.

The tracked `wokwi/diagram.json` models the two active-low gate inputs and
captures direct outputs `d1`, `d2`, `a4`, and `a3` with an external logic
analyzer. The generated scenario uses the production JSONL protocol to run two
cycles each of the moderate and combined-high corpora, polls target-side
compiler durations, samples streamed tick durations, verifies every required
response, exercises one controlled invalid replacement, checks retained
resources and output health, and validates both scenario pin assertions and
VCD transitions.

The CLI requires a token and consumes hosted simulation quota. On the VPS,
install the official CLI in the account PATH and load `WOKWI_CLI_TOKEN` from
the server secret environment; never place it in the repository, shell
history, logs, or evidence. The sustained optimization loop runs native and
exact-link grades before Wokwi for every candidate and schedules the physical
subset only for retained candidates. Candidate comparisons use the same
compiler toolchain and Wokwi CLI version.

## Goal-loop contract

The goal loop optimizes one named resource cause per candidate. It may change
representation, layout, algorithms, and target-specific lowering, but it may
not reduce a public language or workload capacity, weaken a conformance case,
remove a failure/recovery assertion, or raise a budget merely to admit a
candidate. Each candidate is retained only when the complete goal gate passes
and its before/after evidence improves a named flash, static-RAM, runtime-
headroom, compilation, or tick metric without a material regression elsewhere.

The loop stops when the production image and observation image satisfy their
static gates, the moderate and combined-high target workloads satisfy every
runtime gate, and further changes do not yield a reproducible improvement.
This stopping condition is a comfortable-fit criterion, not merely successful
linking. Physical-device evidence remains required before timing or endurance
is attributed to the program card itself.

## Optimization discipline

- Preserve the conformance corpus and transactional rejection behavior.
- Change one named resource cause at a time and retain before/after ELF and
  workload JSON.
- Do not infer target timing from x86 measurements or memory headroom from a
  simulator process.
- Do not reduce a public capacity unless the implemented capability contract
  proves the removed combinations unreachable or the specification is changed
  explicitly.
- A failed workload must leave the previous program executable and all bounded
  resources reusable; accepting a diagnostic alone is insufficient.
