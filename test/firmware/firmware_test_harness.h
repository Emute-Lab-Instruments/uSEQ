#ifndef TEST_FIRMWARE_TEST_HARNESS_H
#define TEST_FIRMWARE_TEST_HARNESS_H

// E2E test harness for the firmware — runs the full tick loop on desktop
// with deterministic time, simulated inputs, and output capture.
//
// Usage:
//   FirmwareTestHarness h;
//   h.init();
//   h.eval("(a1 (usin beat))");
//   h.set_input_sine(2, 10.0);     // ain1 = 10Hz sine
//   h.start_capture();
//   h.run_ticks(1000);             // 1 second at 1kHz
//   auto out = h.get_captured(0);  // a1 output buffer
//   REQUIRE(dsp::all_finite(out));

#include "../../uSEQ/src/firmware/firmware.h"
#include "../../uSEQ/src/firmware/flash_storage.h"
#include "../../uSEQ/src/signal_engine/cold_eval.h"
#include "../../uSEQ/src/signal_engine/graph_builder.h"
#include "../../uSEQ/src/signal_engine/executor.h"
#include "../../uSEQ/src/utils/time.h"
#include "../../uSEQ/src/ports/mocks/MockStorage.h"

#include <vector>
#include <cmath>
#include <cstring>
#include <functional>

// ── Input Waveform Types ──────────────────────────────────────────────────

enum class WaveformType { NONE, SINE, TRIANGLE, SQUARE, SAW, NOISE, CUSTOM };

struct InputWaveform {
    WaveformType type = WaveformType::NONE;
    double frequency  = 1.0;   // Hz
    double amplitude  = 0.5;   // half-swing (output = offset ± amplitude)
    double offset     = 0.5;   // DC center
    double phase      = 0.0;   // initial phase in radians
    std::function<double(double)> custom_fn;  // for CUSTOM type
};

// ── Firmware Test Harness ─────────────────────────────────────────────────

class FirmwareTestHarness {
public:
    // ── Configuration ─────────────────────────────────────────────────────
    double tick_rate = 1000.0;  // Hz (default 1kHz)

    // ── Setup / Teardown ──────────────────────────────────────────────────

    void init()
    {
        // Install deterministic time
        sim_time_ = 0.0;
        g_test_time_ptr = &sim_time_;

        // Init signal engine symbols (must happen before any eval)
        sig::GraphBuilder::init_symbols();

        // Init firmware (on desktop: no-op for hardware, sets up engine)
        fw.engine.init_defaults();
        fw.engine.state.is_playing = true;

        // Inject mock storage for flash
        storage_ = MockStorage(65536);
        fw.flash.set_storage(&storage_);

        tick_count_ = 0;
    }

    ~FirmwareTestHarness()
    {
        // Remove time override
        g_test_time_ptr = nullptr;
    }

    // ── Command Injection ─────────────────────────────────────────────────

    sig::EvalResult eval(const char* code)
    {
        return sig::eval_cold(code, static_cast<uint32_t>(std::strlen(code)),
                              fw.engine);
    }

    sig::EvalResult eval(const std::string& code)
    {
        return eval(code.c_str());
    }

    // ── Input Simulation ──────────────────────────────────────────────────

    void set_input_waveform(int channel, InputWaveform wf)
    {
        if (channel >= 0 && channel < static_cast<int>(firmware::MAX_HW_INPUTS))
            input_waveforms_[channel] = wf;
    }

    void set_input_sine(int channel, double freq_hz,
                        double amplitude = 0.5, double offset = 0.5)
    {
        set_input_waveform(channel, {WaveformType::SINE, freq_hz, amplitude, offset});
    }

    void set_input_square(int channel, double freq_hz,
                          double amplitude = 0.5, double offset = 0.5)
    {
        set_input_waveform(channel, {WaveformType::SQUARE, freq_hz, amplitude, offset});
    }

    void set_input_triangle(int channel, double freq_hz,
                            double amplitude = 0.5, double offset = 0.5)
    {
        set_input_waveform(channel, {WaveformType::TRIANGLE, freq_hz, amplitude, offset});
    }

    void set_input_saw(int channel, double freq_hz,
                       double amplitude = 0.5, double offset = 0.5)
    {
        set_input_waveform(channel, {WaveformType::SAW, freq_hz, amplitude, offset});
    }

    void set_input_value(int channel, double value)
    {
        set_input_waveform(channel, {WaveformType::CUSTOM, 0, 0, 0, 0,
                                     [value](double) { return value; }});
    }

    void set_input_custom(int channel, std::function<double(double)> fn)
    {
        set_input_waveform(channel, {WaveformType::CUSTOM, 0, 0, 0, 0, fn});
    }

    void clear_input(int channel)
    {
        if (channel >= 0 && channel < static_cast<int>(firmware::MAX_HW_INPUTS))
            input_waveforms_[channel].type = WaveformType::NONE;
    }

    // ── Tick Execution ────────────────────────────────────────────────────

    // Run a single tick at the current simulated time.
    void tick()
    {
        // Update inputs from waveform generators
        update_simulated_inputs();

        // Run the firmware tick (which reads sim_time_ via g_test_time_ptr)
        fw.tick();

        // Capture outputs if recording
        if (capturing_) {
            OutputSnapshot snap;
            snap.time = sim_time_;
            std::memcpy(snap.values, fw.output_values,
                        sizeof(double) * sig::MAX_OUTPUTS);
            captured_.push_back(snap);
        }

        // Advance simulated time
        sim_time_ += 1.0 / tick_rate;
        tick_count_++;
    }

    // Run N ticks.
    void run_ticks(int n)
    {
        for (int i = 0; i < n; i++) tick();
    }

    // Run ticks for a given duration (seconds).
    void run_for(double seconds)
    {
        int n = static_cast<int>(seconds * tick_rate);
        run_ticks(n);
    }

    // ── Time Control ──────────────────────────────────────────────────────

    double current_time() const { return sim_time_; }
    uint64_t ticks_elapsed() const { return tick_count_; }

    void set_time(double t)
    {
        sim_time_ = t;
    }

    void advance_time(double dt)
    {
        sim_time_ += dt;
    }

    // ── Output Capture ────────────────────────────────────────────────────

    void start_capture()
    {
        captured_.clear();
        capturing_ = true;
    }

    void stop_capture()
    {
        capturing_ = false;
    }

    // Get the captured output for a specific channel as a vector.
    std::vector<double> get_captured(int channel) const
    {
        std::vector<double> result;
        result.reserve(captured_.size());
        for (const auto& snap : captured_) {
            result.push_back(snap.values[channel]);
        }
        return result;
    }

    // Get captured time values.
    std::vector<double> get_captured_times() const
    {
        std::vector<double> result;
        result.reserve(captured_.size());
        for (const auto& snap : captured_) {
            result.push_back(snap.time);
        }
        return result;
    }

    size_t capture_length() const { return captured_.size(); }

    void clear_capture() { captured_.clear(); }

    // ── Direct Access ─────────────────────────────────────────────────────

    double get_output(int channel) const { return fw.output_values[channel]; }
    double get_input(int channel) const { return fw.io.inputs[channel]; }

    firmware::Firmware& firmware() { return fw; }
    sig::SignalEngine& engine() { return fw.engine; }
    MockStorage& storage() { return storage_; }

private:
    firmware::Firmware fw;
    MockStorage storage_{65536};

    // Time
    double sim_time_ = 0.0;
    uint64_t tick_count_ = 0;

    // Input waveforms
    InputWaveform input_waveforms_[firmware::MAX_HW_INPUTS] = {};

    // Output capture
    struct OutputSnapshot {
        double time;
        double values[sig::MAX_OUTPUTS];
    };
    std::vector<OutputSnapshot> captured_;
    bool capturing_ = false;

    // ── Internal: generate waveform sample at time t ──────────────────────

    double sample_waveform(const InputWaveform& wf, double t) const
    {
        switch (wf.type) {
        case WaveformType::NONE:
            return 0.0;

        case WaveformType::SINE: {
            double phase = 2.0 * M_PI * wf.frequency * t + wf.phase;
            return wf.offset + wf.amplitude * std::sin(phase);
        }

        case WaveformType::TRIANGLE: {
            double phase = std::fmod(wf.frequency * t, 1.0);
            double tri = (phase < 0.5)
                ? (4.0 * phase - 1.0)
                : (3.0 - 4.0 * phase);
            return wf.offset + wf.amplitude * tri;
        }

        case WaveformType::SQUARE: {
            double phase = std::fmod(wf.frequency * t, 1.0);
            double sq = (phase < 0.5) ? 1.0 : -1.0;
            return wf.offset + wf.amplitude * sq;
        }

        case WaveformType::SAW: {
            double phase = std::fmod(wf.frequency * t, 1.0);
            double saw = 2.0 * phase - 1.0;
            return wf.offset + wf.amplitude * saw;
        }

        case WaveformType::NOISE: {
            // Deterministic hash-based noise (reproducible)
            uint64_t hash = static_cast<uint64_t>(t * 1e9);
            hash ^= hash >> 33;
            hash *= 0xff51afd7ed558ccdULL;
            hash ^= hash >> 33;
            hash *= 0xc4ceb9fe1a85ec53ULL;
            hash ^= hash >> 33;
            double norm = static_cast<double>(hash & 0xFFFFFFFF) / 4294967295.0;
            return wf.offset + wf.amplitude * (2.0 * norm - 1.0);
        }

        case WaveformType::CUSTOM:
            return wf.custom_fn ? wf.custom_fn(t) : 0.0;
        }
        return 0.0;
    }

    void update_simulated_inputs()
    {
        for (size_t i = 0; i < firmware::MAX_HW_INPUTS; i++) {
            if (input_waveforms_[i].type != WaveformType::NONE) {
                fw.io.inputs[i] = sample_waveform(input_waveforms_[i], sim_time_);
            }
        }
    }
};

#endif // TEST_FIRMWARE_TEST_HARNESS_H
