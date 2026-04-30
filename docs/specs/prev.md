# Cross-Output Reads (`prev`)

> Spec: how outputs read each other's previous-sample values, the bare-name
> sugar, batch semantics, feedback loops. Counterpart to [MAIN.md](MAIN.md).
> See [outputs.md](outputs.md) for the output model.

1.1 Output values produced in the current sample are not readable from other outputs in the same sample. To reference another output, you read its **previous sample** value.

1.2 `(prev a1)` reads the value `a1` produced one sample ago in the current sampling pass.

1.3 Bare output names in expression position are sugar for `prev`: `(a2 (* a1 0.5))` ≡ `(a2 (* (prev a1) 0.5))`.

1.4 **"Previous sample within batch"**: when the engine is rendering a window of samples (typical in WASM batch mode), `prev` reads the immediately preceding sample within that window. On firmware single-sample execution, this collapses to "previous tick".

1.5 The first sample of any batch (or first tick after a reset) reads the neutral default for any output that has never produced a value.

1.6 `prev` enables feedback loops between outputs: `(a2 (+ (prev a2) 0.01))` integrates by `0.01` per sample.

1.7 **`prev` does not introduce hidden state into the signal graph.** The "state" lives in the engine's per-output sample buffer; the graph itself remains a pure function of `(t, cells, inputs, prev_outputs)`.

1.8 `prev` is rejected in any context where the referenced output has not been declared. Self-reference (`(a1 (prev a1))`) is allowed.

## Open / Deferred

2.1 **`prev` window across batches.** The contract is "previous sample within the current batch / previous tick on firmware". The exact semantics at batch boundaries (does `prev` at the first sample of a new batch read the last sample of the previous batch, or the neutral default?) needs an explicit answer; current engines tend to carry forward, but this should be normalised. See [MAIN.md §5.9](MAIN.md).
