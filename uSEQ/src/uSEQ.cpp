#include "uSEQ.h"
#include "lisp/LispLibrary.h"
#include "lisp/interpreter.h"
#include "lisp/value.h"
#ifdef ARDUINO
#include "uSEQ/i2cHost.h"
#endif
#include "utils.h"
#include "utils/log.h"
#include "utils/serial_message.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <sys/types.h>
// #include "dsp/dsp-queues.hpp"
#include "dsp/dsp-q-data.hpp"

#ifdef ARDUINO
#define FAST_FUNC(x) __not_in_flash_func(x)
#else
#define FAST_FUNC(x) x
#define __not_in_flash(section) 
#define __not_in_flash_func(x) 
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <thread>

// Note: Arduino timing function stubs moved to hardware_includes.h
// Note: micros() already exists in utils.h, don't redefine it
#endif


#include "hardware_includes.h"
#ifdef ARDUINO
#include "uSEQ/i2cClient.h"
#else
// Desktop stubs for I2C variables needed by uSEQ.cpp
bool bNewI2CMessage = false;
int nI2CBytesRead = 0;
char i2cInBuff[500];
String i2cPrintStr = "";

#endif

#ifdef ARDUINO
#include "hardware/flash.h"
#include "uSEQ/piopwm.h"
#endif

// #include "lisp/library.h"
#include <cmath>

// statics
#ifdef ARDUINO
uSEQ* __not_in_flash("useq") uSEQ::instance;
#else
uSEQ* uSEQ::instance;
#endif

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

#ifdef ARDUINO
void uSEQ::init_dsp_queues() {
    // for (int i = 0; i < N_INPUT_QUEUES; i++) {
    //     queue_init(&DSPQ::q_inputs[i], sizeof(double), 1);
    // }

    // for (int i = 0; i < N_OUTPUT_QUEUES; i++) {
    //     queue_init(&DSPQ::q_outputs[i], sizeof(double), 1);
    //     dsp_output_names[i] = "ppp" + String(i);
    //     set(dsp_output_names[i],0);
    // }  
    
    queue_init(&DSPQ::q_engine_commands, sizeof(uSEQDSPEngine::command_info), 8);
    queue_init(&DSPQ::q_engine_responses, sizeof(DSPQ::response_info), 16);

    uSEQDSPEngine::command_info cmd;
    cmd.command = uSEQDSPEngine::COMMANDS::SETUP;
    queue_try_add(&DSPQ::q_engine_commands, &cmd);

    cmd.command = uSEQDSPEngine::COMMANDS::GETUGENINFO;
    queue_try_add(&DSPQ::q_engine_commands, &cmd);
}
#endif



#ifdef ARDUINO
void __not_in_flash_func(uSEQ::check_dsp_output_queues)() {
    // double tmp;
    // for (size_t i = 0; i < N_OUTPUT_QUEUES; i++) {
    //     if (queue_try_remove(&DSPQ::q_outputs[i], &tmp)) {
    //         set(dsp_output_names[i], tmp);
    //     }
    // }

    DSPQ::response_info response;
    if (queue_try_remove(&DSPQ::q_engine_responses, &response)) {
        switch (response.response) {
            case DSPQ::RESPONSES::UGENINFO:
                println("ugen info: " + String(response.data.ugenInfo.key) + " " + response.data.ugenInfo.name);
                set("ugen-" + String(response.data.ugenInfo.name), Value(static_cast<int>(response.data.ugenInfo.key)));
                break;
            case DSPQ::RESPONSES::MESSAGE:
                println("ugen message from " + String(response.data.ugenMessage.key) + ": " + response.data.ugenMessage.msg);
                break;
            case DSPQ::RESPONSES::ADD_OUTPUT_QUEUE:
            {
                // println("queue received from " + String(response.data.queueInfo.key));
                ugenOutputQueue newq;
                newq.q = response.data.queueInfo.queueptr;
                newq.index = response.data.queueInfo.index;
                newq.queueSize = response.data.queueInfo.queueSize;
                newq.key = response.data.queueInfo.key;
                size_t qIndex = dspEngine.nextKey++;
                dspEngine.ugenOutputQueues[qIndex] = newq;

                String ugenName = dspEngine.ugenInstances[response.data.queueInfo.key];
                // println("UGEN name: " + ugenName);
                String queueName = ugenName + "-out" + String(newq.index);
                println("Created output queue: " + queueName);                
                set(queueName, Value(static_cast<int>(qIndex)));
                break;
            }
            case DSPQ::RESPONSES::ADD_INPUT_QUEUE:
            {
                // println("queue received from " + String(response.data.queueInfo.key));
                ugenInputQueue newq;
                newq.q = response.data.queueInfo.queueptr;
                newq.index = response.data.queueInfo.index;
                newq.queueSize = response.data.queueInfo.queueSize;
                newq.key = response.data.queueInfo.key;
                size_t qIndex = dspEngine.nextKey++;
                dspEngine.ugenInputQueues[qIndex] = newq;

                String ugenName = dspEngine.ugenInstances[response.data.queueInfo.key];
                // println("UGEN name: " + ugenName);
                String queueName = ugenName + "-in" + String(newq.index);
                println("Created input queue: " + queueName);                
                set(queueName, Value(static_cast<int>(qIndex)));
                break;
            }
            default:
            break;
        }
    }
}
#endif

void uSEQ::init()
{
    DBG("uSEQ::init");
#ifdef ARDUINO
    init_dsp_queues();
#endif
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

#ifdef ARDUINO
    autoload_flash();
#endif

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
void FAST_FUNC(uSEQ::tick())
{
    DBG("uSEQ::tick");

    updateSpeed = micros() - ts;
    set("fps", Value(1000000.0 / updateSpeed));
    set("qt", Value(updateSpeed * 0.001));
    ts = micros();

    // Don't run the rest of the update loop if we're in sync mode
    if (m_waiting_for_sync_trigger)
    {
        delayMicroseconds(100);
        return;
    }
// Read & cache the hardware & software inputs
#if HAS_INPUTS
#ifdef ARDUINO
    check_dsp_output_queues();
#endif
    // Read & cache the hardware & software inputs
    update_inputs();
#endif
    // Update time
    update_time();
    // check_code_quant_phasor();
    run_scheduled_items();
    update_Q0();
    // Re-run & cache output signal forms

    update_signals();

    // Write cached output signals to hardware and/or software outputs
#if HAS_OUTPUTS
    update_outs();
#endif

    // Check for new code and eval (or schedule it)
    check_and_handle_user_input();

    // tiny delay to allow for interrupts etc
    delayMicroseconds(100);
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////



// return true if either serial or I2C has new code
#ifdef ARDUINO
bool is_new_code_waiting() { return Serial.available() || bNewI2CMessage; }
#else
bool is_new_code_waiting() { return false; }
#endif

#ifdef ARDUINO
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
#else
String get_code_waiting()
{
    return String("");
}
#endif

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
#ifdef ARDUINO
        else
            first_byte = Serial.read();
#else
        else
            first_byte = 0;
#endif

        // SERIAL
        if (first_byte == SerialMsg::message_begin_marker /*31*/)
        {
#ifdef ARDUINO
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
#endif
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


#ifdef ARDUINO
Value uSEQ::useq_dsp_start(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "ppp-go";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].display());
        return Value::error();
    }
    uSEQDSPEngine::command_info cmd;
    cmd.command= uSEQDSPEngine::COMMANDS::START;
    cmd.data.start.sampleRate = args[0].as_float();
    queue_try_add(&DSPQ::q_engine_commands, &cmd);
    return Value::string("PPP Started");
}

Value uSEQ::useq_dsp_stop(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "ppp-stop";

    // Checking number of args
    if (!(args.size() == 0))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 0, -1);
        return Value::error();
    }

    uSEQDSPEngine::command_info cmd;
    cmd.command = uSEQDSPEngine::COMMANDS::STOP;
    queue_try_add(&DSPQ::q_engine_commands, &cmd);
    return Value::string("PPP Stopped");
}

Value uSEQ::useq_dsp_create(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "ppp-mount";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Checking individual args
    if (!(args[0].is_symbol()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a symbol",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    args[1] = args[1].eval(env);

    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY


    uSEQDSPEngine::command_info cmd;
    cmd.command = uSEQDSPEngine::COMMANDS::CREATE;
    cmd.data.create.key=dspEngine.nextKey++;
    cmd.data.create.processor = args[1].as_int();
    
    queue_try_add(&DSPQ::q_engine_commands, &cmd);

    String name  = args[0].display();
    env.set(name, Value(static_cast<int>(cmd.data.create.key)));
    dspEngine.ugenInstances[cmd.data.create.key] =  name;
    return Value::string("Ugen mounting...");
}

Value uSEQ::useq_dsp_kill(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "ppp-unmount";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Checking individual args
    // if (!(args[0].is_symbol()))
    // {
    //     report_error_wrong_specific_pred(user_facing_name, 1, "a symbol",
    //                                      args[0].to_lisp_src());
    //     return Value::error();
    // }

    // BODY

    uSEQDSPEngine::command_info cmd;
    cmd.command = uSEQDSPEngine::COMMANDS::DESTROY;
    if ((args[0].is_symbol())) {
        String name  = args[0].display();
        cmd.data.destroy.key=static_cast<size_t>(env.get(name)->as_int());
    }else{
        cmd.data.destroy.key=static_cast<size_t>(args[0].as_int());
    }

    queue_try_add(&DSPQ::q_engine_commands, &cmd);

    dspEngine.ugenInstances.erase(cmd.data.destroy.key);

    return Value::string("Ugen unmounting...");
}

Value uSEQ::useq_dsp_connect(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "ppp-patch";

    // Checking number of args
    if (!(args.size() == 4))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 4, -1);
        return Value::error();
    }

    // BODY

    //todo: error checking

    String srcname  = args[0].display();
    String destname  = args[2].display();
    uSEQDSPEngine::command_info cmd;
    cmd.command = uSEQDSPEngine::COMMANDS::CONNECT;
    cmd.data.connect.srcKey=static_cast<size_t>(env.get(srcname)->as_int());
    cmd.data.connect.channelSrc=static_cast<size_t>(args[1].as_int());
    cmd.data.connect.destKey=static_cast<size_t>(env.get(destname)->as_int());
    cmd.data.connect.channelDest=static_cast<size_t>(args[3].as_int());
    queue_try_add(&DSPQ::q_engine_commands, &cmd);

    return Value::string("Ugen patching...");
}

Value uSEQ::useq_dsp_disconnect(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "ppp-unpatch";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 4, -1);
        return Value::error();
    }

    // BODY

    //todo: error checking

    String srcname  = args[0].display();
    uSEQDSPEngine::command_info cmd;
    cmd.command = uSEQDSPEngine::COMMANDS::DISCONNECT;
    cmd.data.disconnect.srcKey=static_cast<size_t>(env.get(srcname)->as_int());
    cmd.data.disconnect.channelSrc=static_cast<size_t>(args[1].as_int());
    queue_try_add(&DSPQ::q_engine_commands, &cmd);

    return Value::string("Ugen unpatching...");
}

Value uSEQ::useq_dsp_getugens(std::vector<Value>& args, Environment& env) {
    uSEQDSPEngine::command_info cmd;
    cmd.command = uSEQDSPEngine::COMMANDS::GETUGENINFO;
    queue_try_add(&DSPQ::q_engine_commands, &cmd);
    return Value::string("ugens requested");
}

Value uSEQ::useq_dsp_reset(std::vector<Value>& args, Environment& env) {
    uSEQDSPEngine::command_info cmd;
    cmd.command = uSEQDSPEngine::COMMANDS::RESET;
    queue_try_add(&DSPQ::q_engine_commands, &cmd);
    dspEngine.ugenInstances.clear();
    dspEngine.ugenOutputQueues.clear();
    dspEngine.ugenInputQueues.clear();
    return Value::string("reset requested");
}

Value uSEQ::useq_dsp_message(std::vector<Value>& args, Environment& env) {
    constexpr const char* user_facing_name = "ppp-msg";

    if (!(2 <= args.size() <= 3))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::Between, 2, 3);
        return Value::error();
    }
    if (!(args[0].is_symbol() || args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a symbol or a number",
                                         args[0].to_lisp_src());
        return Value::error();
    }
    if (!(args[1].is_symbol()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a symbol",
                                         args[1].display());
        return Value::error();
    }
    if (args.size()==3)
    {
        if (!(args[2].is_number() || args[2].is_string()))
        {
            report_error_wrong_specific_pred(user_facing_name, 3, "an int, float or string",
                                            args[2].to_lisp_src());
            return Value::error();
        }
    }    
    uSEQDSPEngine::command_info cmd;
    cmd.command = uSEQDSPEngine::COMMANDS::MESSAGE;
    if ((args[0].is_symbol())) {
        String name  = args[0].display();
        auto val = env.get(name);
        if (!val) {
            println("Warning: " + name + " not found in environment");
            return Value::error();
        }
        cmd.data.message.ugen_key=static_cast<size_t>(val->as_int());
    }else{
        cmd.data.message.ugen_key=static_cast<size_t>(args[0].as_int());
    }
    std::strncpy(cmd.data.message.message, args[1].display().c_str(), uSEQDSPEngine::MAX_MSG_KEY_LENGTH-2);
    cmd.data.message.message[uSEQDSPEngine::MAX_MSG_KEY_LENGTH-2] = '\0';
    if (args.size() == 3 ) {
        if (args[2].is_number()) {
            cmd.data.message.value.floatData.value = static_cast<float>(args[2].as_float());
        }
        else if (args[2].is_string()) {
            std::strncpy(cmd.data.message.value.stringData.value, args[2].display().c_str(), uSeqGen_Base::MAX_MSG_VALUE_LENGTH-2);
            cmd.data.message.value.stringData.value[uSeqGen_Base::MAX_MSG_VALUE_LENGTH-2] = '\0';
        }
    } else {
        cmd.data.message.value.floatData.value = 0.f; //default value
    }
    // println("msg: " + String(cmd.data.message.message));
    // println("ugen_key: " + String(cmd.data.message.ugen_key));
    // println("value: " + String(cmd.data.message.value));
    queue_try_add(&DSPQ::q_engine_commands, &cmd);
    return Value::string("msg sent");
}

Value uSEQ::useq_dsp_qget(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "ppp-get";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Checking individual args
    if (!(args[0].is_symbol()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a symbol",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    // BODY

    //get queue index from env
    String name  = args[0].display();
    float qvalue = 0.f;

    auto queueIndexVal = env.get(name);
    if(queueIndexVal)
    {
        size_t queueIndex = static_cast<size_t>(queueIndexVal.value().as_int());
        // println("get: " + name + String(queueIndex));
        
        //use index to get queue info
        ugenOutputQueue *qInfo =  &(dspEngine.ugenOutputQueues[queueIndex]);

        //get value from queue
        if (!queue_try_remove(qInfo->q, &qvalue) ) {
            qvalue = qInfo->lastValue[0];  //TODO: expand for lists
            // println("no new value available");
        }else{
            // println("New value: " + String(qvalue));
            qInfo->lastValue[0] = qvalue;
        }
    }else{
        // println("Warning: " + name + " not found in env");
    }
    return Value(qvalue);
}

Value uSEQ::useq_dsp_qset(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "ppp-set";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Checking individual args
    if (!(args[0].is_symbol()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a symbol",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    args[1] = args[1].eval(env);

    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY

    //get queue index from env
    String name  = args[0].display();
    const float qvalue = args[1].as_float();

    auto queueIndexVal = env.get(name);
    if(queueIndexVal)
    {
        size_t queueIndex = static_cast<size_t>(queueIndexVal.value().as_int());
        // println("get: " + name + String(queueIndex));
        
        //use index to get queue info
        if (dspEngine.ugenInputQueues.count(queueIndex) == 0) {
            println("Warning: queue not found");
            return Value::error();
        }
        ugenInputQueue *qInfo =  &(dspEngine.ugenInputQueues[queueIndex]);
        queue_try_add(qInfo->q, &qvalue);
    }else{
        // println("Warning: " + name + " not found in env");
    }
    return Value(qvalue);
}

void uSEQ::initDSP() {
    dspEngine.obj = std::make_unique<uSEQDSPEngine>();
    // dspEngine.obj->setup();
    // dspEngine->run(2);
}


void FAST_FUNC(uSEQ::tick_dsp)() {
    // Serial.printf("core1 %s\n", HOST);

    delay(1000);
}
#endif

#ifdef ARDUINO
Value uSEQ::useq_send_sync_trigger_i2c(std::vector<Value>& args, Environment& env)



{


    constexpr const char* user_facing_name = "useq-send-sync-trigger";


    


    // Check no arguments


    if (args.size() != 0)


    {


        report_error_wrong_num_args(user_facing_name, args.size(),


                                    NumArgsComparison::EqualTo, 0, -1);


        return Value::error();


    }


    


    // Send high on all digital outputs

    Wire1.setSDA(38);
    Wire1.setSCL(39);
    Wire1.begin();
    delay(100);
    float tmp_outputs[8];

    // digital_write_with_led(i, 1);
    for(size_t i=0; i<8; i++) {
        tmp_outputs[i] = 1;
    }
    Wire1.beginTransmission(1);
    Wire1.write((uint8_t*) &tmp_outputs, sizeof(tmp_outputs));
    int res = Wire1.endTransmission(true);         


  // Reset our own transport



    reset_logical_time();

    // Brief delay to ensure the trigger is registered

    delay(50);

    // Return outputs to low


    // digital_write_with_led(i, 1);
    for(size_t i=0; i<8; i++) {
        tmp_outputs[i] = 0;
    }
    Wire1.beginTransmission(1);
    Wire1.write((uint8_t*) &tmp_outputs, sizeof(tmp_outputs));
    res = Wire1.endTransmission(true);    
    delay(10);     

    Wire1.end();

    println("Sync sent");


    return Value::nil();
}
#endif
