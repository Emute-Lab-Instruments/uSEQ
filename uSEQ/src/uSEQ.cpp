#include "uSEQ.h"
#include "lisp/LispLibrary.h"
#include "lisp/interpreter.h"
#include "lisp/value.h"
#include "uSEQ/i2cHost.h"
#include "utils.h"
#include "utils/log.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <sys/types.h>

#include "uSEQ/i2cClient.h"
#include "hardware_includes.h"

#ifdef ARDUINO
#include "hardware/flash.h"
#include "uSEQ/piopwm.h"
#endif

// #include "lisp/library.h"
#include <cmath>

// statics
uSEQ* uSEQ::instance;

double maxiFilter::lopass(double input, double cutoff)
{
    output = z + cutoff * (input - z);
    z      = output;
    return (output);
}

maxiFilter cvInFilter[2];

// uSEQ MEMBER FUNCTIONS

// void dbg(String s) { std::cout << s.c_str() << std::endl; }

#if defined(ARDUINO) && defined(USEQHARDWARE_1_0)
#include <Wire.h>
#endif

String exit_command = "@@exit";

void uSEQ::run()
{
    if (!m_initialised)
    {
        init();
    }

    start_loop_blocking();
}

void uSEQ::eval_lisp_library()
{
    DBG("eval_lisp_library");

    for (int i = 0; i < LispLibrarySize; i++)
    {
        String code = LispLibrary[i];
        dbg("Evalling code " + String(i) + ":\n" + code);
        eval(code);
    }
}




void uSEQ::init()
{
    DBG("uSEQ::init");
    setup_leds();

    // dbg("free heap (start):" + String(free_heap()));
    // if (!m_initialised)
    // {
    dbg("Setting instance pointer");

    Interpreter::useq_instance_ptr = this;
    Interpreter::init();

    uSEQ::instance = this;

    init_builtinfuncs();
    // eval_lisp_library();

    led_animation();
#ifdef USEQHARDWARE_1_0
    start_pdm();
#endif

    setup_IO();

    // dbg("Lisp library loaded.");

    // uSEQ software setup
    set_bpm(m_defaultBPM, 0.0);
    update_time();
    init_ASTs();

    autoload_flash();

    m_initialised = true;
}


void uSEQ::run_scheduled_items()
{
    DBG("uSEQ::runScheduledItems");

    for (size_t i = 0; i < m_scheduledItems.size(); i++)
    {
        // run the statement once every period
        size_t run = static_cast<size_t>(m_bar_phase * m_scheduledItems[i].period);
        //        size_t run_norm = run > m_scheduledItems[i].lastRun ? run : run +
        //        m_scheduledItems[i].period;
        size_t numRuns =
            run >= m_scheduledItems[i].lastRun
                ? run - m_scheduledItems[i].lastRun
                : m_scheduledItems[i].period - m_scheduledItems[i].lastRun;
        for (size_t j = 0; j < numRuns; j++)
        {
            // run the statement
            //             println(m_scheduledItems[i].id);
            // TODO: #99
            eval(m_scheduledItems[i].ast);
        }
        m_scheduledItems[i].lastRun = run;
    }
}

void uSEQ::start_loop_blocking()
{
    while (!m_should_quit)
    {
        tick();
    }

    println("Exiting REPL.");
}

void uSEQ::update_Q0()
{
    Value result = eval(m_q0AST);
    if (result.is_error())
    {
        println("Error in q0 output function, clearing");
        m_q0AST = {};
    }
}

void uSEQ::check_code_quant_phasor()
{
    DBG("uSEQ::check_code_quant_phasor");
    double newCqpVal = eval(m_cqpAST).as_float();
    // double cqpAvgTime = cqpMA.process(newCqpVal - lastCQP);
    if (newCqpVal < m_last_CQP)
    {
        update_Q0();
        for (size_t q = 0; q < m_runQueue.size(); q++)
        {
            Value res;
            int cmdts = micros();
            res       = eval(m_runQueue[q]);
            cmdts     = micros() - cmdts;
            println(res.to_lisp_src());
        }
        m_runQueue.clear();
    }
    m_last_CQP = newCqpVal;
}

// TODO does order matter?
// e.g. when user code is evaluated, does it make
// a difference if the inputs have been updated already?
void uSEQ::tick()
{
    DBG("uSEQ::tick");

    updateSpeed = micros() - ts;
    set("fps", Value(1000000.0 / updateSpeed));
    set("qt", Value(updateSpeed * 0.001));
    ts = micros();

// Read & cache the hardware & software inputs
#if HAS_INPUTS
    update_inputs();
#endif
    // Update time
    update_time();
    check_code_quant_phasor();
    run_scheduled_items();
    // Re-run & cache output signal forms
    if (!this->g_sync_mode_active) {
        update_signals();

        // Write cached output signals to hardware and/or software outputs
    #if HAS_OUTPUTS
        update_outs();
    #endif
    }

    if (this->g_sync_mode_active) {
        // Sync mode active: Check for triggers here

        // Momentary Switch Check (assuming USEQM1 is the correct enum/index for the momentary switch)
        // Ensure m_input_vals has been updated by update_inputs() before this point in tick().
        bool momentary_switch_pressed = (this->m_input_vals[USEQM1] > 0.5); // Use 0.5 for digital inputs that are 0 or 1

        // Analog Input Check
        // Define a threshold for "HIGH" (e.g., for values normalized 0.0-1.0)
        const double ANALOG_HIGH_THRESHOLD = 0.8; 
        bool analog_trigger = false;

        // Check analog inputs based on USEQHARDWARE version.
        // These indices (USEQAI1, USEQAI2) should be defined in pinmap.h or be accessible.
        // Assuming m_input_vals stores normalized values (0.0 to 1.0).
        #if defined(USEQHARDWARE_1_0) || defined(MUSICTHING)
            if (this->m_input_vals[USEQAI1] > ANALOG_HIGH_THRESHOLD) {
                analog_trigger = true;
            }
            if (!analog_trigger && this->m_input_vals[USEQAI2] > ANALOG_HIGH_THRESHOLD) { // Check USEQAI2 only if USEQAI1 didn't trigger
                analog_trigger = true;
            }
        #endif
        // Add more analog inputs here if other hardware versions have them and need to be checked.
        // For example, if there was a USEQAI3:
        // #if defined(SOME_OTHER_HARDWARE)
        //     if (!analog_trigger && this->m_input_vals[USEQAI3] > ANALOG_HIGH_THRESHOLD) {
        //         analog_trigger = true;
        //     }
        // #endif

        if (momentary_switch_pressed || analog_trigger) {
            // Action: Write 1 to all digital outputs
            // m_num_binary_outs is a class member storing the count of binary outputs.
            for (unsigned int i = 0; i < this->m_num_binary_outs; ++i) {
                this->digital_write_with_led(i, 1); // Assuming val=1 means HIGH
            }

            // Action: Set transport to 0
            this->reset_logical_time();

            // Action: Exit sync mode
            this->g_sync_mode_active = false;

            // Optional: Print a message for debugging
            // this->println("Sync mode exited, transport reset."); 
        }
    }

    // Check for new code and eval (or schedule it)
    check_and_handle_user_input();

    // tiny delay to allow for interrupts etc
    delayMicroseconds(100);
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////



// return true if either serial or I2C has new code
bool is_new_code_waiting() { return Serial.available() || bNewI2CMessage; }

String get_code_waiting()
{
    // we might get arway with just return i2cInBuff... :)
    if (bNewI2CMessage)
    {
        String mC = String(i2cInBuff);
        // rrplce with substr (was a hack)
        mC.remove(0, 1);
        mC.remove(mC.length() - 1, 1);
        println(mC);
        return mC;
    }
    else
        return Serial.readStringUntil('\n');
}

void uSEQ::check_and_handle_user_input()
{
    DBG("uSEQ::check_and_handle_user_input");
    // m_repl.check_and_handle_input();

    if (is_new_code_waiting())
    {
        m_manual_evaluation = true;

        int first_byte;
        // Incomming serial stream isn't implemented on I2C
        // but sending I2C host should add the correct run now or later firstByte
        if (bNewI2CMessage)
            first_byte = i2cInBuff[0];
        else
            first_byte = Serial.read();

        // SERIAL
        if (first_byte == SerialMsg::message_begin_marker /*31*/)
        {
            // incoming serial stream
            size_t channel = Serial.read();
            char buffer[8];
            Serial.readBytes(buffer, 8);
            if (channel > 0 && channel <= m_num_serial_ins)
            {
                double v = 0;
                memcpy(&v, buffer, 8);
                m_serial_input_streams[(channel - 1)] = v;
            }
        }
        else
        {
            // Read code
            m_last_received_code = get_code_waiting();
            // println(m_last_received_code);
            // println(m_last_received_code.length());
            // println("*");

            if (m_last_received_code == exit_command)
            {
                m_should_quit = true;
            }

            // I2C specific routing could be filtered here - note the first_byte gets
            // passed to preserve execution time if (first_byte == '$')
            // i2cParse(first_byte+m_last_received_code); //in i2cHost.h else if ...
            // EXECUTE NOW
            if (first_byte == SerialMsg::execute_now_marker /*'@'*/)
            {
                // Clear error queue
                error_msg_q.clear();

                String result = eval(m_last_received_code);

                if (error_msg_q.size() > 0)
                {
                    if (bNewI2CMessage)
                        i2cPrintStr +=
                            error_msg_q[0]; // maybe move this routing to within
                                            // println? //TODO add i2c ID
                    else
                        println(error_msg_q[0]);
                }

                if (bNewI2CMessage)
                    i2cPrintStr += result;
                else
                    println(result);
            }
            // SCHEDULE FOR LATER
            else
            {
                m_last_received_code =
                    String((char)first_byte) + m_last_received_code;
                if (bNewI2CMessage)
                    i2cPrintStr += m_last_received_code;
                else
                    println(m_last_received_code);
                Value expr = parse(m_last_received_code);
                m_runQueue.push_back(expr);
            }
        }

        m_manual_evaluation = false;
        // flush_print_jobs();

        // clear new i2c message flags if required
        if (bNewI2CMessage)
        {
            bNewI2CMessage = false;
            nI2CBytesRead  = 0;
        }
    }
}




void uSEQ::init_ASTs()
{
    DBG("uSEQ::init_ASTs");

    for (int i = 0; i < m_num_binary_outs; i++)
    {
        m_binary_ASTs.push_back(default_binary_expr);
        m_binary_vals.push_back(0);
    }

    for (int i = 0; i < m_num_continuous_outs; i++)
    {
        m_continuous_ASTs.push_back(default_continuous_expr);
        m_continuous_vals.push_back(0.0);
    }

    for (int i = 0; i < m_num_serial_outs; i++)
    {
        m_serial_ASTs.push_back(default_serial_expr);
        m_serial_vals.push_back(std::nullopt);
    }
}


// NOTE: from here
// https://forums.raspberrypi.com/viewtopic.php?t=318747
// Testing
// Value uSEQ::useq_eval_at_time(std::vector<Value>& args, Environment& env)
// {
//     constexpr const char* user_facing_name = "eval-at-time";

//     // Checking number of args
//     // if (!(2 <= args.size() <= 3))
//     if (!(args.size() == 2))
//     {
//         // error_wrong_num_args(user_facing_name, args.size(),
//         //                      NumArgsComparison::Between, 2, 3);
//         error_wrong_num_args(user_facing_name, args.size(),
//                              NumArgsComparison::EqualTo, 2, -1);
//         return Value::error();
//     }

//     // NOTE: This needs to eval both of its args, including the list,
//     // to cover for cases where the user passes anything other than a
//     // list literal (e.g. a symbol that points to a list)
//     //
//     // Evaluating & checking args for errors
//     Value pre_eval = args[0];
//     args[0]        = args[0].eval(env);
//     if (args[0].is_error())
//     {
//         error_arg_is_error(user_facing_name, 1, pre_eval.display());
//         return Value::error();
//     }

//     // Checking individual args
//     if (!(args[0].is_number()))
//     {
//         error_wrong_specific_pred(user_facing_name, 1, "a number",
//                                   args[0].display());
//         return Value::error();
//     }

//     // NOTE: go from seconds to micros for internal calculations
//     double time = args[0].as_float() * 1e+6;

//     // BODY
//     return eval_at_time(args[1], env, time);
// }


//////////////////////////

