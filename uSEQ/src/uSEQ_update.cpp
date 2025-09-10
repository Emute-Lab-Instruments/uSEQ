#include "uSEQ.h"
#include "uSEQ/hardware_output.h"
#include "utils.h"
#ifdef MUSICTHING
#include "dsp/uSeqGens/uSeqGen_MT_DAC.h"
#endif

/// UPDATE methods
#if defined(USE_NOT_IN_FLASH)
void __not_in_flash_func(uSEQ::update_continuous_signals)()
#else
void uSEQ::update_continuous_signals()
#endif
{
    DBG("uSEQ::update_continuous_signals");

    for (int i = 0; i < m_num_continuous_outs; i++)
    {
        // Clear error queue
        error_msg_q.clear();
#ifdef WASM_BUILD
        static String outputnames[16] = { "a1",  "a2",  "a3",  "a4",  "a5",  "a6",
                                          "a7",  "a8",  "a9",  "a10", "a11", "a12",
                                          "a13", "a14", "a15", "a16" };
#else
        static __not_in_flash("mem")
            String outputnames[16] = { "a1",  "a2",  "a3",  "a4",  "a5",  "a6",
                                       "a7",  "a8",  "a9",  "a10", "a11", "a12",
                                       "a13", "a14", "a15", "a16" };
#endif
        String expr_name = outputnames[i]; //"a" + (i + 1);
        set_atom_currently_being_evaluated(expr_name);

        Value expr = m_continuous_ASTs[i];
        if (expr.is_nil())
        {
            m_continuous_vals[i] = 0.0;
        }
        else
        {
            dbg("Evalling: " + expr.display());

            Value result = eval(expr);
            if (!result.is_number())
            {
                println("**Warning**: Clearing the expression for **a" +
                        String(i + 1) +
                        "** because it doesn't evaluate to a number:\n    " +
                        expr.display());

                // Print first error that was added to q
                if (error_msg_q.size() > 0)
                {
                    println(error_msg_q[0]);
                }

                m_continuous_ASTs[i] = default_continuous_expr;
                m_continuous_vals[i] = 0.5;
            }
            else
            {
                m_continuous_vals[i] = result.as_float();
            }
        }
    }

#ifdef MUSICTHING
    update_dac_leds();
#endif
}

#if defined(USE_NOT_IN_FLASH)
void __not_in_flash_func(uSEQ::update_binary_signals)()
#else
void uSEQ::update_binary_signals()
#endif
{
    DBG("uSEQ::update_binary_signals");

    for (int i = 0; i < m_num_binary_outs; i++)
    {
        // Clear error queue
        error_msg_q.clear();
        String expr_name = String("d") + String(i + 1);
        set_atom_currently_being_evaluated(expr_name);

        Value expr = m_binary_ASTs[i];
        if (expr.is_nil())
        {
            m_binary_vals[i] = 0.0;
        }
        else
        {
            dbg("Evalling: " + expr.display());
            Value result = eval(expr);
            if (!result.is_number())
            {

                println("**Warning**: Clearing the expression for **d" +
                        String(i + 1) +
                        "** because it doesn't evaluate to a number:\n    " +
                        expr.display());

                // Print first error that was added to q
                if (error_msg_q.size() > 0)
                {
                    println(error_msg_q[0]);
                }

                m_binary_ASTs[i] = default_binary_expr;
                m_binary_vals[i] = 0.0;
            }
            else
            {
                m_binary_vals[i] = result.as_float();
            }
        }
    }
}

void uSEQ::update_serial_signals()
{
    DBG("uSEQ::update_serial_signals");

    for (int i = 0; i < m_num_serial_outs; i++)
    {
        // Clear error queue
        error_msg_q.clear();
        String expr_name = String("s") + String(i + 1);
        set_atom_currently_being_evaluated(expr_name);

        Value expr = m_serial_ASTs[i];
        // if it's nil we don't need to go through
        // the overhead of calling eval (nil evals to itself)
        if (expr.is_nil())
        {
            // signal that there's no value to write
            m_serial_vals[i] = std::nullopt;
        }
        else
        {
            dbg("Expr: " + expr.display());
            // Eval
            Value result = eval(expr);

            if (!result.is_number())
            {
                println("**Warning**: Clearing the expression for **s" +
                        String(i + 1) +
                        "** because it doesn't evaluate to a number:\n    " +
                        expr.display());

                // Print first error that was added to q
                if (error_msg_q.size() > 0)
                {
                    println(error_msg_q[0]);
                }

                m_serial_ASTs[i] = default_serial_expr;
                m_serial_vals[i] = std::nullopt;
            }
            else
            {
                // since we know it's a number we can unbox and cache it
                m_serial_vals[i] = result.as_float();
            }
        }
    }
}

#if defined(USE_NOT_IN_FLASH)
void __not_in_flash_func(uSEQ::update_signals)()
#else
void uSEQ::update_signals()
#endif
{
    DBG("uSEQ::update_signals");

    // Flip flag on only for evals that happen
    // for output signals
    set_attempt_expr_eval_first(true);
    set_update_loop_evaluation(true);

    // BODY
    update_continuous_signals();
    update_binary_signals();
    update_serial_signals();

    set_attempt_expr_eval_first(false);
    set_update_loop_evaluation(false);
}

#if HAS_OUTPUTS
void uSEQ::update_outs()
{
    DBG("uSEQ::update_outs");
    // FIXME: if the order is flipped and binary goes
    // after continuous, then all LEDs behave like binary
    update_binary_outs();
    update_continuous_outs();
    update_serial_outs();

#ifdef MIDIOUT
    update_midi_out();
#endif
}

void uSEQ::update_continuous_outs()
{
    DBG("uSEQ::update_continuous_outs");

    for (size_t i = 0; i < m_num_continuous_outs; i++)
    {
        dbg(String(i));
        analog_write_with_led(i, m_continuous_vals[i]);
    }
}

void uSEQ::update_binary_outs()
{
    DBG("uSEQ::update_binary_outs");

    for (size_t i = 0; i < m_num_binary_outs; i++)
    {
        dbg(String(i));
        digital_write_with_led(i, m_binary_vals[i]);
    }
}

void uSEQ::update_serial_outs()
{
    DBG("uSEQ::update_serial_outs");

    unsigned long serial_now = micros();
    // rate limiting of serial messages
    unsigned long serial_time_elapsed = serial_now - serial_out_timestamp;
    if (serial_time_elapsed > SerialMsg::serial_message_rate_limit)
    {
        for (size_t i = 0; i < m_num_serial_outs; i++)
        {
            dbg(String(i));
            std::optional<SERIAL_OUTPUT_VALUE_TYPE> v = m_serial_vals[i];
            // only write if there is a value
            if (v)
            {
                dbg("writing value: " + String(*v));
                serial_write(i, *v);
            }
        }
        serial_out_timestamp = serial_now - (serial_time_elapsed -
                                             SerialMsg::serial_message_rate_limit);
    }
}

#ifdef MUSICTHING
void uSEQ::update_dac_leds()
{
    DBG("uSEQ::update_dac_leds");

    // Static circular buffers for running average
    static constexpr size_t BUFFER_SIZE       = 32;
    static uint16_t left_buffer[BUFFER_SIZE]  = { 0 };
    static uint16_t right_buffer[BUFFER_SIZE] = { 0 };
    static size_t buffer_index                = 0;

    // Read latest DAC values from DSP thread (thread-safe volatile read)
    uint16_t dac_left  = uSeqGen_MT_DAC::latest_dac_left;
    uint16_t dac_right = uSeqGen_MT_DAC::latest_dac_right;

    // Convert from 12-bit DAC range (0-4095 with 2048 center) to absolute values
    // DAC center is 2048 (0V), so we calculate distance from center
    int left_signed  = static_cast<int>(dac_left) - 2048;
    int right_signed = static_cast<int>(dac_right) - 2048;

    // Store absolute values in circular buffer
    left_buffer[buffer_index]  = static_cast<uint16_t>(abs(left_signed));
    right_buffer[buffer_index] = static_cast<uint16_t>(abs(right_signed));

    // Calculate running average of absolute values
    uint32_t left_sum  = 0;
    uint32_t right_sum = 0;
    for (size_t i = 0; i < BUFFER_SIZE; i++)
    {
        left_sum += left_buffer[i];
        right_sum += right_buffer[i];
    }

    // Average and scale to 0-2047 range (matching existing LED scaling)
    // Max absolute value is 2048, so average can be at most 2048
    int led_val_left  = static_cast<int>(left_sum / BUFFER_SIZE);
    int led_val_right = static_cast<int>(right_sum / BUFFER_SIZE);

    // Apply exponential curve for visual response (same as analog_write_with_led)
    led_val_left  = (led_val_left * led_val_left) >> 11;
    led_val_right = (led_val_right * led_val_right) >> 11;

    // Invert the values so low average = high LED brightness
    led_val_left  = 2047 - led_val_left;
    led_val_right = 2047 - led_val_right;

    // Update circular buffer index
    buffer_index = (buffer_index + 1) % BUFFER_SIZE;

#ifdef ARDUINO
    HardwareOutput::analog(static_cast<uint8_t>(USEQ_LED_PIN_AUDIO_L), static_cast<uint16_t>(led_val_left));
    HardwareOutput::analog(static_cast<uint8_t>(USEQ_LED_PIN_AUDIO_R), static_cast<uint16_t>(led_val_right));
#endif
}
#endif // MUSICTHING

#endif // HAS_OUTPUTS
