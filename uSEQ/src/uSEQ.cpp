#include "uSEQ.h"
#include "lisp/LispLibrary.h"
#include "lisp/interpreter.h"
#include "lisp/value.h"
#include <cstddef>
#ifdef ARDUINO
#include "uSEQ/piopwm.h"
#endif
#include "utils.h"
#include "utils/log.h"

// #include "lisp/library.h"
#include <cmath>

//statics
uSEQ* uSEQ::instance;


double maxiFilter::lopass(double input, double cutoff) {
	output=z + cutoff*(input-z);
	z=output;
	return(output);
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

float pdm_y=0;
float pdm_err=0;
float pdm_w=0;


bool timer_callback(repeating_timer_t *mst) {
    pdm_y = pdm_w > pdm_err ? 1 : 0;
    pdm_err = pdm_y - pdm_w + pdm_err;
    if (pdm_y == 1) {
      //on
      digitalWrite(USEQ_PIN_LED_AI1, HIGH);
    }else{
      //off
      digitalWrite(USEQ_PIN_LED_AI1, LOW);
    }


//   w = w + 0.00000001;
//   if (w>=1) w=0;

  return true;
}

void setup_leds()
{
    DBG("uSEQ::setup_leds");

#ifndef MUSICTHING
    pinMode(LED_BOARD, OUTPUT); // test LED
    digitalWrite(LED_BOARD, 1);
    pinMode(USEQ_PIN_LED_I1, OUTPUT_2MA);
    pinMode(USEQ_PIN_LED_I2, OUTPUT_2MA);
#endif

#ifdef USEQHARDWARE_1_0
    pinMode(USEQ_PIN_LED_AI1, OUTPUT_2MA);
    pinMode(USEQ_PIN_LED_AI2, OUTPUT_2MA);
#endif

    for (int i = 0; i < 6; i++)
    {
        pinMode(useq_output_led_pins[i], OUTPUT_2MA);
        gpio_set_slew_rate(useq_output_led_pins[i], GPIO_SLEW_RATE_SLOW);
    }
  static repeating_timer_t mst;
  add_repeating_timer_us(150, timer_callback, NULL, &mst);

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
    eval_lisp_library();

    led_animation();
    setup_IO();

    // dbg("Lisp library loaded.");

    // uSEQ software setup
    set_bpm(m_defaultBPM, 0.0);
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
            println(res.debug());
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
    update_inputs();
    // Update time
    update_time();
    check_code_quant_phasor();
    run_scheduled_items();
    // Re-run & cache output signal forms
    update_signals();
    // Write cached output signals to hardware and/or software outputs
    update_outs();
    // Check for new code and eval (or schedule it)
    check_and_handle_user_input();

    //tiny delay to allow for interrupts etc
    delayMicroseconds(100);
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


MedianFilter mf1(51), mf2(51);


void uSEQ::update_inputs()
{
    DBG("uSEQ::update_inputs");

#ifdef USEQHARDWARE_0_2
    read_rotary_encoders();
#endif

    // inputs are input_pullup, so invert
    auto now              = micros();
    const double recp4096 = 0.000244141; // 1/4096
    const double recp2048 = 1/2048.0;
    const double recp1024 = 1/1024.0;
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
    //switch off LED while making measurements
    // digitalWrite(USEQ_PIN_LED_AI1, 0);
    // digitalWrite(USEQ_PIN_LED_AI2, 0);
    // delayMicroseconds(100);

    // pdm_w = 0;
    // delayMicroseconds(10);


    auto v_ai1    = analogRead(USEQ_PIN_AI1);
    auto v_ai2    = analogRead(USEQ_PIN_AI2);

    auto v_ai1_11 = v_ai1;                  
    v_ai1_11      = (v_ai1_11 * v_ai1_11) >> 11; // sqr to get exp curve

    // analogWriteFreq(25000);    // out of hearing range

    // digitalWrite(USEQ_PIN_LED_AI2, 0);

    // analogWrite(USEQ_PIN_LED_AI1, v_ai1_11 + random(-100,100));
    pdm_w = v_ai1_11 / 2048.0;
    auto v_ai2_11 = v_ai2;

    v_ai2_11      = (v_ai2_11 * v_ai2_11) >> 11;
    analogWrite(USEQ_PIN_LED_AI2, v_ai2_11);

    const double lpcoeff = 0.2; //0.009;
    double filt1 = mf1.process(v_ai1 * recp2048);
    double filt2 = mf2.process(v_ai1 * recp2048);
    // m_input_vals[USEQAI1] = cvInFilter[0].lopass(v_ai1 * recp2048, lpcoeff);
    // m_input_vals[USEQAI2] = cvInFilter[1].lopass(v_ai2 * recp2048, lpcoeff);
    m_input_vals[USEQAI1] = filt1;
    m_input_vals[USEQAI2] = filt2;
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
        // TODO remove
        user_interaction = true;

        int first_byte = Serial.read();
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

            if (m_last_received_code == exit_command)
            {
                m_should_quit = true;
            }
            // EXECUTE NOW
            if (first_byte == SerialMsg::execute_now_marker /*'@'*/)
            {
                String result = eval(m_last_received_code);
                println(result);
            }
            // SCHEDULE FOR LATER
            else
            {
                m_last_received_code =
                    String((char)first_byte) + m_last_received_code;
                println(m_last_received_code);
                Value expr = parse(m_last_received_code);
                m_runQueue.push_back(expr);
            }
        }

        // TODO remove
        user_interaction = false;
    }
}

/// UPDATE methods
void uSEQ::update_continuous_signals()
{
    DBG("uSEQ::update_continuous_signals");

    for (int i = 0; i < m_num_continuous_outs; i++)
    {
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
}

void uSEQ::update_binary_signals()
{
    DBG("uSEQ::update_binary_signals");

    for (int i = 0; i < m_num_binary_outs; i++)
    {
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
                error("Expression specified for a" + String(i + 1) +
                      " does not result in a number - resetting to default.");
                error("Expression: \n" + expr.display());
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

    // Flip flag on only for evals that happen
    // for output signals
    m_attempt_eval_as_signals = true;

    update_continuous_signals();
    update_binary_signals();
    update_serial_signals();

    m_attempt_eval_as_signals = false;
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

void uSEQ::update_time()
{
    DBG("uSEQ::update_time");

    // 1. Get time-since-boot reading from the board
    m_time_since_boot = static_cast<TimeValue>(micros());

    // 2. Check if it has overflowed
    if (m_time_since_boot - m_last_known_time_since_boot < 0.0)
    {
        println("WARNING: overflow detected, compensating for it...");
        m_overflow_counter++;
    }

    // Max value that size_t can hold before overflow
    constexpr TimeValue max_size_t = static_cast<TimeValue>((size_t)-1);

    // 3. Offset according to how many overflows we've had so far
    m_time_since_boot += static_cast<TimeValue>(m_overflow_counter) * max_size_t;

    // 4. U
    update_logical_time(m_time_since_boot);

    m_last_known_time_since_boot = m_time_since_boot;
}

void uSEQ::reset_logical_time() {
    m_last_transport_reset_time = m_time_since_boot;
    update_logical_time(m_time_since_boot);
}

void uSEQ::update_logical_time(TimeValue actual_time)
{
    DBG("uSEQ::set_time");

    // Update the main UI timekeeping variable
    m_transport_time = actual_time - m_last_transport_reset_time;

    // Phasors
    m_beat_phase    = beat_at_time(m_transport_time);
    m_bar_phase     = bar_at_time(m_transport_time);
    m_phrase_phase  = phrase_at_time(m_transport_time);
    m_section_phase = section_at_time(m_transport_time);

    // Push them to the interpreter
    update_lisp_time_variables();
}

void uSEQ::update_lisp_time_variables()
{
    DBG("uSEQ::update_lisp_time_variables");

    // These should appear as seconds in Lisp-land
    TimeValue time_s = m_time_since_boot * 1e-6;
    TimeValue t_s    = m_transport_time * 1e-6;
    set("time", Value(time_s));
    set("t", Value(t_s));

    // dbg("time_s = " + String(time_s));
    // dbg("t_s = " + String(t_s));
    // dbg("norm_beat = " + String(norm_beat));
    // dbg("norm_bar = " + String(norm_bar));
    // dbg("norm_phrase = " + String(norm_phrase));
    // dbg("norm_section = " + String(norm_section));

    set("beat", Value(m_beat_phase));
    set("bar", Value(m_bar_phase));
    set("phrase", Value(m_phrase_phase));
    set("section", Value(m_section_phase));
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
        pinMode(useq_output_pins[i], OUTPUT_2MA);
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
    analogWriteFreq(80000);    // out of hearing range
    analogWriteResolution(11); // about the best we can get

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
        // pwm_program_init(pioInstance, smIdx, pioOffset, useq_output_pins[i]);
        pwm_program_init(pioInstance, smIdx, pioOffset, useq_output_led_pins[i]);
        pio_pwm_set_period(pioInstance, smIdx, (1u << 11) - 1);
    }
}


void uSEQ::update_clock_from_external(double ts) {
    double newBPM = tempoI1.averageBPM(ts);
    if (ext_clock_tracker.count == 0) {
        newBPM *= (4.0 / meter_numerator / ext_clock_tracker.div);
        // println(String(newBPM));
        // println(String(beatCountI1));
        // println("bar: " + String(barCountI1));
        // println("barpf: " + String(m_bars_per_phrase));
        double std = tempoI1.std();
        // println("std: " + String(std));
        bool highstd = std > 100.0;
        //adjust every bar in high variance, otherwise every phrase
        if ((ext_clock_tracker.beat_count == 0 & highstd) || ext_clock_tracker.beat_count == 0) {
            // println("----------------------------------------reset");
            set_bpm(newBPM, 0);    
            reset_logical_time();
        }
        ext_clock_tracker.beat_count++;
        if (meter_denominator == ext_clock_tracker.beat_count) {
            ext_clock_tracker.beat_count = 0;
            ext_clock_tracker.bar_count++;
            if (ext_clock_tracker.bar_count == static_cast<size_t>(m_bars_per_phrase)) {
                ext_clock_tracker.bar_count=0;
            }
        }
    }
    ext_clock_tracker.count++;
    if (ext_clock_tracker.count == ext_clock_tracker.div) {
        ext_clock_tracker.count=0;
        // println("clock=0");
    }
}
/*
    const auto input1    = 1 - digitalRead(USEQ_PIN_I1);
    const auto input2    = 1 - digitalRead(USEQ_PIN_I2);
    m_input_vals[USEQI1] = input1;
    m_input_vals[USEQI2] = input2;

    digitalWrite(USEQ_PIN_LED_I1, input1);
    digitalWrite(USEQ_PIN_LED_I2, input2);

    m_input_vals[USEQM1] = 1 - digitalRead(USEQ_PIN_SWITCH_M1);
*/

void uSEQ::set_input_val(size_t index, double value) {
    m_input_vals[index] = value;
}

void uSEQ::gpio_irq_gate1() {
    double ts = static_cast<double>(micros());
    const auto input1 = 1 - digitalRead(USEQ_PIN_I1);
    uSEQ::instance->set_input_val(USEQI1, input1);
    digitalWrite(USEQ_PIN_LED_I1, input1);
    if (input1==1 && uSEQ::instance->getClockSource() == uSEQ::CLOCK_SOURCES::EXTERNAL_I1) {
        uSEQ::instance->update_clock_from_external(ts);
    }
}

void uSEQ::gpio_irq_gate2() {
    double ts = static_cast<double>(micros());
    const auto input2 = 1 - digitalRead(USEQ_PIN_I2);
    uSEQ::instance->set_input_val(USEQI2, input2);
    digitalWrite(USEQ_PIN_LED_I2, input2);
    if (input2==1 && uSEQ::instance->getClockSource() == uSEQ::CLOCK_SOURCES::EXTERNAL_I2) {
        uSEQ::instance->update_clock_from_external(ts);
    }

}

void uSEQ::setup_digital_ins()
{
    pinMode(USEQ_PIN_I1, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(USEQ_PIN_I1), uSEQ::gpio_irq_gate1, CHANGE);    
    pinMode(USEQ_PIN_I2, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(USEQ_PIN_I2), uSEQ::gpio_irq_gate2, CHANGE);    
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
    analogReadResolution(11);
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
        m_serial_ASTs.push_back(default_serial_expr);
        m_serial_vals.push_back(std::nullopt);
    }
}

double bpm_to_micros_per_beat(double bpm)
{
    // 60 seconds * 1e+6 microseconds
    constexpr double micros_in_minute = 60e+6;
    return micros_in_minute / bpm;
}

void uSEQ::set_bpm(double newBpm, double changeThreshold = 0.0)
{
    DBG("uSEQ::setBPM");

    if (newBpm <= 0.0)
    {
        error("Invalid BPM requested: " + String(newBpm));
    }
    else if (fabs(newBpm - m_bpm) >= changeThreshold)
    {
        m_bpm = newBpm;
        // Derive phasor lengths (in micros)
        m_beat_length = bpm_to_micros_per_beat(newBpm);
        m_bar_length = m_beat_length * (4.0 / meter_denominator) * meter_numerator;
        m_phrase_length  = m_bar_length * m_bars_per_phrase;
        m_section_length = m_phrase_length * m_phrases_per_section;

        update_bpm_variables();
    }
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

// NOTE: outputs are 0-indexed,
void uSEQ::analog_write_with_led(int output, double val)
{
    DBG("uSEQ::analog_write_with_led");

    constexpr double maxpwm = 2047.0;

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
    // int led_pin   = analog_out_LED_pin(output + 1);
    int pwm_pin = analog_out_pin(output+1);
    int ledsigval = scaled_val;// >> 2; // shift to 11 bit range for the LED
    ledsigval =
        (ledsigval * ledsigval) >> 11; // cheap way to square and get a exp curve

    dbg("output = " + String(output));
    dbg("pin = " + String(pin));
    dbg("led pin = " + String(led_pin));
    dbg("val = " + String(val));
    dbg("scaled_val = " + String(scaled_val));

    // // write pwm
    // pio_pwm_set_level(output < 4 ? pio0 : pio1, output % 4, scaled_val);
    // // write led
    // analogWrite(led_pin, ledsigval);

    // write pwm
    pio_pwm_set_level(pio0, output, ledsigval);
    analogWrite(pwm_pin, scaled_val);
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

    // write digi
#ifdef DIGI_OUT_INVERT
    digitalWrite(pin, 1 - val);
#else
    digitalWrite(pin, val);
#endif
    // write led
    digitalWrite(led_pin, val);
}

void uSEQ::serial_write(int out, double val)
{
    DBG("uSEQ::serial_write");

    Serial.write(SerialMsg::message_begin_marker);
    Serial.write((u_int8_t)SerialMsg::serial_message_types::STREAM);
    Serial.write((u_int8_t)(out + 1));
    u_int8_t* byteArray = reinterpret_cast<u_int8_t*>(&val);
    for (size_t b = 0; b < 8; b++)
    {
        Serial.write(byteArray[b]);
    }
}

void uSEQ::set_time_sig(double numerator, double denominator)
{
    meter_denominator = denominator;
    meter_numerator   = numerator;
    // This will refresh the phasor durations
    // with the new meter
    set_bpm(m_bpm, 0.0);
}

double fast(double speed, double phasor)
{
    phasor *= speed;
    double phase = fmod(phasor, 1.0);
    return phase;
}

// BUILTINFUNC_MEMBER(useq_fast_old, double speed = args[0].as_float();
//                    double phasor     = args[1].as_float();
//                    double fastPhasor = fast(speed, phasor); ret =
//                    Value(fastPhasor); , 2)

// Value uSEQ::useq_fast(std::vector<Value>& args, Environment& env)
// {
//     DBG("uSEQ::fast");
//     constexpr const char* user_facing_name = "fast";

//     // Checking number of args
//     if (!(args.size() == 2))
//     {
//         error_wrong_num_args(user_facing_name, args.size(),
//                              NumArgsComparison::EqualTo, 2, 0);
//         return Value::error();
//     }

//     // Eval first arg only
//     Value pre_eval = args[0];
//     args[0]        = args[0].eval(env);
//     if (args[0].is_error())
//     {
//         error_arg_is_error(user_facing_name, 1, pre_eval.display());
//         return Value::error();
//     }
//     // Checking first arg
//     if (!(args[0].is_number()))
//     {
//         error_wrong_specific_pred(user_facing_name, 1, "a number",
//                                   args[1].display());
//         return Value::error();
//     }

//     // BODY
//     Value result = Value::nil();

//     // double factor = args[0].as_float();
//     // // save current time to restore when done
//     // TimeValue current_transport_time = m_transport_time;
//     // TimeValue tmp_time               = m_transport_time * factor;
//     // dbg("factor: " + String(factor));
//     // dbg("actual_time: " + String(actual_time));
//     // dbg("tmp_time: " + String(tmp_time));
//     // //
//     // update_logical_time(tmp_time);
//     // //
//     // Value sig = args[1];
//     // result    = sig.eval(env);
//     // // restore the interpreter's time
//     // update_logical_time(current_transport_time);
//     // result = fast(factor, sig.as_float());

//     return result;
// }
Value uSEQ::useq_fast(std::vector<Value>& args, Environment& env)
{
    DBG("uSEQ::fast");
    constexpr const char* user_facing_name = "fast";

    // Checking number of args
    if (!(args.size() == 2))
    {
        error_wrong_num_args(user_facing_name, args.size(),
                             NumArgsComparison::EqualTo, 2, 0);
        return Value::error();
    }

    // Eval first arg
    Value pre_eval = args[0];
    args[0]        = args[0].eval(env);
    if (args[0].is_error())
    {
        error_arg_is_error(user_facing_name, 1, pre_eval.display());
        return Value::error();
    }
    // Checking first arg
    if (!(args[0].is_number()))
    {
        error_wrong_specific_pred(user_facing_name, 1, "a number",
                                  args[0].display());
        return Value::error();
    }

    // BODY
    Value result           = Value::nil();
    double current_time_s  = env.get("t").value().as_float();
    double factor          = args[0].as_float();
    double new_time_micros = (current_time_s * factor) * 1e+6;
    result                 = eval_at_time(args[1], env, new_time_micros);
    return result;
}

Value uSEQ::useq_slow(std::vector<Value>& args, Environment& env)
{
    DBG("uSEQ::useq_slow");
    constexpr const char* user_facing_name = "slow";

    // Checking number of args
    if (!(args.size() == 2))
    {
        error_wrong_num_args(user_facing_name, args.size(),
                             NumArgsComparison::EqualTo, 2, 0);
        return Value::error();
    }

    // Eval first arg only
    Value pre_eval = args[0];
    args[0]        = args[0].eval(env);
    if (args[0].is_error())
    {
        error_arg_is_error(user_facing_name, 1, pre_eval.display());
        return Value::error();
    }
    // Checking first arg
    if (!(args[0].is_number()))
    {
        error_wrong_specific_pred(user_facing_name, 1, "a number",
                                  args[1].display());
        return Value::error();
    }

    // BODY
    Value result           = Value::nil();
    double current_time_s  = env.get("t").value().as_float();
    double factor          = args[0].as_float();
    double new_time_micros = (current_time_s / factor) * 1e+6;
    result                 = eval_at_time(args[1], env, new_time_micros);
    return result;
}

Value uSEQ::useq_offset_time(std::vector<Value>& args, Environment& env)
{
    DBG("uSEQ::fast");
    constexpr const char* user_facing_name = "fast";

    // Checking number of args
    if (!(args.size() == 2))
    {
        error_wrong_num_args(user_facing_name, args.size(),
                             NumArgsComparison::EqualTo, 2, 0);
        return Value::error();
    }

    // Eval first arg
    Value pre_eval = args[0];
    args[0]        = args[0].eval(env);
    if (args[0].is_error())
    {
        error_arg_is_error(user_facing_name, 1, pre_eval.display());
        return Value::error();
    }
    // Checking first arg
    if (!(args[0].is_number()))
    {
        error_wrong_specific_pred(user_facing_name, 1, "a number",
                                  args[0].display());
        return Value::error();
    }

    // BODY
    Value result           = Value::nil();
    double current_time_s  = env.get("t").value().as_float();
    double amt             = args[0].as_float();
    double new_time_micros = (current_time_s + amt) * 1e+6;
    result                 = eval_at_time(args[1], env, new_time_micros);
    return result;
}

// (schedule <name> <period> <expr>)
Value uSEQ::useq_schedule(std::vector<Value>& args, Environment& env)
{
    DBG("uSEQ::lisp_schedule");
    constexpr const char* user_facing_name = "schedule";

    // Checking number of args
    if (!(args.size() == 3))
    {
        error_wrong_num_args(user_facing_name, args.size(),
                             NumArgsComparison::EqualTo, 3, 0);
        return Value::error();
    }

    // Evaluating ONLY first 2 args & checking for errors
    for (size_t i = 0; i < 2; i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_string()))
    {
        error_wrong_specific_pred(user_facing_name, 1, "a string",
                                  args[0].display());
        return Value::error();
    }
    if (!(args[1].is_number()))
    {
        error_wrong_specific_pred(user_facing_name, 2, "a number",
                                  args[1].display());
        return Value::error();
    }
    // BODY
    const auto itemName = args[0].as_string();
    const auto period   = args[1].as_float();
    const auto ast      = args[2];
    scheduledItem v;
    v.id      = itemName;
    v.period  = period;
    v.lastRun = 0;
    v.ast     = ast;
    m_scheduledItems.push_back(v);
    return Value::nil();
}

// (schedule <name> <period> <expr>)
Value uSEQ::useq_unschedule(std::vector<Value>& args, Environment& env)
{
    DBG("uSEQ::useq_unschedule");
    constexpr const char* user_facing_name = "unschedule";

    // Checking number of args
    if (!(args.size() == 1))
    {
        error_wrong_num_args(user_facing_name, args.size(),
                             NumArgsComparison::EqualTo, 1, 0);
        return Value::error();
    }

    // Evaluating ONLY first arg & checking for errors
    Value pre_eval = args[0];
    args[0]        = args[0].eval(env);
    if (args[0].is_error())
    {
        error_arg_is_error(user_facing_name, 1, pre_eval.display());
        return Value::error();
    }

    // Checking individual args
    if (!(args[0].is_string()))
    {
        error_wrong_specific_pred(user_facing_name, 1, "a string",
                                  args[0].display());
        return Value::error();
    }

    const String id = args[0].as_string();
    auto is_item    = [id](scheduledItem& v) { return v.id == id; };

    if (auto it = std::find_if(std::begin(m_scheduledItems),
                               std::end(m_scheduledItems), is_item);
        it != std::end(m_scheduledItems))
    {
        m_scheduledItems.erase(it);
        println("- (unschedule) Item " + args[0].str + " removed successfully.");
    }
    else
    {
        println("- (unschedule) Item " + args[0].str + " not found; ignoring.");
    }
    return Value::nil();
}

Value uSEQ::useq_setbpm(std::vector<Value>& args, Environment& env)
{
    DBG("uSEQ::useq_setbpm");
    constexpr const char* user_facing_name = "set-bpm";

    // Checking number of args
    if (!(1 <= args.size() <= 2))
    {
        error_wrong_num_args(user_facing_name, args.size(),
                             NumArgsComparison::Between, 1, 2);
        return Value::error();
    }

    // Eval args
    for (size_t i = 0; i < args.size(); i++)
    {
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        error_wrong_specific_pred(user_facing_name, 1, "a number",
                                  args[0].display());
        return Value::error();
    }

    double thresh = 0.0;
    double newBpm = args[0].as_float();

    if (args.size() == 2)
    {
        if (!(args[1].is_number()))
        {
            error_wrong_specific_pred(user_facing_name, 2, "a number",
                                      args[1].display());
            return Value::error();
        }
        else
        {
            thresh = args[1].as_float();
        }
    }

    set_bpm(newBpm, thresh);
    return args[0];
}

Value uSEQ::useq_get_input_bpm(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "get-input-bpm";

    // Checking number of args
    if (!(args.size() == 1))
    {
        error_wrong_num_args(user_facing_name, args.size(),
                             NumArgsComparison::EqualTo, 1, 0);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        error_wrong_specific_pred(user_facing_name, 1, "a number",
                                  args[0].display());
        return Value::error();
    }

    Value result;

    int index = args[0].as_int();
    if (index == 1)
    {
        result = tempoI1.avgBPM;
    }
    else if (index == 1)
    {
        result = tempoI2.avgBPM;
    }
    else
    {
        result = 0;
    }
    return result;
}

Value uSEQ::useq_set_time_sig(std::vector<Value>& args, Environment& env)
{
    DBG("uSEQ::useq_set_time_sig");
    constexpr const char* user_facing_name = "set-time-sig";

    // Checking number of args
    if (!(args.size() == 2))
    {
        error_wrong_num_args(user_facing_name, args.size(),
                             NumArgsComparison::EqualTo, 2, 0);
        return Value::error();
    }

    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        error_wrong_specific_pred(user_facing_name, 1, "a number",
                                  args[0].display());
        return Value::error();
    }
    if (!(args[1].is_number()))
    {
        error_wrong_specific_pred(user_facing_name, 2, "a number",
                                  args[1].display());
        return Value::error();
    }

    set_time_sig(args[0].as_float(), args[1].as_float());
    return Value::nil();
}

BUILTINFUNC_MEMBER(useq_in1, ret = Value(m_input_vals[USEQI1]);, 0)
BUILTINFUNC_MEMBER(useq_in2, ret = Value(m_input_vals[USEQI2]);, 0)
BUILTINFUNC_MEMBER(useq_ain1, ret = Value(m_input_vals[USEQAI1]);, 0)
BUILTINFUNC_MEMBER(useq_ain2, ret = Value(m_input_vals[USEQAI2]);, 0)

//clock sources

BUILTINFUNC_MEMBER(useq_reset_external_clock_tracking,
    constexpr const char* user_facing_name = "reset-clock-ext";
    if (!(args.size() == 0))
    {
        error_wrong_num_args(user_facing_name, args.size(),
                             NumArgsComparison::EqualTo, 0, 0);
        return Value::error();
    }
    reset_ext_tracking();
    return Value::nil();
    , 0)

BUILTINFUNC_MEMBER(useq_reset_internal_clock,
    constexpr const char* user_facing_name = "reset-clock-int";
    if (!(args.size() == 0))
    {
        error_wrong_num_args(user_facing_name, args.size(),
                             NumArgsComparison::EqualTo, 0, 0);
        return Value::error();
    }
    reset_logical_time();
    return Value::nil();
    , 0)

BUILTINFUNC_MEMBER(useq_get_clock_source,
    constexpr const char* user_facing_name = "get-clock-source";
    if (!(args.size() == 0))
    {
        error_wrong_num_args(user_facing_name, args.size(),
                             NumArgsComparison::EqualTo, 0, 0);
        return Value::error();
    }

    switch(useq_clock_source) {
        case uSEQ::CLOCK_SOURCES::INTERNAL:
            println("Internal");
            break;
        case uSEQ::CLOCK_SOURCES::EXTERNAL_I1:
            println("External 1");
            break;
        case uSEQ::CLOCK_SOURCES::EXTERNAL_I2:
            println("External 2");
            break;
    }
    return Value((int)useq_clock_source);
    , 0)

BUILTINFUNC_MEMBER(useq_set_clock_internal, 
    useq_clock_source = uSEQ::CLOCK_SOURCES::INTERNAL;
    println("Clock source set to internal");
    return Value::nil();
    , 0)

BUILTINFUNC_MEMBER(useq_set_clock_external, 
    constexpr const char* user_facing_name = "set-clock-ext";

    // Checking number of args
    if (!(args.size() == 2))
    {
        error_wrong_num_args(user_facing_name, args.size(),
                             NumArgsComparison::EqualTo, 1, 0);
        return Value::error();
    }
    if (!(args[0].is_number()))
    {
        error_wrong_specific_pred(user_facing_name, 1, "a number",
                                  args[0].display());
        return Value::error();
    }else if (args[0] < 1 || args[0] > 2) {
        custom_function_error(user_facing_name, "The clock source can be either input 1 or 2");
    }
    if (!(args[1].is_number()))
    {
        error_wrong_specific_pred(user_facing_name, 1, "a number",
                                  args[0].display());
        return Value::error();
    }else if (args[1] <= 0) {
        custom_function_error(user_facing_name, "The clock divisor must be more than 0");
    }

    //update settings
    if (args[0] == 1) {
        useq_clock_source = uSEQ::CLOCK_SOURCES::EXTERNAL_I1;
    }else if (args[0] == 2) {
        useq_clock_source = uSEQ::CLOCK_SOURCES::EXTERNAL_I2;
    }
    
    set_ext_clock_div(args[1].as_int());

    //notify player
    println("Clock source set to external: " + String(args[0].as_int()));
    println("Clock divisor: " + String(args[1].as_int()));

    return Value::nil();
    , 2)



#ifdef MUSICTHING
BUILTINFUNC_MEMBER(useq_mt_knob, ret = Value(m_input_vals[MTMAINKNOB]);, 0)
BUILTINFUNC_MEMBER(useq_mt_knobx, ret = Value(m_input_vals[MTXKNOB]);, 0)
BUILTINFUNC_MEMBER(useq_mt_knoby, ret = Value(m_input_vals[MTYKNOB]);, 0)
BUILTINFUNC_MEMBER(useq_mt_swz, ret = Value(m_input_vals[MTZSWITCH]);, 0)
#endif

BUILTINFUNC_MEMBER(useq_swr, ret = Value(m_input_vals[USEQRS1]);, 0)

BUILTINFUNC_MEMBER(useq_rot, ret = Value(m_input_vals[USEQR1]);, 0)

Value uSEQ::useq_ssin(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "ssin";

    // Checking number of args
    if (!(args.size() == 1))
    {
        error_wrong_num_args(user_facing_name, args.size(),
                             NumArgsComparison::EqualTo, 1, 0);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        error_wrong_specific_pred(user_facing_name, 1, "a number",
                                  args[0].display());
        return Value::error();
    }

    int index    = args[0].as_int();
    Value result = Value::nil();
    if (index > 0 && index <= m_num_serial_ins)
    {
        result = Value(m_serial_input_streams[index - 1]);
    }
    else
    {
        user_warning("(ssin) Received request for index " + String(index) +
                     ", which is out of bounds; returning nil.");
    }

    return result;
}

Value uSEQ::useq_swm(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "swm";

    // Checking number of args
    if (!(args.size() == 1))
    {
        error_wrong_num_args(user_facing_name, args.size(),
                             NumArgsComparison::EqualTo, 1, 0);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        error_wrong_specific_pred(user_facing_name, 1, "a number",
                                  args[0].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    int index = args[0].as_int();
    if (index == 1)
    {
        result = Value(m_input_vals[USEQM1]);
    }
    else
    {
        result = Value(m_input_vals[USEQM2]);
    }
    return result;
}

Value uSEQ::useq_swt(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "swt";

    // Checking number of args
    if (!(args.size() == 1))
    {
        error_wrong_num_args(user_facing_name, args.size(),
                             NumArgsComparison::EqualTo, 1, 0);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        error_wrong_specific_pred(user_facing_name, 1, "a number",
                                  args[0].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    int index = args[0].as_int();
    if (index == 1)
    {
        result = Value(m_input_vals[USEQT1]);
    }
    else
    {
        result = Value(m_input_vals[USEQT2]);
    }
    return result;
}

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
    // keep index in bounds
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
//////////////
//////////////

// NOTE: doesn't eval its arguments until they're selected by the phasor
Value uSEQ::useq_fromList(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "fromList";

    // Checking number of args
    // if (!(2 <= args.size() <= 3))
    if (!(args.size() == 2))
    {
        // error_wrong_num_args(user_facing_name, args.size(),
        //                      NumArgsComparison::Between, 2, 3);
        error_wrong_num_args(user_facing_name, args.size(),
                             NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // NOTE: This needs to eval both of its args, including the list,
    // to cover for cases where the user passes anything other than a
    // list literal (e.g. a symbol that points to a list)
    //
    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_sequential()))
    {
        error_wrong_specific_pred(user_facing_name, 1,
                                  "a sequential structure (e.g. a list or a vector)",
                                  args[0].display());
        return Value::error();
    }
    // Checking individual args
    if (!(args[1].is_number()))
    {
        error_wrong_specific_pred(user_facing_name, 2, "a number",
                                  args[1].display());
        return Value::error();
    }

    // BODY
    auto lst            = args[0].as_sequential();
    const double phasor = args[1].as_float();
    return fromList(lst, phasor, env);
}

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

#ifdef MIDIOUT
// midi drum out
BUILTINFUNC_MEMBER(
    useq_mdo, int midiNote = args[0].as_int(); if (args[1] != 0) {
        useqMDOMap[midiNote] = args[1];
    } else { useqMDOMap.erase(midiNote); },
                                               2)
#endif

BUILTINFUNC_NOEVAL_MEMBER(useq_q0, set("q-expr", args[0]); m_q0AST = { args[0] };, 1)

// TODO: there is potentially a lot of duplicated/wasted memory by storing
// the exprs in both the environment and the class member vectors
// especially once the exprs get more and more complex

BUILTINFUNC_NOEVAL_MEMBER(
    useq_a1,
    if (NUM_CONTINUOUS_OUTS >= 1) {
        set_expr("a1", args[0]);
        m_continuous_ASTs[0] = args[0];
        ret                  = Value::atom("a1");
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_a2,
    if (NUM_CONTINUOUS_OUTS >= 2) {
        set_expr("a2", args[0]);
        m_continuous_ASTs[1] = { args[0] };
        ret                  = Value::atom("a2");
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_a3,
    if (NUM_CONTINUOUS_OUTS >= 3) {
        set_expr("a3", args[0]);
        m_continuous_ASTs[2] = { args[0] };
        ret                  = Value::atom("a3");
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_a4,
    if (NUM_CONTINUOUS_OUTS >= 4) {
        set_expr("a4", args[0]);
        m_continuous_ASTs[3] = { args[0] };
        ret                  = Value::atom("a4");
    },
    1)

BUILTINFUNC_NOEVAL_MEMBER(
    useq_a5,
    if (NUM_CONTINUOUS_OUTS >= 5) {
        set_expr("a5", args[0]);
        m_continuous_ASTs[4] = { args[0] };
        ret                  = Value::atom("a5");
    },
    1)

BUILTINFUNC_NOEVAL_MEMBER(
    useq_a6,
    if (NUM_CONTINUOUS_OUTS >= 6) {
        set_expr("a6", args[0]);
        m_continuous_ASTs[5] = { args[0] };
        ret                  = Value::atom("a6");
    },
    1)

// DIGITAL OUTS
BUILTINFUNC_NOEVAL_MEMBER(
    useq_d1,
    if (NUM_BINARY_OUTS >= 1) {
        set_expr("d1", args[0]);
        m_binary_ASTs[0] = { args[0] };
        ret              = Value::atom("d1");
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_d2,
    if (NUM_BINARY_OUTS >= 2) {
        set_expr("d2", args[0]);
        m_binary_ASTs[1] = { args[0] };
        ret              = Value::atom("d2");
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_d3,
    if (NUM_BINARY_OUTS >= 3) {
        set_expr("d3", args[0]);
        m_binary_ASTs[2] = { args[0] };
        ret              = Value::atom("d3");
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_d4,
    if (NUM_BINARY_OUTS >= 4) {
        set_expr("d4", args[0]);
        m_binary_ASTs[3] = { args[0] };
        ret              = Value::atom("d4");
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_d5,
    if (NUM_BINARY_OUTS >= 5) {
        set_expr("d5", args[0]);
        m_binary_ASTs[4] = { args[0] };
        ret              = Value::atom("d5");
    },
    1)
BUILTINFUNC_NOEVAL_MEMBER(
    useq_d6,
    if (NUM_BINARY_OUTS >= 6) {
        set_expr("d6", args[0]);
        m_binary_ASTs[5] = { args[0] };
        ret              = Value::atom("d6");
    },
    1)

BUILTINFUNC_NOEVAL_MEMBER(useq_s1, set_expr("s1", args[0]);
                          m_serial_ASTs[0] = { args[0] }; ret = Value::atom("s1");
                          ,
                          // Serial.println(m_serial_ASTs.size());,
                          1)

BUILTINFUNC_NOEVAL_MEMBER(useq_s2, set_expr("s2", args[0]);
                          m_serial_ASTs[1] = { args[0] }; ret = Value::atom("s2");
                          , 1)
BUILTINFUNC_NOEVAL_MEMBER(useq_s3, set_expr("s3", args[0]);
                          m_serial_ASTs[2] = { args[0] }; ret = Value::atom("s3");
                          , 1)
BUILTINFUNC_NOEVAL_MEMBER(useq_s4, set_expr("s4", args[0]);
                          m_serial_ASTs[3] = { args[0] }; ret = Value::atom("s5");
                          , 1)
BUILTINFUNC_NOEVAL_MEMBER(useq_s5, set_expr("s5", args[0]);
                          m_serial_ASTs[4] = { args[0] }; ret = Value::atom("s5");
                          , 1)
BUILTINFUNC_NOEVAL_MEMBER(useq_s6, set_expr("s6", args[0]);
                          m_serial_ASTs[5] = { args[0] }; ret = Value::atom("s6");
                          , 1)
BUILTINFUNC_NOEVAL_MEMBER(useq_s7, set_expr("s7", args[0]);
                          m_serial_ASTs[6] = { args[0] }; ret = Value::atom("s7");
                          , 1)
BUILTINFUNC_NOEVAL_MEMBER(useq_s8, set_expr("s8", args[0]);
                          m_serial_ASTs[7] = { args[0] }; ret = Value::atom("s8");
                          , 1)

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

Value uSEQ::useq_eval_at_time(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "eval-at-time";

    // Checking number of args
    // if (!(2 <= args.size() <= 3))
    if (!(args.size() == 2))
    {
        // error_wrong_num_args(user_facing_name, args.size(),
        //                      NumArgsComparison::Between, 2, 3);
        error_wrong_num_args(user_facing_name, args.size(),
                             NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // NOTE: This needs to eval both of its args, including the list,
    // to cover for cases where the user passes anything other than a
    // list literal (e.g. a symbol that points to a list)
    //
    // Evaluating & checking args for errors
    Value pre_eval = args[0];
    args[0]        = args[0].eval(env);
    if (args[0].is_error())
    {
        error_arg_is_error(user_facing_name, 1, pre_eval.display());
        return Value::error();
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        error_wrong_specific_pred(user_facing_name, 1, "a number",
                                  args[0].display());
        return Value::error();
    }

    // NOTE: go from seconds to micros for internal calculations
    double time = args[0].as_float() * 1e+6;

    // BODY
    return eval_at_time(args[1], env, time);
}
// Creates a Lisp Value of type BUILTIN_METHOD,
// which requires
#define INSERT_BUILTINDEF(__name__, __func_name__)                                  \
    Environment::builtindefs[__name__] =                                            \
        Value((String)__name__, &uSEQ::__func_name__);

void uSEQ::init_builtinfuncs()
{
    DBG("uSEQ::init_builtinfuncs");

    INSERT_BUILTINDEF("eval-at-time", useq_eval_at_time);

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

    INSERT_BUILTINDEF("slow", useq_slow);
    INSERT_BUILTINDEF("fast", useq_fast);
    INSERT_BUILTINDEF("offset", useq_offset_time);

    INSERT_BUILTINDEF("set-bpm", useq_setbpm);
    INSERT_BUILTINDEF("get-input-bpm", useq_get_input_bpm);
    INSERT_BUILTINDEF("set-time-sig", useq_set_time_sig);
    INSERT_BUILTINDEF("schedule", useq_schedule);
    INSERT_BUILTINDEF("unschedule", useq_unschedule);

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

    //clock sources and management
    INSERT_BUILTINDEF("reset-clock-ext", useq_reset_external_clock_tracking);
    INSERT_BUILTINDEF("reset-clock-int", useq_reset_internal_clock);
    INSERT_BUILTINDEF("get-clock-source", useq_get_clock_source);
    INSERT_BUILTINDEF("set-clock-int", useq_set_clock_internal);
    INSERT_BUILTINDEF("set-clock-ext", useq_set_clock_external);

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

PhaseValue uSEQ::beat_at_time(TimeValue time)
{
    return fmod(time, m_beat_length) / m_beat_length;
}

PhaseValue uSEQ::bar_at_time(TimeValue time)
{
    // println("time: " + String(time));
    // println("m_bar_length: " + String(time));
    return fmod(time, m_bar_length) / m_bar_length;
}

PhaseValue uSEQ::phrase_at_time(TimeValue time)
{
    return fmod(time, m_phrase_length) / m_phrase_length;
}

PhaseValue uSEQ::section_at_time(TimeValue time)
{
    return fmod(time, m_section_length) / m_section_length;
}

Value uSEQ::eval_at_time(Value& expr, Environment& env, TimeValue time_micros)
{
    // Prepare new env with appropriate time vars
    // and current env as parent
    Environment new_env = make_env_for_time(time_micros);
    new_env.set_parent_scope(&env);
    // Eval in new env
    Value result = Interpreter::eval_in(expr, new_env);
    return result;
}

Environment uSEQ::make_env_for_time(TimeValue t_micros)
{
    Environment env;

    // TimeValue time_s = m_time_since_boot * 1e-6;
    TimeValue t_s = t_micros * 1e-6;

    // env.set("time", Value(time_s));
    env.set("t", Value(t_s));
    env.set("beat", Value(beat_at_time(t_micros)));
    env.set("bar", Value(bar_at_time(t_micros)));
    env.set("phrase", Value(phrase_at_time(t_micros)));
    env.set("section", Value(section_at_time(t_micros)));

    return env;
}
