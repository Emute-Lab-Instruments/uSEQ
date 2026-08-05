# Alignment

Last full review: **2026-08-02**.

## Mission

uSEQ provides a bounded, deterministic compiler/control runtime for live-coded
signals on embedded hardware and generated WebAssembly. Its central obligation
is continuity under editing: valid changes publish coherently, invalid changes
leave the active program and bounded capacity unchanged, and each execution
profile states only the behavior it implements and measures.

## Top defects

### 1. Target runtime headroom is incompletely measured *(2026-08-05)*

**What.** The local pinned-toolchain `musicthing` build after removing the
host/WASM synth compiler from the firmware profile records 101,076 bytes of
PlatformIO static RAM, a 160,872-byte linked main-RAM heap span, and a
334,140-byte flash image. The exact ELF contains no synth objects, symbols, or
text. Firmware-profile and generated-WASM conformance each pass 85/85, and
optimized/sanitized 2,000-cycle endurance runs share checksum
`3efedf962013216e`. A Pico physical smoke verified flash, `hello`, `ping`,
synth-form rejection, and recovery, but did not record heap low-water or stack
watermark under the declared workloads. The Wokwi adapter enforces target
telemetry, protocol, sustained-tick, and configured-clock contracts; no hosted
simulator run has yet exercised this revision.

**Why it matters.** Static fit alone does not establish safe combined runtime
headroom under deep compilation, protocol activity, and firmware execution.

**Rough cost.** S-M: provide the hosted-simulator token and run the composed
candidate gate, then run the instrumented physical workload subset before
claiming runtime headroom on silicon.

### 2. Clause-level evidence remains incomplete *(2026-08-02)*

**What.** Aggregate native and generated suites pass, while parts of the
prospective clause registry still lack a machine-readable clause-to-case-to-
artifact join. The capacity/timing serial adapter is present; a full serial
conformance adapter and the physical-target subset are not.

**Why it matters.** Aggregate suite success cannot establish every normative
clause or every target profile.

**Rough cost.** M-L: split compound clauses, finish stable case identifiers,
add the serial subset, and make missing joins fail the intended grade.

### 3. Reference-defined numerical families need frozen comparison records *(2026-08-02)*

**What.** Complex rhythm, random, ratio, and remaining stateful families are
explicitly reference-defined, and transcendental equivalence is tolerance-
based. Their frozen source/corpus identities and per-operation tolerance
records are not yet complete.

**Why it matters.** These functions remain usable, but cannot support a claim
of independent reimplementation or universal cross-library equality.

**Rough cost.** M: publish the reference-set digests, trace corpus, and
operator-specific tolerance table.

## Open mission questions

- Which common-capability cases must run on a physical device before a release
  may describe hardware and generated WASM as behaviorally interchangeable?
- Should exact firmware-profile conformance run on every release or only when
  compiler limits, adapters, or target toolchains change?

## Deferred / accepted debt

- Browser Worker/AudioWorklet scheduling belongs to the `useq-perform`
  application profile rather than this compiler repository.
- Generated manifests are intentionally unsigned until a concrete publisher-
  authentication requirement and trust anchor exist.
- Simultaneous realization of every individual capacity maximum is not a
  supported theorem; only declared compatible combinations may be claimed.
