#include "uSEQ.h"
#include "lisp/LispLibrary.h"
#include "lisp/interpreter.h"
#include "lisp/value.h"
#ifdef ARDUINO
#include "uSEQ/piopwm.h"
#endif
#include "utils.h"
#include "utils/log.h"
// #include "lisp/library.h"
#include <cmath>

// uSEQ MEMBER FUNCTIONS

// void dbg(String s) { std::cout << s.c_str() << std::endl; }

#if defined(USEQHARDWARE_1_0)
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

// void uSEQ::init_inputs()
// {
//     for (int i = 0; i < m_num_binary_ins; i++)
//     {
//         m_inputs.push_back(Input(INPUT_BINARY, i));
//     }
// }

// void uSEQ::init_outputs()
// {
//     // BINARY
//     for (int i = 0; i < m_num_binary_outs; i++)
//     {
//         m_outputs.push_back(Output(OUTPUT_BINARY, i));
//     }

//     // CONTINUOUS
//     for (int i = 0; i < m_num_continuous_outs; i++)
//     {
//         m_outputs.push_back(Output(INPUT_CONTINUOUS, i));
//     }
// }

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

// Write `level` to TX FIFO. State machine will copy this into X.
void pio_pwm_set_level(PIO pio, uint sm, uint32_t level)
{
    DBG("uSEQ::pio_pwm_set_level");
    dbg(String(reinterpret_cast<size_t>(pio)));
    dbg(String(sm));
    dbg(String(level));
    pio_sm_put_blocking(pio, sm, level);
}

void pio_pwm_set_period(PIO pio, uint sm, uint32_t period)
{
    pio_sm_set_enabled(pio, sm, false);
    pio_sm_put_blocking(pio, sm, period);
    pio_sm_exec(pio, sm, pio_encode_pull(false, false));
    pio_sm_exec(pio, sm, pio_encode_out(pio_isr, 32));
    pio_sm_set_enabled(pio, sm, true);
}

void setup_leds()
{
    DBG("uSEQ::setup_leds");

#ifndef MUSICTHING
    pinMode(LED_BOARD, OUTPUT); // test LED
    digitalWrite(LED_BOARD, 1);
    pinMode(USEQ_PIN_LED_I1, OUTPUT);
    pinMode(USEQ_PIN_LED_I2, OUTPUT);
#endif

#ifdef USEQHARDWARE_1_0
    pinMode(USEQ_PIN_LED_AI1, OUTPUT);
    pinMode(USEQ_PIN_LED_AI2, OUTPUT);
#endif

    for (int i = 0; i < 6; i++)
    {
        pinMode(useq_output_led_pins[i], OUTPUT);
    }
}

void uSEQ::init()
{
    DBG("uSEQ::init");
    setup_IO();

    // dbg("free heap (start):" + String(free_heap()));
    // if (!m_initialised)
    // {
    dbg("Setting instance pointer");
    Interpreter::useq_instance_ptr = this;
    Interpreter::init();

    init_builtinfuncs();
    eval_lisp_library();

    setup_leds();
    led_animation();

    // dbg("Lisp library loaded.");

    // uSEQ software setup
    set_bpm(m_defaultBPM, 0);
    update_time();
    init_ASTs();
    m_initialised = true;
    // }
    // else
    // {
    //     dbg("warning: already initialised");
    // }
    // dbg("free heap (end):" + String(free_heap()));
}

void uSEQ::led_animation()
{
    int ledDelay = 30;
#ifdef MUSICTHING
    ledDelay = 40;
    for (int i = 0; i < 8; i++)
    {
        digitalWrite(useq_output_led_pins[0], 1);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[2], 1);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[4], 1);
        digitalWrite(useq_output_led_pins[0], 0);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[5], 1);
        digitalWrite(useq_output_led_pins[2], 0);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[3], 1);
        digitalWrite(useq_output_led_pins[4], 0);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[1], 1);
        digitalWrite(useq_output_led_pins[5], 0);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[3], 0);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[1], 0);
        ledDelauy -= 3;
    }
#endif
#ifdef USEQHARDWARE_0_2
    for (int i = 0; i < 8; i++)
    {
        digitalWrite(USEQ_PIN_LED_I1, 1);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_A1, 1);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_D1, 1);
        digitalWrite(USEQ_PIN_LED_I1, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_D3, 1);
        digitalWrite(USEQ_PIN_LED_A1, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_D4, 1);
        digitalWrite(USEQ_PIN_LED_D1, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_D2, 1);
        digitalWrite(USEQ_PIN_LED_D3, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_A2, 1);
        digitalWrite(USEQ_PIN_LED_D4, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_I2, 1);
        digitalWrite(USEQ_PIN_LED_D2, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_A2, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_I2, 0);
        delay(ledDelay);
        ledDelay -= 3;
    }
#endif
#ifdef USEQHARDWARE_1_0
    for (int i = 0; i < 8; i++)
    {
        digitalWrite(USEQ_PIN_LED_AI1, 1);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_AI2, 1);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_A1, 1);
        digitalWrite(USEQ_PIN_LED_AI1, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_A2, 1);
        digitalWrite(USEQ_PIN_LED_AI2, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_A3, 1);
        digitalWrite(USEQ_PIN_LED_A1, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_D3, 1);
        digitalWrite(USEQ_PIN_LED_A2, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_D2, 1);
        digitalWrite(USEQ_PIN_LED_A3, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_D1, 1);
        digitalWrite(USEQ_PIN_LED_D3, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_I2, 1);
        digitalWrite(USEQ_PIN_LED_D2, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_I1, 1);
        digitalWrite(USEQ_PIN_LED_D1, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_I2, 0);
        delay(ledDelay);
        digitalWrite(USEQ_PIN_LED_I1, 0);
        delay(ledDelay);
        ledDelay -= 3;
    }
#endif
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
            //             Serial.println(m_scheduledItems[i].id);
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
        Serial.println("Error in q0 output function, clearing");
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
            // Serial.println(res.debug());
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
    updateSpeed = micros() - ts;
    set("fps", Value(1000000.0 / updateSpeed));
    set("qt", Value(updateSpeed * 0.001));
    ts = micros();

    DBG("uSEQ::tick");
    m_num_tick_starts += 1;
    dbg("tick starts: " + String(m_num_tick_starts));
    // Read & cache the hardware & software inputs
    update_inputs();
    // Update time
    update_time();
    check_code_quant_phasor();
    run_scheduled_items();
    // Re-run & cache output signal forms
    update_signals();
    // // Write cached output signals to hardware & software outputs
    update_outs();
    // // Check for new code and eval
    check_and_handle_user_input();
    m_num_tick_ends += 1;
    dbg("tick ends: " + String(m_num_tick_ends));
}

///////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////
int analog_out_LED_pin(int out)
{
    int res = -1;
    if (out <= NUM_CONTINUOUS_OUTS)
        res = useq_output_led_pins[out - 1];
    return res;
}

int digital_out_LED_pin(int out)
{
    int res    = -1;
    int pindex = NUM_CONTINUOUS_OUTS + out;
    if (pindex <= 6)
        res = useq_output_led_pins[pindex - 1];
    return res;
}

int analog_out_pin(int out)
{
    int res = -1;
    if (out <= NUM_CONTINUOUS_OUTS)
        res = useq_output_pins[out - 1];
    return res;
}

int digital_out_pin(int out)
{
    int res    = -1;
    int pindex = NUM_CONTINUOUS_OUTS + out;
    if (pindex <= 6)
        res = useq_output_pins[pindex - 1];
    return res;
}

BUILTINFUNC_MEMBER(
    ard_useqdw,
    if (evalled_args_contain_errors(args)) {
        error("useqdw has an error in one or more of its args");
    } else {
        int out = args[0].as_int();
        int val = args[1].as_int();
        digital_write_with_led(out, val);
    },
    2)

BUILTINFUNC_MEMBER(
    ard_useqaw,
    if (evalled_args_contain_errors(args)) {
        error("useqaw has an error in one or more of its args");
    } else {
        analog_write_with_led(args[0].as_int(), args[1].as_float());
        // int pin     = analog_out_pin();
        // int led_pin = analog_out_LED_pin(args[0].as_int());
        // int val     = args[1].as_float() * 2047.0;
        // analogWrite(pin, val);
        // analogWrite(led_pin, val);
    },
    2)

static uint8_t prevNextCode = 0;
static uint16_t store       = 0;

#ifdef USEQHARDWARE_0_2
int8_t read_rotary()
{
    static int8_t rot_enc_table[] = {
        0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0
    };

    prevNextCode <<= 2;
    if (digitalRead(USEQ_PIN_ROTARYENC_B))
        prevNextCode |= 0x02;
    if (digitalRead(USEQ_PIN_ROTARYENC_A))
        prevNextCode |= 0x01;
    prevNextCode &= 0x0f;

    // If valid then store as 16 bit data.
    if (rot_enc_table[prevNextCode])
    {
        store <<= 4;
        store |= prevNextCode;
        // if (store==0xd42b) return 1;
        // if (store==0xe817) return -1;
        if ((store & 0xff) == 0x2b)
            return -1;
        if ((store & 0xff) == 0x17)
            return 1;
    }
    return 0;
}

void uSEQ::read_rotary_encoders()
{
    static int32_t c, val;

    if (val = read_rotary())
    {
        m_input_vals[USEQR1] += val;
    }
}

void uSEQ::setup_rotary_encoder()
{
    pinMode(USEQ_PIN_SWITCH_R1, INPUT_PULLUP);
    pinMode(USEQ_PIN_ROTARYENC_A, INPUT_PULLUP);
    pinMode(USEQ_PIN_ROTARYENC_B, INPUT_PULLUP);
    m_input_vals[USEQR1] = 0;
}
#endif // useq 0.2 rotary

void uSEQ::update_inputs()
{
    DBG("uSEQ::update_inputs");

#ifdef USEQHARDWARE_0_2
    read_rotary_encoders();
#endif

    // inputs are input_pullup, so invert
    auto now              = micros();
    const double recp4096 = 0.000244141; // 1/4096
#ifdef MUSICTHING
    const size_t muxdelay = 2;
    // unroll loop for efficiency
    digitalWrite(MUX_LOGIC_A, 0);
    digitalWrite(MUX_LOGIC_B, 0);
    delayMicroseconds(muxdelay);
    m_input_vals[MTMAINKNOB] = analogRead(MUX_IN_1) * recp4096;
    m_input_vals[USEQAI1]    = analogRead(MUX_IN_2) * recp4096;
    digitalWrite(MUX_LOGIC_A, 0);
    digitalWrite(MUX_LOGIC_B, 1);
    delayMicroseconds(muxdelay);
    m_input_vals[MTYKNOB] = analogRead(MUX_IN_1) * recp4096;
    digitalWrite(MUX_LOGIC_A, 1);
    digitalWrite(MUX_LOGIC_B, 0);
    delayMicroseconds(muxdelay);
    m_input_vals[MTXKNOB] = analogRead(MUX_IN_1) * recp4096;
    m_input_vals[USEQAI2] = analogRead(MUX_IN_2) * recp4096;
    digitalWrite(MUX_LOGIC_A, 1);
    digitalWrite(MUX_LOGIC_B, 1);
    delayMicroseconds(muxdelay);
    int switchVal = analogRead(MUX_IN_1);
    if (switchVal < 100)
    {
        switchVal = 0;
    }
    else if (switchVal > 3500)
    {
        switchVal = 2;
    }
    else
    {
        switchVal = 1;
    }
    m_input_vals[MTZSWITCH] = switchVal;

    // Serial.print(m_input_vals[MTMAINKNOB]);
    // Serial.print("\t");
    // Serial.print(m_input_vals[MTXKNOB]);
    // Serial.print("\t");
    // Serial.print(m_input_vals[MTYKNOB]);
    // Serial.print("\t");
    // Serial.println(m_input_vals[MTZSWITCH]);

    const int input1 = 1 - digitalRead(USEQ_PIN_I1);
    const int input2 = 1 - digitalRead(USEQ_PIN_I2);
    digitalWrite(useq_output_led_pins[4], input1);
    digitalWrite(useq_output_led_pins[5], input2);
    m_input_vals[USEQI1] = input1;
    m_input_vals[USEQI2] = input2;

#else
    const auto input1    = 1 - digitalRead(USEQ_PIN_I1);
    const auto input2    = 1 - digitalRead(USEQ_PIN_I2);
    m_input_vals[USEQI1] = input1;
    m_input_vals[USEQI2] = input2;

    digitalWrite(USEQ_PIN_LED_I1, input1);
    digitalWrite(USEQ_PIN_LED_I2, input2);

    // tempo estimates
    tempoI1.averageBPM(input1, now);
    tempoI2.averageBPM(input2, now);

    m_input_vals[USEQM1] = 1 - digitalRead(USEQ_PIN_SWITCH_M1);
#ifdef USEQ_1_0_c
    const int ts_a       = 1 - digitalRead(USEQ_PIN_SWITCH_T1);
    const int ts_b       = 1 - digitalRead(USEQ_PIN_SWITCH_T2);
    if ((ts_a == 0) && (ts_b == 0))
    {
        m_input_vals[USEQT1] = 1;
    }
    else
    {
        if (ts_a == 1)
        {
            m_input_vals[USEQT1] = 2;
        }
        else
        {
            m_input_vals[USEQT1] = 0;
        }
    }
#else
    m_input_vals[USEQT1] = 1 - digitalRead(USEQ_PIN_SWITCH_T1);
#endif

#endif

#ifdef USEQHARDWARE_0_2
    m_input_vals[USEQRS1] = 1 - digitalRead(USEQ_PIN_SWITCH_R1);
    m_input_vals[USEQM2]  = 1 - digitalRead(USEQ_PIN_SWITCH_M2);
    m_input_vals[USEQT2]  = 1 - digitalRead(USEQ_PIN_SWITCH_T2);
#endif

#ifdef USEQHARDWARE_1_0
    auto v_ai1    = analogRead(USEQ_PIN_AI1);
    auto v_ai1_11 = v_ai1 >> 1;                  // scale from 12 bit to 11 bit range
    v_ai1_11      = (v_ai1_11 * v_ai1_11) >> 11; // sqr to get exp curve
    analogWrite(USEQ_PIN_LED_AI1, v_ai1_11);
    auto v_ai2    = analogRead(USEQ_PIN_AI2);
    auto v_ai2_11 = v_ai2 >> 1;
    v_ai2_11      = (v_ai2_11 * v_ai2_11) >> 11;
    analogWrite(USEQ_PIN_LED_AI2, v_ai2_11 >> 1);
    m_input_vals[USEQAI1] = v_ai1 * recp4096;
    m_input_vals[USEQAI2] = v_ai2 * recp4096;
#endif

    dbg("updating inputs...DONE");
}

bool is_new_code_waiting() { return Serial.available(); }

String get_code_waiting() { return Serial.readStringUntil('\n'); }

void uSEQ::check_and_handle_user_input()
{
    DBG("uSEQ::check_and_handle_user_input");
    // m_repl.check_and_handle_input();

    if (is_new_code_waiting())
    {
        int first_byte = Serial.read();
        // SERIAL
        if (first_byte == m_serial_stream_begin_marker /*31*/)
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

            if (m_last_received_code == exit_command)
            {
                m_should_quit = true;
            }
            // EXECUTE NOW
            if (first_byte == m_execute_now_marker /*'@'*/)
            {
                String result = eval(m_last_received_code);
                print("==> ");
                print(result);
                print("\n");
                print(">> ");
            }
            // SCHEDULE FOR LATER
            else
            {
                m_last_received_code =
                    String((char)first_byte) + m_last_received_code;
                // Serial.println(cmd);
                Value expr = parse(m_last_received_code);
                m_runQueue.push_back(expr);
            }
        }
    }
}

/// UPDATE methods
void uSEQ::update_continuous_signals()
{
    DBG("uSEQ::update_continuous_signals");

    for (int i = 0; i < m_num_continuous_outs; i++)
    {
        Value expr = m_continuous_ASTs[i];
        dbg("Evalling: " + expr.display());
        Value result = eval(expr);
        if (!result.is_number())
        {
            error("Expression specified for a" + String(i + 1) +
                  " does not result in a number - resetting to default.");
            error("Expression: \n" + expr.display());
            m_continuous_ASTs[i] = default_continuous_expr;
            m_continuous_vals[i] = 0.0;
        }
        else
        {
            m_continuous_vals[i] = result.as_float();
        }
    }
}

void uSEQ::update_binary_signals()
{
    DBG("uSEQ::update_binary_signals");

    for (int i = 0; i < m_num_binary_outs; i++)
    {
        Value expr = m_binary_ASTs[i];
        dbg("Evalling: " + expr.display());
        Value result = eval(expr);

        if (!result.is_number())
        {
            error("Expression specified for d" + String(i + 1) +
                  " does not eval to a number - resetting to default.");
            error("Expression: \n" + expr.display());
            m_binary_ASTs[i] = default_binary_expr;
            m_binary_vals[i] = 0;
        }
        else
        {
            m_binary_vals[i] = result.as_int();
        }
    }
}

void uSEQ::update_serial_signals()
{
    DBG("uSEQ::update_serial_signals");

    for (int i = 0; i < m_num_serial_outs; i++)
    {
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
                error("Expression specified for s" + String(i + 1) +
                      " does not eval to either a number or nil - resetting to "
                      "default.");
                error("Expression: \n" + expr.display());
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

void uSEQ::update_signals()
{
    DBG("uSEQ::update_signals");
    update_continuous_signals();
    update_binary_signals();
    update_serial_signals();
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
}

void uSEQ::set_time(size_t new_time_micros)
{
    DBG("uSEQ::set_time");

    time = new_time_micros;
    // last_t = t;
    t               = time - lastResetTime;
    m_beat_phase    = t % m_beat_length;
    m_bar_phase     = t % m_bar_length;
    m_phrase_phase  = t % m_phrase_length;
    m_section_phase = t % m_section_length;
    update_lisp_time_variables();
}

void uSEQ::update_lisp_time_variables()
{
    DBG("uSEQ::update_lisp_time_variables");

    // These should appear as seconds in Lisp-land
    double time_s = (double)time * 1e-6;
    double t_s    = (double)t * 1e-6;
    set("time", Value(time_s));
    set("t", Value(t_s));

    // Normalise the phasors
    double norm_beat    = (double)m_beat_phase / (double)m_beat_length;
    double norm_bar     = (double)m_bar_phase / (double)m_bar_length;
    double norm_phrase  = (double)m_phrase_phase / (double)m_phrase_length;
    double norm_section = (double)m_section_phase / (double)m_section_length;

    dbg("time_s = " + String(time_s));
    dbg("t_s = " + String(t_s));
    dbg("norm_beat = " + String(norm_beat));
    dbg("norm_bar = " + String(norm_bar));
    dbg("norm_phrase = " + String(norm_phrase));
    dbg("norm_section = " + String(norm_section));

    set("beat", Value(norm_beat));
    set("bar", Value(norm_bar));
    set("phrase", Value(norm_phrase));
    set("section", Value(norm_section));
}

void uSEQ::update_time()
{
    DBG("uSEQ::update_time");
    set_time(micros());
}

void uSEQ::update_bpm_variables()
{
    DBG("uSEQ::update_bpm_variables");

    set("bpm", Value(m_bpm));
    set("bps", Value(m_bpm / 60.0));
    // These should appear as seconds in Lisp-land
    set("beatDur", Value(m_beat_length * 1e-6));
    set("barDur", Value(m_bar_length * 1e-6));
    set("phraseDur", Value(m_phrase_length * 1e-6));
    set("sectionDur", Value(m_section_length * 1e-6));
}

#ifdef MIDIOUT
double last_midi_t = 0;
void uSEQ::update_midi_out()
{
    DBG("uSEQ::update_midi_out");
    const double midiRes        = 48 * meter_numerator * 1;
    const double timeUnitMillis = (barDur / midiRes);

    const double timeDeltaMillis = t - last_midi_t;
    size_t steps                 = floor(timeDeltaMillis / timeUnitMillis);
    double initValPhase          = bar - (timeDeltaMillis / barDur);

    if (steps > 0)
    {
        const double timeUnitBar = 1.0 / midiRes;

        auto itr = useqMDOMap.begin();
        for (; itr != useqMDOMap.end(); itr++)
        {
            // Iterate through the keys process MIDI events
            Value midiFunction = itr->second;
            if (initValPhase < 0)
                initValPhase++;
            std::vector<Value> mdoArgs = { Value(initValPhase) };
            Value prev                 = midiFunction.apply(mdoArgs, env);
            for (size_t step = 0; step < steps; step++)
            {
                double t_step = bar - ((steps - (step + 1)) * timeUnitBar);
                // wrap phasor
                if (t_step < 0)
                    t_step += 1.0;
                // Serial.println(t_step);
                mdoArgs[0] = Value(t_step);
                Value val  = midiFunction.apply(mdoArgs, env);

                // Serial.println(val.as_float());
                if (val > prev)
                {
                    Serial1.write(0x99);
                    Serial1.write(itr->first);
                    Serial1.write(val.as_int() * 14);
                }
                else if (val < prev)
                {
                    Serial1.write(0x89);
                    Serial1.write(itr->first);
                    Serial1.write((byte)0);
                }
                prev = val;
            }
        }
        last_midi_t = t;
    }
}
#endif // end of MIDI OUT SECTION

void uSEQ::setup_outs()
{
    DBG("uSEQ::setup_outs");
    for (int i = 0; i < NUM_CONTINUOUS_OUTS + NUM_BINARY_OUTS; i++)
    {
        pinMode(useq_output_pins[i], OUTPUT);
    }

#ifdef MUSICTHING
    pinMode(MUX_LOGIC_A, OUTPUT);
    pinMode(MUX_LOGIC_B, OUTPUT);
#endif
}

void setup_analog_outs()
{
    DBG("uSEQ::setup_analog_outs");
    dbg(String(NUM_CONTINUOUS_OUTS));
    // PWM outputs
    analogWriteFreq(30000);    // out of hearing range
    analogWriteResolution(11); // about the best we can get for 30kHz

    // set PIO PWM state machines to run PWM outputs
    uint offset  = pio_add_program(pio0, &pwm_program);
    uint offset2 = pio_add_program(pio1, &pwm_program);
    // printf("Loaded program at %d\n", offset);

    for (int i = 0; i < NUM_CONTINUOUS_OUTS; i++)
    {
        auto pioInstance = i < 4 ? pio0 : pio1;
        uint pioOffset   = i < 4 ? offset : offset2;
        auto smIdx       = i % 4;
        dbg(String(reinterpret_cast<size_t>(pioInstance)));
        dbg(String(reinterpret_cast<size_t>(pioOffset)));
        pwm_program_init(pioInstance, smIdx, pioOffset, useq_output_pins[i]);
        pio_pwm_set_period(pioInstance, smIdx, (1u << 13) - 1);
    }
}

void uSEQ::setup_digital_ins()
{
    pinMode(USEQ_PIN_I1, INPUT_PULLUP);
    pinMode(USEQ_PIN_I2, INPUT_PULLUP);
}

void uSEQ::setup_switches()
{
#ifdef USEQHARDWARE_1_0
    pinMode(USEQ_PIN_SWITCH_M1, INPUT_PULLUP);

    pinMode(USEQ_PIN_SWITCH_T1, INPUT_PULLUP);
#ifdef USEQ_1_0_c
    pinMode(USEQ_PIN_SWITCH_T2, INPUT_PULLUP);
#endif
#endif
#ifdef USEQHARDWARE_0_2
    pinMode(USEQ_PIN_SWITCH_M1, INPUT_PULLUP);
    pinMode(USEQ_PIN_SWITCH_M2, INPUT_PULLUP);

    pinMode(USEQ_PIN_SWITCH_T1, INPUT_PULLUP);
    pinMode(USEQ_PIN_SWITCH_T2, INPUT_PULLUP);
#endif
}

#ifdef ANALOG_INPUTS
void uSEQ::setup_analog_ins()
{
    analogReadResolution(12);
#ifdef USEQHARDWARE_1_0
    pinMode(USEQ_PIN_AI1, INPUT);
    pinMode(USEQ_PIN_AI2, INPUT);
#endif
#ifdef MUSICTHING
    pinMode(MUX_IN_1, INPUT);
    pinMode(MUX_IN_2, INPUT);
    pinMode(AUDIO_IN_1, INPUT);
    pinMode(AUDIO_IN_2, INPUT);
#endif
}
#endif

void uSEQ::setup_IO()
{
    DBG("uSEQ::setup_IO");

    setup_outs();
    setup_analog_outs();
    setup_digital_ins();
    setup_switches();
#ifdef USEQHARDWARE_0_2
    setup_rotary_encoder();
#endif
#ifdef ANALOG_INPUTS
    setup_analog_ins();
#endif

#ifdef MIDIOUT
    Serial1.setRX(1);
    Serial1.setTX(0);
    Serial1.begin(31250);
#endif

#ifdef USEQHARDWARE_1_0
    Wire.setSDA(0);
    Wire.setSCL(1);
    // peripheral
    //  Wire.begin(4);
    //  Wire.onReceive(receiveEvent);

    // controller
    //  Wire.begin();
#endif
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
        // TODO
        // m_continuous_ASTs.push_back(default_continuous_form);
        m_serial_ASTs.push_back(default_serial_expr);
        m_serial_vals.push_back(std::nullopt);
    }
}

size_t bpm_to_micros_per_beat(double bpm)
{
    // If input was nonsensical, return max size_t
    if (bpm <= 0.0)
    {
        return (size_t)-1;
    }
    return static_cast<size_t>(60000000.0 / bpm);
}

void uSEQ::set_bpm(double newBpm, double changeThreshold = 0.0)
{
    DBG("uSEQ::setBPM");

    if (fabs(newBpm - m_bpm) >= changeThreshold)
    {
        m_bpm = newBpm;
        // Derive phasor lengths (in micros)
        m_beat_length = bpm_to_micros_per_beat(newBpm);
        // FIXME: this should not assume quarters
        m_bar_length = m_beat_length * (4.0 / meter_denominator) * meter_numerator;
        m_phrase_length  = m_bar_length * m_barsPerPhrase;
        m_section_length = m_phrase_length * m_phrasesPerSection;

        update_bpm_variables();
    }
}

// NOTE: outputs are 0-indexed,
void uSEQ::analog_write_with_led(int output, double val)
{
    DBG("uSEQ::analog_write_with_led");

    constexpr double maxpwm = 8191.0;

    int scaled_val = val * maxpwm;
    dbg("scaled_val (before clamping) = " + String(scaled_val));

    // clamping
    if (scaled_val > maxpwm)
    {
        dbg("over maxpwm, clamping");
        scaled_val = maxpwm;
    }
    if (scaled_val < 0)
    {
        dbg("less than 0, clamping");
        scaled_val = 0;
    }

    // led
    int led_pin   = analog_out_LED_pin(output + 1);
    int ledsigval = scaled_val >> 2; // shift to 11 bit range for the LED
    ledsigval =
        (ledsigval * ledsigval) >> 11; // cheap way to square and get a exp curve

    dbg("output = " + String(output));
    dbg("pin = " + String(pin));
    dbg("led pin = " + String(led_pin));
    dbg("val = " + String(val));
    dbg("scaled_val = " + String(scaled_val));

    // write pwm
    pio_pwm_set_level(output < 4 ? pio0 : pio1, output % 4, scaled_val);
    // write led
    analogWrite(led_pin, ledsigval);
}

// NOTE: outputs are 0-indexed,
void uSEQ::digital_write_with_led(int output, int val)
{
    DBG("uSEQ::digital_write_with_led");

    int pin     = digital_out_pin(output + 1);
    int led_pin = digital_out_LED_pin(output + 1);

    dbg("output = " + String(output));
    dbg("pin = " + String(pin));
    dbg("led pin = " + String(led_pin));
    dbg("val = " + String(val));

#ifdef DIGI_OUT_INVERT
    digitalWrite(pin, 1 - val);
#else
    digitalWrite(output, val);
#endif
    digitalWrite(led_pin, val);
}

void uSEQ::serial_write(int out, double val)
{
    DBG("uSEQ::serial_write");

    Serial.write(m_serial_stream_begin_marker);
    Serial.write((u_int8_t)(out + 1));
    char* byteArray = reinterpret_cast<char*>(&val);
    for (size_t b = 0; b < 8; b++)
    {
        Serial.write(byteArray[b]);
    }
}

BUILTINFUNC_NOEVAL_MEMBER(useq_q0, set("q-expr", args[0]); m_q0AST = { args[0] };, 1)

// TODO: there is potentially a lot of duplicated/wasted memory by storing
// the exprs in both the environment and the class member vectors
// especially once the exprs get more and more complex

BUILTINFUNC_NOEVAL_MEMBER(
    useq_a1,
    if (NUM_CONTINUOUS_OUTS >= 1) {
        set("a1-expr", args[0]);
        m_continuous_ASTs[0] = { args[0] };
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_a2,
    if (NUM_CONTINUOUS_OUTS >= 2) {
        set("a2-expr", args[0]);
        m_continuous_ASTs[1] = { args[0] };
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_a3,
    if (NUM_CONTINUOUS_OUTS >= 3) {
        set("a3-expr", args[0]);
        m_continuous_ASTs[2] = { args[0] };
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_a4,
    if (NUM_CONTINUOUS_OUTS >= 4) {
        set("a4-expr", args[0]);
        m_continuous_ASTs[3] = { args[0] };
    },
    1)

BUILTINFUNC_NOEVAL_MEMBER(
    useq_a5,
    if (NUM_CONTINUOUS_OUTS >= 5) {
        set("a5-expr", args[0]);
        m_continuous_ASTs[4] = { args[0] };
    },
    1)

BUILTINFUNC_NOEVAL_MEMBER(
    useq_a6,
    if (NUM_CONTINUOUS_OUTS >= 6) {
        set("a6-expr", args[0]);
        m_continuous_ASTs[5] = { args[0] };
    },
    1)

// DIGITAL OUTS
BUILTINFUNC_NOEVAL_MEMBER(
    useq_d1,
    if (NUM_BINARY_OUTS >= 1) {
        set("d1-expr", args[0]);
        m_binary_ASTs[0] = { args[0] };
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_d2,
    if (NUM_BINARY_OUTS >= 2) {
        set("d2-expr", args[0]);
        m_binary_ASTs[1] = { args[0] };
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_d3,
    if (NUM_BINARY_OUTS >= 3) {
        set("d3-expr", args[0]);
        m_binary_ASTs[2] = { args[0] };
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_d4,
    if (NUM_BINARY_OUTS >= 4) {
        set("d4-expr", args[0]);
        m_binary_ASTs[3] = { args[0] };
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_d5,
    if (NUM_BINARY_OUTS >= 5) {
        set("d5-expr", args[0]);
        m_binary_ASTs[4] = { args[0] };
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_d6,
    if (NUM_BINARY_OUTS >= 6) {
        set("d6-expr", args[0]);
        m_binary_ASTs[5] = { args[0] };
    },
    1)

BUILTINFUNC_NOEVAL_MEMBER(useq_s1, set("s1-expr", args[0]);
                          m_serial_ASTs[0] = { args[0] };, 1)
BUILTINFUNC_NOEVAL_MEMBER(useq_s2, set("s2-expr", args[0]);
                          m_serial_ASTs[1] = { args[0] };, 1)
BUILTINFUNC_NOEVAL_MEMBER(useq_s3, set("s3-expr", args[0]);
                          m_serial_ASTs[2] = { args[0] };, 1)
BUILTINFUNC_NOEVAL_MEMBER(useq_s4, set("s4-expr", args[0]);
                          m_serial_ASTs[3] = { args[0] };, 1)
BUILTINFUNC_NOEVAL_MEMBER(useq_s5, set("s5-expr", args[0]);
                          m_serial_ASTs[4] = { args[0] };, 1)
BUILTINFUNC_NOEVAL_MEMBER(useq_s6, set("s6-expr", args[0]);
                          m_serial_ASTs[5] = { args[0] };, 1)
BUILTINFUNC_NOEVAL_MEMBER(useq_s7, set("s7-expr", args[0]);
                          m_serial_ASTs[6] = { args[0] };, 1)
BUILTINFUNC_NOEVAL_MEMBER(useq_s8, set("s8-expr", args[0]);
                          m_serial_ASTs[7] = { args[0] };, 1)

double fast(double speed, double phasor)
{
    phasor *= speed;
    double phase = fmod(phasor, 1.0);
    return phase;
}

// BUILTINFUNC_MEMBER(lisp_fast, double speed = args[0].as_float();
//                    double phasor     = args[1].as_float();
//                    double fastPhasor = fast(speed, phasor); ret =
//                    Value(fastPhasor); , 2)

BUILTINFUNC_NOEVAL_MEMBER(
    useq_fast, DBG("uSEQ::fast");
    double factor = Interpreter::eval_in(args[0], env).as_float();
    Value expr    = args[1];
    // store the current time to reset later
    size_t actual_time = time;
    // update the interpreter's time just for this expr
    size_t tmp_time = (double)actual_time * factor;
    dbg("factor: " + String(factor)); dbg("actual_time: " + String(actual_time));
    dbg("tmp_time: " + String(tmp_time));
    //
    set_time(tmp_time);
    //
    ret = Interpreter::eval_in(expr, env).as_float();
    // restore the interpreter's time
    set_time(actual_time);, 2)

BUILTINFUNC_NOEVAL_MEMBER(
    useq_slow, DBG("uSEQ::slow");
    double factor = Interpreter::eval_in(args[0], env).as_float();
    Value expr    = args[1];
    // store the current time to reset later
    size_t actual_time = time;
    // update the interpreter's time just for this expr
    size_t tmp_time = (double)actual_time / factor;
    dbg("factor: " + String(factor)); dbg("actual_time: " + String(actual_time));
    dbg("tmp_time: " + String(tmp_time));
    //
    set_time(tmp_time);
    //
    ret = Interpreter::eval_in(expr, env).as_float();
    // restore the interpreter's time
    set_time(actual_time);, 2)

// (schedule <name> <statement> <period>)
BUILTINFUNC_NOEVAL_MEMBER(lisp_schedule, const auto itemName = args[0].as_string();
                          const auto ast    = args[1];
                          const auto period = args[2].as_float(); scheduledItem v;
                          v.id = itemName; v.period = period; v.lastRun = 0;
                          //    v.statement = statement;
                          v.ast                              = { ast };
                          m_scheduledItems.push_back(v); ret = Value(0);, 3)

BUILTINFUNC_MEMBER(
    lisp_unschedule, const String id = args[0].as_string();
    auto is_item = [id](scheduledItem& v) { return v.id == id; };

    if (auto it = std::find_if(std::begin(m_scheduledItems),
                               std::end(m_scheduledItems), is_item);
        it != std::end(m_scheduledItems)) {
        m_scheduledItems.erase(it);
        println("Item removed");
    } else { println("Item not found"); },
    1)

BUILTINFUNC_VARGS_MEMBER(useq_setbpm, double newBpm = args[0].as_float();
                         double thresh = args.size() == 2 ? args[1].as_float() : 0;
                         set_bpm(newBpm, thresh); ret = args[0];, 1, 2)

BUILTINFUNC_MEMBER(
    useq_getbpm, int index = args[0].as_int(); if (index == 1) {
        ret = tempoI1.avgBPM;
    } else if (index == 1) { ret = tempoI2.avgBPM; } else { ret = 0; },
                                               1)

BUILTINFUNC_MEMBER(useq_settimesig,
                   set_time_signature(args[0].as_float(), args[1].as_float());
                   ret = Value(1);, 2)

BUILTINFUNC_MEMBER(useq_in1, ret = Value(m_input_vals[USEQI1]);, 0)
BUILTINFUNC_MEMBER(useq_in2, ret = Value(m_input_vals[USEQI2]);, 0)
BUILTINFUNC_MEMBER(useq_ain1, ret = Value(m_input_vals[USEQAI1]);, 0)
BUILTINFUNC_MEMBER(useq_ain2, ret = Value(m_input_vals[USEQAI2]);, 0)

#ifdef MUSICTHING
BUILTINFUNC_MEMBER(useq_mt_knob, ret = Value(m_input_vals[MTMAINKNOB]);, 0)
BUILTINFUNC_MEMBER(useq_mt_knobx, ret = Value(m_input_vals[MTXKNOB]);, 0)
BUILTINFUNC_MEMBER(useq_mt_knoby, ret = Value(m_input_vals[MTYKNOB]);, 0)
BUILTINFUNC_MEMBER(useq_mt_swz, ret = Value(m_input_vals[MTZSWITCH]);, 0)
#endif

BUILTINFUNC_MEMBER(
    useq_ssin, int index = args[0].as_int();
    if (index > 0 && index <= m_num_serial_ins) {
        ret = Value(m_serial_input_streams[index - 1]);
    },
    1)

BUILTINFUNC_MEMBER(
    useq_swm, int index = args[0].as_int(); if (index == 1) {
        ret = Value(m_input_vals[USEQM1]);
    } else { ret = Value(m_input_vals[USEQM2]); },
                                            1)

BUILTINFUNC_MEMBER(
    useq_swt, int index = args[0].as_int(); if (index == 1) {
        ret = Value(m_input_vals[USEQT1]);
    } else { ret = Value(m_input_vals[USEQT2]); },
                                            1)

BUILTINFUNC_MEMBER(useq_swr, ret = Value(m_input_vals[USEQRS1]);, 0)

BUILTINFUNC_MEMBER(useq_rot, ret = Value(m_input_vals[USEQR1]);, 0)

//(drum-predict <input-pattern>) -> list
// BUILTINFUNC_MEMBER(
//     useq_drumpredict, const std::vector<Value> inputs = args[0].as_list();
//     std::vector<char> invec(32, 1); for (size_t i = 0; i < 32; i++) {
//     invec[i] = inputs[i].as_int(); } std::vector<int> outvec(14, 0);
//     apply_logic_gate_net_singleval(invec.data(), outvec.data());
//     std::vector<Value> result(14); for (size_t i = 0; i < 14; i++) {
//     result.at(i) = Value(outvec.at(i)); } ret = Value(result);, 1)

// NOTE: doesn't eval its arguments until they're selected by the phasor
Value fromList(std::vector<Value>& lst, double phasor, Environment& env)
{
    if (phasor < 0.0)
    {
        phasor = 0;
    }
    else if (phasor > 1.0)
    {
        phasor = 1.0;
    }
    double scaled_phasor = lst.size() * phasor;
    size_t idx           = floor(scaled_phasor);
    if (idx == lst.size())
        idx--;
    return Interpreter::eval_in(lst[idx], env);
}

BUILTINFUNC_MEMBER(useq_dm, auto index = args[0].as_int();
                   auto v1 = args[1].as_float(); auto v2 = args[2].as_float();
                   ret = Value(index > 0 ? v2 : v1);, 3)

BUILTINFUNC_VARGS_MEMBER(
    useq_gates, auto lst = args[0].as_list();
    const double phasor     = args[1].as_float();
    const double speed      = args[2].as_float();
    const double pulseWidth = args.size() == 4 ? args[3].as_float() : 0.5;
    const double val        = fromList(lst, fast(speed, phasor), env).as_int();
    const double gates = fast(speed * lst.size(), phasor) < pulseWidth ? 1.0 : 0.0;
    ret                = Value(val * gates);, 3, 4)

BUILTINFUNC_VARGS_MEMBER(
    useq_gatesw, auto lst = args[0].as_list();
    const double phasor     = args[1].as_float();
    const double speed      = args.size() == 3 ? args[2].as_float() : 1.0;
    const double val        = fromList(lst, fast(speed, phasor), env).as_int();
    const double pulseWidth = val / 9.0;
    const double gate = fast(speed * lst.size(), phasor) < pulseWidth ? 1.0 : 0.0;
    ret               = Value((val > 0 ? 1.0 : 0.0) * gate);, 2, 3)

BUILTINFUNC_VARGS_MEMBER(
    useq_trigs, auto lst = args[0].as_list();
    const double phasor     = args[1].as_float();
    const double speed      = args.size() == 3 ? args[2].as_float() : 1.0;
    const double val        = fromList(lst, fast(speed, phasor), env).as_int();
    const double amp        = val / 9.0;
    const double pulseWidth = args.size() == 4 ? args[3].as_float() : 0.1;
    const double gate = fast(speed * lst.size(), phasor) < pulseWidth ? 1.0 : 0.0;
    ret               = Value((val > 0 ? 1.0 : 0.0) * gate * amp);, 2, 4)

BUILTINFUNC_MEMBER(useq_loopPhasor,
                   auto phasor                   = args[0].as_float();
                   auto loopPoint                = args[1].as_float();
                   if (loopPoint == 0) loopPoint = 1; // avoid infinity
                   double spedupPhasor           = fast(1.0 / loopPoint, phasor);
                   ret                           = spedupPhasor * loopPoint;, 2)

// (euclid <phasor> <n> <k> (<offset>) (<pulsewidth>)
BUILTINFUNC_VARGS_MEMBER(
    useq_euclidean, const double phasor = args[0].as_float();
    const int n = args[1].as_int(); const int k = args[2].as_int();
    const int offset       = (args.size() >= 4) ? args[3].as_int() : 0;
    const float pulseWidth = (args.size() == 5) ? args[4].as_float() : 0.5;
    const float fi = phasor * n; int i = static_cast<int>(fi);
    const float rem                    = fi - i;
    if (i == n) { i--; } const int idx = ((i + n - offset) * k) % n;
    ret = Value(idx < k && rem < pulseWidth ? 1 : 0);, 3, 5)

// (step <phasor> <count> (<offset>))

//////////////
BUILTINFUNC_VARGS_MEMBER(
    useq_fromList, auto lst = args[0].as_list();
    const double phasor = args[1].as_float(); ret = fromList(lst, phasor, env);
    if (args.size() == 3) {
        double scale = args[2].as_float();
        if (scale != 0)
        {
            ret = Value(ret / scale);
        }
    },
    2, 3)

Value flatten(Value& val, Environment& env)
{
    std::vector<Value> flattened;
    if (!val.is_list())
    {
        flattened.push_back(val);
    }
    else
    {
        auto valList = val.as_list();
        for (size_t i = 0; i < valList.size(); i++)
        {
            Value evaluatedElement = Interpreter::eval_in(valList[i], env);
            if (evaluatedElement.is_list())
            {
                auto flattenedElement = flatten(evaluatedElement, env).as_list();
                flattened.insert(flattened.end(), flattenedElement.begin(),
                                 flattenedElement.end());
            }
            else
            {
                flattened.push_back(evaluatedElement);
            }
        }
    }
    return Value(flattened);
}

BUILTINFUNC_MEMBER(useq_fromFlattenedList,
                   auto lst      = flatten(args[0], env).as_list();
                   double phasor = args[1].as_float();
                   ret           = fromList(lst, phasor, env);, 2)
BUILTINFUNC_MEMBER(useq_flatten, ret = flatten(args[0], env);, 1)
BUILTINFUNC_MEMBER(
    useq_interpolate, auto lst = args[0].as_list();
    double phasor = args[1].as_float();
    if (phasor < 0.0) { phasor = 0; } else if (phasor > 1) { phasor = 1; } float a;
    double index = phasor * (lst.size() - 1);
    size_t pos0  = static_cast<size_t>(index); if (pos0 == (lst.size() - 1)) pos0--;
    a            = (index - pos0);
    double v2    = Interpreter::eval_in(lst[pos0 + 1], env).as_float();
    double v1    = Interpreter::eval_in(lst[pos0], env).as_float();
    ret          = Value(((v2 - v1) * a) + v1);, 2)

// (step <phasor> <count> (<offset>))
BUILTINFUNC_VARGS_MEMBER(
    useq_step, const double phasor = args[0].as_float();
    const int count     = args[1].as_int();
    const double offset = (args.size() == 3) ? args[2].as_float() : 0;
    double val = static_cast<int>(phasor * abs(count)); if (val == count) val--;
    ret        = Value((count > 0 ? val : count - 1 - val) + offset);, 2, 3)

void uSEQ::set_time_signature(double numerator, double denominator)
{
    meter_denominator = denominator;
    meter_numerator   = numerator;
    set_bpm(m_bpm);
}

#ifdef MIDIOUT
// midi drum out
BUILTINFUNC_MEMBER(
    useq_mdo, int midiNote = args[0].as_int(); if (args[1] != 0) {
        useqMDOMap[midiNote] = args[1];
    } else { useqMDOMap.erase(midiNote); },
                                               2)
#endif

// Creates a Lisp Value of type BUILTIN_METHOD,
// which requires
#define INSERT_BUILTINDEF(__name__, __func_name__)                                  \
    Environment::builtindefs[__name__] =                                            \
        Value((String)__name__, &uSEQ::__func_name__);

void uSEQ::init_builtinfuncs()
{
    DBG("uSEQ::init_builtinfuncs");

    // a
    INSERT_BUILTINDEF("a1", useq_a1);
    INSERT_BUILTINDEF("a2", useq_a2);
    INSERT_BUILTINDEF("a3", useq_a3);
    INSERT_BUILTINDEF("a4", useq_a4);
    INSERT_BUILTINDEF("a5", useq_a5);
    INSERT_BUILTINDEF("a6", useq_a6);
    // d
    INSERT_BUILTINDEF("d1", useq_d1);
    INSERT_BUILTINDEF("d2", useq_d2);
    INSERT_BUILTINDEF("d3", useq_d3);
    INSERT_BUILTINDEF("d4", useq_d4);
    INSERT_BUILTINDEF("d5", useq_d5);
    INSERT_BUILTINDEF("d6", useq_d6);
    // s
    INSERT_BUILTINDEF("s1", useq_s1);
    INSERT_BUILTINDEF("s2", useq_s2);
    INSERT_BUILTINDEF("s3", useq_s3);
    INSERT_BUILTINDEF("s4", useq_s4);
    INSERT_BUILTINDEF("s5", useq_s5);
    INSERT_BUILTINDEF("s6", useq_s6);

    // These are not class methods, so they can be inserted normally
    INSERT_BUILTINDEF("useqaw", ard_useqaw);
    INSERT_BUILTINDEF("useqdw", ard_useqdw);

    // These are all class methods
    INSERT_BUILTINDEF("swm", useq_swm);
    INSERT_BUILTINDEF("swt", useq_swt);
    INSERT_BUILTINDEF("swr", useq_swr);
    INSERT_BUILTINDEF("rot", useq_rot);
    INSERT_BUILTINDEF("ssin", useq_ssin);

    INSERT_BUILTINDEF("in1", useq_in1);
    INSERT_BUILTINDEF("in2", useq_in2);
    INSERT_BUILTINDEF("gin1", useq_in1);
    INSERT_BUILTINDEF("gin2", useq_in2);
    INSERT_BUILTINDEF("ain1", useq_ain1);
    INSERT_BUILTINDEF("ain2", useq_ain2);

    INSERT_BUILTINDEF("fast", useq_fast);
    INSERT_BUILTINDEF("slow", useq_slow);

    INSERT_BUILTINDEF("setbpm", useq_setbpm);
    INSERT_BUILTINDEF("getbpm", useq_getbpm);
    INSERT_BUILTINDEF("settimesig", useq_settimesig);
    INSERT_BUILTINDEF("schedule", lisp_schedule);
    INSERT_BUILTINDEF("unschedule", lisp_unschedule);

    INSERT_BUILTINDEF("looph", useq_loopPhasor);
    INSERT_BUILTINDEF("dm", useq_dm);
    INSERT_BUILTINDEF("gates", useq_gates);
    INSERT_BUILTINDEF("gatesw", useq_gatesw);
    INSERT_BUILTINDEF("trigs", useq_trigs);
    INSERT_BUILTINDEF("euclid", useq_euclidean);
    // NOTE: different names for the same function
    INSERT_BUILTINDEF("fromList", useq_fromList);
    INSERT_BUILTINDEF("seq", useq_fromList);
    //
    INSERT_BUILTINDEF("flatIdx", useq_fromFlattenedList);
    INSERT_BUILTINDEF("flat", useq_flatten);
    INSERT_BUILTINDEF("interp", useq_interpolate);
    INSERT_BUILTINDEF("step", useq_step);

    // TODO
#ifdef MUSICTHING
    Environment::builtindefs["knob"]  = Value("knob", builtin::useq_mt_knob);
    Environment::builtindefs["knobx"] = Value("knobx", builtin::useq_mt_knobx);
    Environment::builtindefs["knoby"] = Value("knoby", builtin::useq_mt_knoby);
    Environment::builtindefs["swz"]   = Value("swz", builtin::useq_mt_swz);
#endif

#ifdef MIDIOUT
    INSERT_BUILTINDEF("mdo", useq_mdo);
#endif
}
