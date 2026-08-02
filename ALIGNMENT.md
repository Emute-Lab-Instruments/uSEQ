# Alignment

Last full review: **2026-08-02**.

## Mission

uSEQ provides a bounded, deterministic compiler/control runtime for live-coded
signals on embedded hardware and generated WebAssembly. Its central obligation
is continuity under editing: valid changes publish coherently, invalid changes
leave the active program and bounded capacity unchanged, and each execution
profile states only the behavior it implements and measures.

## Top defects

### 1. Exact firmware-profile execution remains unobserved *(2026-08-02)*

**What.** Native and generated-WASM probes execute the shared corpus, and the
`musicthing` target compiles and links, but there is no host runner using the
exact firmware capacities and adapter path.

**Why it matters.** Desktop results and target buildability cannot establish
firmware execution equivalence or exact boundary behavior.

**Rough cost.** M: construct the profile runner, select the representable
corpus, and retain a distinct result grade. Tracked in ergo `2f440020`.

### 2. Target runtime headroom is incompletely measured *(2026-08-02)*

**What.** The current target build uses 92.3% of static RAM. Flash use is
17.0%, but no target stack watermark has been measured.

**Why it matters.** Static fit alone does not establish safe combined runtime
headroom under deep compilation, protocol activity, and firmware execution.

**Rough cost.** M: add a target stack-watermark observation and representative
combined workloads before claiming runtime headroom.

### 3. Clause-level evidence remains incomplete *(2026-08-02)*

**What.** Aggregate native and generated suites pass, while parts of the
prospective clause registry still lack a machine-readable clause-to-case-to-
artifact join. The serial adapter and physical-target subset are not present.

**Why it matters.** Aggregate suite success cannot establish every normative
clause or every target profile.

**Rough cost.** M-L: split compound clauses, finish stable case identifiers,
add the serial subset, and make missing joins fail the intended grade.

### 4. Reference-defined numerical families need frozen comparison records *(2026-08-02)*

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
