#ifndef REEVALUATE_SCOPE_H_
#define REEVALUATE_SCOPE_H_

#include "modulisp_interpreter.h"

/**
 * RAII helper to temporarily enable re-evaluation of time-dependent expressions.
 *
 * When symbols are defined in terms of time variables (t, beat, bar, etc.),
 * their stored values become stale as time progresses. This scope helper
 * enables re-evaluation mode for the duration of the scope, ensuring that
 * time-dependent expressions are freshly evaluated rather than returning
 * cached values.
 *
 * Example usage:
 * ```cpp
 * // Define a time-dependent symbol
 * eval("(def phase (+ t 0.5))");
 *
 * // Later, in update loop:
 * {
 *     ReevaluateScope scope;  // Enable re-evaluation
 *     Value result = eval("phase");  // Gets current t + 0.5
 *     // ...
 * }  // Scope exits, restores previous state
 * ```
 *
 * Technical details:
 * The environment stores both evaluated values (m_defs) and unevaluated
 * expressions (m_def_exprs) for each definition. Normally, symbol lookup
 * returns the cached value from m_defs. When re-evaluation is enabled,
 * symbol lookup instead re-evaluates the expression from m_def_exprs,
 * giving access to current time variable values.
 *
 * This is particularly important for:
 * - Output expressions evaluated in the update loop
 * - Scheduled expressions that depend on timing
 * - Any code that needs fresh time-dependent values
 */
class ReevaluateScope
{
    bool m_previous_value;

public:
    /**
     * Construct scope and enable re-evaluation of time-dependent expressions.
     * Saves the previous state for restoration on destruction.
     */
    ReevaluateScope()
        : m_previous_value(ModuLispInterpreter::get_attempt_expr_eval_first())
    {
        ModuLispInterpreter::set_attempt_expr_eval_first(true);
    }

    /**
     * Destroy scope and restore previous re-evaluation state.
     * This ensures nested scopes work correctly.
     */
    ~ReevaluateScope()
    {
        ModuLispInterpreter::set_attempt_expr_eval_first(m_previous_value);
    }

    // Prevent copying and moving (RAII class)
    ReevaluateScope(const ReevaluateScope&)            = delete;
    ReevaluateScope& operator=(const ReevaluateScope&) = delete;
    ReevaluateScope(ReevaluateScope&&)                 = delete;
    ReevaluateScope& operator=(ReevaluateScope&&)      = delete;
};

/**
 * Alternative name for clarity in specific contexts.
 * Same functionality as ReevaluateScope.
 */
using TimeDependentEvaluationScope = ReevaluateScope;

#endif // REEVALUATE_SCOPE_H_
