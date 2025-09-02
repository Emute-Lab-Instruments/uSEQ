#ifndef USEQ_DSP_H_
#define USEQ_DSP_H_

#include "uSEQ/configure.h"

#ifdef ARDUINO
#ifdef ENABLE_DSP_ENGINE

////////////////////////////////////////////////////////////////////////////////
/// DSP AND AUDIO PROCESSING LISP FUNCTION DECLARATIONS
/// This header contains DSP-related LISP function declarations and data types
/// that need to be included within the uSEQ class definition.
////////////////////////////////////////////////////////////////////////////////

#include "dsp/dsp-engine.hpp"
#include <unordered_map>
#include <memory>
#include "utils/string.h"

// DSP Engine Management
LISP_FUNC_DECL(useq_dsp_start);
LISP_FUNC_DECL(useq_dsp_stop);
LISP_FUNC_DECL(useq_dsp_create);
LISP_FUNC_DECL(useq_dsp_kill);
LISP_FUNC_DECL(useq_dsp_connect);
LISP_FUNC_DECL(useq_dsp_disconnect);
LISP_FUNC_DECL(useq_dsp_getugens);
LISP_FUNC_DECL(useq_dsp_qget);
LISP_FUNC_DECL(useq_dsp_qset);
LISP_FUNC_DECL(useq_dsp_reset);
LISP_FUNC_DECL(useq_dsp_message);
LISP_FUNC_DECL(useq_dsp_listqueues);

// DSP-related data structures and types
struct ugenOutputQueue {
    queue_t *q;
    size_t index;
    size_t queueSize;
    size_t key;
    std::vector<float> lastValue = {0.f}; // TODO: expand for list outputs
};

struct ugenInputQueue {
    queue_t *q;
    size_t index;
    size_t queueSize;
    size_t key;
};

struct dsp_engine_info {
    std::unique_ptr<uSEQDSPEngine> obj;
    size_t nextKey = 0;

    std::unordered_map<size_t, String> ugenInstances;
    std::unordered_map<size_t, ugenOutputQueue> ugenOutputQueues;
    std::unordered_map<size_t, ugenInputQueue> ugenInputQueues;
};

#endif // ENABLE_DSP_ENGINE
#endif // ARDUINO

#endif // USEQ_DSP_H_