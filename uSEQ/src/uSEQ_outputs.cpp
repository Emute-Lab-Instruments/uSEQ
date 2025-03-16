#include "uSEQ.h"



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
        String expr_name                 = "a" + (i + 1);
        m_atom_currently_being_evaluated = expr_name;

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
        String expr_name                 = "d" + (i + 1);
        m_atom_currently_being_evaluated = expr_name;

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
        String expr_name                 = "s" + (i + 1);
        m_atom_currently_being_evaluated = expr_name;

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
    m_attempt_expr_eval_first = true;
    m_update_loop_evaluation  = true;

    // BODY
    update_continuous_signals();
    update_binary_signals();
    update_serial_signals();

    m_attempt_expr_eval_first = false;
    m_update_loop_evaluation  = false;
}

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