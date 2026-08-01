// Exhaustive, table-driven conformance for the compiler's recognised symbol
// and form surface.  symbols.def is the inventory under test: adding a symbol
// there without classifying and exercising it here is a test failure.

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "src/signal_engine/signal_engine.h"

#include <cstring>
#include <string>

using namespace sig;

namespace {

struct ManifestEntry {
    const char* name;
    const char* category;
};

static const ManifestEntry kManifest[] = {
#define SYM(field, spelling, category) {spelling, #category},
#include "src/signal_engine/symbols.def"
#undef SYM
};

struct SignalCase {
    const char* name;
    const char* category;
    const char* valid;
    const char* too_few;
    const char* too_many;
};

// One row for every form callable by GraphBuilder.  Optional/variadic forms
// use nullptr where a lower or upper arity does not exist.
static const SignalCase kSignalCases[] = {
    {"+", "arith", "(+ 1 2)", nullptr, nullptr},
    {"-", "arith", "(- 3 2)", "(-)", nullptr},
    {"*", "arith", "(* 3 2)", nullptr, nullptr},
    {"/", "arith", "(/ 6 2)", "(/)", nullptr},
    {"%", "arith", "(% 7 3)", "(% 7)", nullptr},
    {">", "cmp", "(> 2 1)", "(> 1)", "(> 2 1 0)"},
    {"<", "cmp", "(< 1 2)", "(< 1)", "(< 1 2 3)"},
    {">=", "cmp", "(>= 2 2)", "(>= 2)", "(>= 2 2 1)"},
    {"<=", "cmp", "(<= 2 2)", "(<= 2)", "(<= 2 2 1)"},
    {"=", "cmp", "(= 2 2)", "(= 2)", "(= 2 2 1)"},
    {"not", "logic", "(not 0)", "(not)", "(not 0 1)"},
    {"and", "logic", "(and 1 1)", "(and 1)", "(and 1 1 1)"},
    {"or", "logic", "(or 0 1)", "(or 0)", "(or 0 1 1)"},
    {"time-as", "time_warp", "(time-as 1 beat)", "(time-as 1)", "(time-as 1 beat 2)"},
    {"fast", "time_warp", "(fast 2 beat)", "(fast 2)", "(fast 2 beat 3)"},
    {"slow", "time_warp", "(slow 2 beat)", "(slow 2)", "(slow 2 beat 3)"},
    {"offset", "time_warp", "(offset 1 beat)", "(offset 1)", "(offset 1 beat 3)"},
    {"shift", "time_warp", "(shift 1 beat)", "(shift 1)", "(shift 1 beat 3)"},
    {"if", "control", "(if 1 2 3)", "(if 1)", "(if 1 2 3 4)"},
    {"let", "control", "(let [x 1] x)", "(let [x])", nullptr},
    {"do", "control", "(do 1 2)", nullptr, nullptr},
    {"for", "control", "(for x [1 2] x)", "(for x [1 2])", "(for x [1 2] x 9)"},
    {"while", "control", "(while 1 2)", "(while 1)", "(while 1 2 3)"},
    {"fn", "control", "((fn [x] x) 1)", "((fn [x] x))", "((fn [x] x) 1 2)"},
    {"lambda", "control", "((lambda [x] x) 1)", "((lambda [x] x))", "((lambda [x] x) 1 2)"},
    {"scope", "control", "(scope 1 2)", nullptr, nullptr},
    {"sin", "unary", "(sin 1)", "(sin)", "(sin 1 2)"},
    {"cos", "unary", "(cos 1)", "(cos)", "(cos 1 2)"},
    {"tan", "unary", "(tan 1)", "(tan)", "(tan 1 2)"},
    {"abs", "unary", "(abs -1)", "(abs)", "(abs 1 2)"},
    {"floor", "unary", "(floor 1.5)", "(floor)", "(floor 1 2)"},
    {"ceil", "unary", "(ceil 1.5)", "(ceil)", "(ceil 1 2)"},
    {"sqrt", "unary", "(sqrt 4)", "(sqrt)", "(sqrt 4 2)"},
    {"neg", "unary", "(neg 1)", "(neg)", "(neg 1 2)"},
    {"frac", "unary", "(frac 1.5)", "(frac)", "(frac 1 2)"},
    {"usin", "unary", "(usin beat)", "(usin)", "(usin beat 2)"},
    {"ucos", "unary", "(ucos beat)", "(ucos)", "(ucos beat 2)"},
    {"bi-to-uni", "unary", "(bi-to-uni -1)", "(bi-to-uni)", "(bi-to-uni 1 2)"},
    {"b>u", "unary", "(b>u -1)", "(b>u)", "(b>u 1 2)"},
    {"uni-to-bi", "unary", "(uni-to-bi 0.5)", "(uni-to-bi)", "(uni-to-bi 1 2)"},
    {"u>b", "unary", "(u>b 0.5)", "(u>b)", "(u>b 1 2)"},
    {"min", "arith", "(min 1 2 3)", "(min)", nullptr},
    {"max", "arith", "(max 1 2 3)", "(max)", nullptr},
    {"pow", "binary", "(pow 2 3)", "(pow 2)", "(pow 2 3 4)"},
    {"expt", "binary", "(expt 2 3)", "(expt 2)", "(expt 2 3 4)"},
    {"mod", "binary", "(mod 7 3)", "(mod 7)", "(mod 7 3 2)"},
    {"pulse", "binary", "(pulse beat 0.5)", "(pulse beat)", "(pulse beat 0.5 1)"},
    {"clamp", "ternary", "(clamp 0 1 0.5)", "(clamp 0 1)", "(clamp 0 1 0.5 2)"},
    {"lerp", "ternary", "(lerp 0 1 0.5)", "(lerp 0 1)", "(lerp 0 1 0.5 2)"},
    {"scale", "ternary", "(scale 0.5 0 10)", "(scale 0.5 0)", "(scale 0.5 0 10 2)"},
    {"tri", "waveform", "(tri beat)", "(tri)", "(tri 0.5 beat)"},
    {"sqr", "waveform", "(sqr beat)", "(sqr)", "(sqr 0.5 beat)"},
    {"step", "signal", "(step [1 2] beat)", "(step)", "(step [1 2] beat 1)"},
    {"gates", "signal", "(gates [1 0] beat)", "(gates)", "(gates [1] 0.5 beat 1)"},
    {"trigs", "signal", "(trigs [1 0] beat)", "(trigs)", "(trigs [1] 0.5 beat 1)"},
    {"euclid", "signal", "(euclid 8 3 beat)", "(euclid 8)", "(euclid 8 3 0.5 beat 1)"},
    {"eu", "signal", "(eu 8 3 beat)", "(eu 8)", "(eu 8 3 0.5 beat 1)"},
    {"seq", "signal", "(seq [1 2] beat)", "(seq)", "(seq [1 2] beat 1)"},
    {"from-list", "signal", "(from-list [1 2] beat)", "(from-list)", "(from-list [1 2] beat 1)"},
    {"interp", "signal", "(interp [1 2] beat)", "(interp)", "(interp [1 2] beat 1)"},
    {"flatseq", "signal", "(flatseq [1 2] beat)", "(flatseq)", "(flatseq [1 2] beat 1)"},
    {"dm", "signal", "(dm 1 0 2)", "(dm 1 0)", "(dm 1 0 2 3)"},
    {"range", "signal", "(range 4)", "(range)", "(range 0 4 1 2)"},
    {"gatesw", "signal", "(gatesw [9 0] beat)", "(gatesw)", "(gatesw [9] beat 1)"},
    {"random", "signal", "(random 0 1)", nullptr, "(random 0 1 2)"},
    {"index-rand", "signal", "(index-rand 1 0 1)", "(index-rand)", "(index-rand 1 0 1 2)"},
    {"loop-at", "signal", "(loop-at 1 beat)", "(loop-at 1)", "(loop-at 1 beat 2)"},
    {"eval-at-time", "signal", "(eval-at-time 1 beat)", "(eval-at-time 1)", "(eval-at-time 1 beat 2)"},
    {"rpulse", "signal", "(rpulse [1 2] 0.5 beat)", "(rpulse [1 2])", "(rpulse [1 2] 0.5 beat 1)"},
    {"rstep", "signal", "(rstep [1 2] beat)", "(rstep)", "(rstep [1 2] beat 1)"},
    {"ridx", "signal", "(ridx [1 2] beat)", "(ridx)", "(ridx [1 2] beat 1)"},
    {"rwarp", "signal", "(rwarp [1 2] beat)", "(rwarp)", "(rwarp [1 2] beat 1)"},
    {"integrate", "signal", "(integrate 1 :id \"catalogue-integrate\")", "(integrate)", "(integrate 1 2)"},
    {"phasor", "signal", "(phasor 1 :id \"catalogue-phasor\")", "(phasor)", "(phasor 1 2)"},
    {"lfo", "signal", "(lfo 1 :id \"catalogue-lfo\")", "(lfo)", "(lfo 1 2)"},
    {"slew", "signal", "(slew 1 2 :id \"catalogue-slew\")", "(slew 1)", "(slew 1 2 3)"},
    {"one-pole", "signal", "(one-pole 1 2 :id \"catalogue-one-pole\")", "(one-pole 1)", "(one-pole 1 2 3)"},
    {"env-follow", "signal", "(env-follow 1 :id \"catalogue-env\")", "(env-follow)", "(env-follow 1 2 3 4)"},
    {"sah", "signal", "(sah 1 1 :id \"catalogue-sah\")", "(sah 1)", "(sah 1 1 2)"},
    {"noise", "signal", "(noise :id \"catalogue-noise\")", nullptr, "(noise 1)"},
    {"toggle", "signal", "(toggle 1 :id \"catalogue-toggle\")", "(toggle)", "(toggle 1 2)"},
    {"count", "signal", "(count 1 :id \"catalogue-count\")", "(count)", "(count 1 2)"},
    {"osc", "signal", "(osc 1 :id \"catalogue-osc\")", "(osc)", "(osc 1 2)"},
    {"tri-osc", "signal", "(tri-osc 1 :id \"catalogue-tri-osc\")", "(tri-osc)", "(tri-osc 1 2)"},
    {"saw", "signal", "(saw 1 :id \"catalogue-saw\")", "(saw)", "(saw 1 2)"},
    {"sqr-osc", "signal", "(sqr-osc 1 :id \"catalogue-sqr-osc\")", "(sqr-osc)", "(sqr-osc 1 2)"},
    {"envelope-follower", "signal", "(envelope-follower 1 :id \"catalogue-envelope\")", "(envelope-follower)", "(envelope-follower 1 2 3 4)"},
    {"latch", "signal", "(latch 1 1 :id \"catalogue-latch\")", "(latch 1)", "(latch 1 1 2)"},
    {"live-edit", "signal", "(live-edit 0.5 :id \"catalogue-live\" :min 0 :max 1)", "(live-edit)", "(live-edit 0.5 :id \"x\" :min 0 :max 1 2)"},
};

struct TopLevelCase {
    const char* name;
    const char* category;
    const char* prelude;
    const char* valid;
    const char* invalid;
};

static const TopLevelCase kTopLevelCases[] = {
    {"defstate", "side_effect", nullptr, "(defstate catalogue-state 0 (+ catalogue-state 1))", "(defstate catalogue-state 0)"},
    {"define", "side_effect", nullptr, "(define catalogue-a 1)", "(define catalogue-a)"},
    {"def", "side_effect", nullptr, "(def catalogue-b 1)", "(def catalogue-b)"},
    {"defn", "side_effect", nullptr, "(defn catalogue-f [x] x)", "(defn catalogue-f [x])"},
    {"defun", "side_effect", nullptr, "(defun catalogue-g [x] x)", "(defun catalogue-g [x])"},
    {"defs", "side_effect", nullptr, "(defs [catalogue-c 1 catalogue-d 2])", "(defs [catalogue-c])"},
    {"set", "side_effect", nullptr, "(set catalogue-e 1)", "(set catalogue-e)"},
    {"unassign", "side_effect", "(a2 0.5)", "(unassign a2)", "(unassign a2 a3)"},
    {"zeros", "side_effect", nullptr, "(zeros 4)", "(zeros)"},
    {"get-expr", "side_effect", "(defn catalogue-h [x] x)", "(get-expr catalogue-h)", "(get-expr catalogue-h extra)"},
    {"set-bpm", "side_effect", nullptr, "(set-bpm 123)", "(set-bpm)"},
    {"set-time-sig", "side_effect", nullptr, "(set-time-sig 3 4)", "(set-time-sig 6 8)"},
    {"useq-clear", "side_effect", nullptr, "(useq-clear)", "(useq-clear 1)"},
    {"useq-set-time-offset", "side_effect", nullptr, "(useq-set-time-offset 1)", "(useq-set-time-offset)"},
    {"useq-nudge-time", "side_effect", nullptr, "(useq-nudge-time 0.1)", "(useq-nudge-time)"},
    {"useq-play", "side_effect", nullptr, "(useq-play)", "(useq-play 1)"},
    {"useq-pause", "side_effect", nullptr, "(useq-pause)", "(useq-pause 1)"},
    {"useq-stop", "side_effect", nullptr, "(useq-stop)", "(useq-stop 1)"},
    {"useq-rewind", "side_effect", nullptr, "(useq-rewind)", "(useq-rewind 1)"},
    {"synth", "side_effect", nullptr, "(synth \"osc/sine\" :freq 440)", "(synth)"},
    {"with-state-id", "control", nullptr, "(with-state-id \"catalogue-wrapper\" (synth \"osc/sine\" :freq 440))", "(with-state-id \"catalogue-wrapper\")"},
};

struct ClassifiedOnly {
    const char* name;
    const char* category;
    const char* valid_bare;
};

// `none` means the symbol is a leaf, keyword, or explicitly-special form,
// never an untested callable dispatch category.
static const ClassifiedOnly kClassifiedOnly[] = {
    {"t", "none", "t"}, {"beat", "none", "beat"},
    {"bar", "none", "bar"}, {"phrase", "none", "phrase"},
    {"section", "none", "section"}, {"beat-num", "none", "beat-num"},
    {"bar-num", "none", "bar-num"}, {"bpm", "none", "bpm"},
    {"beats-per-bar", "none", "beats-per-bar"},
    {"bars-per-phrase", "none", "bars-per-phrase"},
    {"phrases-per-section", "none", "phrases-per-section"},
    {"dt", "none", "dt"}, {"beat-dur", "none", "beat-dur"},
    {"bar-dur", "none", "bar-dur"},
    {"prev", "none", "(prev a1)"}, {"input", "none", "(input 0)"},
    {"quote", "none", nullptr},
    {":wave", "none", nullptr}, {":phase", "none", nullptr},
    {":pw", "none", nullptr}, {":id", "none", nullptr},
    {":fresh", "none", nullptr}, {":attack", "none", nullptr},
    {":release", "none", nullptr}, {":reset", "none", nullptr},
    {":sin", "none", nullptr}, {":tri", "none", nullptr},
    {":saw", "none", nullptr}, {":sqr", "none", nullptr},
    {":freq", "none", nullptr}, {":amp", "none", nullptr},
    {":name", "none", nullptr}, {":version", "none", nullptr},
    {":min", "none", nullptr}, {":max", "none", nullptr},
    {":options", "none", nullptr}, {":step", "none", nullptr},
    {":precision", "none", nullptr},
};

struct SemanticSnapshot {
    uint16_t root;
    bool valid;
    double sample;
    OutputSource source;
    OutputDeps deps;
    uint16_t state_slots;
    uint16_t live_slots;
    uint16_t registry_entries;
    uint8_t data_tables;
    uint32_t arena_head;
    SynthRevision synth_revision;
};

static EvalResult eval(SignalEngine& engine, const std::string& code) {
    return eval_cold(code.c_str(), (uint32_t)code.size(), engine);
}

static double execute_output(SignalEngine& engine, uint16_t output_index,
                             const double* hardware_inputs = nullptr,
                             bool commit = false) {
    double cells[MAX_CELLS] = {};
    double zero_inputs[32] = {};
    double outputs[MAX_OUTPUTS] = {};
    double workspace[MAX_TOTAL_NODES] = {};
    engine.cells.snapshot_values(cells, MAX_CELLS);
    ExecutionContext ctx{0.25, 0.001, cells,
                         hardware_inputs ? hardware_inputs : zero_inputs,
                         engine.cells.data_pool, engine.cells.data_offsets,
                         engine.cells.data_lengths,
                         engine.pool.prev_output_values, outputs, workspace};
    execute_all_outputs(engine.pool, ctx);
    const double result = outputs[output_index];
    if (commit) commit_outputs(engine.pool, outputs);
    return result;
}

static double sample_a1(SignalEngine& engine) {
    return execute_output(engine, 0);
}

static SemanticSnapshot snapshot(SignalEngine& engine) {
    SemanticSnapshot s{};
    s.root = engine.pool.outputs[0].root_node;
    s.valid = engine.pool.outputs[0].valid;
    s.sample = sample_a1(engine);
    s.source = engine.output_sources[0];
    s.deps = engine.pool.output_deps[0];
    s.state_slots = engine.pool.state_slot_count;
    s.live_slots = engine.pool.live_slot_count;
    s.registry_entries = engine.registry.entry_count;
    s.data_tables = engine.cells.data_table_count;
    s.arena_head = engine.arena.write_head;
    s.synth_revision = engine.synth_graph.revision;
    return s;
}

static void require_same_semantics(const SemanticSnapshot& before,
                                   SignalEngine& engine) {
    REQUIRE(engine.pool.outputs[0].root_node == before.root);
    REQUIRE(engine.pool.outputs[0].valid == before.valid);
    REQUIRE(sample_a1(engine) == Approx(before.sample));
    REQUIRE(std::memcmp(&engine.output_sources[0], &before.source,
                        sizeof(before.source)) == 0);
    REQUIRE(std::memcmp(&engine.pool.output_deps[0], &before.deps,
                        sizeof(before.deps)) == 0);
    REQUIRE(engine.pool.state_slot_count == before.state_slots);
    REQUIRE(engine.pool.live_slot_count == before.live_slots);
    REQUIRE(engine.registry.entry_count == before.registry_entries);
    REQUIRE(engine.cells.data_table_count == before.data_tables);
    REQUIRE(engine.arena.write_head == before.arena_head);
    REQUIRE(engine.synth_graph.revision == before.synth_revision);
}

static size_t catalogue_count(const ManifestEntry& manifest) {
    size_t count = 0;
    for (const auto& row : kSignalCases)
        if (std::strcmp(row.name, manifest.name) == 0 &&
            std::strcmp(row.category, manifest.category) == 0) ++count;
    for (const auto& row : kTopLevelCases)
        if (std::strcmp(row.name, manifest.name) == 0 &&
            std::strcmp(row.category, manifest.category) == 0) ++count;
    for (const auto& row : kClassifiedOnly)
        if (std::strcmp(row.name, manifest.name) == 0 &&
            std::strcmp(row.category, manifest.category) == 0) ++count;
    return count;
}

static size_t manifest_count(const char* name, const char* category) {
    size_t count = 0;
    for (const auto& entry : kManifest) {
        if (std::strcmp(name, entry.name) == 0 &&
            std::strcmp(category, entry.category) == 0) ++count;
    }
    return count;
}

} // namespace

TEST_CASE("Every symbols.def entry has one executable catalogue disposition",
          "[builtins][catalogue]") {
    for (const auto& entry : kManifest) {
        INFO("symbol: " << entry.name << " category: " << entry.category);
        REQUIRE(manifest_count(entry.name, entry.category) == 1);
        REQUIRE(catalogue_count(entry) == 1);
    }

    for (const auto& row : kSignalCases)
        REQUIRE(manifest_count(row.name, row.category) == 1);
    for (const auto& row : kTopLevelCases)
        REQUIRE(manifest_count(row.name, row.category) == 1);
    for (const auto& row : kClassifiedOnly)
        REQUIRE(manifest_count(row.name, row.category) == 1);
}

TEST_CASE("Every compiler-dispatched form accepts its documented witness",
          "[builtins][catalogue][positive]") {
    for (const auto& row : kSignalCases) {
        DYNAMIC_SECTION(row.name << " accepts " << row.valid) {
            SignalEngine engine;
            engine.init_defaults(120.0, 4);
            const std::string assignment =
                std::string("(a1 ") + row.valid + ")";
            EvalResult r = eval(engine, assignment);
            if (r.kind == EvalResult::Error && r.diagnostic_count > 0) {
                const char* message = r.diagnostics[0].message;
                INFO("diagnostic: " << (message ? message : ""));
            }
            REQUIRE(r.kind != EvalResult::Error);
        }
    }
    for (const auto& row : kClassifiedOnly) {
        if (!row.valid_bare) continue;
        DYNAMIC_SECTION(row.name << " resolves as a leaf/special form") {
            SignalEngine engine;
            engine.init_defaults(120.0, 4);
            REQUIRE(eval(engine, row.valid_bare).kind != EvalResult::Error);
        }
    }
}

TEST_CASE("Arity rejection is failure-atomic for every bounded form",
          "[builtins][arity][transaction]") {
    for (const auto& row : kSignalCases) {
        const char* rejected[] = {row.too_few, row.too_many};
        for (const char* code : rejected) {
            if (!code) continue;
            DYNAMIC_SECTION(row.name << " rejects " << code) {
                SignalEngine engine;
                engine.init_defaults(120.0, 4);
                REQUIRE(eval(engine, "(a1 0.375)").kind != EvalResult::Error);
                SemanticSnapshot before = snapshot(engine);
                std::string assignment = std::string("(a1 ") + code + ")";
                EvalResult r = eval(engine, assignment);
                INFO("candidate: " << assignment);
                REQUIRE(r.kind == EvalResult::Error);
                require_same_semantics(before, engine);
            }
        }
    }
}

TEST_CASE("Top-level surfaces validate before mutation and are forbidden in signals",
          "[builtins][top-level][boundary][transaction]") {
    for (const auto& row : kTopLevelCases) {
        DYNAMIC_SECTION(row.name << " valid, invalid, and boundary cases") {
            SignalEngine valid_engine;
            valid_engine.init_defaults(120.0, 4);
            if (row.prelude)
                REQUIRE(eval(valid_engine, row.prelude).kind != EvalResult::Error);
            EvalResult ok = eval(valid_engine, row.valid);
            if (ok.kind == EvalResult::Error && ok.diagnostic_count > 0) {
                const char* message = ok.diagnostics[0].message;
                INFO("diagnostic: " << (message ? message : ""));
            }
            REQUIRE(ok.kind != EvalResult::Error);

            SignalEngine rejected_engine;
            rejected_engine.init_defaults(120.0, 4);
            REQUIRE(eval(rejected_engine, "(a1 0.375)").kind != EvalResult::Error);
            SemanticSnapshot before = snapshot(rejected_engine);
            REQUIRE(eval(rejected_engine, row.invalid).kind == EvalResult::Error);
            require_same_semantics(before, rejected_engine);

            SignalEngine nested_engine;
            nested_engine.init_defaults(120.0, 4);
            REQUIRE(eval(nested_engine, "(a1 0.375)").kind != EvalResult::Error);
            SemanticSnapshot nested_before = snapshot(nested_engine);
            std::string nested = std::string("(a1 ") + row.valid + ")";
            REQUIRE(eval(nested_engine, nested).kind == EvalResult::Error);
            require_same_semantics(nested_before, nested_engine);
        }
    }
}

TEST_CASE("Builtin types and UGen keywords reject incoherent inputs atomically",
          "[builtins][type][keyword][transaction]") {
    static const char* rejected[] = {
        "(input beat)",
        "(step 1 beat)",
        "(range beat)",
        "(phasor 1 :phase beat)",
        "(lfo 1 :wave :not-a-wave)",
        "(live-edit 0.5 :id 1 :min 0 :max 1)",
        "(live-edit 0.5 :id \"bad-name\" :min 0 :max 1 :name 1)",
        "(live-edit 0.5 :id \"bad-step\" :min 0 :max 1 :step 0)",
        "(live-edit 0.5 :id \"bad-precision\" :min 0 :max 1 :precision 1.5)",
        "(live-edit 0.5 :id \"bad-options\" :min 0 :max 1 :options [:a])",
        "(live-edit :a :id \"missing-seed-option\" :options [:b])",
        "(live-edit :a :id \"bad-option-type\" :options [:a 1])",
        "(live-edit :a :id \"full-unknown\" :min 0 :max 1 :name \"A\" "
        ":options [:a] :step 0.1 :precision 2 :bogus 1)",
        "(integrate 1 :bogus 2)",
        "(phasor 1 :bogus 2)",
        "(lfo 1 :bogus 2)",
        "(slew 1 2 :bogus 2)",
        "(one-pole 1 2 :bogus 2)",
        "(env-follow 1 :bogus 2)",
        "(sah 1 1 :bogus 2)",
        "(noise :bogus 2)",
        "(toggle 1 :bogus 2)",
        "(count 1 :bogus 2)",
        "(osc 1 :bogus 2)",
        "(tri-osc 1 :bogus 2)",
        "(saw 1 :bogus 2)",
        "(sqr-osc 1 :bogus 2)",
        "(envelope-follower 1 :bogus 2)",
        "(latch 1 1 :bogus 2)",
        "(live-edit 0.5 :id \"bad-kw\" :min 0 :max 1 :bogus 2)",
    };

    for (const char* code : rejected) {
        DYNAMIC_SECTION(code) {
            SignalEngine engine;
            engine.init_defaults(120.0, 4);
            REQUIRE(eval(engine, "(a1 0.375)").kind != EvalResult::Error);
            SemanticSnapshot before = snapshot(engine);
            std::string assignment = std::string("(a1 ") + code + ")";
            REQUIRE(eval(engine, assignment).kind == EvalResult::Error);
            require_same_semantics(before, engine);
        }
    }
}

TEST_CASE("Duplicate UGen and live-edit keywords are rejected atomically",
          "[builtins][keyword][duplicate][transaction]") {
    static const char* rejected[] = {
        "(integrate 1 :id \"a\" :id \"b\")",
        "(phasor 1 :phase 0 :phase 0.5)",
        "(phasor 1 :id \"a\" :id \"b\")",
        "(lfo 1 :wave :sin :wave :tri)",
        "(lfo 1 :phase 0 :phase 0.5)",
        "(lfo 1 :pw 0.25 :pw 0.75)",
        "(lfo 1 :id \"a\" :id \"b\")",
        "(slew 1 2 :id \"a\" :id \"b\")",
        "(one-pole 1 2 :id \"a\" :id \"b\")",
        "(env-follow 1 :id \"a\" :id \"b\")",
        "(sah 1 1 :id \"a\" :id \"b\")",
        "(noise :id \"a\" :id \"b\")",
        "(toggle 1 :id \"a\" :id \"b\")",
        "(count 1 :reset 0 :reset 1)",
        "(count 1 :id \"a\" :id \"b\")",
        "(live-edit 0.5 :id \"a\" :id \"b\" :min 0 :max 1)",
        "(live-edit 0.5 :id \"a\" :min 0 :min -1 :max 1)",
        "(live-edit 0.5 :id \"a\" :min 0 :max 1 :max 2)",
        "(live-edit 0.5 :id \"a\" :min 0 :max 1 :name \"x\" :name \"y\")",
        "(live-edit 0.5 :id \"a\" :min 0 :max 1 :step 0.1 :step 0.2)",
        "(live-edit 0.5 :id \"a\" :min 0 :max 1 :precision 2 :precision 3)",
        "(live-edit :a :id \"a\" :options [:a] :options [:a :b])",
    };

    for (const char* code : rejected) {
        DYNAMIC_SECTION(code) {
            SignalEngine engine;
            engine.init_defaults(120.0, 4);
            REQUIRE(eval(engine, "(a1 0.375)").kind != EvalResult::Error);
            SemanticSnapshot before = snapshot(engine);
            const std::string assignment = std::string("(a1 ") + code + ")";
            REQUIRE(eval(engine, assignment).kind == EvalResult::Error);
            require_same_semantics(before, engine);
        }
    }
}

TEST_CASE("Duplicate synth keywords are rejected atomically",
          "[builtins][synth][keyword][duplicate][transaction]") {
    static const char* rejected[] = {
        "(synth \"osc/sine\" :freq 440 :freq 880)",
        "(synth \"osc/sine\" :freq 440 :amp 0.1 :amp 0.2)",
        "(synth \"osc/sine\" :name \"a\" :name \"b\" :freq 440)",
        "(synth \"osc/sine\" :version 2 :version 2 :freq 440)",
        "(synth \"osc/sine\" :id \"a\" :id \"b\" :freq 440)",
    };

    for (const char* code : rejected) {
        DYNAMIC_SECTION(code) {
            SignalEngine engine;
            engine.init_defaults(120.0, 4);
            REQUIRE(eval(engine, "(a1 0.375)").kind != EvalResult::Error);
            SemanticSnapshot before = snapshot(engine);
            REQUIRE(eval(engine, code).kind == EvalResult::Error);
            require_same_semantics(before, engine);
        }
    }
}

TEST_CASE("All four hardware-input leaves preserve their channel mapping",
          "[builtins][inputs][leaves]") {
    struct InputCase { const char* name; uint16_t index; double value; };
    static const InputCase inputs[] = {
        {"in1", 0, 0.125}, {"in2", 1, 0.25},
        {"ain1", 8, 0.625}, {"ain2", 9, 0.875},
    };

    for (const auto& input : inputs) {
        DYNAMIC_SECTION(input.name << " maps to hardware channel " << input.index) {
            SignalEngine engine;
            engine.init_defaults(120.0, 4);
            REQUIRE(eval(engine, std::string("(a1 ") + input.name + ")").kind !=
                    EvalResult::Error);
            double hardware_inputs[32] = {};
            hardware_inputs[input.index] = input.value;
            REQUIRE(execute_output(engine, 0, hardware_inputs) ==
                    Approx(input.value));
            REQUIRE((engine.pool.output_input_mask[0] &
                     (uint32_t{1} << input.index)) != 0);
        }
    }
}

TEST_CASE("All 24 output sinks support assignment, bare reads, and unassignment",
          "[builtins][outputs][prev][unassign]") {
    for (const char prefix : {'a', 'd', 's'}) {
        for (int n = 1; n <= 8; ++n) {
            DYNAMIC_SECTION(prefix << n) {
                SignalEngine engine;
                engine.init_defaults(120.0, 4);
                std::string name;
                name += prefix;
                name += char('0' + n);
                REQUIRE(eval(engine, "(" + name + " 0.5)").kind != EvalResult::Error);
                uint16_t index = GraphBuilder::resolve_output_index(internSymbol(name.c_str()));
                REQUIRE(index != NODE_NONE);
                REQUIRE(engine.pool.outputs[index].valid);

                const std::string reader = name == "a1" ? "a2" : "a1";
                const uint16_t reader_index = GraphBuilder::resolve_output_index(
                    internSymbol(reader.c_str()));
                REQUIRE(eval(engine, "(" + reader + " " + name + ")").kind !=
                        EvalResult::Error);
                // Bare output names are previous-committed-sample reads. The
                // first tick commits the target; the second observes 0.5.
                execute_output(engine, reader_index, nullptr, true);
                REQUIRE(execute_output(engine, reader_index, nullptr, true) ==
                        Approx(0.5));

                REQUIRE(eval(engine, "(unassign " + name + ")").kind != EvalResult::Error);
                REQUIRE_FALSE(engine.pool.outputs[index].valid);
                REQUIRE(engine.pool.outputs[index].root_node == NODE_NONE);
                REQUIRE(engine.pool.outputs[index].lkg_value == 0.0);
                REQUIRE(engine.pool.prev_output_values[index] == 0.0);
                REQUIRE_FALSE(engine.output_sources[index].has_source);
                REQUIRE(engine.pool.output_deps[index].count == 0);
            }
        }
    }
}

TEST_CASE("The four formerly ambiguous surfaces have one explicit meaning",
          "[builtins][paper-blockers]") {
    SignalEngine engine;
    engine.init_defaults(120.0, 4);

    REQUIRE(eval(engine, "(a1 (tri beat))").kind != EvalResult::Error);
    REQUIRE(eval(engine, "(a1 (tri 0.5 beat))").kind == EvalResult::Error);
    REQUIRE(eval(engine, "(a1 (sqr beat))").kind != EvalResult::Error);
    REQUIRE(eval(engine, "(a1 (sqr 0.5 beat))").kind == EvalResult::Error);
    REQUIRE(eval(engine, "(a2 (% 7))").kind == EvalResult::Error);
    REQUIRE(eval(engine, "(a2 (% 7 3))").kind != EvalResult::Error);
    REQUIRE(eval(engine, "(a2 (% 20 6 4))").kind != EvalResult::Error);
    REQUIRE(eval(engine, "(a2 (min 1))").kind != EvalResult::Error);
    REQUIRE(eval(engine, "(a2 (max 1 2 3))").kind != EvalResult::Error);

    auto& si = SymbolIntern::getInstance();
    SymbolID bpb = si.intern("beats-per-bar");
    REQUIRE(eval(engine, "(set-time-sig 3 4)").kind != EvalResult::Error);
    REQUIRE(engine.cells.cells[bpb].value == 3.0);
    REQUIRE(eval(engine, "(set-time-sig 6 8)").kind == EvalResult::Error);
    REQUIRE(engine.cells.cells[bpb].value == 3.0);
    REQUIRE(eval(engine, "(set-time-sig 0 4)").kind == EvalResult::Error);
    REQUIRE(eval(engine, "(set-time-sig -3 4)").kind == EvalResult::Error);
    REQUIRE(eval(engine, "(set-time-sig 3.5 4)").kind == EvalResult::Error);
    REQUIRE(engine.cells.cells[bpb].value == 3.0);

    REQUIRE(eval(engine, "(a3 0.75)").kind != EvalResult::Error);
    REQUIRE(eval(engine, "(unassign a3)").kind != EvalResult::Error);
    REQUIRE_FALSE(engine.pool.outputs[2].valid);
    REQUIRE(eval(engine, "(unassign not-an-output)").kind == EvalResult::Error);
    REQUIRE(eval(engine, "(unassign)").kind == EvalResult::Error);
}
