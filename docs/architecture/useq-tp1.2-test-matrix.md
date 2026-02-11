# uSEQ TP1.2 Refactor Safety Test Matrix

- Task: `useq-tp1.2`
- Date: 2026-02-11
- Scope: lock down seams before major extraction tasks in `useq-tp1`

## Seam Coverage Matrix

| Refactor seam | Test targets | Protection intent |
| --- | --- | --- |
| Parser / Value / Environment behavior | `parser_test`, `parser_api_test`, `parser_error_reporting_test`, `value_api_test`, `environment_api_test` | Preserve parse correctness, value typing/semantics, and environment mutation/lookup behavior while splitting ModuLisp core internals. |
| Builtin argument/type validation | `error_handling_test`, `builtin_validation_matrix_test` | Preserve arity and type diagnostics for builtin calls, including table-driven checks for common failure paths. |
| Routing and protocol decoding | `json_protocol_routing_test`, `execution_semantics_test` | Preserve JSON request parsing defaults/escaping/error paths and request-to-evaluation routing behavior. |
| Output evaluation consistency | `output_sync_test`, `output_sampling_test` | Preserve synchronized output time evaluation and windowed sampling behavior during output-evaluation extraction. |

## Local and CI Test Paths

- Local full suite:
  - `meson setup build`
  - `meson test -C build`
- Local seam-focused quick run:
  - `meson test -C build parser_test value_api_test environment_api_test error_handling_test builtin_validation_matrix_test json_protocol_routing_test output_sync_test output_sampling_test`
- CI expectation:
  - These targets are part of the Meson test graph and should run under the standard `meson test -C build` CI invocation.
