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

float pdm_y   = 0;
float pdm_err = 0;
float pdm_w   = 0;

#ifdef USEQHARDWARE_1_0

bool timer_callback(repeating_timer_t* mst)
{
    pdm_y   = pdm_w > pdm_err ? 1 : 0;
    pdm_err = pdm_y - pdm_w + pdm_err;
    if (pdm_y == 1)
    {
        // on
        digitalWrite(USEQ_PIN_LED_AI1, HIGH);
    }
    else
    {
        // off
        digitalWrite(USEQ_PIN_LED_AI1, LOW);
    }

    //   w = w + 0.00000001;
    //   if (w>=1) w=0;

    return true;
}
#endif

void setup_leds()
{
    DBG("uSEQ::setup_leds");

#ifndef MUSICTHING
    // pinMode(LED_BOARD, OUTPUT); // test LED
    // digitalWrite(LED_BOARD, 1);
#endif

#ifdef USEQHARDWARE_1_0
    pinMode(USEQ_PIN_LED_AI1, OUTPUT_2MA);
    pinMode(USEQ_PIN_LED_AI2, OUTPUT_2MA);
    pinMode(USEQ_PIN_LED_I1, OUTPUT_2MA);
    pinMode(USEQ_PIN_LED_I2, OUTPUT_2MA);
#endif

    for (int i = 0; i < 6; i++)
    {
        pinMode(useq_output_led_pins[i], OUTPUT_2MA);
        gpio_set_slew_rate(useq_output_led_pins[i], GPIO_SLEW_RATE_SLOW);
    }
}

#ifdef USEQHARDWARE_1_0
void start_pdm()
{
    static repeating_timer_t mst;

    add_repeating_timer_us(150, timer_callback, NULL, &mst);
}
#endif
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
        ledDelay -= 3;
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

    // tiny delay to allow for interrupts etc
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
    // auto now              = micros();
    const double recp4096 = 0.000244141; // 1/4096
    const double recp2048 = 1 / 2048.0;
    const double recp1024 = 1 / 1024.0;

#ifdef MUSICTHING
    const size_t muxdelay = 2;

    // unroll loop for efficiency
    digitalWrite(MUX_LOGIC_A, 0);
    digitalWrite(MUX_LOGIC_B, 0);
    delayMicroseconds(muxdelay);
    m_input_vals[MTMAINKNOB] = analogRead(MUX_IN_1) * recp4096;
    m_input_vals[USEQAI1]    = 1.0 - (analogRead(MUX_IN_2) * recp4096);

    digitalWrite(MUX_LOGIC_A, 0);
    digitalWrite(MUX_LOGIC_B, 1);
    delayMicroseconds(muxdelay);
    m_input_vals[MTYKNOB] = analogRead(MUX_IN_1) * recp4096;

    digitalWrite(MUX_LOGIC_A, 1);
    digitalWrite(MUX_LOGIC_B, 0);
    delayMicroseconds(muxdelay);
    m_input_vals[MTXKNOB] = analogRead(MUX_IN_1) * recp4096;
    m_input_vals[USEQAI2] = 1.0 - (analogRead(MUX_IN_2) * recp4096);

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

    // const int input1 = 1 - digitalRead(USEQ_PIN_I1);
    // const int input2 = 1 - digitalRead(USEQ_PIN_I2);
    // digitalWrite(useq_output_led_pins[4], input1);
    // digitalWrite(useq_output_led_pins[5], input2);
    // m_input_vals[USEQI1] = input1;
    // m_input_vals[USEQI2] = input2;

#else

    // #ifdef USEQHARDWARE_1_0
    // #else
    m_input_vals[USEQT1] = 1 - digitalRead(USEQ_PIN_SWITCH_T1);
#endif // MUSICTHING

#ifdef USEQHARDWARE_0_2
    m_input_vals[USEQRS1] = 1 - digitalRead(USEQ_PIN_SWITCH_R1);
    m_input_vals[USEQM2]  = 1 - digitalRead(USEQ_PIN_SWITCH_M2);
    m_input_vals[USEQT2]  = 1 - digitalRead(USEQ_PIN_SWITCH_T2);
#endif

#ifdef USEQHARDWARE_1_0
    // TOGGLES
    const int ts_a = 1 - digitalRead(USEQ_PIN_SWITCH_T1);
    const int ts_b = 1 - digitalRead(USEQ_PIN_SWITCH_T2);

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

    // MOMENTARY
    m_input_vals[USEQM1] = 1 - digitalRead(USEQ_PIN_SWITCH_M1);

    // switch off LED while making measurements
    //  digitalWrite(USEQ_PIN_LED_AI1, 0);
    //  digitalWrite(USEQ_PIN_LED_AI2, 0);
    //  delayMicroseconds(100);

    // pdm_w = 0;
    // delayMicroseconds(10);

    auto v_ai1 = analogRead(USEQ_PIN_AI1);
    // <<<<<<< HEAD
    //     auto v_ai1_11 = v_ai1;                       // scale from 10 bit to 11
    //     bit range v_ai1_11      = (v_ai1_11 * v_ai1_11) >> 11; // sqr to get exp
    //     curve analogWrite(USEQ_PIN_LED_AI1, v_ai1_11);

    //     auto v_ai2    = analogRead(USEQ_PIN_AI2);
    //     auto v_ai2_11 = v_ai2;
    //     v_ai2_11      = (v_ai2_11 * v_ai2_11) >> 11;
    //     analogWrite(USEQ_PIN_LED_AI2, v_ai2_11);

    //     const double lpcoeff  = 0.05; // 0.009;
    //     m_input_vals[USEQAI1] = cvInFilter[0].lopass(v_ai1 * recp2048, lpcoeff);
    //     m_input_vals[USEQAI2] = cvInFilter[1].lopass(v_ai2 * recp2048, lpcoeff);
    // =======
    auto v_ai2 = analogRead(USEQ_PIN_AI2);

    auto v_ai1_11 = v_ai1;
    v_ai1_11      = (v_ai1_11 * v_ai1_11) >> 11; // sqr to get exp curve

    // analogWriteFreq(25000);    // out of hearing range

    // digitalWrite(USEQ_PIN_LED_AI2, 0);

    // analogWrite(USEQ_PIN_LED_AI1, v_ai1_11 + random(-100,100));
    pdm_w         = v_ai1_11 / 2048.0;
    auto v_ai2_11 = v_ai2;

    v_ai2_11 = (v_ai2_11 * v_ai2_11) >> 11;
    analogWrite(USEQ_PIN_LED_AI2, v_ai2_11);

    const double lpcoeff = 0.2; // 0.009;
    double filt1         = mf1.process(v_ai1 * recp2048);
    double filt2         = mf2.process(v_ai2 * recp2048);
    // m_input_vals[USEQAI1] = cvInFilter[0].lopass(v_ai1 * recp2048, lpcoeff);
    // m_input_vals[USEQAI2] = cvInFilter[1].lopass(v_ai2 * recp2048, lpcoeff);
    m_input_vals[USEQAI1] = filt1;
    m_input_vals[USEQAI2] = filt2;
#endif

    dbg("updating inputs...DONE");
}

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
        Serial.println(mC);
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
            // Serial.print(m_last_received_code);
            // Serial.print(m_last_received_code.length());
            // Serial.println("*");

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

// Max value that size_t can hold before overflow
constexpr TimeValue max_size_t = static_cast<TimeValue>((size_t)-1);

void uSEQ::update_time()
{
    DBG("uSEQ::update_time");

    // Cache previous values
    m_micros_raw_last            = m_micros_raw;
    m_last_known_time_since_boot = m_time_since_boot;

    // 1. Get time-since-boot reading from the board
    m_micros_raw = micros();

    // 2. Check if it has overflowed
    if (m_micros_raw < m_micros_raw_last)
    {
        dbg("INFO: overflow occurred, incrementing counter.");
        m_overflow_counter++;
    }

    // 3. Add an offset according to how many overflows we've had so far
    m_time_since_boot = static_cast<TimeValue>(m_micros_raw) +
                        (max_size_t * static_cast<TimeValue>(m_overflow_counter));

    update_logical_time(m_time_since_boot);
}

void uSEQ::reset_logical_time()
{
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
    analogWriteFreq(100000);   // out of hearing range
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

void uSEQ::update_clock_from_external(double ts)
{
    double newBPM = tempoI1.averageBPM(ts);
    if (ext_clock_tracker.count == 0)
    {
        newBPM *= (4.0 / meter_numerator / ext_clock_tracker.div);
        // println(String(newBPM));
        // println(String(beatCountI1));
        // println("bar: " + String(barCountI1));
        // println("barpf: " + String(m_bars_per_phrase));
        double std = tempoI1.std();
        // println("std: " + String(std));
        bool highstd = std > 100.0;
        // adjust every bar in high variance, otherwise every phrase
        if ((ext_clock_tracker.beat_count == 0 & highstd) ||
            ext_clock_tracker.beat_count == 0)
        {
            // println("----------------------------------------reset");
            set_bpm(newBPM, 0);
            reset_logical_time();
        }
        ext_clock_tracker.beat_count++;
        if (meter_denominator == ext_clock_tracker.beat_count)
        {
            ext_clock_tracker.beat_count = 0;
            ext_clock_tracker.bar_count++;
            if (ext_clock_tracker.bar_count ==
                static_cast<size_t>(m_bars_per_phrase))
            {
                ext_clock_tracker.bar_count = 0;
            }
        }
    }
    ext_clock_tracker.count++;
    if (ext_clock_tracker.count == ext_clock_tracker.div)
    {
        ext_clock_tracker.count = 0;
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

void uSEQ::set_input_val(size_t index, double value) { m_input_vals[index] = value; }

void uSEQ::gpio_irq_gate1()
{
    double ts         = static_cast<double>(micros());
    const auto input1 = 1 - digitalRead(USEQ_PIN_I1);
    uSEQ::instance->set_input_val(USEQI1, input1);
    digitalWrite(USEQ_PIN_LED_I1, input1);
    if (input1 == 1 &&
        uSEQ::instance->getClockSource() == uSEQ::CLOCK_SOURCES::EXTERNAL_I1)
    {
        uSEQ::instance->update_clock_from_external(ts);
    }
}

void uSEQ::gpio_irq_gate2()
{
    double ts         = static_cast<double>(micros());
    const auto input2 = 1 - digitalRead(USEQ_PIN_I2);
    uSEQ::instance->set_input_val(USEQI2, input2);
    digitalWrite(USEQ_PIN_LED_I2, input2);
    if (input2 == 1 &&
        uSEQ::instance->getClockSource() == uSEQ::CLOCK_SOURCES::EXTERNAL_I2)
    {
        uSEQ::instance->update_clock_from_external(ts);
    }
}

// TODO: delete if not needed
// void uSEQ::update_clock_from_external(double ts)
// {
//     double newBPM = tempoI1.averageBPM(ts);
//     if (ext_clock_tracker.count == 0)
//     {
//         newBPM *= (4.0 / meter_numerator / ext_clock_tracker.div);
//         // println(String(newBPM));
//         // println(String(beatCountI1));
//         // println("bar: " + String(barCountI1));
//         // println("barpf: " + String(m_bars_per_phrase));
//         double std = tempoI1.std();
//         // println("std: " + String(std));
//         bool highstd = std > 100.0;
//         // adjust every bar in high variance, otherwise every phrase
//         if ((ext_clock_tracker.beat_count == 0 & highstd) ||
//             ext_clock_tracker.beat_count == 0)
//         {
//             // println("----------------------------------------reset");
//             set_bpm(newBPM, 0);
//             reset_logical_time();
//         }
//         ext_clock_tracker.beat_count++;
//         if (meter_denominator == ext_clock_tracker.beat_count)
//         {
//             ext_clock_tracker.beat_count = 0;
//             ext_clock_tracker.bar_count++;
//             if (ext_clock_tracker.bar_count ==
//                 static_cast<size_t>(m_bars_per_phrase))
//             {
//                 ext_clock_tracker.bar_count = 0;
//             }
//         }
//     }
//     ext_clock_tracker.count++;
//     if (ext_clock_tracker.count == ext_clock_tracker.div)
//     {
//         ext_clock_tracker.count = 0;
//         // println("clock=0");
//     }
// }
/*
    const auto input1    = 1 - digitalRead(USEQ_PIN_I1);
    const auto input2    = 1 - digitalRead(USEQ_PIN_I2);
    m_input_vals[USEQI1] = input1;
    m_input_vals[USEQI2] = input2;

    digitalWrite(USEQ_PIN_LED_I1, input1);
    digitalWrite(USEQ_PIN_LED_I2, input2);

    m_input_vals[USEQM1] = 1 - digitalRead(USEQ_PIN_SWITCH_M1);
*/

void uSEQ::setup_digital_ins()
{
    pinMode(USEQ_PIN_I1, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(USEQ_PIN_I1), uSEQ::gpio_irq_gate1,
                    CHANGE);
    pinMode(USEQ_PIN_I2, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(USEQ_PIN_I2), uSEQ::gpio_irq_gate2,
                    CHANGE);
}

void uSEQ::setup_switches()
{
#ifdef USEQHARDWARE_1_0
    pinMode(USEQ_PIN_SWITCH_M1, INPUT_PULLUP);

    pinMode(USEQ_PIN_SWITCH_T1, INPUT_PULLUP);
    pinMode(USEQ_PIN_SWITCH_T2, INPUT_PULLUP);
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
#ifdef USEQHARDWARE_1_0
    analogReadResolution(11);
    pinMode(USEQ_PIN_AI1, INPUT);
    pinMode(USEQ_PIN_AI2, INPUT);
#endif
#ifdef MUSICTHING
    analogReadResolution(12);
    pinMode(MUX_IN_1, INPUT);
    pinMode(MUX_IN_2, INPUT);
    pinMode(AUDIO_IN_L, INPUT);
    pinMode(AUDIO_IN_R, INPUT);
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

#ifdef ENABLEI2CCLIENT
    bI2CclientMode = true;
    bI2ChostMode   = false;
    setup_i2cCLIENT();
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
        report_generic_error("Invalid BPM requested: " + String(newBpm));
    }
    else if (fabs(newBpm - m_bpm) >= changeThreshold)
    {
        m_bpm = newBpm;
        // Derive phasor lengths (in micros)
        m_beat_length = bpm_to_micros_per_beat(newBpm);
        m_bar_length  = m_beat_length * (4.0 / meter_denominator) * meter_numerator;
        m_bar_length  = m_beat_length * (4.0 / meter_denominator) * meter_numerator;
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
    int pwm_pin   = analog_out_pin(output + 1);
    int ledsigval = scaled_val; // >> 2; // shift to 11 bit range for the LED

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
    // write led
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
    digitalWrite(pin, 1 - (val > 0));
#else
    digitalWrite(pin, val > 0);
#endif
    // write led
    digitalWrite(led_pin, val > 0);
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

////////////////////
// USEQ API
Value uSEQ::ard_useqdw(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "useqdw";

    if (!(args.size() == 2))
    {
        // error_wrong_num_args(user_facing_name, args.size(),
        //                      NumArgsComparison::Between, 2, 3);
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }

        if (!(args[i].is_number()))
        {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].display());
            return Value::error();
        }
    }

    // BODY
    int out = args[0].as_int();
    int val = args[1].as_int();
    digital_write_with_led(out, val);

    return Value::nil();
}

Value uSEQ::ard_useqaw(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "useqaw";

    if (!(args.size() == 2))
    {
        // error_wrong_num_args(user_facing_name, args.size(),
        //                      NumArgsComparison::Between, 2, 3);
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }

        if (!(args[i].is_number()))
        {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].display());
            return Value::error();
        }
    }

    // BODY
    analog_write_with_led(args[0].as_int(), args[1].as_float());

    return Value::nil();
}

Value uSEQ::useq_fast(std::vector<Value>& args, Environment& env)
{
    DBG("uSEQ::fast");
    constexpr const char* user_facing_name = "fast";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, 0);
        return Value::error();
    }

    // Eval first arg
    Value pre_eval = args[0];
    args[0]        = args[0].eval(env);
    if (args[0].is_error())
    {
        report_error_arg_is_error(user_facing_name, 1, pre_eval.display());
        return Value::error();
    }
    // Checking first arg
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
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
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, 0);
        return Value::error();
    }

    // Eval first arg only
    Value pre_eval = args[0];
    args[0]        = args[0].eval(env);
    if (args[0].is_error())
    {
        report_error_arg_is_error(user_facing_name, 1, pre_eval.display());
        return Value::error();
    }
    // Checking first arg
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
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
    DBG("uSEQ::offset");
    constexpr const char* user_facing_name = "offset";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, 0);
        return Value::error();
    }

    // Eval first arg
    Value pre_eval = args[0];
    args[0]        = args[0].eval(env);
    if (args[0].is_error())
    {
        report_error_arg_is_error(user_facing_name, 1, pre_eval.display());
        return Value::error();
    }
    // Checking first arg
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
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
        report_error_wrong_num_args(user_facing_name, args.size(),
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_string()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a string",
                                         args[0].display());
        return Value::error();
    }
    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
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
    // remove if exists
    const String searchId = args[0].as_string();

    auto is_item = [searchId](scheduledItem& candidate)
    { return candidate.id == searchId; };

    if (auto it = std::find_if(std::begin(m_scheduledItems),
                               std::end(m_scheduledItems), is_item);
        it != std::end(m_scheduledItems))
    {
        m_scheduledItems.erase(it);
    }
    // add to scheduler list
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
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, 0);
        return Value::error();
    }

    // Evaluating ONLY first arg & checking for errors
    Value pre_eval = args[0];
    args[0]        = args[0].eval(env);
    if (args[0].is_error())
    {
        report_error_arg_is_error(user_facing_name, 1, pre_eval.display());
        return Value::error();
    }

    // Checking individual args
    if (!(args[0].is_string()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a string",
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
        report_error_wrong_num_args(user_facing_name, args.size(),
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].display());
        return Value::error();
    }

    double thresh = 0.0;
    double newBpm = args[0].as_float();

    if (args.size() == 2)
    {
        if (!(args[1].is_number()))
        {
            report_error_wrong_specific_pred(user_facing_name, 2, "a number",
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
        report_error_wrong_num_args(user_facing_name, args.size(),
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
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
        report_error_wrong_num_args(user_facing_name, args.size(),
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].display());
        return Value::error();
    }
    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }

    set_time_sig(args[0].as_float(), args[1].as_float());
    return Value::nil();
}

Value uSEQ::useq_in1(std::vector<Value>& args, Environment& env)
{
    return Value(m_input_vals[USEQI1]);
}

Value uSEQ::useq_in2(std::vector<Value>& args, Environment& env)
{
    return Value(m_input_vals[USEQI2]);
}

Value uSEQ::useq_ain1(std::vector<Value>& args, Environment& env)
{
    return Value(m_input_vals[USEQAI1]);
}

Value uSEQ::useq_ain2(std::vector<Value>& args, Environment& env)
{
    return Value(m_input_vals[USEQAI2]);
}

#ifdef MUSICTHING

Value uSEQ::useq_mt_knob(std::vector<Value>& args, Environment& env)
{
    return Value(m_input_vals[MTMAINKNOB]);
}

Value uSEQ::useq_mt_knobx(std::vector<Value>& args, Environment& env)
{
    return Value(m_input_vals[MTXKNOB]);
}

Value uSEQ::useq_mt_knoby(std::vector<Value>& args, Environment& env)
{
    return Value(m_input_vals[MTYKNOB]);
}

Value uSEQ::useq_mt_swz(std::vector<Value>& args, Environment& env)
{
    return Value(m_input_vals[MTZSWITCH]);
}
#endif
// clock sources

// BUILTINFUNC_MEMBER(
//     useq_reset_internal_clock,
//     constexpr const char* user_facing_name = "reset-clock-int";
//     if (!(args.size() == 0)) {
//         report_error_wrong_num_args(user_facing_name, args.size(),
//                                     NumArgsComparison::EqualTo, 0, 0);
//         return Value::error();
//     } reset_logical_time();
//     return Value::nil();, 0)

// BUILTINFUNC_MEMBER(
//     useq_get_clock_source,
//     constexpr const char* user_facing_name = "get-clock-source";
//     if (!(args.size() == 0)) {
//         report_error_wrong_num_args(user_facing_name, args.size(),
//                                     NumArgsComparison::EqualTo, 0, 0);
//         return Value::error();
//     }

//     switch (useq_clock_source) {
// case uSEQ::CLOCK_SOURCES::INTERNAL:
//     println("Internal");
//     break;
// case uSEQ::CLOCK_SOURCES::EXTERNAL_I1:
//     println("External 1");
//     break;
// case uSEQ::CLOCK_SOURCES::EXTERNAL_I2:
//     println("External 2");
//     break;
//     } return Value((int)useq_clock_source);
//     , 0)

// BUILTINFUNC_MEMBER(useq_set_clock_internal,
//                    useq_clock_source = uSEQ::CLOCK_SOURCES::INTERNAL;
//                    println("Clock source set to internal"); return Value::nil();,
//                    0)

// BUILTINFUNC_MEMBER(
//     useq_set_clock_external,
//     constexpr const char* user_facing_name = "set-clock-ext";

//     // Checking number of args
//     if (!(args.size() == 2)) {
//         report_error_wrong_num_args(user_facing_name, args.size(),
//                                     NumArgsComparison::EqualTo, 1, 0);
//         return Value::error();
//     } if (!(args[0].is_number())) {
//         report_error_wrong_specific_pred(user_facing_name, 1, "a number",
//                                          args[0].display());
//         return Value::error();
//     } else if (args[0] < 1 || args[0] > 2) {
//         report_custom_function_error(user_facing_name,
//                                      "The clock source can be either input 1 or
//                                      2");
//     } if (!(args[1].is_number())) {
//         report_error_wrong_specific_pred(user_facing_name, 1, "a number",
//                                          args[0].display());
//         return Value::error();
//     } else if (args[1] <= 0) {
//         report_custom_function_error(user_facing_name,
//                                      "The clock divisor must be more than 0");
//     }

//     // update settings
//     if (args[0] == 1) {
//         useq_clock_source = uSEQ::CLOCK_SOURCES::EXTERNAL_I1;
//     } else if (args[0] == 2) {
//         useq_clock_source = uSEQ::CLOCK_SOURCES::EXTERNAL_I2;
//     }

//     set_ext_clock_div(args[1].as_int());

//     // notify player
//     println("Clock source set to external: " + String(args[0].as_int()));
//     println("Clock divisor: " + String(args[1].as_int()));

//     return Value::nil();, 2)

// clock sources

BUILTINFUNC_MEMBER(
    useq_reset_external_clock_tracking,
    constexpr const char* user_facing_name = "reset-clock-ext";
    if (!(args.size() == 0)) {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 0, 0);
        return Value::error();
    } reset_ext_tracking();
    return Value::nil();, 0)

BUILTINFUNC_MEMBER(
    useq_reset_internal_clock,
    constexpr const char* user_facing_name = "reset-clock-int";
    if (!(args.size() == 0)) {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 0, 0);
        return Value::error();
    } reset_logical_time();
    return Value::nil();, 0)

BUILTINFUNC_MEMBER(
    useq_get_clock_source,
    constexpr const char* user_facing_name = "get-clock-source";
    if (!(args.size() == 0)) {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 0, 0);
        return Value::error();
    }

    switch (useq_clock_source) {
case uSEQ::CLOCK_SOURCES::INTERNAL:
    println("Internal");
    break;
case uSEQ::CLOCK_SOURCES::EXTERNAL_I1:
    println("External 1");
    break;
case uSEQ::CLOCK_SOURCES::EXTERNAL_I2:
    println("External 2");
    break;
    } return Value((int)useq_clock_source);
    , 0)

BUILTINFUNC_MEMBER(useq_set_clock_internal,
                   useq_clock_source = uSEQ::CLOCK_SOURCES::INTERNAL;
                   println("Clock source set to internal"); return Value::nil();, 0)

BUILTINFUNC_MEMBER(
    useq_set_clock_external,
    constexpr const char* user_facing_name = "set-clock-ext";

    // Checking number of args
    if (!(args.size() == 2)) {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, 0);
        return Value::error();
    } if (!(args[0].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].display());
        return Value::error();
    } else if (args[0] < 1 || args[0] > 2) {
        report_custom_function_error(user_facing_name,
                                     "The clock source can be either input 1 or 2");
    } if (!(args[1].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].display());
        return Value::error();
    } else if (args[1] <= 0) {
        report_custom_function_error(user_facing_name,
                                     "The clock divisor must be more than 0");
    }

    // update settings
    if (args[0] == 1) {
        useq_clock_source = uSEQ::CLOCK_SOURCES::EXTERNAL_I1;
    } else if (args[0] == 2) {
        useq_clock_source = uSEQ::CLOCK_SOURCES::EXTERNAL_I2;
    }

    set_ext_clock_div(args[1].as_int());

    // notify player
    println("Clock source set to external: " + String(args[0].as_int()));
    println("Clock divisor: " + String(args[1].as_int()));

    return Value::nil();, 2)

BUILTINFUNC_MEMBER(useq_swr, ret = Value(m_input_vals[USEQRS1]);, 0)

BUILTINFUNC_MEMBER(useq_rot, ret = Value(m_input_vals[USEQR1]);, 0)

BUILTINFUNC_MEMBER(
    useq_tri, constexpr const char* user_facing_name = "tri";
    if (!(args.size() == 2)) {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, 0);
        return Value::error();
    } if (!(args[0].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].display());
        return Value::error();
    } if (!(args[1].is_number())) {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    } double duty = args[0].as_float();
    if (duty < 0.01) { duty = 0.01; } if (duty > 0.99) {
        duty = 0.99;
    } double phase = args[1].as_float();
    // w - ((p-w) * (w/(1-w)))
    if (phase > duty) {
        phase = (duty - ((phase - duty) * (duty / (1 - duty))));
    } return phase /
    duty;
    , 2)

Value uSEQ::useq_ssin(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "ssin";

    // Checking number of args
    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
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
        report_user_warning("(ssin) Received request for index " + String(index) +
                            ", which is out of bounds; returning nil.");
    }

    return result;
}

Value uSEQ::useq_swm(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "swm";

    // Checking number of args
    if (!(args.size() == 0))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 0, 0);
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    result       = Value(m_input_vals[USEQM1]);
    return result;
}

Value uSEQ::useq_swt(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "swt";

    // Checking number of args
    if (!(args.size() == 0))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 0, 0);
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    result       = Value(m_input_vals[USEQT1]);
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

Value uSEQ::useq_toggle_select(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "toggle-sel";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, 0);
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
            report_error_arg_is_error(user_facing_name, i + 1,
                                      pre_eval.to_lisp_src());
            return Value::error();
        }
    }

    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a vector or list",
                                         args[0].to_lisp_src());
        return Value::error();
    }

    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[1].to_lisp_src());
        return Value::error();
    }
    // BODY
    Value result     = Value::nil();
    std::vector list = args[0].as_sequential();
    float phasor     = args[1].as_float();

    result = Value(fromList(list, phasor, env));

    return result;
}

Value uSEQ::useq_dm(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "dm";

    // Checking number of args
    if (!(args.size() == 3))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 3, -1);
        return Value::error();
    }

    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
        // Check all-pred(s)

        if (!(args[i].is_number()))
        {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].display());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();

    int index = args[0].as_int();
    double v1 = args[1].as_float();
    double v2 = args[2].as_float();
    result    = Value(index > 0 ? v2 : v1);

    return result;
}

// FIXME shouldn't the pulsewidth be compared to the
// 1/nth phase of the phasor, where n is the number of gates?
// the way it is now it defaults to muting the first half of the gates
Value uSEQ::useq_gates(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "gates";

    // Checking number of args
    if (!(2 <= args.size() <= 3))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::Between, 2, 3);
        return Value::error();
    }

    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
        // Check all-pred(s)

        if (i == 0)
        {
            if (!(args[i].is_sequential()))
            {
                report_error_wrong_specific_pred(
                    user_facing_name, i + 1, "a vector or list", args[i].display());
                return Value::error();
            }
        }
        else
        {
            if (!(args[i].is_number()))
            {
                report_error_wrong_specific_pred(user_facing_name, i + 1, "a number",
                                                 args[i].display());
                return Value::error();
            }
        }
    }

    // BODY
    Value result = Value::nil();

    bool pulse_width_specified = args.size() == 3;
    auto gates_vec             = args[0].as_sequential();

    double pulseWidth;
    double phasor;
    if (pulse_width_specified)
    {
        pulseWidth = args[1].as_float();
        phasor     = args[2].as_float();
    }
    else
    {
        pulseWidth = 0.5;
        phasor     = args[1].as_float();
    }

    const double val = fromList(gates_vec, phasor, env).as_int();
    const double gates =
        (fmod(phasor * gates_vec.size(), 1.0)) < pulseWidth ? 1.0 : 0.0;
    result = Value(val * gates);

    return result;
}

Value uSEQ::useq_gatesw(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "gatesw";

    // Checking number of args
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Check specific preds
    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a vector or list",
                                         args[0].display());
        return Value::error();
    }

    if (!(args[1].is_number()))
    {
        report_error_wrong_all_pred(user_facing_name, 2, "a number",
                                    args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    auto gates_vec               = args[0].as_sequential();
    const double phasor          = args[1].as_float();
    const double val             = fromList(gates_vec, phasor, env).as_int();
    const double pulseWidth      = val / 9.0;
    const double relative_phasor = fmod(phasor * gates_vec.size(), 1.0);
    const double gate            = relative_phasor < pulseWidth ? 1.0 : 0.0;

    result = Value((val > 0 ? 1.0 : 0.0) * gate);

    return result;
}

Value uSEQ::useq_trigs(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "trigs";

    // Checking number of args
    if (!(2 <= args.size() <= 3))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::Between, 2, 3);
        return Value::error();
    }

    // Evaluating args, checking for errors & all-arg constraints
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        Value pre_eval = args[i];
        args[i]        = args[i].eval(env);
        if (args[i].is_error())
        {
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Check specific preds
    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a vector or list",
                                         args[0].display());
        return Value::error();
    }

    if (!(args.back().is_number()))
    {
        report_error_wrong_all_pred(user_facing_name, args.size() + 1, "a number",
                                    args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    auto lst = args[0].as_sequential();
    // NOTE: phasor at the end
    const double phasor     = args.back().as_float();
    const double val        = fromList(lst, phasor, env).as_int();
    const double amp        = std::clamp(val / 9.0, 0.0, 1.0);
    const double pulseWidth = args.size() == 3 ? args[1].as_float() : 0.1;
    const double gate = fmod(phasor * lst.size(), 1.0) < pulseWidth ? 1.0 : 0.0;
    result            = Value((val > 0 ? 1.0 : 0.0) * gate * amp);

    return result;
}

Value uSEQ::useq_euclidean(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "euclid";

    // Checking number of args
    if (!(3 <= args.size() <= 5))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::Between, 3, 5);
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }

        // Check all-pred(s)
        if (!(args[i].is_number()))
        {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].display());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();

    // NOTE: Phasor is the last arg
    const double phasor    = args.back().as_float();
    const int n            = args[0].as_int();
    const int k            = args[1].as_int();
    const int offset       = (args.size() >= 4) ? args[2].as_int() : 0;
    const float pulseWidth = (args.size() == 5) ? args[3].as_float() : 0.5;
    const float fi         = phasor * n;
    int i                  = static_cast<int>(fi);
    const float rem        = fi - i;
    if (i == n)
    {
        i--;
    }
    const int idx = ((i + n - offset) * k) % n;
    result        = Value(idx < k && rem < pulseWidth ? 1 : 0);

    return result;
}

Value uSEQ::useq_eu(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "eu";

    // Checking number of args
    if (!(3 <= args.size() <= 5))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::Between, 3, 4);
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }

        // Check all-pred(s)
        if (!(args[i].is_number()))
        {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].display());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();

    // NOTE: Phasor is the last arg
    const double phasor    = args.back().as_float();
    const int n            = args[0].as_int();
    const int k            = args[1].as_int();
    const float pulseWidth = (args.size() == 4) ? args[2].as_float() : 0.5;
    const float fi         = phasor * n;
    int i                  = static_cast<int>(fi);
    const float rem        = fi - i;
    if (i == n)
    {
        i--;
    }
    // NOTE: original, testing
    const int idx = ((i + n) * k) % n;
    result        = Value(idx < k && rem < pulseWidth ? 1 : 0);

    // NOTE: working one
    // const int idx             = ((i + n) * k) % n;
    // float effectivePulseWidth = (idx < k) ? pulseWidth : 0;
    // result                    = Value(rem < effectivePulseWidth ? 1 : 0);

    return result;
}

Value uSEQ::useq_ratiotrig(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "rpulse";

    // Checking number of args
    if (!(3 == args.size()))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 3, -1);
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }
    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(
            user_facing_name, 1, "a sequential structure (e.g. a list or a vector)",
            args[0].display());
        return Value::error();
    }
    // Checking individual args
    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }
    if (!(args[2].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    auto ratios           = args[0].as_sequential();
    const auto pulseWidth = args[1].as_float();
    const auto phase      = args[2].as_float();

    double trig     = 0;
    double ratioSum = 0;
    for (Value v : ratios)
    {
        ratioSum += v.as_float();
    }
    double phaseAdj           = ratioSum * phase;
    double accumulatedSum     = 0;
    double lastAccumulatedSum = 0;
    for (Value v : ratios)
    {
        accumulatedSum += v.as_float();
        if (phaseAdj <= accumulatedSum)
        {
            // check pulse width
            double beatPhase = (phaseAdj - lastAccumulatedSum) /
                               (accumulatedSum - lastAccumulatedSum);
            trig = beatPhase <= pulseWidth;
            break;
        }
        lastAccumulatedSum = accumulatedSum;
    }
    result = Value(trig);

    return result;
}

Value uSEQ::useq_ratiostep(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "rstep";

    // Checking number of args
    if (!(2 == args.size()))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            Value pre_eval = args[i];
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }
    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(
            user_facing_name, 1, "a sequential structure (e.g. a list or a vector)",
            args[0].display());
        return Value::error();
    }
    // Checking individual args
    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    auto ratios      = args[0].as_sequential();
    const auto phase = args[1].as_float();

    double phaseOut = 0;
    double ratioSum = 0;
    for (Value v : ratios)
    {
        ratioSum += v.as_float();
    }
    double phaseAdj           = ratioSum * phase;
    double accumulatedSum     = 0;
    double lastAccumulatedSum = 0;
    for (Value v : ratios)
    {
        accumulatedSum += v.as_float();
        if (phaseAdj <= accumulatedSum)
        {
            phaseOut = lastAccumulatedSum;
            break;
        }
        lastAccumulatedSum = accumulatedSum;
    }
    result = Value(phaseOut / ratioSum);
    return result;
}

LISP_FUNC_DECL(uSEQ::useq_ratioindex)
// Value uSEQ::useq_ratioindex(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "ridx";

    // Checking number of args
    if (!(2 == args.size()))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            Value pre_eval = args[i];
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }
    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(
            user_facing_name, 1, "a sequential structure (e.g. a list or a vector)",
            args[0].display());
        return Value::error();
    }
    // Checking individual args
    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    auto ratios      = args[0].as_sequential();
    const auto phase = args[1].as_float();

    double index    = 0;
    double ratioSum = 0;
    for (Value v : ratios)
    {
        ratioSum += v.as_float();
    }
    double phaseAdj       = ratioSum * phase;
    double accumulatedSum = 0;
    for (Value v : ratios)
    {
        accumulatedSum += v.as_float();
        if (phaseAdj <= accumulatedSum)
        {
            break;
        }
        index++;
    }
    index /= static_cast<double>(ratios.size());
    result = Value(index);
    return result;
}

LISP_FUNC_DECL(uSEQ::useq_ratiowarp)
// Value uSEQ::useq_ratiowarp(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "rwarp";

    // Checking number of args
    if (!(2 == args.size()))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // Evaluating & checking args for errors
    for (size_t i = 0; i < args.size(); i++)
    {
        // Eval
        args[i] = args[i].eval(env);
        if (args[i].is_error())
        {
            Value pre_eval = args[i];
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }
    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(
            user_facing_name, 1, "a sequential structure (e.g. a list or a vector)",
            args[0].display());
        return Value::error();
    }
    // Checking individual args
    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    auto ratios      = args[0].as_sequential();
    const auto phase = args[1].as_float();

    double output = 0;

    if (ratios.size() > 0)
    {
        double index      = 0;
        double indexWidth = 1.0 / ratios.size();

        double ratioSum = 0;
        for (Value v : ratios)
        {
            ratioSum += v.as_float();
        }

        double phaseAdj           = ratioSum * phase;
        double accumulatedSum     = 0;
        double lastAccumulatedSum = 0;
        for (Value v : ratios)
        {
            accumulatedSum += v.as_float();
            if (phaseAdj <= accumulatedSum)
            {
                double beatPhase = (phaseAdj - lastAccumulatedSum) /
                                   (accumulatedSum - lastAccumulatedSum);
                output = (index * indexWidth) + (beatPhase * indexWidth);
                break;
            }
            lastAccumulatedSum = accumulatedSum;
            index++;
        }
    }

    result = Value(output);
    return result;
}

Value uSEQ::useq_phasor_offset(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "shift";

    // Checking number of args
    if (!(2 == args.size()))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 3, -1);
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }
    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }
    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    const auto offset = args[0].as_float();
    auto phase        = args[1].as_float();

    phase  = std::fmod(phase + offset, 1.0);
    result = Value(phase);

    return result;
}

// NOTE: doesn't eval its arguments until they're selected by the phasor
Value uSEQ::useq_fromList(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "from-list";

    // Checking number of args
    // if (!(2 <= args.size() <= 3))
    if (!(args.size() == 2))
    {
        // error_wrong_num_args(user_facing_name, args.size(),
        //                      NumArgsComparison::Between, 2, 3);
        report_error_wrong_num_args(user_facing_name, args.size(),
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(
            user_facing_name, 1, "a sequential structure (e.g. a list or a vector)",
            args[0].display());
        return Value::error();
    }
    // Checking individual args
    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    auto lst            = args[0].as_sequential();
    const double phasor = args[1].as_float();
    return fromList(lst, phasor, env);
}

// NOTE: duplicate of fromList, FIXME
Value uSEQ::useq_seq(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "seq";

    // Checking number of args
    // if (!(2 <= args.size() <= 3))
    if (!(args.size() == 2))
    {
        // error_wrong_num_args(user_facing_name, args.size(),
        //                      NumArgsComparison::Between, 2, 3);
        report_error_wrong_num_args(user_facing_name, args.size(),
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(
            user_facing_name, 1, "a sequential structure (e.g. a list or a vector)",
            args[0].display());
        return Value::error();
    }
    // Checking individual args
    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 2, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    auto lst            = args[0].as_sequential();
    const double phasor = args[1].as_float();
    return fromList(lst, phasor, env);
}

Value flatten_impl(const Value& val, Environment& env)
{
    std::vector<Value> flattened;

    // int original_type = val.type;

    if (!val.is_sequential())
    {
        flattened.push_back(val);
    }
    else
    {
        auto valList = val.as_sequential();
        for (size_t i = 0; i < valList.size(); i++)
        {
            Value evaluatedElement = Interpreter::eval_in(valList[i], env);
            if (evaluatedElement.is_sequential())
            {
                auto flattenedElement =
                    flatten_impl(evaluatedElement, env).as_list();
                flattened.insert(flattened.end(), flattenedElement.begin(),
                                 flattenedElement.end());
            }
            else
            {
                flattened.push_back(evaluatedElement);
            }
        }
    }

    Value result;

    // Only return a list if the input was a list,
    // otherwise a vec
    // if (original_type == Value::LIST)
    // {
    //     result = Value(flattened);
    // }
    // else
    // {
    //     result = Value::vector(flattened);
    // }

    result = Value::vector(flattened);
    return result;
}

// TODO test
Value uSEQ::useq_flatten(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "flatten";

    if (!(args.size() == 1))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 1, -1);
        return Value::error();
    }

    // Check list for error
    Value pre_eval = args[0];
    args[0]        = args[0].eval(env);
    if (args[0].is_error())
    {
        report_error_arg_is_error(user_facing_name, 1, pre_eval.display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();
    result       = flatten_impl(args[0], env);

    return result;
}

// NOTE: duplicate of fromFlattenedList, FIXME
Value uSEQ::useq_flatseq(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "flatseq";

    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(user_facing_name, 0, "a list or a vector",
                                         args[0].display());
        return Value::error();
    }

    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    auto lst      = flatten_impl(args[0], env).as_sequential();
    double phasor = args[1].as_float();
    result        = fromList(lst, phasor, env);

    return result;
}

Value uSEQ::useq_fromFlattenedList(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "from-flat-list";

    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(
            user_facing_name, 0, "a sequential structure (e.g. a list or a vector)",
            args[0].display());
        return Value::error();
    }

    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    auto lst      = flatten_impl(args[0], env).as_sequential();
    double phasor = args[1].as_float();
    result        = fromList(lst, phasor, env);

    return result;
}

Value uSEQ::useq_interpolate(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "interp";

    // Check num arguments
    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }
    }

    // Checking individual args
    if (!(args[0].is_sequential()))
    {
        report_error_wrong_specific_pred(user_facing_name, 0, "a list or a vector",
                                         args[0].display());
        return Value::error();
    }

    if (!(args[1].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[1].display());
        return Value::error();
    }

    // BODY
    Value result  = Value::nil();
    auto lst      = args[0].as_list();
    double phasor = args[1].as_float();
    if (phasor < 0.0)
    {
        phasor = 0;
    }
    else if (phasor > 1)
    {
        phasor = 1;
    }
    float a;
    double index = phasor * (lst.size() - 1);
    size_t pos0  = static_cast<size_t>(index);
    if (pos0 == (lst.size() - 1))
        pos0--;
    a         = (index - pos0);
    double v2 = lst[pos0 + 1].as_float();
    double v1 = lst[pos0].as_float();
    result    = Value(((v2 - v1) * a) + v1);

    return result;
}

Value uSEQ::useq_step(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "step";

    // Check num arguments
    if (!(2 <= args.size() <= 3))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::Between, 2, 3);
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
            report_error_arg_is_error(user_facing_name, i + 1, pre_eval.display());
            return Value::error();
        }

        // Check all-pred(s)
        if (!(args[i].is_number()))
        {
            report_error_wrong_all_pred(user_facing_name, i + 1, "a number",
                                        args[i].display());
            return Value::error();
        }
    }

    // BODY
    Value result = Value::nil();

    const int count      = args[0].as_int();
    bool offset_provided = args.size() == 3;
    double phasor, offset;

    if (offset_provided)
    {
        offset = args[1].as_float();
        phasor = args[2].as_float();
    }
    else
    {
        offset = 0;
        phasor = args[1].as_float();
    }

    double val = static_cast<int>(phasor * abs(count));
    if (val == count)
        val--;
    result = Value((count > 0 ? val : count - 1 - val) + offset);
    return result;
}

Value uSEQ::useq_send_to(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "send-to";

    if (!(args.size() == 2))
    {
        report_error_wrong_num_args(user_facing_name, args.size(),
                                    NumArgsComparison::EqualTo, 2, -1);
        return Value::error();
    }

    // if (!(args[0].is_string() || args[0].is_number()))
    // {
    //     report_error_wrong_specific_pred(
    //         user_facing_name, 0, "a string (module name) or a number (I2C index)",
    //         args[0].display());
    //     return Value::error();
    // }

    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 0, "a number",
                                         args[0].display());
        return Value::error();
    }

    // BODY
    Value result = Value::nil();

    int i2c_idx = -1;

    // if (args[0].is_string())
    // {
    //     i2c_idx = i2c_idx_map[args[0].as_string()];
    // }
    // else
    // {
    i2c_idx = args[0].as_int();
    // }

    // the body is everything after the first arg
    // Value body = Value::vector({ args.data() + 1, args.size() - 1 });
    // NOTE: for now, the body is expected to be just one expr
    // if multiple expressions needed, use a do block
    Value body = args[1];

    String body_str = "@" + body.to_lisp_src();

    i2cWriteString(i2c_idx, body_str);

    println("String being sent to i2c: ");
    println(body_str);

    return result;
}

#ifdef MIDIOUT
// midi drum out
BUILTINFUNC_MEMBER(
    useq_mdo, int midiNote = args[0].as_int(); if (args[1] != 0) {
        useqMDOMap[midiNote] = args[1];
    } else { useqMDOMap.erase(midiNote); },
                                               2)
#endif

BUILTINFUNC_NOEVAL_MEMBER(useq_q0, set("q-expr", args[0]); m_q0AST = args[0];, 1)

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

Value uSEQ::useq_eval_at_time(std::vector<Value>& args, Environment& env)
{
    constexpr const char* user_facing_name = "eval-at-time";

    // Checking number of args
    // if (!(2 <= args.size() <= 3))
    if (!(args.size() == 2))
    {
        // error_wrong_num_args(user_facing_name, args.size(),
        //                      NumArgsComparison::Between, 2, 3);
        report_error_wrong_num_args(user_facing_name, args.size(),
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
        report_error_arg_is_error(user_facing_name, 1, pre_eval.display());
        return Value::error();
    }

    // Checking individual args
    if (!(args[0].is_number()))
    {
        report_error_wrong_specific_pred(user_facing_name, 1, "a number",
                                         args[0].display());
        return Value::error();
    }

    // NOTE: go from seconds to micros for internal calculations
    double time = args[0].as_float() * 1e+6;

    // BODY
    return eval_at_time(args[1], env, time);
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

// FLASH

constexpr uintptr_t PICO_FLASH_START_ADDR = reinterpret_cast<uintptr_t>(XIP_BASE);
constexpr uintptr_t NUM_SECTORS = PICO_FLASH_SIZE_BYTES / FLASH_SECTOR_SIZE;

constexpr uintptr_t FLASH_INFO_SECTOR_SIZE = FLASH_SECTOR_SIZE;
constexpr uintptr_t FLASH_ENV_SECTOR_SIZE  = FLASH_SECTOR_SIZE;

constexpr uintptr_t FLASH_INFO_SECTOR_OFFSET_START =
    (NUM_SECTORS - 1) * FLASH_SECTOR_SIZE;
constexpr uintptr_t FLASH_INFO_SECTOR_OFFSET_END =
    FLASH_INFO_SECTOR_OFFSET_START + FLASH_SECTOR_SIZE - 1;

constexpr uintptr_t FLASH_ENV_SECTOR_OFFSET_START =
    (NUM_SECTORS - 2) * FLASH_SECTOR_SIZE;
constexpr uintptr_t FLASH_ENV_SECTOR_OFFSET_END =
    FLASH_ENV_SECTOR_OFFSET_START + FLASH_SECTOR_SIZE - 1;

// Write an arbitrary byte buffer to an offset position in flash storage
void write_to_flash(u_int8_t* buffer, size_t OFFSET_START, size_t SECTOR_SIZE,
                    size_t WRITE_SIZE = -1)
{
    // Default to writing the entire sector's worth of pages
    if (WRITE_SIZE == -1)
    {
        WRITE_SIZE = SECTOR_SIZE;
    }

    // 1. Save and temporarily disable interrupts
    uint32_t interrupts = save_and_disable_interrupts();
    // 2. Clear the entire sector
    // NOTE: this is mandatory for the write operation to work properly
    flash_range_erase(OFFSET_START, SECTOR_SIZE);
    // 3. Write the actual pages (which may be smaller than
    // the sector size we cleared)
    flash_range_program(OFFSET_START, buffer, WRITE_SIZE);
    // 4. Restore and resume interrupts
    restore_interrupts(interrupts);
}

void print_flash_vars()
{
    println("FLASH_START_ADDR: " + String((uint)PICO_FLASH_START_ADDR));
    println("PICO_FLASH_SIZE_BYTES: " + String(PICO_FLASH_SIZE_BYTES));
    println("FLASH_INFO_SECTOR_SIZE: " + String(FLASH_INFO_SECTOR_SIZE));
    println("FLASH_INFO_SECTOR_OFFSET_START: " +
            String(FLASH_INFO_SECTOR_OFFSET_START));
}

// Lisp interfaces
BUILTINFUNC_NOEVAL_MEMBER(useq_reboot,
                          //
                          reboot();
                          , 0)

BUILTINFUNC_MEMBER(useq_write_flash_info,
                   //
                   write_flash_info();
                   , 0)

BUILTINFUNC_MEMBER(useq_load_flash_info,
                   //
                   // print_flash_vars();
                   load_flash_info();
                   , 0)

BUILTINFUNC_MEMBER(useq_write_flash_env,
                   //
                   write_flash_env();
                   , 0)

BUILTINFUNC_MEMBER(useq_load_flash_env,
                   //
                   // print_flash_vars();
                   // println("Free heap: " + String(free_heap()));
                   load_flash_env();
                   , 0)

BUILTINFUNC_NOEVAL_MEMBER(useq_get_my_id,
                          //
                          ret = Value(m_my_id);
                          , 0)

BUILTINFUNC_NOEVAL_MEMBER(useq_autoload_flash,
                          //
                          autoload_flash();
                          , 0)

BUILTINFUNC_NOEVAL_MEMBER(useq_rewind_logical_time,
                          //
                          reset_logical_time();
                          , 0)

// NOTE: only these are meant for user interface
BUILTINFUNC_MEMBER(
    useq_set_my_id,                  //
    if (args[0].is_number())         //
    { set_my_id(args[0].as_int()); } //
    else {
        report_error_wrong_specific_pred("useq-set-id", 1, "a number",
                                         args[0].display());
    } //
    ,
    1)

BUILTINFUNC_NOEVAL_MEMBER(useq_memory_save, write_flash_env();, 0)
BUILTINFUNC_NOEVAL_MEMBER(useq_memory_restore, //
                          load_flash_info();
                          load_flash_env(); //
                          , 0);
BUILTINFUNC_NOEVAL_MEMBER(useq_memory_erase, //
                          reset_flash_env_var_info();
                          , 0)

BUILTINFUNC_NOEVAL_MEMBER(useq_stop_all, //
                          clear_all_outputs();
                          println("All outputs cleared.");, 0)

void uSEQ::clear_all_outputs()
{
    for (int i = 0; i < m_continuous_ASTs.size(); i++)
    {
        String name          = "a" + String(i + 1);
        m_continuous_ASTs[i] = default_continuous_expr;
        m_def_exprs.erase(name);
    }

    for (int i = 0; i < m_binary_ASTs.size(); i++)
    {
        String name      = "d" + String(i + 1);
        m_binary_ASTs[i] = default_binary_expr;
        m_def_exprs.erase(name);
    }

    for (int i = 0; i < m_serial_ASTs.size(); i++)
    {
        String name      = "s" + String(i + 1);
        m_serial_ASTs[i] = default_serial_expr;
        m_def_exprs.erase(name);
    }
}

void uSEQ::set_my_id(int num)
{
    m_my_id = num;
    write_flash_info();
}

// Utils
void uSEQ::copy_def_strings_to_buffer(char* buffer)
{
    char* write_pos = buffer;

    for (auto& map : { m_defs, m_def_exprs })
    {
        for (auto& pair : map)
        {
            // NOTE: Unless we cast to c_str first, it seems
            // that there is an issue with multi-byte UTF-8 chars
            // confusing the String() constructor when reading...
            String name_ascii = pair.first.c_str();
            String def_ascii  = pair.second.to_lisp_src().c_str();

            // Write the name
            size_t name_length = name_ascii.length() + 1;
            memcpy(write_pos, name_ascii.c_str(), name_length);
            write_pos += name_length;

            // Write the definition
            size_t def_length = def_ascii.length() + 1;
            memcpy(write_pos, def_ascii.c_str(), def_length);
            write_pos += def_length;
        }
    }
}

size_t padding_to_nearest_multiple_of(size_t in, size_t quant)
{
    return (quant - (in % quant)) % quant;
}

size_t pad_to_nearest_multiple_of(size_t in, size_t quant)
{
    return in + padding_to_nearest_multiple_of(in, quant);
}

void uSEQ::erase_info_flash()
{
    flash_range_erase(FLASH_INFO_SECTOR_OFFSET_START, FLASH_INFO_SECTOR_SIZE);
    println("Erased info flash.");
}

bool uSEQ::flash_has_been_written_before()
{
    // Get a char* to the start of the info block and check to see whether
    // that points to the start of a c-style string (spelling "uSEQ" as
    // of 1.0 release)
    const char* const info_start =
        (char*)(PICO_FLASH_START_ADDR + FLASH_INFO_SECTOR_OFFSET_START);
    return std::strcmp(info_start, m_flash_stamp_str) == 0;
}

void uSEQ::write_flash_info()
{
    // 4k buffer
    u_int8_t buffer[FLASH_INFO_SECTOR_SIZE];

    // This will increment as we're writing
    u_int8_t* write_ptr = &buffer[0];

    // Write a specific type's worth of bytes to the buffer and
    // increment the write pointer
#define WRITE_SEQUENTIALLY_TO_BUFFER_SIZE(__item__, __size__)                       \
    std::memcpy(write_ptr, (u_int8_t*)&__item__, __size__);                         \
    write_ptr += __size__;

#define WRITE_SEQUENTIALLY_TO_BUFFER(__item__)                                      \
    WRITE_SEQUENTIALLY_TO_BUFFER_SIZE(__item__, sizeof(__item__));

    // This should always be at the top so that we can know if we've written
    // to the flash at least once
    std::strcpy((char*)write_ptr, m_flash_stamp_str);
    write_ptr += m_flash_stamp_size_bytes;

    // Writing starting from 0
    // NOTE: The order here should match the order in load_flash_info
    // NOTE: The order here should match the order in load_flash_info
    WRITE_SEQUENTIALLY_TO_BUFFER(m_my_id);
    WRITE_SEQUENTIALLY_TO_BUFFER(m_FLASH_ENV_SECTOR_SIZE);
    WRITE_SEQUENTIALLY_TO_BUFFER(m_FLASH_ENV_SECTOR_OFFSET_START);
    WRITE_SEQUENTIALLY_TO_BUFFER(m_FLASH_ENV_DEFS_SIZE);
    WRITE_SEQUENTIALLY_TO_BUFFER(m_FLASH_ENV_EXPRS_SIZE);
    WRITE_SEQUENTIALLY_TO_BUFFER(m_FLASH_ENV_STRING_BUFFER_SIZE);

    // Write buffer to flash
    write_to_flash(&buffer[0], FLASH_INFO_SECTOR_OFFSET_START,
                   FLASH_INFO_SECTOR_SIZE);

    println("Wrote module info to flash successfully.");
}

void uSEQ::load_flash_info()
{
    const u_int8_t* const info_sector_addr_ptr = reinterpret_cast<u_int8_t*>(
        PICO_FLASH_START_ADDR + FLASH_INFO_SECTOR_OFFSET_START);

    const u_int8_t* read_ptr = info_sector_addr_ptr;

#define READ_SEQUENTIALLY_FROM_FLASH_SIZE(__dest__, __size__)                       \
    std::memcpy(&__dest__, read_ptr, __size__);                                     \
    read_ptr += __size__;

#define READ_SEQUENTIALLY_FROM_FLASH(__item__)                                      \
    READ_SEQUENTIALLY_FROM_FLASH_SIZE(__item__, sizeof(__item__));

    if (flash_has_been_written_before())
    {
        // Skip the flash marker
        read_ptr += m_flash_stamp_size_bytes;

        // NOTE: The order here should match the order in write_flash_info
        READ_SEQUENTIALLY_FROM_FLASH(m_my_id);
        READ_SEQUENTIALLY_FROM_FLASH(m_FLASH_ENV_SECTOR_SIZE);
        READ_SEQUENTIALLY_FROM_FLASH(m_FLASH_ENV_SECTOR_OFFSET_START);
        READ_SEQUENTIALLY_FROM_FLASH(m_FLASH_ENV_DEFS_SIZE);
        READ_SEQUENTIALLY_FROM_FLASH(m_FLASH_ENV_EXPRS_SIZE);
        READ_SEQUENTIALLY_FROM_FLASH(m_FLASH_ENV_STRING_BUFFER_SIZE);
    }
    else
    {
        println("Warning: Attempted to read info from flash but it seems that "
                "nothing has been written there yet. Try running "
                "`(write-flash-info)` first. You will only need to do this "
                "once.");
    }
}

std::pair<size_t, size_t> uSEQ::num_bytes_def_strs() const
{
    size_t defs_size  = 0;
    size_t exprs_size = 0;

    int i = 0;
    for (const auto& map : { m_defs, m_def_exprs })
    {
        size_t size = 0;

        for (const auto& pair : map)
        {
            size += pair.first.length() + pair.second.to_lisp_src().length() + 2;
        }

        if (i == 0)
        {
            defs_size = size;
        }
        else if (i == 1)
        {
            exprs_size = size;
        }

        i++;
    }

    return { defs_size, exprs_size };
}

void uSEQ::write_flash_env()
{
    // 1. Collect all env strings and figure out their total size
    std::pair<size_t, size_t> pair = num_bytes_def_strs();
    m_FLASH_ENV_DEFS_SIZE          = pair.first;
    m_FLASH_ENV_EXPRS_SIZE         = pair.second;
    m_FLASH_ENV_STRING_BUFFER_SIZE = pair.first + pair.second;

    size_t buffer_size =
        pad_to_nearest_multiple_of(m_FLASH_ENV_STRING_BUFFER_SIZE, FLASH_PAGE_SIZE);

    if (buffer_size % FLASH_PAGE_SIZE != 0)
    {
        println("Error: Flash env buffer size is not a multiple of the page "
                "size: " +
                String(buffer_size));
        return;
    }

    m_FLASH_ENV_SECTOR_SIZE =
        pad_to_nearest_multiple_of(buffer_size, FLASH_SECTOR_SIZE);

    // The env sector ends where the info sector begins
    m_FLASH_ENV_SECTOR_OFFSET_END = FLASH_INFO_SECTOR_OFFSET_START;

    m_FLASH_ENV_SECTOR_OFFSET_START =
        m_FLASH_ENV_SECTOR_OFFSET_END - m_FLASH_ENV_SECTOR_SIZE;

    if (m_FLASH_ENV_SECTOR_SIZE % FLASH_SECTOR_SIZE != 0)
    {
        println("Error: Flash env sector size is not a multiple of the "
                "sector size: " +
                String(buffer_size));
        return;
    }

    if (m_FLASH_ENV_SECTOR_OFFSET_START % FLASH_SECTOR_SIZE != 0)
    {
        println("Error: Flash env sector start position does not allign "
                "with flash "
                "sector boundaries: " +
                String(buffer_size));
        println(String((uint)m_FLASH_ENV_SECTOR_OFFSET_START));
        return;
    }

    // 2. Allocate a padded buffer to hold packed strings
    char* buffer = new char[buffer_size];
    copy_def_strings_to_buffer(buffer);

    // Flush the in-memory buffer to the flash
    write_to_flash((u_int8_t*)buffer, m_FLASH_ENV_SECTOR_OFFSET_START,
                   m_FLASH_ENV_SECTOR_SIZE, buffer_size);

    delete[] buffer;

    println("Wrote current variable definitions to flash.");

    // 5. Update the info sector with the new locations/sizes
    write_flash_info();
}

void uSEQ::load_flash_env()
{
    println("Loading previously saved state...");

    char* strings_start =
        (char*)(PICO_FLASH_START_ADDR + m_FLASH_ENV_SECTOR_OFFSET_START);

    char* read_ptr    = strings_start;
    size_t bytes_read = 0;
    // We read defs first, then swap to def_exprs
    ValueMap* map_ptr = &m_defs;

    while (bytes_read < m_FLASH_ENV_STRING_BUFFER_SIZE)
    {
        String name_str = String(read_ptr);
        read_ptr += name_str.length() + 1;

        String def_str = String(read_ptr);
        read_ptr += def_str.length() + 1;

        // Update bytes read (to see if we need to move
        // on to loading def exprs instead)
        bytes_read = (size_t)(read_ptr - strings_start);

        if (bytes_read > m_FLASH_ENV_DEFS_SIZE)
        {
            // Swap pointers
            map_ptr = &m_def_exprs;
        }

        Value val = parse(def_str);

        if (val.is_error())
        {
            println("Warning: Expression for " + name_str +
                    " could not be parsed (ignoring):\n" + "    " + def_str);
        }
        else
        {
            (*map_ptr)[name_str] = val;
        }
    }

    for (int i = 0; i < m_continuous_ASTs.size(); i++)
    {
        String name               = "a" + String(i + 1);
        std::optional<Value> expr = get_expr(name);
        if (expr)
        {
            m_continuous_ASTs[i] = *expr;
        }
        else
        {
            // println("Warning: Expression for output " + name +
            //         " was not found, ignoring...");
        }
    }

    for (int i = 0; i < m_binary_ASTs.size(); i++)
    {
        String name               = "d" + String(i + 1);
        std::optional<Value> expr = get_expr(name);
        if (expr)
        {
            m_binary_ASTs[i] = *expr;
        }
        else
        {
            // println("Warning: Expression for output " + name +
            //         " was not found, ignoring...");
        }
    }

    for (int i = 0; i < m_serial_ASTs.size(); i++)
    {
        String name               = "s" + String(i + 1);
        std::optional<Value> expr = get_expr(name);
        if (expr)
        {
            m_serial_ASTs[i] = *expr;
        }
        else
        {
            // println("Warning: Expression for output " + name +
            //         " was not found, ignoring...");
        }
    }

    println("Previously saved state loaded successfully.");
}

void uSEQ::autoload_flash()
{
    if (flash_has_been_written_before())
    {
        load_flash_info();

        if (m_FLASH_ENV_SECTOR_SIZE > 0 && m_FLASH_ENV_SECTOR_OFFSET_START > 0)
        {
            // println("All good, reading env...");
            load_flash_env();
        }
        else
        {
            println("Not ready to read env:");

            String s = "m_FLASH_ENV_SECTOR_SIZE: ";
            s += String((size_t)m_FLASH_ENV_SECTOR_SIZE);
            println(s);

            s = "m_FLASH_ENV_SECTOR_OFFSET_START: ";
            s += String((size_t)m_FLASH_ENV_SECTOR_OFFSET_START);
            println(s);
        }
    }
    else
    {
        // println("Flash NOT written before - ignoring.");
    }
}

// FIXME hangs
// void uSEQ::clear_non_program_flash()
// {
//     // NOTE: this was taken from the Arduino's IDE printout
//     // during compilation
//     constexpr uint32_t max_code_bytes = 1044480;

//     for (uint32_t addr = max_code_bytes; addr < PICO_FLASH_SIZE_BYTES;
//          addr += FLASH_SECTOR_SIZE)
//     {
//         if (addr % FLASH_SECTOR_SIZE != 0)
//         {
//             println("Address " + String(addr) +
//                     " is NOT aligned on sector borders.");
//         }
//         else
//         {
//             flash_range_erase(addr, FLASH_SECTOR_SIZE);
//         }
//     }

//     println("Cleared flash memory.");
// }

void uSEQ::reboot()
{
#define AIRCR_Register (*((volatile uint32_t*)(PPB_BASE + 0x0ED0C)))
    AIRCR_Register = 0x5FA0004;
}

void uSEQ::reset_flash_env_var_info()
{
    m_FLASH_ENV_SECTOR_SIZE         = 0;
    m_FLASH_ENV_DEFS_SIZE           = 0;
    m_FLASH_ENV_EXPRS_SIZE          = 0;
    m_FLASH_ENV_SECTOR_OFFSET_START = 0;
    m_FLASH_ENV_SECTOR_OFFSET_END   = 0;
    m_FLASH_ENV_STRING_BUFFER_SIZE  = 0;
    write_flash_info();
}

//////////////////////////

// Creates a Lisp Value of type BUILTIN_METHOD,
// which requires
#define INSERT_BUILTINDEF(__name__, __func_name__)                                  \
    Environment::builtindefs[__name__] =                                            \
        Value((String)__name__, &uSEQ::__func_name__);

void uSEQ::init_builtinfuncs()
{
    DBG("uSEQ::init_builtinfuncs");

    INSERT_BUILTINDEF("eval-at-time", useq_eval_at_time);
    INSERT_BUILTINDEF("useq-rewind", useq_rewind_logical_time);

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
    INSERT_BUILTINDEF("s7", useq_s7);
    INSERT_BUILTINDEF("s8", useq_s8);

    // These are not class methods, so they can be inserted normally
    INSERT_BUILTINDEF("useqaw", ard_useqaw);
    INSERT_BUILTINDEF("useqdw", ard_useqdw);

    // These are all class methods
    INSERT_BUILTINDEF("toggle-sel", useq_toggle_select);
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

    INSERT_BUILTINDEF("tri", useq_tri);

    // INSERT_BUILTINDEF("looph", useq_loopPhasor);
    INSERT_BUILTINDEF("dm", useq_dm);
    INSERT_BUILTINDEF("gates", useq_gates);
    INSERT_BUILTINDEF("gatesw", useq_gatesw);
    INSERT_BUILTINDEF("trigs", useq_trigs);
    INSERT_BUILTINDEF("euclid", useq_euclidean);
    INSERT_BUILTINDEF("eu", useq_eu);
    INSERT_BUILTINDEF("rpulse", useq_ratiotrig);
    INSERT_BUILTINDEF("rstep", useq_ratiostep);
    INSERT_BUILTINDEF("ridx", useq_ratioindex);
    INSERT_BUILTINDEF("rwarp", useq_ratiowarp);
    INSERT_BUILTINDEF("shift", useq_phasor_offset);

    // NOTE: different names for the same function
    INSERT_BUILTINDEF("from-list", useq_fromList);
    INSERT_BUILTINDEF("seq", useq_seq);
    INSERT_BUILTINDEF("flatseq", useq_flatseq);
    //
    INSERT_BUILTINDEF("from-flattened-list", useq_fromFlattenedList);
    INSERT_BUILTINDEF("flatten", useq_flatten);
    INSERT_BUILTINDEF("interp", useq_interpolate);
    INSERT_BUILTINDEF("step", useq_step);

    // clock sources and management
    INSERT_BUILTINDEF("reset-clock-ext", useq_reset_external_clock_tracking);
    INSERT_BUILTINDEF("reset-clock-int", useq_reset_internal_clock);
    INSERT_BUILTINDEF("get-clock-source", useq_get_clock_source);
    INSERT_BUILTINDEF("set-clock-int", useq_set_clock_internal);
    INSERT_BUILTINDEF("set-clock-ext", useq_set_clock_external);

    // TODO
#ifdef MUSICTHING
    INSERT_BUILTINDEF("knob", useq_mt_knob);
    INSERT_BUILTINDEF("knobx", useq_mt_knobx);
    INSERT_BUILTINDEF("knoby", useq_mt_knoby);
    INSERT_BUILTINDEF("swz", useq_mt_swz);
#endif

#ifdef MIDIOUT
    INSERT_BUILTINDEF("mdo", useq_mdo);
#endif

    // SYSTEM API
    INSERT_BUILTINDEF("useq-reboot", useq_reboot);

    // FLASH API
    // NOTE: These are meant to be the main user interface
    // ID
    INSERT_BUILTINDEF("useq-set-id", useq_set_my_id);
    INSERT_BUILTINDEF("useq-get-id", useq_get_my_id);
    // MEMORY
    INSERT_BUILTINDEF("useq-memory-save", useq_memory_save);
    INSERT_BUILTINDEF("useq-memory-restore", useq_memory_restore);
    INSERT_BUILTINDEF("useq-memory-erase", useq_memory_erase);
    // NOTE: aliases
    INSERT_BUILTINDEF("useq-memory-load", useq_memory_restore);
    INSERT_BUILTINDEF("useq-memory-clear", useq_memory_erase);

    // NOTE: these are NOT meant to be user interface, just for
    // more granular dev tests
    INSERT_BUILTINDEF("write-flash-info", useq_write_flash_info);
    INSERT_BUILTINDEF("load-flash-info", useq_load_flash_info);

    INSERT_BUILTINDEF("write-flash-env", useq_write_flash_env);
    INSERT_BUILTINDEF("load-flash-env", useq_load_flash_env);

    INSERT_BUILTINDEF("useq-autoload-flash", useq_autoload_flash);
    INSERT_BUILTINDEF("useq-stop-all", useq_stop_all);

    // FIRMWARE
    // NOTE: for user interface only
    INSERT_BUILTINDEF("useq-firmware-info", useq_firmware_info);
    // NOTE: for editor use only
    INSERT_BUILTINDEF("useq-report-firmware-info", useq_report_firmware_info);
    // INSERT_BUILTINDEF("useq-clear-flash", useq_clear_non_program_flash);

    INSERT_BUILTINDEF("send-to", useq_send_to);
}

BUILTINFUNC_NOEVAL_MEMBER(useq_firmware_info, //
                                              // println(USEQ_FIRMWARE_VERSION);
                          String msg = "uSEQ Firmware Version: " +
                                       String(USEQ_FIRMWARE_VERSION);
#ifdef MUSICTHING
                          msg += " (Music Thing Workshop Computer Edition)";
#endif
                          ret = Value::string(msg);, 0)

BUILTINFUNC_NOEVAL_MEMBER(useq_report_firmware_info, //
                          message_editor((String)USEQ_FIRMWARE_VERSION);
                          // String msg = "{";
                          // msg += "\"release_date\": \"";
                          // msg += String((String)USEQ_FIRMWARE_RELEASE_DATE);
                          // msg += "\", {\"version\": \"";
                          // msg += String((String)USEQ_FIRMWARE_VERSION); //
                          // msg += "\"}";                                 //
                          // println(msg);
                          , 0)
