#include "io_manager.h"
#include "../uSEQ.h"
#include "../utils.h"
#include "../utils/serial_message.h"

#ifndef ARDUINO
#include "../hardware_includes.h"
#endif

#ifdef ARDUINO
#include "pinmap.h"
#include "piopwm.h"
#ifdef USEQHARDWARE_1_0
#include "hardware/timer.h"
#endif
#include <hardware/pio.h>
#include <hardware/gpio.h>
#endif

// Static instance for interrupt callbacks
IOManager* IOManager::s_instance = nullptr;

// PDM globals for hardware 1.0
#ifdef USEQHARDWARE_1_0
float pdm_y = 0;
float pdm_err = 0;
float pdm_w = 0;
#endif

// Constructor
IOManager::IOManager(uSEQ* parent, IIo* io_adapter)
    : m_parent(parent)
    , m_io_adapter(io_adapter)
    , m_filter1(51)
    , m_filter2(51)
{
    // Initialize input values to zero
    for (int i = 0; i < 14; i++) {
        m_input_vals[i] = 0.0;
    }
    
    // Set static instance for interrupt callbacks
    IOManager::set_instance(this);
}

// === Initialization ===

void IOManager::init() {
    setup_io();
}

void IOManager::setup_io() {
    DBG("IOManager::setup_io");

#if HAS_OUTPUTS
    setup_outputs();
    setup_analog_outputs();
#endif

#if HAS_INPUTS
    setup_digital_inputs();
#ifdef ANALOG_INPUTS
    setup_analog_inputs();
#endif
#endif

#if HAS_CONTROLS
    setup_switches();
#endif

#ifdef USEQHARDWARE_0_2
    setup_rotary_encoder();
#endif

#ifdef ENABLE_LED_CONTROL
    setup_leds();
#endif

#ifdef MIDIOUT
    setup_midi();
#endif
}

// === Setup Functions ===

#if HAS_OUTPUTS
void IOManager::setup_outputs() {
    DBG("IOManager::setup_outputs");
    
#ifdef ARDUINO
    for (int i = 0; i < NUM_CONTINUOUS_OUTS + NUM_BINARY_OUTS; i++) {
        pinMode(useq_output_pins[i], OUTPUT_2MA);
    }

#ifdef MUSICTHING
    pinMode(MUX_LOGIC_A, OUTPUT);
    pinMode(MUX_LOGIC_B, OUTPUT);
#endif
#endif // ARDUINO
}

void IOManager::setup_analog_outputs() {
    DBG("IOManager::setup_analog_outputs");
    
#ifdef ARDUINO
    // PWM outputs
    analogWriteFreq(100000);   // out of hearing range
    analogWriteResolution(11); // about the best we can get

#ifndef USEQHARDWARE_EXPANDER_OUT_0_1
    // set PIO PWM state machines to run PWM outputs
    uint offset = pio_add_program(pio0, &pwm_program);
    uint offset2 = pio_add_program(pio1, &pwm_program);

    for (int i = 0; i < NUM_CONTINUOUS_OUTS; i++) {
        auto pioInstance = i < 4 ? pio0 : pio1;
        uint pioOffset = i < 4 ? offset : offset2;
        auto smIdx = i % 4;
        pwm_program_init(pioInstance, smIdx, pioOffset, useq_output_led_pins[i]);
        pio_pwm_set_period(pioInstance, smIdx, (1u << 11) - 1);
    }
#endif // NOT EXPANDER
#endif // ARDUINO
}
#endif // HAS_OUTPUTS

#if HAS_INPUTS
void IOManager::setup_digital_inputs() {
    DBG("IOManager::setup_digital_inputs");
    
#ifdef ARDUINO
    pinMode(USEQ_PIN_I1, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(USEQ_PIN_I1), IOManager::gpio_irq_gate1, CHANGE);
    pinMode(USEQ_PIN_I2, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(USEQ_PIN_I2), IOManager::gpio_irq_gate2, CHANGE);
#endif
}

#ifdef ANALOG_INPUTS
void IOManager::setup_analog_inputs() {
    DBG("IOManager::setup_analog_inputs");
    
#ifdef ARDUINO
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
#endif // ARDUINO
}
#endif // ANALOG_INPUTS
#endif // HAS_INPUTS

#if HAS_CONTROLS
void IOManager::setup_switches() {
    DBG("IOManager::setup_switches");
    
#ifdef ARDUINO
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
#endif // ARDUINO
}
#endif

#ifdef ENABLE_LED_CONTROL
void IOManager::setup_leds() {
    DBG("IOManager::setup_leds");
    
#ifdef ARDUINO
#ifdef USEQHARDWARE_1_0
    pinMode(USEQ_PIN_LED_AI1, OUTPUT_2MA);
    pinMode(USEQ_PIN_LED_AI2, OUTPUT_2MA);
    pinMode(USEQ_PIN_LED_I1, OUTPUT_2MA);
    pinMode(USEQ_PIN_LED_I2, OUTPUT_2MA);
#endif

    for (int i = 0; i < (NUM_CONTINUOUS_OUTS + NUM_BINARY_OUTS); i++) {
        pinMode(useq_output_led_pins[i], OUTPUT_2MA);
        gpio_set_slew_rate(useq_output_led_pins[i], GPIO_SLEW_RATE_SLOW);
    }
#endif // ARDUINO
}
#endif

#ifdef MIDIOUT
void IOManager::setup_midi() {
#ifdef ARDUINO
    Serial1.setRX(1);
    Serial1.setTX(0);
    Serial1.begin(31250);
#endif
}
#endif

// === Input Operations ===

void IOManager::set_input_value(size_t index, double value) {
    if (index < 14) {
        m_input_vals[index] = value;
    }
}

double IOManager::get_input_value(size_t index) const {
    if (index < 14) {
        return m_input_vals[index];
    }
    return 0.0;
}

#if HAS_INPUTS
void IOManager::update_inputs() {
    DBG("IOManager::update_inputs");

#ifdef USEQHARDWARE_0_2
    read_rotary_encoders();
#endif

    const double recp4096 = 0.000244141; // 1/4096
    const double recp2048 = 1 / 2048.0;

#ifdef MUSICTHING
    read_musicthing_inputs();
#else
    m_input_vals[USEQT1] = 1 - digitalRead(USEQ_PIN_SWITCH_T1);
#endif

#ifdef USEQHARDWARE_0_2
    m_input_vals[USEQRS1] = 1 - digitalRead(USEQ_PIN_SWITCH_R1);
    m_input_vals[USEQM2] = 1 - digitalRead(USEQ_PIN_SWITCH_M2);
    m_input_vals[USEQT2] = 1 - digitalRead(USEQ_PIN_SWITCH_T2);
#endif

#ifdef USEQHARDWARE_1_0
    read_hardware_1_0_inputs();
#endif

    dbg("updating inputs...DONE");
}

#ifdef MUSICTHING
void IOManager::read_musicthing_inputs() {
    const double recp4096 = 0.000244141;
    const size_t muxdelay = 2; // FIXME increase this for stability

    // unroll loop for efficiency
    digitalWrite(MUX_LOGIC_A, 0);
    digitalWrite(MUX_LOGIC_B, 0);
    delayMicroseconds(muxdelay);
    m_input_vals[MTMAINKNOB] = analogRead(MUX_IN_1) * recp4096;
    m_input_vals[USEQAI1] = 1.0 - (analogRead(MUX_IN_2) * recp4096);

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
    if (switchVal < 100) {
        switchVal = 0;
    } else if (switchVal > 3500) {
        switchVal = 2;
    } else {
        switchVal = 1;
    }
    m_input_vals[MTZSWITCH] = switchVal;
}
#endif

#ifdef USEQHARDWARE_1_0
void IOManager::read_hardware_1_0_inputs() {
    const double recp2048 = 1 / 2048.0;
    
    // TOGGLES
    const int ts_a = 1 - digitalRead(USEQ_PIN_SWITCH_T1);
    const int ts_b = 1 - digitalRead(USEQ_PIN_SWITCH_T2);

    if ((ts_a == 0) && (ts_b == 0)) {
        m_input_vals[USEQT1] = 1;
    } else {
        if (ts_a == 1) {
            m_input_vals[USEQT1] = 2;
        } else {
            m_input_vals[USEQT1] = 0;
        }
    }

    // MOMENTARY
    m_input_vals[USEQM1] = 1 - digitalRead(USEQ_PIN_SWITCH_M1);

    // ANALOG INPUTS
    auto v_ai1 = analogRead(USEQ_PIN_AI1);
    auto v_ai2 = analogRead(USEQ_PIN_AI2);

    auto v_ai1_11 = v_ai1;
    v_ai1_11 = (v_ai1_11 * v_ai1_11) >> 11; // sqr to get exp curve
    pdm_w = v_ai1_11 / 2048.0;
    
    auto v_ai2_11 = v_ai2;
    v_ai2_11 = (v_ai2_11 * v_ai2_11) >> 11;
    analogWrite(USEQ_PIN_LED_AI2, v_ai2_11);

    double filt1 = m_filter1.process(v_ai1 * recp2048);
    double filt2 = m_filter2.process(v_ai2 * recp2048);
    m_input_vals[USEQAI1] = filt1;
    m_input_vals[USEQAI2] = filt2;
}
#endif

// Static interrupt handlers
void IOManager::gpio_irq_gate1() {
    if (!s_instance || !s_instance->m_parent) return;
    
#ifdef ARDUINO
    double ts = static_cast<double>(micros());
    const auto input1 = 1 - digitalRead(USEQ_PIN_I1);
    s_instance->set_input_value(USEQI1, input1);
    digitalWrite(USEQ_PIN_LED_I1, input1);
    
    // Delegate to parent for clock handling
    s_instance->m_parent->handle_input1_interrupt(ts, input1);
#endif
}

void IOManager::gpio_irq_gate2() {
    if (!s_instance || !s_instance->m_parent) return;
    
#ifdef ARDUINO
    double ts = static_cast<double>(micros());
    const auto input2 = 1 - digitalRead(USEQ_PIN_I2);
    s_instance->set_input_value(USEQI2, input2);
    digitalWrite(USEQ_PIN_LED_I2, input2);
    
    // Delegate to parent for clock handling
    s_instance->m_parent->handle_input2_interrupt(ts, input2);
#endif
}
#endif // HAS_INPUTS

// === Output Operations ===

void IOManager::analog_write_with_led(int output, CONTINUOUS_OUTPUT_VALUE_TYPE val) {
    DBG("IOManager::analog_write_with_led");

    constexpr double maxpwm = 2047.0;

    int scaled_val = val * maxpwm;
    dbg("scaled_val (before clamping) = " + String(scaled_val));

    // clamping
    if (scaled_val > maxpwm) {
        dbg("over maxpwm, clamping");
        scaled_val = maxpwm;
    }
    if (scaled_val < 0) {
        dbg("less than 0, clamping");
        scaled_val = 0;
    }

    // led
    int led_pin = get_analog_out_led_pin(output + 1);
    int pwm_pin = get_analog_out_pin(output + 1);
    int ledsigval = scaled_val;
    ledsigval = (ledsigval * ledsigval) >> 11; // cheap way to square and get a exp curve

    dbg("output = " + String(output));
    dbg("pin = " + String(pwm_pin));
    dbg("led pin = " + String(led_pin));
    dbg("val = " + String(val));
    dbg("scaled_val = " + String(scaled_val));

    // If an I/O adapter is provided, use it and return (desktop tests)
    if (m_io_adapter) {
        m_io_adapter->analogWrite(static_cast<uint8_t>(led_pin), ledsigval);
        m_io_adapter->analogWrite(static_cast<uint8_t>(pwm_pin), scaled_val);
        return;
    }

#ifdef ARDUINO
#ifdef USEQHARDWARE_EXPANDER_OUT_0_1
    // write led
    analogWrite(led_pin, ledsigval);
#else
    // write pwm
    pio_pwm_set_level(pio0, output, ledsigval);
#endif
    // write output
    analogWrite(pwm_pin, scaled_val);
#else
    (void)led_pin; (void)pwm_pin; (void)ledsigval; (void)scaled_val;
#endif
}

void IOManager::digital_write_with_led(int output, BINARY_OUTPUT_VALUE_TYPE val) {
    DBG("IOManager::digital_write_with_led");

    int pin = get_digital_out_pin(output + 1);
    int led_pin = get_digital_out_led_pin(output + 1);

    dbg("output = " + String(output));
    dbg("pin = " + String(pin));
    dbg("led pin = " + String(led_pin));
    dbg("val = " + String(val));

    // If an I/O adapter is provided, use it and return
    if (m_io_adapter) {
        uint8_t v = static_cast<uint8_t>(val > 0);
        m_io_adapter->digitalWrite(static_cast<uint8_t>(pin), v);
        m_io_adapter->digitalWrite(static_cast<uint8_t>(led_pin), v);
        return;
    }

#ifdef ARDUINO
    // write digi
#ifdef DIGI_OUT_INVERTED
    digitalWrite(pin, 1 - (val > 0));
#else
    digitalWrite(pin, val > 0);
#endif
    // write led
    digitalWrite(led_pin, val > 0);
#else
    (void)pin; (void)led_pin; (void)val;
#endif
}

void IOManager::serial_write(int out, SERIAL_OUTPUT_VALUE_TYPE val) {
    DBG("IOManager::serial_write");

    if (m_io_adapter) {
        m_io_adapter->serialWrite(static_cast<uint8_t>(out), val);
        return;
    }

#ifdef ARDUINO
    Serial.write(SerialMsg::message_begin_marker);
    Serial.write((u_int8_t)SerialMsg::serial_message_types::STREAM);
    Serial.write((u_int8_t)(out + 1));
    u_int8_t* byteArray = reinterpret_cast<u_int8_t*>(&val);
    for (size_t b = 0; b < 8; b++) {
        Serial.write(byteArray[b]);
    }
#else
    (void)out; (void)val;
#endif
}

// === LED Control ===

void IOManager::led_animation() {
#ifdef ARDUINO
    int ledDelay = 30;
#ifdef MUSICTHING
    ledDelay = 40;
    for (int i = 0; i < 8; i++) {
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
    for (int i = 0; i < 8; i++) {
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
    for (int i = 0; i < 8; i++) {
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
#ifdef USEQHARDWARE_EXPANDER_OUT_0_1
    for (int i = 0; i < 8; i++) {
        digitalWrite(useq_output_led_pins[0], 1);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[1], 1);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[0], 0);
        digitalWrite(useq_output_led_pins[2], 1);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[1], 0);
        digitalWrite(useq_output_led_pins[3], 1);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[2], 0);
        digitalWrite(useq_output_led_pins[4], 1);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[3], 0);
        digitalWrite(useq_output_led_pins[5], 1);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[4], 0);
        digitalWrite(useq_output_led_pins[6], 1);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[5], 0);
        digitalWrite(useq_output_led_pins[7], 1);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[6], 0);
        delay(ledDelay);
        digitalWrite(useq_output_led_pins[7], 0);
        ledDelay -= 3;
    }
#endif
#else
    // Desktop stub - LED animation not supported
    (void)0; // No-op for desktop builds
#endif // ARDUINO
}

// === Helper Functions ===

int IOManager::get_analog_out_pin(int out) const {
#ifdef ARDUINO
    if (out > 0 && out <= NUM_CONTINUOUS_OUTS) {
        return useq_output_pins[out - 1];
    }
#else
    (void)out;
#endif
    return -1;
}

int IOManager::get_analog_out_led_pin(int out) const {
#ifdef ARDUINO
    if (out > 0 && out <= NUM_CONTINUOUS_OUTS) {
        return useq_output_led_pins[out - 1];
    }
#else
    (void)out;
#endif
    return -1;
}

int IOManager::get_digital_out_pin(int out) const {
#ifdef ARDUINO
    int pindex = NUM_CONTINUOUS_OUTS + out;
    if (pindex > 0 && pindex <= (NUM_CONTINUOUS_OUTS + NUM_BINARY_OUTS)) {
        return useq_output_pins[pindex - 1];
    }
#else
    (void)out;
#endif
    return -1;
}

int IOManager::get_digital_out_led_pin(int out) const {
#ifdef ARDUINO
    int pindex = NUM_CONTINUOUS_OUTS + out;
    if (pindex > 0 && pindex <= 6) {
        return useq_output_led_pins[pindex - 1];
    }
#else
    (void)out;
#endif
    return -1;
}

// === Rotary Encoder Support (Hardware 0.2) ===

#ifdef USEQHARDWARE_0_2
void IOManager::setup_rotary_encoder() {
#ifdef ARDUINO
    pinMode(USEQ_PIN_SWITCH_R1, INPUT_PULLUP);
    pinMode(USEQ_PIN_ROTARYENC_A, INPUT_PULLUP);
    pinMode(USEQ_PIN_ROTARYENC_B, INPUT_PULLUP);
    m_input_vals[USEQR1] = 0;
#endif
}

int8_t IOManager::read_rotary() {
#ifdef ARDUINO
    static int8_t rot_enc_table[] = {
        0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0
    };

    m_prev_next_code <<= 2;
    if (digitalRead(USEQ_PIN_ROTARYENC_B))
        m_prev_next_code |= 0x02;
    if (digitalRead(USEQ_PIN_ROTARYENC_A))
        m_prev_next_code |= 0x01;
    m_prev_next_code &= 0x0f;

    // If valid then store as 16 bit data.
    if (rot_enc_table[m_prev_next_code]) {
        m_store <<= 4;
        m_store |= m_prev_next_code;
        if ((m_store & 0xff) == 0x2b)
            return -1;
        if ((m_store & 0xff) == 0x17)
            return 1;
    }
#endif
    return 0;
}

void IOManager::read_rotary_encoders() {
#ifdef ARDUINO
    int8_t val = read_rotary();
    if (val) {
        m_input_vals[USEQR1] += val;
    }
#endif
}
#endif // USEQHARDWARE_0_2

// === PIO PWM Functions (Arduino Pico) ===

#ifdef ARDUINO
void IOManager::pio_pwm_set_level(PIO pio, uint sm, uint32_t level) {
    DBG("IOManager::pio_pwm_set_level");
    dbg(String(reinterpret_cast<size_t>(pio)));
    dbg(String(sm));
    dbg(String(level));
    pio_sm_put_blocking(pio, sm, level);
}

void IOManager::pio_pwm_set_period(PIO pio, uint sm, uint32_t period) {
    pio_sm_set_enabled(pio, sm, false);
    pio_sm_put_blocking(pio, sm, period);
    pio_sm_exec(pio, sm, pio_encode_pull(false, false));
    pio_sm_exec(pio, sm, pio_encode_out(pio_isr, 32));
    pio_sm_set_enabled(pio, sm, true);
}

// Global helper functions for backward compatibility
int analog_out_pin(int out) {
    if (out > 0 && out <= NUM_CONTINUOUS_OUTS) {
        return useq_output_pins[out - 1];
    }
    return -1;
}

int analog_out_LED_pin(int out) {
    if (out > 0 && out <= NUM_CONTINUOUS_OUTS) {
        return useq_output_led_pins[out - 1];
    }
    return -1;
}

int digital_out_pin(int out) {
    int pindex = NUM_CONTINUOUS_OUTS + out;
    if (pindex > 0 && pindex <= (NUM_CONTINUOUS_OUTS + NUM_BINARY_OUTS)) {
        return useq_output_pins[pindex - 1];
    }
    return -1;
}

int digital_out_LED_pin(int out) {
    int pindex = NUM_CONTINUOUS_OUTS + out;
    if (pindex > 0 && pindex <= 6) {
        return useq_output_led_pins[pindex - 1];
    }
    return -1;
}
#endif // ARDUINO

// === PDM Timer Callback (Hardware 1.0) ===

#ifdef USEQHARDWARE_1_0
bool timer_callback(repeating_timer_t* rt) {
    pdm_y = pdm_w > pdm_err ? 1 : 0;
    pdm_err = pdm_y - pdm_w + pdm_err;
    if (pdm_y == 1) {
        digitalWrite(USEQ_PIN_LED_AI1, HIGH);
    } else {
        digitalWrite(USEQ_PIN_LED_AI1, LOW);
    }
    return true;
}

void start_pdm() {
#ifdef ARDUINO
    static repeating_timer_t mst;
    add_repeating_timer_us(150, timer_callback, NULL, &mst);
#endif
}
#endif // USEQHARDWARE_1_0

// === maxiFilter Implementation ===

double maxiFilter::lopass(double input, double cutoff) {
    z = z + (input - z) * cutoff;
    return z;
}