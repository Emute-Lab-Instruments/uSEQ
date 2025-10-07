#include "uSEQ.h"
#include "uSEQ/hardware_output.h"
#include "utils.h"
#include "modulisp/reevaluate_scope.h"

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
            m_continuous_vals[i] = 0.5;
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

    if (!m_serial_vals.empty())
    {
        double time_seconds = 0.0;
        if (auto* time_manager = m_interpreter.get_time_manager())
        {
            time_seconds = time_manager->get_time_since_boot() / 1e6;
        }
        m_serial_vals[0] = time_seconds;
    }

    for (int i = 1; i < m_num_serial_outs; i++)
    {
        // Clear error queue
        error_msg_q.clear();
        String expr_name = String("s") + String(i);
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
                        String(i) +
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

    // Use RAII scope to automatically enable/disable re-evaluation
    // of time-dependent expressions. This ensures that symbols defined
    // in terms of time variables (t, beat, bar, etc.) are freshly
    // evaluated with current time values during output updates.
    ReevaluateScope reevaluate_scope;
    set_update_loop_evaluation(true);

    // BODY
    update_continuous_signals();
    update_binary_signals();
    update_serial_signals();

    set_update_loop_evaluation(false);
    // ReevaluateScope automatically restores previous state on scope exit
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
        float val = m_continuous_vals[i];

        dbg(String(i));
        #if defined(MUSICTHING)
        if (i == 0)
        {
            // Left channel: scale from [0,1] to [-1,+1] for CV modulation
            float scaled_val = (val * 2.0f) - 1.0f;
            if (m_q_dac_output_left_ptr)
            {
                queue_try_add(m_q_dac_output_left_ptr, &scaled_val);
            }
            continue;
        }else if (i == 1)
        {
            // Right channel: scale from [0,1] to [-1,+1] for CV modulation
            float scaled_val = (val * 2.0f) - 1.0f;
            if (m_q_dac_output_right_ptr)
            {
                queue_try_add(m_q_dac_output_right_ptr, &scaled_val);
            }
            continue;
        }
        // For remaining outputs (a3, a4), use normal analog output
        analog_write_with_led(i, val);
        #else
        analog_write_with_led(i, val);
        #endif
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


#endif // HAS_OUTPUTS
