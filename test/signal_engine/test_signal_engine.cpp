// Signal Engine smoke tests
// Tests the core data structures, graph builder, and executor.

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "src/signal_engine/signal_engine.h"
#include <cmath>

using namespace sig;

// ── Data Structure Tests ────────────────────────────────────────────────────

TEST_CASE("CellStore basic operations", "[signal_engine][cell_store]") {
    CellStore store;

    SECTION("Empty cell has Empty kind") {
        REQUIRE(store.cells[0].kind == CellKind::Empty);
    }

    SECTION("Store and retrieve number cell") {
        store.cells[1].kind = CellKind::Number;
        store.cells[1].value = 440.0;
        store.cells[1].revision = 1;
        REQUIRE(store.cells[1].value == 440.0);
    }

    SECTION("Store data table") {
        double values[] = {1.0, 0.0, 1.0, 0.0};
        uint16_t id = store.store_data_table(values, 4);
        REQUIRE(id == 0);

        uint16_t len;
        const double* data = store.get_data_table(id, len);
        REQUIRE(len == 4);
        REQUIRE(data[0] == 1.0);
        REQUIRE(data[1] == 0.0);
    }

    SECTION("Snapshot values") {
        store.cells[0].value = 1.0;
        store.cells[1].value = 2.0;
        store.cells[2].value = 3.0;
        double snap[MAX_CELLS];
        store.snapshot_values(snap, MAX_CELLS);
        REQUIRE(snap[0] == 1.0);
        REQUIRE(snap[1] == 2.0);
        REQUIRE(snap[2] == 3.0);
    }
}

TEST_CASE("SourceArena", "[signal_engine][source_arena]") {
    SourceArena arena;

    SECTION("Store and read") {
        const char* text = "(+ 1 2)";
        uint32_t offset = arena.store(text, 7);
        REQUIRE(offset == 0);
        REQUIRE(strncmp(arena.read(offset), "(+ 1 2)", 7) == 0);
    }

    SECTION("Sequential stores") {
        arena.store("abc", 3);
        uint32_t offset = arena.store("def", 3);
        REQUIRE(offset == 3);
        REQUIRE(strncmp(arena.read(offset), "def", 3) == 0);
    }
}

// ── Tokenizer Tests ─────────────────────────────────────────────────────────

TEST_CASE("Tokenizer", "[signal_engine][tokenizer]") {
    Token tokens[MAX_TOKENS];
    Diagnostic errors[8];
    uint8_t error_count = 0;

    SECTION("Number") {
        const char* src = "42";
        uint16_t count = TokenStream::tokenize(src, 2, tokens, MAX_TOKENS, errors, &error_count);
        REQUIRE(error_count == 0);
        REQUIRE(count >= 1);
        REQUIRE(tokens[0].kind == TokenKind::Number);
        REQUIRE(tokens[0].number == 42.0);
    }

    SECTION("Simple expression") {
        const char* src = "(+ 1 2)";
        uint16_t count = TokenStream::tokenize(src, 7, tokens, MAX_TOKENS, errors, &error_count);
        REQUIRE(error_count == 0);
        REQUIRE(tokens[0].kind == TokenKind::LParen);
        REQUIRE(tokens[1].kind == TokenKind::Symbol);
        REQUIRE(tokens[2].kind == TokenKind::Number);
        REQUIRE(tokens[2].number == 1.0);
        REQUIRE(tokens[3].kind == TokenKind::Number);
        REQUIRE(tokens[3].number == 2.0);
        REQUIRE(tokens[4].kind == TokenKind::RParen);
    }

    SECTION("Vector literal") {
        const char* src = "[1 2 3]";
        uint16_t count = TokenStream::tokenize(src, 7, tokens, MAX_TOKENS, errors, &error_count);
        REQUIRE(error_count == 0);
        REQUIRE(tokens[0].kind == TokenKind::LBracket);
        REQUIRE(tokens[1].kind == TokenKind::Number);
        REQUIRE(tokens[3].kind == TokenKind::Number);
        REQUIRE(tokens[4].kind == TokenKind::RBracket);
    }

    SECTION("Negative number") {
        const char* src = "-3.14";
        uint16_t count = TokenStream::tokenize(src, 5, tokens, MAX_TOKENS, errors, &error_count);
        REQUIRE(error_count == 0);
        REQUIRE(tokens[0].kind == TokenKind::Number);
        REQUIRE(tokens[0].number == Approx(-3.14));
    }

    SECTION("Comment skipping") {
        const char* src = "42 ; comment\n43";
        uint16_t count = TokenStream::tokenize(src, 15, tokens, MAX_TOKENS, errors, &error_count);
        REQUIRE(error_count == 0);
        REQUIRE(tokens[0].kind == TokenKind::Number);
        REQUIRE(tokens[0].number == 42.0);
        REQUIRE(tokens[1].kind == TokenKind::Number);
        REQUIRE(tokens[1].number == 43.0);
    }
}

// ── Node Pool Tests ─────────────────────────────────────────────────────────

TEST_CASE("NodePool constant folding", "[signal_engine][node_pool]") {
    NodePool pool;

    SECTION("Const creation") {
        uint16_t c = pool.make_const(42.0);
        REQUIRE(c != NODE_NONE);
        REQUIRE(pool.nodes[c].op == NodeOp::Const);
        REQUIRE(pool.nodes[c].imm == 42.0);
    }

    SECTION("CSE deduplication") {
        uint16_t a = pool.make_const(42.0);
        uint16_t b = pool.make_const(42.0);
        REQUIRE(a == b); // same node reused
    }

    SECTION("Different constants are different") {
        uint16_t a = pool.make_const(1.0);
        uint16_t b = pool.make_const(2.0);
        REQUIRE(a != b);
    }

    SECTION("Binary constant folding") {
        uint16_t a = pool.make_const(3.0);
        uint16_t b = pool.make_const(4.0);
        uint16_t sum = pool.make_binop(NodeOp::Add, a, b);
        REQUIRE(pool.nodes[sum].op == NodeOp::Const);
        REQUIRE(pool.nodes[sum].imm == 7.0);
    }

    SECTION("Unary constant folding") {
        uint16_t a = pool.make_const(-5.0);
        uint16_t abs_a = pool.make_unary(NodeOp::Abs, a);
        REQUIRE(pool.nodes[abs_a].op == NodeOp::Const);
        REQUIRE(pool.nodes[abs_a].imm == 5.0);
    }

    SECTION("Algebraic simplification: x + 0 = x") {
        uint16_t x = pool.make_raw_time_load();
        uint16_t zero = pool.make_const(0.0);
        uint16_t result = pool.make_binop(NodeOp::Add, x, zero);
        REQUIRE(result == x);
    }

    SECTION("Algebraic simplification: x * 1 = x") {
        uint16_t x = pool.make_raw_time_load();
        uint16_t one = pool.make_const(1.0);
        uint16_t result = pool.make_binop(NodeOp::Mul, x, one);
        REQUIRE(result == x);
    }

    SECTION("Algebraic simplification: x * 0 = 0") {
        uint16_t x = pool.make_raw_time_load();
        uint16_t zero = pool.make_const(0.0);
        uint16_t result = pool.make_binop(NodeOp::Mul, x, zero);
        REQUIRE(pool.nodes[result].op == NodeOp::Const);
        REQUIRE(pool.nodes[result].imm == 0.0);
    }

    SECTION("Algebraic simplification: x - x = 0") {
        uint16_t x = pool.make_raw_time_load();
        uint16_t result = pool.make_binop(NodeOp::Sub, x, x);
        REQUIRE(pool.nodes[result].op == NodeOp::Const);
        REQUIRE(pool.nodes[result].imm == 0.0);
    }

    SECTION("Transitive constant folding: sin(pi) ≈ 0") {
        uint16_t pi = pool.make_const(M_PI);
        uint16_t result = pool.make_unary(NodeOp::Sin, pi);
        REQUIRE(pool.nodes[result].op == NodeOp::Const);
        REQUIRE(pool.nodes[result].imm == Approx(0.0).margin(1e-10));
    }

    SECTION("Time-invariance propagation") {
        uint16_t a = pool.make_const(2.0); // time-invariant
        uint16_t b = pool.make_const(3.0); // time-invariant
        // Folded to constant, which is time-invariant
        uint16_t sum = pool.make_binop(NodeOp::Add, a, b);
        REQUIRE(pool.nodes[sum].flags & FLAG_TIME_INVARIANT);

        uint16_t t = pool.make_raw_time_load(); // NOT time-invariant
        uint16_t mul = pool.make_binop(NodeOp::Mul, sum, t);
        REQUIRE(!(pool.nodes[mul].flags & FLAG_TIME_INVARIANT));
    }

    SECTION("Select with constant condition folds") {
        uint16_t cond = pool.make_const(1.0); // truthy
        uint16_t then_val = pool.make_const(42.0);
        uint16_t else_val = pool.make_const(99.0);
        uint16_t result = pool.make_select(cond, then_val, else_val);
        REQUIRE(result == then_val);
    }
}

// ── Executor Tests ──────────────────────────────────────────────────────────

TEST_CASE("Executor single-sample", "[signal_engine][executor]") {
    NodePool pool;
    CellStore cells;

    SECTION("Constant output") {
        uint16_t c = pool.make_const(0.5);
        pool.outputs[0].root_node = c;
        pool.outputs[0].valid = true;
        pool.rebuild_execution_order();

        double cell_vals[MAX_CELLS] = {};
        double hw_inputs[32] = {};
        double outputs[MAX_OUTPUTS] = {};
        double workspace[MAX_TOTAL_NODES] = {};

        execute_all_outputs(pool, 0.0, cell_vals, hw_inputs,
                            cells.data_pool, cells.data_offsets, cells.data_lengths,
                            pool.prev_output_values, outputs, workspace);

        REQUIRE(outputs[0] == 0.5);
    }

    SECTION("Time passthrough") {
        uint16_t t = pool.make_raw_time_load();
        pool.outputs[0].root_node = t;
        pool.outputs[0].valid = true;
        pool.rebuild_execution_order();

        double cell_vals[MAX_CELLS] = {};
        double hw_inputs[32] = {};
        double outputs[MAX_OUTPUTS] = {};
        double workspace[MAX_TOTAL_NODES] = {};

        execute_all_outputs(pool, 1.5, cell_vals, hw_inputs,
                            cells.data_pool, cells.data_offsets, cells.data_lengths,
                            pool.prev_output_values, outputs, workspace);

        REQUIRE(outputs[0] == 1.5);
    }

    SECTION("Arithmetic: (+ 1 (* t 2))") {
        uint16_t t = pool.make_raw_time_load();
        uint16_t two = pool.make_const(2.0);
        uint16_t one = pool.make_const(1.0);
        uint16_t product = pool.make_binop(NodeOp::Mul, t, two);
        uint16_t sum = pool.make_binop(NodeOp::Add, one, product);

        pool.outputs[0].root_node = sum;
        pool.outputs[0].valid = true;
        pool.rebuild_execution_order();

        double cell_vals[MAX_CELLS] = {};
        double hw_inputs[32] = {};
        double outputs[MAX_OUTPUTS] = {};
        double workspace[MAX_TOTAL_NODES] = {};

        execute_all_outputs(pool, 3.0, cell_vals, hw_inputs,
                            cells.data_pool, cells.data_offsets, cells.data_lengths,
                            pool.prev_output_values, outputs, workspace);

        REQUIRE(outputs[0] == 7.0);
    }

    SECTION("NaN guard") {
        // 0/0 = NaN → guarded to 0.0
        uint16_t zero = pool.make_const(0.0);
        // Can't fold 0/0 because we return 0.0 for div by zero in eval_binop
        // So we need a runtime division by zero.
        // Use time/something where something will be zero
        uint16_t t = pool.make_raw_time_load();
        uint16_t sqrt_neg = pool.make_unary(NodeOp::Sqrt, pool.make_const(-1.0));
        // sqrt(-1) folds to sqrt(abs(-1)) = 1.0 in our impl... let's test differently

        // Use a CellLoad that's 0 to force runtime div by zero
        pool.outputs[0].root_node = pool.make_binop(NodeOp::Div, t, pool.make_cell_load(0));
        pool.outputs[0].valid = true;
        pool.rebuild_execution_order();

        double cell_vals[MAX_CELLS] = {};
        cell_vals[0] = 0.0;
        double hw_inputs[32] = {};
        double outputs[MAX_OUTPUTS] = {};
        double workspace[MAX_TOTAL_NODES] = {};

        execute_all_outputs(pool, 1.0, cell_vals, hw_inputs,
                            cells.data_pool, cells.data_offsets, cells.data_lengths,
                            pool.prev_output_values, outputs, workspace);

        REQUIRE(outputs[0] == 0.0); // guarded to 0
    }

    SECTION("VecIndex") {
        double values[] = {10.0, 20.0, 30.0, 40.0};
        uint16_t table_id = cells.store_data_table(values, 4);

        // Node: VecIndex with floor(phase * 4) where phase = 0.5 → index 2 → 30.0
        uint16_t phase = pool.make_const(0.5);
        uint16_t len = pool.make_const(4.0);
        uint16_t scaled = pool.make_binop(NodeOp::Mul, phase, len);
        uint16_t idx = pool.make_unary(NodeOp::Floor, scaled);

        Node vec_node;
        vec_node.op = NodeOp::VecIndex;
        vec_node.input_a = idx;
        vec_node.imm = (double)table_id;
        uint16_t vec = pool.intern_node(vec_node);

        pool.outputs[0].root_node = vec;
        pool.outputs[0].valid = true;
        pool.rebuild_execution_order();

        double cell_vals[MAX_CELLS] = {};
        double hw_inputs[32] = {};
        double outputs[MAX_OUTPUTS] = {};
        double workspace[MAX_TOTAL_NODES] = {};

        execute_all_outputs(pool, 0.0, cell_vals, hw_inputs,
                            cells.data_pool, cells.data_offsets, cells.data_lengths,
                            pool.prev_output_values, outputs, workspace);

        REQUIRE(outputs[0] == 30.0);
    }
}

// ── Graph Builder Integration Tests ─────────────────────────────────────────

TEST_CASE("Graph builder: constant arithmetic", "[signal_engine][graph_builder]") {
    NodePool pool;
    CellStore cells;
    SourceArena arena;

    const char* src = "(+ 1 2)";
    Token tokens[MAX_TOKENS];
    uint8_t errors = 0;
    uint16_t count = TokenStream::tokenize(src, (uint32_t)strlen(src), tokens, MAX_TOKENS, nullptr, &errors);

    TokenStream ts;
    memcpy(ts.tokens, tokens, count * sizeof(Token));
    ts.count = count;
    ts.pos = 0;

    GraphBuildResult result = build_output_graph(pool, ts, cells, arena);
    REQUIRE(!result.has_error);
    REQUIRE(result.root_node != NODE_NONE);

    // Should be constant-folded to 3.0
    REQUIRE(pool.nodes[result.root_node].op == NodeOp::Const);
    REQUIRE(pool.nodes[result.root_node].imm == 3.0);
}

TEST_CASE("Graph builder: nested arithmetic folds", "[signal_engine][graph_builder]") {
    NodePool pool;
    CellStore cells;
    SourceArena arena;

    const char* src = "(+ (* 2 3) (- 10 4))";
    Token tokens[MAX_TOKENS];
    uint8_t errors = 0;
    uint16_t count = TokenStream::tokenize(src, (uint32_t)strlen(src), tokens, MAX_TOKENS, nullptr, &errors);

    TokenStream ts;
    memcpy(ts.tokens, tokens, count * sizeof(Token));
    ts.count = count;
    ts.pos = 0;

    GraphBuildResult result = build_output_graph(pool, ts, cells, arena);
    REQUIRE(!result.has_error);
    REQUIRE(pool.nodes[result.root_node].op == NodeOp::Const);
    REQUIRE(pool.nodes[result.root_node].imm == 12.0);
}

TEST_CASE("Graph builder: time reference", "[signal_engine][graph_builder]") {
    NodePool pool;
    CellStore cells;
    SourceArena arena;

    const char* src = "(* t 2)";
    Token tokens[MAX_TOKENS];
    uint8_t errors = 0;
    uint16_t count = TokenStream::tokenize(src, (uint32_t)strlen(src), tokens, MAX_TOKENS, nullptr, &errors);

    TokenStream ts;
    memcpy(ts.tokens, tokens, count * sizeof(Token));
    ts.count = count;
    ts.pos = 0;

    GraphBuildResult result = build_output_graph(pool, ts, cells, arena);
    REQUIRE(!result.has_error);

    // Should NOT be constant-folded (contains t)
    REQUIRE(pool.nodes[result.root_node].op == NodeOp::Mul);
}

TEST_CASE("Cold eval: define and output", "[signal_engine][cold_eval]") {
    NodePool pool;
    CellStore cells;
    SourceArena arena;

    // Initialize timing cells
    SymbolID bpm_sym = internSymbol("bpm");
    cells.cells[bpm_sym].kind = CellKind::Number;
    cells.cells[bpm_sym].value = 120.0;

    // Define a constant
    EvalResult r1 = eval_cold("(define freq 440)", 17, cells, arena, pool);
    REQUIRE(r1.kind == EvalResult::Ok);

    SymbolID freq_sym = SymbolIntern::getInstance().getID("freq");
    REQUIRE(freq_sym != SymbolIntern::INVALID_ID);
    REQUIRE(cells.cells[freq_sym].kind == CellKind::Number);
    REQUIRE(cells.cells[freq_sym].value == 440.0);

    // Assign output
    EvalResult r2 = eval_cold("(a1 (+ 1 2))", 12, cells, arena, pool);
    REQUIRE(r2.kind == EvalResult::Ok);
    REQUIRE(pool.outputs[0].root_node != NODE_NONE);
    REQUIRE(pool.outputs[0].valid);

    // Execute and check
    pool.rebuild_execution_order();
    double cell_vals[MAX_CELLS] = {};
    cells.snapshot_values(cell_vals, MAX_CELLS);
    double hw_inputs[32] = {};
    double outputs[MAX_OUTPUTS] = {};
    double workspace[MAX_TOTAL_NODES] = {};

    execute_all_outputs(pool, 0.0, cell_vals, hw_inputs,
                        cells.data_pool, cells.data_offsets, cells.data_lengths,
                        pool.prev_output_values, outputs, workspace);

    REQUIRE(outputs[0] == 3.0);
}

TEST_CASE("Cold eval: beat phasor at 120 bpm", "[signal_engine][cold_eval]") {
    NodePool pool;
    CellStore cells;
    SourceArena arena;

    // Set up timing cells
    SymbolID bpm_sym = internSymbol("bpm");
    cells.cells[bpm_sym].kind = CellKind::Number;
    cells.cells[bpm_sym].value = 120.0;

    SymbolID bpb_sym = internSymbol("beats-per-bar");
    cells.cells[bpb_sym].kind = CellKind::Number;
    cells.cells[bpb_sym].value = 4.0;

    // Assign beat to output
    EvalResult r = eval_cold("(a1 beat)", 9, cells, arena, pool);
    REQUIRE(r.kind == EvalResult::Ok);

    pool.rebuild_execution_order();
    double cell_vals[MAX_CELLS] = {};
    cells.snapshot_values(cell_vals, MAX_CELLS);
    double hw_inputs[32] = {};
    double outputs[MAX_OUTPUTS] = {};
    double workspace[MAX_TOTAL_NODES] = {};

    // At t=0, beat should be 0
    execute_all_outputs(pool, 0.0, cell_vals, hw_inputs,
                        cells.data_pool, cells.data_offsets, cells.data_lengths,
                        pool.prev_output_values, outputs, workspace);
    REQUIRE(outputs[0] == Approx(0.0).margin(1e-9));

    // At t=0.25 (half a beat at 120bpm), beat should be 0.5
    execute_all_outputs(pool, 0.25, cell_vals, hw_inputs,
                        cells.data_pool, cells.data_offsets, cells.data_lengths,
                        pool.prev_output_values, outputs, workspace);
    REQUIRE(outputs[0] == Approx(0.5).margin(1e-9));

    // At t=0.5 (one full beat at 120bpm), beat should wrap to 0
    execute_all_outputs(pool, 0.5, cell_vals, hw_inputs,
                        cells.data_pool, cells.data_offsets, cells.data_lengths,
                        pool.prev_output_values, outputs, workspace);
    REQUIRE(outputs[0] == Approx(0.0).margin(1e-9));
}
