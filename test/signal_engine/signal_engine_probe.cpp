// Signal Engine Probe
// Standalone executable that evaluates ModuLisp expressions using the new
// signal engine and outputs JSON results.
//
// Two modes:
//
// 1. One-shot CLI mode (golden-runner compatible, scripts/run_bytecode_vm_golden.py):
//      signal_engine_probe --code "(+ 1 2)" --output a1 --time 0.0 --bpm 120 --time-sig 4,4
//    Output (last line):
//      {"ok": true, "value": 3.0}
//    or
//      {"ok": false, "error": "..."}
//
// 2. Session mode (Layer 0 of docs/testing/conformance-and-bench-design.md):
//      signal_engine_probe --session
//    JSONL request/response over stdin/stdout. One JSON object per line in,
//    one JSON object per line out. Ops:
//      {"op":"eval",   "code":"(define x 5)"}
//        -> {"ok":true} | {"ok":false,"diagnostics":[{"severity":"error",
//              "category":"Boundary","span":[5,7],"msg":"..."}]}
//      {"op":"sample", "output":"a1", "times":[0,0.25,0.5]}
//        -> {"ok":true,"values":[...]}    (read-only: no state commit)
//      {"op":"tick",   "t":0.5}           advance engine time (state commits)
//        -> {"ok":true}
//      {"op":"clear"}                     useq-clear
//        -> {"ok":true}
//      {"op":"health", "output":"a1"}
//        -> {"ok":true,"health":"running"|"fallback"|"error"|"idle"}
//      {"op":"config", "opt_level":N}
//        -> {"ok":false,"error":"unsupported"} for opt_level != 1
//           (no engine hook exists to disable optimizations)
//      {"op":"config", "failure_mode":"lkg"|"zero"}
//        -> {"ok":true}   set non-finite policy (failure-model.md §3)
//
// JSON is hand-rolled (both parsing and emission) — no third-party libs.

#include "src/signal_engine/signal_engine.h"
#include "src/modulisp/lisp/symbol_intern.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <string>

using namespace sig;

// ── Minimal JSON emission helpers ───────────────────────────────────────────

static void json_escape_into(std::string& out, const char* s) {
    if (!s) return;
    for (const char* p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += (char)c;
                }
        }
    }
}

static std::string diagnostics_json(const Diagnostic* diags, uint8_t count) {
    std::string out = "[";
    for (uint8_t i = 0; i < count; i++) {
        const Diagnostic& d = diags[i];
        if (i) out += ",";
        out += "{\"severity\":\"";
        out += severity_to_cstr(d.severity);
        out += "\",\"category\":\"";
        out += category_to_cstr(d.category);
        out += "\",\"span\":[";
        char buf[48];
        snprintf(buf, sizeof(buf), "%u,%u", (unsigned)d.span_start,
                 (unsigned)(d.span_start + d.span_len));
        out += buf;
        out += "],\"msg\":\"";
        json_escape_into(out, d.message ? d.message : "");
        out += "\"";
        if (d.suggestion) {
            out += ",\"suggestion\":\"";
            json_escape_into(out, d.suggestion);
            out += "\"";
        }
        out += "}";
    }
    out += "]";
    return out;
}

static void respond(const std::string& s) {
    fputs(s.c_str(), stdout);
    fputc('\n', stdout);
    fflush(stdout);
}

static void respond_error(const char* msg) {
    std::string out = "{\"ok\":false,\"error\":\"";
    json_escape_into(out, msg);
    out += "\"}";
    respond(out);
}

// ── Minimal JSON extraction helpers ─────────────────────────────────────────
// Just enough to read flat request objects: string fields, number fields,
// and a flat array of numbers. No nesting needed for the request schema.

// Finds `"key"` followed by ':' at top level of a flat object. Returns
// pointer to the first non-space char of the value, or nullptr.
static const char* json_find_value(const char* line, const char* key) {
    size_t klen = strlen(key);
    const char* p = line;
    while ((p = strstr(p, key)) != nullptr) {
        // must be quoted: preceded by '"' and followed by '"' then ':'
        if (p > line && p[-1] == '"' && p[klen] == '"') {
            const char* q = p + klen + 1;
            while (*q && isspace((unsigned char)*q)) q++;
            if (*q == ':') {
                q++;
                while (*q && isspace((unsigned char)*q)) q++;
                return q;
            }
        }
        p += 1;
    }
    return nullptr;
}

// Extracts a string value for key into out. Handles \" \\ \n \r \t escapes.
static bool json_get_string(const char* line, const char* key, std::string& out) {
    const char* v = json_find_value(line, key);
    if (!v || *v != '"') return false;
    v++;
    out.clear();
    while (*v && *v != '"') {
        if (*v == '\\' && v[1]) {
            v++;
            switch (*v) {
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case '"': out += '"';  break;
                case '\\': out += '\\'; break;
                case '/': out += '/';  break;
                case 'u': {
                    // Minimal \uXXXX: only handle ASCII range.
                    if (v[1] && v[2] && v[3] && v[4]) {
                        char hex[5] = { v[1], v[2], v[3], v[4], 0 };
                        long code = strtol(hex, nullptr, 16);
                        if (code < 0x80) out += (char)code;
                        v += 4;
                    }
                    break;
                }
                default: out += *v; break;
            }
            v++;
        } else {
            out += *v++;
        }
    }
    return *v == '"';
}

static bool json_get_number(const char* line, const char* key, double& out) {
    const char* v = json_find_value(line, key);
    if (!v) return false;
    char* end = nullptr;
    double d = strtod(v, &end);
    if (end == v) return false;
    out = d;
    return true;
}

// Extracts a flat array of numbers for key. Returns count, or -1 on error.
static int json_get_number_array(const char* line, const char* key,
                                 double* out, int max_count) {
    const char* v = json_find_value(line, key);
    if (!v || *v != '[') return -1;
    v++;
    int n = 0;
    while (*v) {
        while (*v && (isspace((unsigned char)*v) || *v == ',')) v++;
        if (*v == ']') return n;
        char* end = nullptr;
        double d = strtod(v, &end);
        if (end == v) return -1;
        if (n >= max_count) return -1;
        out[n++] = d;
        v = end;
    }
    return -1; // unterminated
}

// ── Session state ───────────────────────────────────────────────────────────

struct Session {
    SignalEngine* engine = nullptr;
    double prev_tick_time = 0.0;
    double hw_inputs[32] = {};

    void init(double bpm = 120.0, int beats_per_bar = 4) {
        engine = new SignalEngine();
        engine->init_defaults(bpm, beats_per_bar);
        prev_tick_time = 0.0;
    }

    // Execute all outputs at time t into output_values.
    // If commit is true, state slots commit and prev-output values advance
    // (a "tick"); otherwise the engine is left untouched (a "sample").
    void execute_at(double t, double* output_values, bool commit) {
        const uint64_t saved_runtime_fallback_mask =
            engine->pool.runtime_fallback_mask;
        const uint64_t saved_state_update_failure_mask =
            engine->pool.state_update_failure_mask;
        double cell_vals[MAX_CELLS];
        engine->cells.snapshot_values(cell_vals, MAX_CELLS);
        double workspace[MAX_TOTAL_NODES];

        ExecutionContext ctx;
        ctx.t             = t;
        ctx.dt            = t - prev_tick_time;
        ctx.cell_values   = cell_vals;
        ctx.hw_inputs     = hw_inputs;
        ctx.data_pool     = engine->cells.data_pool;
        ctx.data_offsets  = engine->cells.data_offsets;
        ctx.data_lengths  = engine->cells.data_lengths;
        ctx.prev_outputs  = engine->pool.prev_output_values;
        ctx.output_values = output_values;
        ctx.workspace     = workspace;
        execute_all_outputs(engine->pool, ctx);

        if (commit) {
            commit_state(engine->pool, workspace);
            prev_tick_time = t;
            commit_outputs(engine->pool, output_values);
        } else {
            // Sampling is an observation, not a live execution pass. Match
            // the generated-WASM adapter by restoring diagnostic health that
            // execute_all_outputs computes while producing the candidate.
            engine->pool.runtime_fallback_mask =
                saved_runtime_fallback_mask;
            engine->pool.state_update_failure_mask =
                saved_state_update_failure_mask;
        }
    }
};

static bool resolve_output(const char* name, uint16_t& index_out) {
    uint16_t idx = GraphBuilder::resolve_output_index(
        SymbolIntern::getInstance().intern(String(name)));
    if (idx == NODE_NONE) return false;
    index_out = idx;
    return true;
}

// ── Session op handlers ─────────────────────────────────────────────────────

static void op_eval(Session& s, const char* line) {
    std::string code;
    if (!json_get_string(line, "code", code)) {
        respond_error("eval: missing \"code\"");
        return;
    }
    EvalResult result = eval_cold(code.c_str(), (uint32_t)code.size(), *s.engine);
    s.engine->pool.rebuild_execution_order();

    if (result.kind == EvalResult::Error) {
        std::string out = "{\"ok\":false,\"diagnostics\":";
        out += diagnostics_json(result.diagnostics, result.diagnostic_count);
        out += "}";
        respond(out);
        return;
    }

    std::string out = "{\"ok\":true";
    if (result.kind == EvalResult::Number) {
        char buf[48];
        snprintf(buf, sizeof(buf), "%.17g", result.number);
        out += ",\"value\":";
        out += buf;
    }
    if (result.diagnostic_count > 0) {
        out += ",\"diagnostics\":";
        out += diagnostics_json(result.diagnostics, result.diagnostic_count);
    }
    out += "}";
    respond(out);
}

static void op_sample(Session& s, const char* line) {
    std::string output_name;
    if (!json_get_string(line, "output", output_name)) {
        respond_error("sample: missing \"output\"");
        return;
    }
    static const int MAX_TIMES = 4096;
    static double times[MAX_TIMES];
    int n = json_get_number_array(line, "times", times, MAX_TIMES);
    if (n < 0) {
        respond_error("sample: missing or malformed \"times\"");
        return;
    }

    uint16_t output_index;
    if (!resolve_output(output_name.c_str(), output_index) ||
        s.engine->pool.outputs[output_index].root_node == NODE_NONE) {
        std::string msg = "Output ";
        msg += output_name;
        msg += " not assigned";
        respond_error(msg.c_str());
        return;
    }

    std::string out = "{\"ok\":true,\"values\":[";
    for (int i = 0; i < n; i++) {
        double output_values[MAX_OUTPUTS] = {};
        s.execute_at(times[i], output_values, /*commit=*/false);
        char buf[48];
        snprintf(buf, sizeof(buf), "%.17g", output_values[output_index]);
        if (i) out += ",";
        out += buf;
    }
    out += "]}";
    respond(out);
}

static void op_tick(Session& s, const char* line) {
    double t = 0.0;
    if (!json_get_number(line, "t", t)) {
        respond_error("tick: missing \"t\"");
        return;
    }
    double output_values[MAX_OUTPUTS] = {};
    s.execute_at(t, output_values, /*commit=*/true);
    respond("{\"ok\":true}");
}

static void op_clear(Session& s) {
    static const char* CLEAR = "(useq-clear)";
    EvalResult result = eval_cold(CLEAR, (uint32_t)strlen(CLEAR), *s.engine);
    s.engine->pool.rebuild_execution_order();
    s.prev_tick_time = 0.0;
    if (result.kind == EvalResult::Error) {
        respond_error(result.diagnostic_count > 0 &&
                      result.diagnostics[0].message
                          ? result.diagnostics[0].message
                          : "clear failed");
        return;
    }
    respond("{\"ok\":true}");
}

static void op_health(Session& s, const char* line) {
    std::string output_name;
    if (!json_get_string(line, "output", output_name)) {
        respond_error("health: missing \"output\"");
        return;
    }
    uint16_t output_index;
    if (!resolve_output(output_name.c_str(), output_index)) {
        std::string msg = "Unknown output ";
        msg += output_name;
        respond_error(msg.c_str());
        return;
    }
    const char* health = output_health_to_cstr(
        output_health(s.engine->pool, output_index));
    std::string out = "{\"ok\":true,\"health\":\"";
    out += health;
    out += "\"}";
    respond(out);
}

static void op_config(const char* line) {
    // {"op":"config","failure_mode":"lkg"|"zero"} — set the runtime
    // non-finite policy (failure-model.md §3). Mirrors the wire-protocol
    // "set-failure-mode" message and useq_set_failure_mode().
    std::string failure_mode;
    if (json_get_string(line, "failure_mode", failure_mode)) {
        if (failure_mode == "lkg") {
            set_failure_mode(FailureMode::LkgFallback);
        } else if (failure_mode == "zero") {
            set_failure_mode(FailureMode::ZeroSquash);
        } else {
            respond_error("config: failure_mode must be \"lkg\" or \"zero\"");
            return;
        }
        respond("{\"ok\":true}");
        return;
    }

    double opt_level = 0.0;
    if (!json_get_number(line, "opt_level", opt_level)) {
        respond_error("config: missing \"opt_level\" or \"failure_mode\"");
        return;
    }
    // The engine has no hook to disable compile optimizations (CSE, constant
    // folding are unconditional in NodePool). Per the conformance design doc,
    // report unsupported rather than pretending. Default optimization level
    // (anything non-zero) is what already runs, so that is a no-op success.
    if (opt_level == 0.0) {
        respond_error("unsupported");
        return;
    }
    respond("{\"ok\":true}");
}

static int run_session() {
    Session session;
    session.init();

    char* line = nullptr;
    size_t cap = 0;
    ssize_t len;
    while ((len = getline(&line, &cap, stdin)) != -1) {
        // Skip blank lines
        bool blank = true;
        for (ssize_t i = 0; i < len; i++)
            if (!isspace((unsigned char)line[i])) { blank = false; break; }
        if (blank) continue;

        std::string op;
        if (!json_get_string(line, "op", op)) {
            respond_error("missing \"op\"");
            continue;
        }

        if (op == "eval")        op_eval(session, line);
        else if (op == "sample") op_sample(session, line);
        else if (op == "tick")   op_tick(session, line);
        else if (op == "clear")  op_clear(session);
        else if (op == "health") op_health(session, line);
        else if (op == "config") op_config(line);
        else {
            std::string msg = "unknown op: ";
            msg += op;
            respond_error(msg.c_str());
        }
    }
    free(line);
    return 0;
}

// ── One-shot CLI mode (golden runner compatibility) ─────────────────────────

static int run_one_shot(int argc, char* argv[]) {
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

int main(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--session") == 0) {
            return run_session();
        }
    }
    return run_one_shot(argc, argv);
}
