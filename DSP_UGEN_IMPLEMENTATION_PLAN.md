# Dynamic DAC Routing Implementation Plan for uSEQ

## Executive Summary

This document provides a comprehensive analysis and implementation plan for adding dynamic DAC routing to the uSEQ firmware, allowing DAC outputs to be controlled by either the DSP core or Modulisp core dynamically, with the introduction of DSP ugens that can be created, patched, and garbage collected.

## Current Architecture Analysis

### 1. System Overview
- **Platform**: RP2040 microcontroller with dual ARM Cortex-M0+ cores
- **Firmware**: Arduino-based, running at 250MHz
- **Language**: Modulisp (custom LISP implementation)
- **Outputs**: 2-8 DAC outputs (hardware configuration dependent)

### 2. Current DAC Control System

#### Hardware Configuration
- **MUSICTHING**: 2 continuous outputs (a1, a2) + 2 binary outputs
- **USEQHARDWARE_1_0**: 3 continuous outputs + 3 binary outputs
- **USEQHARDWARE_0_2**: 2 continuous outputs + 4 binary outputs

#### Current Implementation
- DAC outputs are hardcoded to specific functions:
  - `a1`, `a2` (etc.) functions directly control DAC outputs via PWM
  - All outputs evaluated in main loop on single core
  - Output values cached in `m_continuous_vals[]` array
  - Hardware writing via `analog_write_with_led()` using PIO PWM

#### Update Flow
1. `tick()` called in main loop
2. `update_signals()` evaluates all output expressions
3. `update_continuous_signals()` evaluates each `m_continuous_ASTs[i]`
4. Results cached in `m_continuous_vals[i]`
5. `update_outs()` writes cached values to hardware

### 3. Modulisp Expression System

- **Value Types**: INT, FLOAT, LIST, ATOM, LAMBDA, BUILTIN, etc.
- **Environment**: Variable bindings with time-based evaluation
- **Evaluation**: Recursive AST evaluation with time context
- **Current Outputs**: Direct expression evaluation per tick

## Proposed Architecture

### 1. Core Separation

#### Core 0 (DSP Core)
- Runs high-priority audio DSP processing
- Manages stateful ugen graph
- Processes incoming control messages from Core 1
- Directly writes to assigned DAC outputs
- Maintains ugen instances and connections

#### Core 1 (Modulisp Core)
- Runs existing Modulisp interpreter
- Evaluates functional expressions
- Sends control messages to DSP core
- Manages ugen lifecycle (creation/deletion)
- Handles user interaction and serial I/O

### 2. New DSP_UGEN Type

#### Value Type Extension
```cpp
// In value.h
enum {
    // ... existing types ...
    DSP_UGEN,  // New type for DSP ugen references
    // ...
} type;

// Additional storage in Value union
union {
    // ... existing members ...
    uint32_t ugen_id;  // ID for DSP ugen reference
} stack_data;
```

#### DSP Ugen Base Class
```cpp
class DSPUgen {
public:
    uint32_t id;
    uint32_t ref_count;
    enum UgenType { LFSAW, ADSR, VCA, FILTER, etc };
    UgenType type;

    virtual float process() = 0;
    virtual void set_param(int param_id, float value) = 0;
    virtual void trigger(float value) = 0;
};
```

### 3. Inter-Core Communication

#### Message Queue System
```cpp
struct DSPMessage {
    enum Type {
        CREATE_UGEN,
        DELETE_UGEN,
        SET_PARAM,
        CONNECT_UGENS,
        ASSIGN_OUTPUT,
        TRIGGER_UGEN
    };
    Type type;
    uint32_t ugen_id;
    uint32_t param_id;
    float value;
    uint32_t target_id;  // for connections
};

// Lockless ring buffer for Core1->Core0 messages
class MessageQueue {
    DSPMessage buffer[256];
    volatile uint8_t write_idx;
    volatile uint8_t read_idx;
};
```

### 4. Dynamic Output Assignment

#### Output Controller
```cpp
class OutputController {
    enum ControlSource { MODULISP, DSP };

    struct OutputState {
        ControlSource source;
        union {
            Value* modulisp_expr;  // For Modulisp control
            uint32_t dsp_ugen_id;  // For DSP control
        };
    } outputs[NUM_CONTINUOUS_OUTS];

    void assign_to_dsp(int output, uint32_t ugen_id);
    void assign_to_modulisp(int output, Value expr);
};
```

### 5. Reference Counting & GC

#### Ugen Manager
```cpp
class UgenManager {
    std::map<uint32_t, std::unique_ptr<DSPUgen>> ugens;
    std::map<uint32_t, std::set<uint32_t>> references;  // who references whom

    uint32_t create_ugen(DSPUgen::UgenType type);
    void add_reference(uint32_t from_id, uint32_t to_id);
    void remove_reference(uint32_t from_id, uint32_t to_id);
    void garbage_collect();  // Remove ugens with ref_count == 0
};
```

## Implementation Steps

### Phase 1: Infrastructure (Week 1-2)

1. **Add DSP_UGEN to Value type system**
   - Extend Value enum and union
   - Add type checking methods (`is_dsp_ugen()`)
   - Implement display/debug methods

2. **Create basic DSPUgen base class**
   - Define interface for all ugens
   - Implement ID and reference counting

3. **Set up inter-core communication**
   - Implement lockless message queue
   - Add message types and structures
   - Test basic message passing

### Phase 2: Core Separation (Week 2-3)

1. **Separate main loop into two cores**
   - Move tick() to Core 1
   - Create DSP processing loop on Core 0
   - Maintain backward compatibility

2. **Implement OutputController**
   - Track output assignments
   - Route outputs to appropriate core
   - Handle transitions between control sources

3. **Create UgenManager**
   - Ugen creation/deletion
   - Reference tracking
   - Basic garbage collection

### Phase 3: DSP Ugens (Week 3-4)

1. **Implement basic ugens**
   ```cpp
   class LFSawUgen : public DSPUgen {
       float frequency;
       float phase;
       float process() override {
           phase += frequency / SAMPLE_RATE;
           if (phase >= 1.0) phase -= 1.0;
           return phase * 2.0 - 1.0;
       }
   };
   ```

2. **Implement ADSR envelope**
   ```cpp
   class ADSRUgen : public DSPUgen {
       float attack, decay, sustain, release;
       float gate, level;
       enum State { IDLE, ATTACK, DECAY, SUSTAIN, RELEASE } state;
       float process() override;
       void trigger(float value) override;
   };
   ```

3. **Implement patching system**
   - Connection management
   - Parameter modulation
   - Trigger routing

### Phase 4: Lisp API (Week 4-5)

1. **Constructor functions**
   ```lisp
   (define my-saw (dsp-lfsaw 440))
   (define my-env (dsp-adsr 0.1 0.2 0.5 0.3))
   ```

2. **Patching function**
   ```lisp
   (dsp-patch source target :param-name)
   (dsp-patch my-trig my-env :gate)
   ```

3. **Output assignment**
   ```lisp
   (a1 my-saw)  ; Assigns DSP ugen to output
   (a1 (slow 2 (usin bar)))  ; Falls back to Modulisp
   ```

### Phase 5: Memory Management (Week 5-6)

1. **Implement reference counting**
   - Track references in environment
   - Update counts on reassignment
   - Handle circular references

2. **Garbage collection**
   - Mark-and-sweep for unreferenced ugens
   - Safe deletion with message passing
   - Memory pool for ugen allocation

3. **Resource limits**
   - Maximum ugen count
   - CPU usage monitoring
   - Graceful degradation

## Technical Considerations

### 1. Timing & Synchronization
- DSP core runs at audio rate (e.g., 48kHz)
- Modulisp core runs at control rate (existing tick rate)
- Message queue handles async communication
- Phase alignment for mixed control

### 2. Memory Management
- Fixed pool of ugens to avoid dynamic allocation
- Careful reference counting to prevent leaks
- Consider using indexes instead of pointers

### 3. Backward Compatibility
- Existing Modulisp code must continue working
- Gradual migration path for users
- Clear documentation of new features

### 4. Performance
- DSP core has higher priority
- Minimize message passing overhead
- Profile and optimize critical paths
- Consider SIMD operations for DSP

## Testing Strategy

### 1. Unit Tests
- Value type operations
- Message queue reliability
- Ugen processing accuracy
- Reference counting correctness

### 2. Integration Tests
- Multi-core communication
- Output assignment switching
- Complex patch configurations
- Memory leak detection

### 3. Performance Tests
- Maximum ugen count
- CPU usage per ugen type
- Message throughput limits
- Latency measurements

### 4. User Acceptance Tests
- Example patches
- Migration of existing code
- Documentation clarity
- Error handling

## Risk Mitigation

### 1. Technical Risks
- **Risk**: Inter-core synchronization issues
  - **Mitigation**: Use proven lockless algorithms, extensive testing

- **Risk**: Memory fragmentation
  - **Mitigation**: Fixed-size memory pools, static allocation

- **Risk**: Performance degradation
  - **Mitigation**: Profiling, optimization, configurable limits

### 2. User Experience Risks
- **Risk**: Breaking changes
  - **Mitigation**: Maintain backward compatibility, clear migration guide

- **Risk**: Complexity increase
  - **Mitigation**: Simple API, good defaults, extensive examples

## Example Usage

### Basic Synthesis
```lisp
; Create a saw wave with envelope
(define osc (dsp-lfsaw 440))
(define env (dsp-adsr 0.01 0.1 0.7 0.5))
(define vca (dsp-vca))

; Patch oscillator through VCA
(dsp-patch osc vca :input)
(dsp-patch env vca :gain)

; Trigger envelope with beat
(dsp-patch (sqr beat) env :gate)

; Output to DAC
(a1 vca)
```

### FM Synthesis
```lisp
; Modulator and carrier
(define modulator (dsp-sine 220))
(define carrier (dsp-sine 440))

; FM depth control
(define depth (dsp-vca))
(dsp-patch modulator depth :input)
(dsp-patch 100 depth :gain)  ; Modulation index

; Apply FM
(dsp-patch depth carrier :fm)

; Output with envelope
(define env (dsp-adsr 0.001 0.1 0.5 1.0))
(define out (dsp-vca))
(dsp-patch carrier out :input)
(dsp-patch env out :gain)
(dsp-patch (sqr (slow 2 beat)) env :gate)

(a1 out)
```

### Mixed Control
```lisp
; DSP oscillator with Modulisp LFO
(define osc (dsp-lfsaw 440))
(define lfo-freq (* 20 (usin (slow 8 bar))))

; Use Modulisp expression to modulate DSP param
(dsp-patch lfo-freq osc :frequency)

; Mix DSP and Modulisp outputs
(a1 osc)                          ; DSP ugen
(a2 (slow 2 (usin bar)))         ; Pure Modulisp
```

## Conclusion

This implementation plan provides a clear path to adding dynamic DAC routing with DSP ugens to the uSEQ firmware. The design maintains backward compatibility while enabling powerful new synthesis capabilities. The phased approach allows for incremental development and testing, reducing risk and ensuring stability.

The key innovation is the seamless integration of stateful DSP processing with the existing functional Modulisp system, allowing users to leverage the strengths of both paradigms. This will significantly expand the sonic possibilities of the uSEQ module while maintaining its live-coding philosophy.

## Next Steps

1. Review and approve implementation plan
2. Set up development environment with dual-core debugging
3. Create proof-of-concept for inter-core messaging
4. Begin Phase 1 implementation
5. Establish testing framework