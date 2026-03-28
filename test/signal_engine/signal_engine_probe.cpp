// Signal Engine Probe
// Standalone executable that evaluates ModuLisp expressions using the new
// signal engine and outputs JSON results. Compatible with the golden test
// runner (scripts/run_bytecode_vm_golden.py).
//
// Usage:
//   signal_engine_probe --code "(+ 1 2)" --output a1 --time 0.0 --bpm 120 --time-sig 4,4
//
// Output (last line):
//   {"ok": true, "value": 3.0}
// or
//   {"ok": false, "error": "..."}

#include "src/signal_engine/signal_engine.h"
#include "src/modulisp/lisp/symbol_intern.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace sig;

int main(int argc, char* argv[]) {
    const char* code = nullptr;
    const char* output_name = "a1";
    double time_val = 0.0;
    double bpm = 120.0;
    int beats_per_bar = 4;
    const char* setup_code = nullptr;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--code") == 0 && i + 1 < argc) {
            code = argv[++i];
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output_name = argv[++i];
        } else if (strcmp(argv[i], "--time") == 0 && i + 1 < argc) {
            time_val = atof(argv[++i]);
        } else if (strcmp(argv[i], "--bpm") == 0 && i + 1 < argc) {
            bpm = atof(argv[++i]);
        } else if (strcmp(argv[i], "--time-sig") == 0 && i + 1 < argc) {
            i++;
            // Parse "4,4" or "4 4"
            beats_per_bar = atoi(argv[i]);
        } else if (strcmp(argv[i], "--setup") == 0 && i + 1 < argc) {
            setup_code = argv[++i];
        }
    }

    // Smoke test mode (no args)
    if (!code) {
        printf("{\"ok\": true, \"value\": 0.0}\n");
        return 0;
    }

    // Initialize engine
    SignalEngine engine;
    engine.init_defaults(bpm, beats_per_bar);

    // Run setup code if provided
    if (setup_code) {
        EvalResult setup_result = eval_cold(setup_code, (uint32_t)strlen(setup_code),
                                             engine);
        if (setup_result.kind == EvalResult::Error) {
            const char* msg = setup_result.diagnostic_count > 0 ?
                setup_result.diagnostics[0].message : "setup failed";
            printf("{\"ok\": false, \"error\": \"Setup error: %s\"}\n", msg ? msg : "unknown");
            return 1;
        }
    }

    // The golden test runner already wraps the expression as (a1 expr),
    // so we evaluate the code as-is.
    EvalResult result = eval_cold(code, (uint32_t)strlen(code), engine);

    if (result.kind == EvalResult::Error) {
        const char* msg = result.diagnostic_count > 0 ?
            result.diagnostics[0].message : "compilation failed";
        printf("{\"ok\": false, \"error\": \"%s\"}\n", msg ? msg : "unknown");
        return 0; // don't return error code, the golden runner checks JSON
    }

    // Now execute at the requested time
    engine.pool.rebuild_execution_order();

    uint16_t output_index = GraphBuilder::resolve_output_index(
        SymbolIntern::getInstance().intern(String(output_name)));

    if (output_index == NODE_NONE || engine.pool.outputs[output_index].root_node == NODE_NONE) {
        printf("{\"ok\": false, \"error\": \"Output %s not assigned\"}\n", output_name);
        return 0;
    }

    double cell_vals[MAX_CELLS];
    engine.cells.snapshot_values(cell_vals, MAX_CELLS);
    double hw_inputs[32] = {};
    double outputs[MAX_OUTPUTS] = {};
    double workspace[MAX_TOTAL_NODES] = {};

    ExecutionContext ctx;
    ctx.t             = time_val;
    ctx.cell_values   = cell_vals;
    ctx.hw_inputs     = hw_inputs;
    ctx.data_pool     = engine.cells.data_pool;
    ctx.data_offsets  = engine.cells.data_offsets;
    ctx.data_lengths  = engine.cells.data_lengths;
    ctx.prev_outputs  = engine.pool.prev_output_values;
    ctx.output_values = outputs;
    ctx.workspace     = workspace;
    execute_all_outputs(engine.pool, ctx);

    double value = outputs[output_index];

    printf("{\"ok\": true, \"value\": %.17g}\n", value);
    return 0;
}
