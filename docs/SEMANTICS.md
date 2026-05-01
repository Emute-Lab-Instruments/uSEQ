# ModuLisp Semantics

> **This file has been split into a multi-file spec under
> [`specs/`](specs/MAIN.md).** Start with [`specs/MAIN.md`](specs/MAIN.md);
> it holds the frame, language-wide failure and performance contracts, the
> stable compatibility surface, cross-cutting open questions, and an index
> of feature-specific sub-specs.
>
> Mapping from the old monolithic sections to the new sub-specs:
>
> | Old section                                  | New file                                                        |
> | -------------------------------------------- | --------------------------------------------------------------- |
> | §1 Frame                                     | [`specs/MAIN.md`](specs/MAIN.md)                                |
> | §2 Two Dialects + §16 Imperative-Mode        | [`specs/dialects.md`](specs/dialects.md)                        |
> | §3 Values and Types                          | [`specs/values-types.md`](specs/values-types.md)                |
> | §4 Signal Model                              | [`specs/signal-model.md`](specs/signal-model.md)                |
> | §5 Time and Phasors                          | [`specs/time.md`](specs/time.md)                                |
> | §6 Time Warps                                | [`specs/time-warps.md`](specs/time-warps.md)                    |
> | §7 Cells and Reactivity                      | [`specs/cells.md`](specs/cells.md)                              |
> | §8 Functions                                 | [`specs/functions.md`](specs/functions.md)                      |
> | §9 Outputs                                   | [`specs/outputs.md`](specs/outputs.md)                          |
> | §10 Cross-Output Reads (`prev`)              | [`specs/prev.md`](specs/prev.md)                                |
> | §11 Hardware Inputs                          | [`specs/inputs.md`](specs/inputs.md)                            |
> | §12 Top-Level Forms                          | [`specs/top-level.md`](specs/top-level.md)                      |
> | §13 Compilation + §15 Signal-Context Reject. | [`specs/compilation.md`](specs/compilation.md)                  |
> | §14 Failure Model                            | [`specs/failure-model.md`](specs/failure-model.md)              |
> | §17 Open / Deferred                          | [`specs/MAIN.md §5`](specs/MAIN.md) + per sub-spec              |
> | §18 Cross-References                         | [`specs/MAIN.md §7`](specs/MAIN.md)                             |
>
> If you find a link pointing to this file, prefer the per-topic sub-spec
> when updating it.
