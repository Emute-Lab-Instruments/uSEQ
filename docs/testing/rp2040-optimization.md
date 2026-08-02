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

The CLI requires a token and consumes hosted simulation quota. The sustained
optimization loop should run native and exact-link grades on every candidate,
use short Wokwi runs only after those pass, and schedule the physical subset
for retained candidates.

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
