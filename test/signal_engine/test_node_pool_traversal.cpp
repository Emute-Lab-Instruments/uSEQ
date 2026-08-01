// Adversarial fixed-stack traversal tests for NodePool reachability.
//
// A high-sharing DAG can have many more incoming edges than nodes. A traversal
// that marks nodes only when popped may enqueue the same shared child hundreds
// of times, fill its MAX_TOTAL_NODES stack, and silently drop an undiscovered
// dependency. Mark-on-discovery keeps the stack bounded by unique nodes.

#define CATCH_CONFIG_MAIN
#include "../catch.hpp"

#include "src/signal_engine/node_pool.h"

using namespace sig;

namespace {

void build_duplicate_push_adversary(NodePool& pool) {
    pool.reset();

    Node leaf;
    leaf.op = NodeOp::Const;
    leaf.flags = FLAG_TIME_INVARIANT;
    leaf.imm = 0.0;
    pool.nodes[0] = leaf;

    // input_c is visited first by the LIFO walk, so the traversal descends the
    // entire chain while two duplicate references to node 0 accumulate at
    // every level. The old mark-on-pop traversal filled its fixed stack around
    // half way down and dropped the next chain node.
    for (uint16_t i = 1; i < MAX_TOTAL_NODES; i++) {
        Node node;
        node.op = NodeOp::Select;
        node.input_a = 0;
        node.input_b = 0;
        node.input_c = (uint16_t)(i - 1);
        pool.nodes[i] = node;
    }
    pool.node_count = MAX_TOTAL_NODES;
    pool.outputs[0].root_node = (uint16_t)(MAX_TOTAL_NODES - 1);
    pool.outputs[0].valid = true;
}

void require_complete_forward_order(const NodePool& pool) {
    REQUIRE(pool.exec_count == MAX_TOTAL_NODES);
    for (uint16_t i = 0; i < MAX_TOTAL_NODES; i++) {
        INFO("execution position " << i);
        REQUIRE(pool.exec_order[i] == i);
    }
}

} // namespace

TEST_CASE("execution-order traversal does not drop a high-sharing dependency",
          "[node_pool][traversal][adversarial]") {
    NodePool pool;
    build_duplicate_push_adversary(pool);

    pool.rebuild_execution_order();

    require_complete_forward_order(pool);
}

TEST_CASE("GC preserves a full high-sharing reachable DAG",
          "[node_pool][gc][adversarial]") {
    NodePool pool;
    build_duplicate_push_adversary(pool);

    pool.gc_unreachable_nodes();

    REQUIRE(pool.node_count == MAX_TOTAL_NODES);
    REQUIRE(pool.outputs[0].root_node == MAX_TOTAL_NODES - 1);
    for (uint16_t i = 1; i < MAX_TOTAL_NODES; i++) {
        INFO("node " << i);
        REQUIRE(pool.nodes[i].input_a == 0);
        REQUIRE(pool.nodes[i].input_b == 0);
        REQUIRE(pool.nodes[i].input_c == i - 1);
    }

    pool.rebuild_execution_order();
    require_complete_forward_order(pool);
}
