#include "uSEQ.h"
#include "utils.h"
#include "hardware_includes.h"

float pdm_y   = 0;
float pdm_err = 0;
float pdm_w   = 0;

void uSEQ::set_input_val(size_t index, double value) { m_input_vals[index] = value; }



#if HAS_OUTPUTS
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

    #ifndef USEQHARDWARE_EXPANDER_OUT_0_1  //NOT EXPANDER WHICH IS NON PIO
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
    #endif // NOT EXPANDER
}
#endif // HAS_OUTPUTS




#if HAS_INPUTS
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

void uSEQ::gpio_irq_gate1()
{
    double ts         = static_cast<double>(micros());
    const auto input1 = 1 - digitalRead(USEQ_PIN_I1);
    uSEQ::instance->set_input_val(USEQI1, input1);
    digitalWrite(USEQ_PIN_LED_I1, input1);
    
    // Check for sync trigger
    if (uSEQ::instance->m_waiting_for_sync_trigger && input1 == 1)
    {
        uSEQ::instance->m_waiting_for_sync_trigger = false;
        uSEQ::instance->reset_logical_time();
        // TODO propagate the trigger to connected I2C devices
        return;
    }
    
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
    
    // Check for sync trigger
    if (uSEQ::instance->m_waiting_for_sync_trigger && input2 == 1)
    {
        uSEQ::instance->m_waiting_for_sync_trigger = false;
        uSEQ::instance->reset_logical_time();
        // TODO propagate the trigger to connected I2C devices
        return;
    }
    
    if (input2 == 1 &&
        uSEQ::instance->getClockSource() == uSEQ::CLOCK_SOURCES::EXTERNAL_I2)
    {
        uSEQ::instance->update_clock_from_external(ts);
    }
}

void uSEQ::setup_digital_ins()
{
    pinMode(USEQ_PIN_I1, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(USEQ_PIN_I1), uSEQ::gpio_irq_gate1,
                    CHANGE);
    pinMode(USEQ_PIN_I2, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(USEQ_PIN_I2), uSEQ::gpio_irq_gate2,
                    CHANGE);
}
#endif // HAS_INPUTS

#if HAS_CONTROLS
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
#endif // HAS_CONTROLS

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

#if HAS_OUTPUTS
    setup_outs();
    setup_analog_outs();
#endif
#if HAS_INPUTS
    setup_digital_ins();
#ifdef ANALOG_INPUTS
    setup_analog_ins();
#endif // ANALOG_INPUTS
#endif // HAS_INPUTS
#if HAS_CONTROLS
    setup_switches();
#endif
#ifdef USEQHARDWARE_0_2
    setup_rotary_encoder();
#endif

#ifdef MIDIOUT
    Serial1.setRX(1);
    Serial1.setTX(0);
    Serial1.begin(31250);
#endif

#ifdef USEQHARDWARE_1_0
    //i2c now setup below
    //Wire.setSDA(0);
    //Wire.setSCL(1);
    // peripheral
    //  Wire.begin(4);
    //  Wire.onReceive(receiveEvent);

    // controller
    //  Wire.begin();
#endif

// all default to CLIENT - will change
#ifdef ENABLEI2CCLIENT
    bI2CclientMode = true;
    bI2ChostMode   = false;
    setup_i2cCLIENT();
#endif
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
    int led_pin   = analog_out_LED_pin(output + 1);
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

    #ifdef USEQHARDWARE_EXPANDER_OUT_0_1
     // write led
     analogWrite(led_pin, ledsigval);
    #else
       // write pwm
    pio_pwm_set_level(pio0, output, ledsigval);
    #endif



    // write led -- output surely? ***
    analogWrite(pwm_pin, scaled_val);
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



MedianFilter mf1(51), mf2(51);

#if HAS_INPUTS
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

    // println(m_input_vals[MTMAINKNOB]);
    // println("\t");
    // println(m_input_vals[MTXKNOB]);
    // println("\t");
    // println(m_input_vals[MTYKNOB]);
    // println("\t");
    // println(m_input_vals[MTZSWITCH]);

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
#endif // HAS_INPUTS


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
    if (pindex <= (NUM_CONTINUOUS_OUTS + NUM_BINARY_OUTS))
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

#ifdef USEQHARDWARE_1_0
void start_pdm()
{
    static repeating_timer_t mst;

    add_repeating_timer_us(150, timer_callback, NULL, &mst);
}
#endif



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





// MIDI OUT

#ifdef MIDIOUT
double last_midi_t = 0;
void uSEQ::update_midi_out()
{
    DBG("uSEQ::update_midi_out");
    const double midiRes        = 48 * meter_numerator * 1;
    // FIXME: where is this barDur supposed to be coming from?
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
                // println(t_step);
                mdoArgs[0] = Value(t_step);
                Value val  = midiFunction.apply(mdoArgs, env);

                // println(val.as_float());
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


