#ifndef USEQGEN_MT_FAC_H
#define USEQGEN_MT_FAC_H

#include "uSeqGen_Base.h"
#include <cmath> // For fabsf, fmaxf, fminf, sqrtf
// #include "Arduino.h"
#ifdef ARDUINO
#include "SPI.h"
#endif

#define DAC_config_chan_A_gain 0b0001000000000000
#define DAC_config_chan_B_gain 0b1001000000000000
#define DAC_config_chan_A_nogain 0b0011000000000000
#define DAC_config_chan_B_nogain 0b1011000000000000
#define PIN_CS 21

class uSeqGen_MT_DAC final : public uSeqGen_Base
{
public:
    uSeqGen_MT_DAC(queue_t* q, size_t key) : uSeqGen_Base(q, key)
    {
        SetInputCount_(2);
        SetOutputCount_(2);

        SPI.setCS(PIN_CS);
        SPI.begin(1); // Initialize the SPI bus

        // pass settings to SPI class
        SPI.beginTransaction(
            SPISettings(20000000, MSBFIRST, SPI_MODE0)); // Start SPI transaction
        SPI.endTransaction();

        // Create clearly named per-channel CV/command input queues
        createInputQueue(0, q_input_left, DSPQ::UGEN_TYPE::DAC);
        createInputQueue(1, q_input_right, DSPQ::UGEN_TYPE::DAC);

        // Initialize LED update rate - check if sample rate is already available
        if (sampleRate > 0)
        {
            m_samples_per_led_update =
                static_cast<uint32_t>(sampleRate / LED_UPDATE_RATE_HZ);
            if (m_samples_per_led_update == 0)
                m_samples_per_led_update = 1;
        }
        else
        {
            // Safe default for when sample rate isn't set yet
            m_samples_per_led_update = 1;
        }
    }

    ~uSeqGen_MT_DAC()
    {
        queue_free(&q_input_left);
        queue_free(&q_input_right);
    }

protected:
    void __force_inline Process_(DSPatch::SignalBus& inputs,
                                 DSPatch::SignalBus& outputs) override
    {
        const float inL = GET_INPUT_SAFE(inputs, float, 0, 0.f);
        const float inR = GET_INPUT_SAFE(inputs, float, 1, 0.f);

        // Read latest values from left and right input queues (if any)
        // Implement sample-and-hold semantics so DC values persist between
        // control-rate updates.
        float newCVLeft;
        if (queue_try_remove(&q_input_left, &newCVLeft))
        {
            m_cv_hold_left = newCVLeft;
        }
        float newCVRight;
        if (queue_try_remove(&q_input_right, &newCVRight))
        {
            m_cv_hold_right = newCVRight;
        }

        const float inL_plus_CV = inL + m_cv_hold_left;  // Left channel modulation
        const float inR_plus_CV = inR + m_cv_hold_right; // Right channel modulation

        int sigL = static_cast<int>(inL_plus_CV * 2048.f) + 2047;
        int sigR = static_cast<int>(inR_plus_CV * 2048.f) + 2047;

#ifdef AUDIO_OUT_INVERTED
        sigL = 4095 - sigL;
        sigR = 4095 - sigR;
#endif

        // assuming SPI already set up, so no need for begin/end transaction
        // CS is hardware controlled
        const uint16_t DAC_dataL = DAC_config_chan_A_nogain | (sigL & 0xFFF);
        const uint16_t DAC_dataR = DAC_config_chan_B_nogain | (sigR & 0xFFF);
        uint16_t ret;
        hw_write_masked(&spi_get_hw(spi0)->cr0, (16 - 1) << SPI_SSPCR0_DSS_LSB,
                        SPI_SSPCR0_DSS_BITS); // Fast set to 16-bits
        spi_write16_read16_blocking(spi0, &DAC_dataL, &ret, 1);
        hw_write_masked(&spi_get_hw(spi0)->cr0, (16 - 1) << SPI_SSPCR0_DSS_LSB,
                        SPI_SSPCR0_DSS_BITS); // Fast set to 16-bits
        spi_write16_read16_blocking(spi0, &DAC_dataR, &ret, 1);

        // Audio level metering for LED updates (30Hz on core 1)
        updateAudioLevels(inL_plus_CV, inR_plus_CV);
    }

private:
    // Per-channel input queues for CV or command injection
    queue_t q_input_left;
    queue_t q_input_right;

    // Hold the most recent CV values so they persist across samples
    float m_cv_hold_left  = 0.0f;
    float m_cv_hold_right = 0.0f;

    // Audio level metering variables (for 30Hz LED updates on core 1)
    static constexpr uint32_t LED_UPDATE_RATE_HZ = 30;
    uint32_t m_led_update_counter                = 0;
    uint32_t m_samples_per_led_update = 0; // Will be calculated based on sample rate
    float m_last_sample_rate          = 0.0f; // Track sample rate changes

    // Peak detection with decay
    float m_peak_left  = 0.0f;
    float m_peak_right = 0.0f;
    static constexpr float PEAK_DECAY_RATE =
        0.997f; // Decay per sample (roughly 300ms at 44.1kHz)

    // RMS calculation buffers (100ms window)
    static constexpr size_t RMS_BUFFER_SIZE =
        128; // ~3ms at 44.1kHz, accumulated over time
    float m_rms_buffer_left[RMS_BUFFER_SIZE]     = { 0.0f };
    float m_rms_buffer_right[RMS_BUFFER_SIZE]    = { 0.0f };
    size_t m_rms_buffer_index                    = 0;
    float m_rms_accumulator_left                 = 0.0f;
    float m_rms_accumulator_right                = 0.0f;
    uint32_t m_rms_sample_count                  = 0;
    static constexpr uint32_t RMS_WINDOW_SAMPLES = 4410; // 100ms at 44.1kHz

    // Audio level metering implementation
    void __force_inline updateAudioLevels(float left_signal, float right_signal)
    {
        // Check for sample rate changes (initial setup or runtime changes)
        if (sampleRate > 0 && sampleRate != m_last_sample_rate)
        {
            m_samples_per_led_update =
                static_cast<uint32_t>(sampleRate / LED_UPDATE_RATE_HZ);
            if (m_samples_per_led_update == 0)
                m_samples_per_led_update = 1; // Prevent division by zero
            m_last_sample_rate = sampleRate;

            // Reset counter to prevent LED update timing issues during sample rate
            // changes
            m_led_update_counter = 0;
        }

        // Get absolute values for level calculation
        float abs_left  = fabsf(left_signal);
        float abs_right = fabsf(right_signal);

        // Update peak detection with decay
        m_peak_left  = fmaxf(abs_left, m_peak_left * PEAK_DECAY_RATE);
        m_peak_right = fmaxf(abs_right, m_peak_right * PEAK_DECAY_RATE);

        // Update RMS calculation
        // Remove old sample from accumulator
        m_rms_accumulator_left -= m_rms_buffer_left[m_rms_buffer_index];
        m_rms_accumulator_right -= m_rms_buffer_right[m_rms_buffer_index];

        // Add new sample squared
        float left_sq                          = left_signal * left_signal;
        float right_sq                         = right_signal * right_signal;
        m_rms_buffer_left[m_rms_buffer_index]  = left_sq;
        m_rms_buffer_right[m_rms_buffer_index] = right_sq;
        m_rms_accumulator_left += left_sq;
        m_rms_accumulator_right += right_sq;

        // Update buffer index
        m_rms_buffer_index = (m_rms_buffer_index + 1) % RMS_BUFFER_SIZE;
        m_rms_sample_count++;

        // Update LEDs at 30Hz rate
        m_led_update_counter++;
        if (m_led_update_counter >= m_samples_per_led_update)
        {
            m_led_update_counter = 0;
            updateLEDs();
        }
    }

    // LED update function (called at 30Hz)
    void updateLEDs()
    {
#ifdef ARDUINO
        // LED pin definitions (copied from pinmap.h to avoid circular dependency)
        static constexpr uint8_t LED_PIN_AUDIO_L = 10;
        static constexpr uint8_t LED_PIN_AUDIO_R = 11;

        // Calculate RMS values
        float rms_left =
            sqrtf(m_rms_accumulator_left / static_cast<float>(RMS_BUFFER_SIZE));
        float rms_right =
            sqrtf(m_rms_accumulator_right / static_cast<float>(RMS_BUFFER_SIZE));

        // Use maximum of peak and RMS for display (like professional meters)
        float display_left  = fmaxf(m_peak_left, rms_left);
        float display_right = fmaxf(m_peak_right, rms_right);

        // Scale to 0-2047 range (12-bit like original)
        // Clamp to reasonable range first (signals should be -1.0 to +1.0)
        display_left  = fminf(display_left, 1.0f);
        display_right = fminf(display_right, 1.0f);

        int led_val_left  = static_cast<int>(display_left * 2047.0f);
        int led_val_right = static_cast<int>(display_right * 2047.0f);

        // Apply exponential curve for visual response (same as original)
        led_val_left  = (led_val_left * led_val_left) >> 11;
        led_val_right = (led_val_right * led_val_right) >> 11;

#ifndef AUDIO_OUT_INVERTED
        // Invert the values so low level = high LED brightness (same as original)
        led_val_left  = 2047 - led_val_left;
        led_val_right = 2047 - led_val_right;
#endif

        // Write directly to LED pins using Arduino analogWrite
        analogWrite(LED_PIN_AUDIO_L, led_val_left);
        analogWrite(LED_PIN_AUDIO_R, led_val_right);
#endif
    }
};

#endif // USEQGEN_MUL_H
