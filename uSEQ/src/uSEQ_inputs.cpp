#include "uSEQ.h"
#include "utils.h"

#ifdef USEQHARDWARE_0_2
int8_t read_rotary()
{
    static int8_t rot_enc_table[] = {
        0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0
    };

    static uint8_t prevNextCode = 0;
    static uint16_t store       = 0;

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

    if (int8_t val = read_rotary())
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
    const double recp2048 = 1 / 2048.0;
    const double recp1024 = 1 / 1024.0;

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
    //pdm_w         = v_ai1_11 / 2048.0;
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