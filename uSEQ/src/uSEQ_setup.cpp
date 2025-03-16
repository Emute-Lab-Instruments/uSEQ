#include "uSEQ.h"
#include "hardware/flash.h"
#include "uSEQ/piopwm.h"


#ifdef ARDUINO
#include "hardware/flash.h"
#endif

extern maxiFilter cvInFilter[2];

// statics
uSEQ* uSEQ::instance;

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
    setup_IO();

    // dbg("Lisp library loaded.");

    // uSEQ software setup
    set_bpm(m_defaultBPM, 0.0);
    update_time();
    init_ASTs();

    autoload_flash();

    m_initialised = true;
}