#ifndef SIGNAL_ENGINE_PROGRAM_SEMANTICS_H
#define SIGNAL_ENGINE_PROGRAM_SEMANTICS_H

#include "build_profile.h"
#include "cell_store.h"
#include "node_pool.h"

#if USEQ_HAS_TEMPORAL_QUERY
#include <vector>
#endif

namespace sig {

// Monotone facts accumulated from the reachable compiled DAG.  These are
// deliberately about denotational dependencies, not optimisation guesses.
enum ProgramEffect : uint32_t {
    EffectNone           = 0,
    EffectLocalTime      = 1u << 0,
    EffectGroundTime     = 1u << 1,
    EffectCell           = 1u << 2,
    EffectHardwareInput  = 1u << 3,
    EffectLiveInput      = 1u << 4,
    EffectPreviousOutput = 1u << 5,
    EffectState          = 1u << 6,
    EffectDeltaTime      = 1u << 7,
    EffectDataTable      = 1u << 8,
    EffectDiscontinuous  = 1u << 9,
    EffectMayBeNonFinite = 1u << 10,
};

struct ProgramAnalysis {
    uint32_t effects = EffectNone;
    uint16_t reachable_nodes = 0;
    bool valid_finite_dag = false;

    bool stateless() const {
        return (effects & (EffectState | EffectPreviousOutput)) == 0;
    }

    bool schedule_independent() const {
        return (effects & (EffectState | EffectPreviousOutput |
                           EffectDeltaTime)) == 0;
    }

    // With cells and data tables frozen by ProgramView, the denotation has
    // exactly one varying input: local time.
    bool closed_function_of_time() const {
        constexpr uint32_t extra_inputs =
            EffectGroundTime | EffectHardwareInput | EffectLiveInput |
            EffectPreviousOutput | EffectState | EffectDeltaTime;
        return valid_finite_dag && (effects & extra_inputs) == 0;
    }

    bool guaranteed_to_terminate() const { return valid_finite_dag; }
};

ProgramAnalysis analyze_program(const NodePool& pool, uint16_t root_node);

#if USEQ_HAS_TEMPORAL_QUERY

// Owning immutable snapshot for proof/query code.  Every value observable by
// the reference evaluator is copied at construction, so later compiler,
// cell, live-input, state, or LKG mutations cannot change its denotation.
class ProgramView {
public:
    const std::vector<Node>& nodes() const { return nodes_; }
    const std::vector<Sample>& cell_values() const { return cell_values_; }
    const std::vector<Sample>& data_pool() const { return data_pool_; }
    const std::vector<uint16_t>& data_offsets() const { return data_offsets_; }
    const std::vector<uint16_t>& data_lengths() const { return data_lengths_; }
    const std::vector<Sample>& live_slot_values() const {
        return live_slot_values_;
    }
    const std::vector<Sample>& previous_outputs() const {
        return previous_outputs_;
    }
    const std::vector<Sample>& state_values() const { return state_values_; }
    uint16_t root_node() const { return root_node_; }
    const ProgramAnalysis& analysis() const { return analysis_; }

private:
    std::vector<Node> nodes_;
    std::vector<Sample> cell_values_;
    std::vector<Sample> data_pool_;
    std::vector<uint16_t> data_offsets_;
    std::vector<uint16_t> data_lengths_;
    std::vector<Sample> live_slot_values_;
    std::vector<Sample> previous_outputs_;
    std::vector<Sample> state_values_;
    uint16_t root_node_ = NODE_NONE;
    ProgramAnalysis analysis_ = {};

    friend ProgramView make_program_view(const NodePool&, const CellStore&,
                                         uint16_t);
};

ProgramView make_program_view(const NodePool& pool, const CellStore& cells,
                              uint16_t root_node);

struct ReferenceEnvironment {
    Sample local_time = 0.0;
    Sample ground_time = 0.0;
    Sample wall_dt = 0.0;
    const Sample* hardware_inputs = nullptr;
    const Sample* previous_outputs = nullptr;
    const Sample* state_values = nullptr;
};

enum class ReferenceEvalStatus : uint8_t {
    Complete,
    InvalidProgram,
    Cycle,
    InvalidReference,
};

struct ReferenceEvalResult {
    ReferenceEvalStatus status = ReferenceEvalStatus::InvalidProgram;
    Sample value = 0.0;
    uint16_t detail_node = NODE_NONE;
};

// Independent, recursive evaluator for conformance and paper witnesses.  It
// does not use exec_order, executor workspaces, LKG, or executor math helpers.
ReferenceEvalResult evaluate_reference(const ProgramView& program,
                                       const ReferenceEnvironment& env);

#endif

} // namespace sig

#endif // SIGNAL_ENGINE_PROGRAM_SEMANTICS_H
