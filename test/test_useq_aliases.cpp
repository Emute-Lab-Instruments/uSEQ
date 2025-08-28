#include "../uSEQ/src/uSEQ.h"
#include "../uSEQ/src/lisp/value.h"
#include "../uSEQ/src/lisp/environment.h"
#include <cassert>
#include <iostream>

#define ASSERT_TRUE(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "ASSERT TRUE failed at line " << __LINE__ << std::endl; \
            assert(false); \
        } \
    } while (0)

#define ASSERT_NEAR(expected, actual, tol) \
    do { \
        if (std::abs((expected) - (actual)) > (tol)) { \
            std::cerr << "ASSERT NEAR failed at line " << __LINE__ \
                      << ": expected " << (expected) << ", got " << (actual) \
                      << ", tol " << (tol) << std::endl; \
            assert(false); \
        } \
    } while (0)

static void run_tick_cycles(uSEQ &useq, int n)
{
    for (int i = 0; i < n; ++i) {
        useq.tick();
    }
}

int main()
{
    uSEQ useq;
    useq.init();

    // 1) Tempo alias: (bpm 140)
    useq.eval("(bpm 140)");
    auto bpmValOpt = useq.get("bpm");
    ASSERT_TRUE(bpmValOpt.has_value());
    ASSERT_NEAR(140.0, bpmValOpt->as_float(), 1e-6);

    // 2) Time signature alias: (time-signature 5 8)
    useq.eval("(time-signature 5 8)");
    // No direct getter; rely on no-crash and subsequent ticks.
    run_tick_cycles(useq, 2);

    // 3) CV aliases: (cv 1 expr) and cv-1 name
    useq.eval("(cv 1 0.5)");
    run_tick_cycles(useq, 2);
    Value getCv1 = useq.eval_v("(get-cv 1)");
    ASSERT_NEAR(0.5, getCv1.as_float(), 1e-3);

    useq.eval("(cv-1 (* 0.25 2.0))");
    run_tick_cycles(useq, 2);
    Value getCv1b = useq.eval_v("(get-cv 1)");
    ASSERT_NEAR(0.5, getCv1b.as_float(), 1e-3);

    // 4) Gate aliases: (gate 2 expr) and gate-2 name
    useq.eval("(gate 2 1)");
    run_tick_cycles(useq, 2);
    Value getG2 = useq.eval_v("(get-gate 2)");
    ASSERT_NEAR(1.0, getG2.as_float(), 1e-6);

    useq.eval("(gate-2 0)");
    run_tick_cycles(useq, 2);
    Value getG2b = useq.eval_v("(get-gate 2)");
    ASSERT_NEAR(0.0, getG2b.as_float(), 1e-6);

    std::cout << "test_useq_aliases passed!" << std::endl;
    return 0;
}

