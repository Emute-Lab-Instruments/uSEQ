#include "hardware_io.h"

// ── Pin definitions and hardware constants ──────────────────────────────────
// We define pin numbers directly rather than including pinmap.h to avoid
// ODR violations from the non-inline arrays declared there.  These values
// are exact copies of the constants in pinmap.h — if pinmap.h changes,
// these must be updated in lockstep.

#ifdef ARDUINO
#include <Arduino.h>
#include <SPI.h>
#include <hardware/gpio.h>
#include <hardware/pio.h>
#include "utils/piopwm.h"
#include "utils/ResponsiveAnalogRead.h"
#include "../utils/serial_message.h"
#include <algorithm>

// PIO PWM helpers (same as in the old io_manager)
static inline void hw_pio_pwm_set_level(PIO pio, uint sm, uint32_t level) {
    pio_sm_put_blocking(pio, sm, level);
}
static inline void hw_pio_pwm_set_period(PIO pio, uint sm, uint32_t period) {
    pio_sm_set_enabled(pio, sm, false);
    pio_sm_put_blocking(pio, sm, period);
    pio_sm_exec(pio, sm, pio_encode_pull(false, false));
    pio_sm_exec(pio, sm, pio_encode_out(pio_isr, 32));
    pio_sm_set_enabled(pio, sm, true);
}

// ── Music Thing pin map ──────────────────────────────────────────────────
#ifdef MUSICTHING

// Digital gate inputs
static constexpr int PIN_I1 = 2;
static constexpr int PIN_I2 = 3;

// LED pins
static constexpr int LED_AUDIO_L = 10;
static constexpr int LED_AUDIO_R = 11;
static constexpr int LED_A3      = 12;
static constexpr int LED_A4      = 13;
static constexpr int LED_D1      = 14;
static constexpr int LED_D2      = 15;

// Output pins: aL, aR, a3, a4, d1, d2
static constexpr int OUTPUT_PINS[]     = { -1, -1, 23, 22, 8, 9 };
static constexpr int OUTPUT_LED_PINS[] = { LED_AUDIO_L, LED_AUDIO_R,
                                           LED_A3, LED_A4,
                                           LED_D1, LED_D2 };

// Mux / analog
static constexpr int MUX_IN_1_PIN  = 28;
static constexpr int MUX_IN_2_PIN  = 29;
static constexpr int MUX_LOGIC_A_PIN = 24;
static constexpr int MUX_LOGIC_B_PIN = 25;
static constexpr int AUDIO_IN_L_PIN = 26;
static constexpr int AUDIO_IN_R_PIN = 27;

// DAC SPI
static constexpr int DAC_SCK_PIN = 18;
static constexpr int DAC_SDI_PIN = 19;
static constexpr int DAC_CS_PIN  = 21;

static constexpr int HW_NUM_CONTINUOUS = 4;
static constexpr int HW_NUM_BINARY     = 2;
static constexpr int HW_NUM_OUTPUTS    = 6;  // continuous + binary

// Inversion flags (Music Thing specific)
static constexpr bool INVERT_ANALOG  = true;
static constexpr bool INVERT_DIGITAL = true;

// MCP4822 SPI DAC — aL (channel 0) and aR (channel 1)
static SPISettings dac_spi_settings(20000000, MSBFIRST, SPI_MODE0);

static inline void dac_write(uint8_t channel, uint16_t value_12bit) {
    if (value_12bit > 4095) value_12bit = 4095;
    // MCP4822: [channel | 0 | gain=1x | active | 12-bit data]
    uint16_t cmd = 0x3000 | (value_12bit & 0x0FFF);
    if (channel & 1) cmd |= 0x8000;
    SPI.beginTransaction(dac_spi_settings);
    digitalWrite(DAC_CS_PIN, LOW);
    SPI.transfer16(cmd);
    digitalWrite(DAC_CS_PIN, HIGH);
    SPI.endTransaction();
}

// Input channels: 0-3 via MUX_IN_1, 4-5 via MUX_IN_2, 6-7 audio direct
static constexpr int NUM_RESPONSIVE_CHANNELS = 8;
static constexpr int OVERSAMPLE_COUNT = 4;

#endif // MUSICTHING

// ── Hardware v1.0 pin map ────────────────────────────────────────────────
#ifdef USEQHARDWARE_1_0

static constexpr int PIN_I1 = 8;
static constexpr int PIN_I2 = 9;

static constexpr int PIN_AI1 = 26;
static constexpr int PIN_AI2 = 27;

static constexpr int LED_I1  = 5;
static constexpr int LED_I2  = 4;
static constexpr int LED_AI1 = 25;
static constexpr int LED_AI2 = 24;
static constexpr int LED_A1  = 3;
static constexpr int LED_A2  = 2;
static constexpr int LED_A3  = 11;
static constexpr int LED_D1  = 12;
static constexpr int LED_D2  = 13;
static constexpr int LED_D3  = 22;

static constexpr int OUTPUT_PINS[]     = { 21, 20, 19, 18, 17, 16 };
static constexpr int OUTPUT_LED_PINS[] = { LED_A1, LED_A2, LED_A3,
                                           LED_D1, LED_D2, LED_D3 };

static constexpr int PIN_SWITCH_M1 = 10;
static constexpr int PIN_SWITCH_T1 = 14;
static constexpr int PIN_SWITCH_T2 = 23;

static constexpr int HW_NUM_CONTINUOUS = 3;
static constexpr int HW_NUM_BINARY     = 3;
static constexpr int HW_NUM_OUTPUTS    = 6;

static constexpr bool INVERT_ANALOG  = false;
static constexpr bool INVERT_DIGITAL = false;

#endif // USEQHARDWARE_1_0

// ── Hardware v0.2 pin map ────────────────────────────────────────────────
#ifdef USEQHARDWARE_0_2

static constexpr int PIN_I1 = 8;
static constexpr int PIN_I2 = 9;

static constexpr int LED_I1 = 5;
static constexpr int LED_I2 = 4;
static constexpr int LED_A1 = 3;
static constexpr int LED_A2 = 2;
static constexpr int LED_D1 = 28;
static constexpr int LED_D2 = 27;
static constexpr int LED_D3 = 26;
static constexpr int LED_D4 = 22;

static constexpr int OUTPUT_PINS[]     = { 21, 20, 19, 18, 17, 16 };
static constexpr int OUTPUT_LED_PINS[] = { LED_A1, LED_A2,
                                           LED_D1, LED_D2,
                                           LED_D3, LED_D4 };

static constexpr int PIN_SWITCH_M1 = 10;
static constexpr int PIN_SWITCH_M2 = 11;
static constexpr int PIN_SWITCH_T1 = 14;
static constexpr int PIN_SWITCH_T2 = 15;
static constexpr int PIN_SWITCH_R1 = 7;
static constexpr int PIN_ROTARYENC_A = 13;
static constexpr int PIN_ROTARYENC_B = 12;

static constexpr int HW_NUM_CONTINUOUS = 2;
static constexpr int HW_NUM_BINARY     = 4;
static constexpr int HW_NUM_OUTPUTS    = 6;

static constexpr bool INVERT_ANALOG  = false;
static constexpr bool INVERT_DIGITAL = false;

#endif // USEQHARDWARE_0_2

// ── Expander v0.1 pin map ────────────────────────────────────────────────
#ifdef USEQHARDWARE_EXPANDER_OUT_0_1

static constexpr int OUTPUT_PINS[]     = { 13, 14, 10, 11, 8, 7, 5, 3 };
static constexpr int OUTPUT_LED_PINS[] = { 15, 20, 17, 12, 9, 6, 2, 0 };

static constexpr int HW_NUM_CONTINUOUS = 8;
static constexpr int HW_NUM_BINARY     = 0;
static constexpr int HW_NUM_OUTPUTS    = 8;

static constexpr bool INVERT_ANALOG  = false;
static constexpr bool INVERT_DIGITAL = false;

#endif // USEQHARDWARE_EXPANDER_OUT_0_1

// ── PWM constants ────────────────────────────────────────────────────────
static constexpr int    MAX_PWM_I = 2047;
static constexpr double MAX_PWM   = 2047.0;
static constexpr double RECP_4096 = 1.0 / 4096.0;
static constexpr double RECP_2048 = 1.0 / 2048.0;

#endif // ARDUINO

// ── Input index convention ──────────────────────────────────────────────
// Matches useqInputNames from configure.h:
//   0=I1, 1=I2, 2=M1, 3=M2, 4=T1, 5=T2, 6=RS1, 7=R1,
//   8=AI1, 9=AI2, 10=MAINKNOB, 11=XKNOB, 12=YKNOB, 13=ZSWITCH, 14=AUDIO_L, 15=AUDIO_R
enum InputIndex : uint8_t {
    INP_I1       = 0,
    INP_I2       = 1,
    INP_M1       = 2,
    INP_M2       = 3,
    INP_T1       = 4,
    INP_T2       = 5,
    INP_RS1      = 6,
    INP_R1       = 7,
    INP_AI1      = 8,
    INP_AI2      = 9,
    INP_MAINKNOB = 10,
    INP_XKNOB    = 11,
    INP_YKNOB    = 12,
    INP_ZSWITCH  = 13,
    INP_AUDIO_L  = 14,
    INP_AUDIO_R  = 15,
};

// ── Simple smoothing filter ─────────────────────────────────────────────
// Lightweight EMA (exponential moving average) replacement for
// ResponsiveAnalogRead.  Good enough for the initial firmware scaffold;
// can be upgraded to the full ResponsiveAnalogRead library later.

namespace {

struct SmoothInput {
    float smooth_value  = 0.0f;
    int   raw_value     = 0;
    int   output_value  = 0;
    bool  initialized   = false;

    static constexpr float ALPHA = 0.15f;  // smoothing factor

    void update(int raw) {
        raw_value = raw;
        if (!initialized) {
            smooth_value = static_cast<float>(raw);
            initialized  = true;
        } else {
            smooth_value += (static_cast<float>(raw) - smooth_value) * ALPHA;
        }
        output_value = static_cast<int>(smooth_value);
    }

    int value() const { return output_value; }
};

} // anonymous namespace

namespace firmware {

// ── File-scope state for input filtering ────────────────────────────────
#if defined(ARDUINO) && defined(MUSICTHING)
static ResponsiveAnalogRead s_responsive[NUM_RESPONSIVE_CHANNELS];

static int oversample_adc(int pin) {
    int sum = 0;
    for (int i = 0; i < OVERSAMPLE_COUNT; i++) {
        sum += analogRead(pin);
        delayMicroseconds(1);
    }
    return sum / OVERSAMPLE_COUNT;
}
#endif

#if defined(ARDUINO) && defined(USEQHARDWARE_1_0)

// Median-of-51 filter matching the original production firmware.
// Fixed-size to avoid heap allocation on embedded.
struct MedianFilter51 {
    static constexpr int SIZE   = 51;
    static constexpr int CENTER = SIZE / 2;

    double buf[SIZE] = {};
    double tmp[SIZE] = {};
    int    idx   = 0;
    int    count = 0;

    double process(double val) {
        buf[idx] = val;
        idx = (idx + 1) % SIZE;
        if (count < SIZE) count++;
        int n = count;
        for (int i = 0; i < n; i++) tmp[i] = buf[i];
        int center = n / 2;
        std::nth_element(tmp, tmp + center, tmp + n);
        return tmp[center];
    }
};

static MedianFilter51 s_ai1_filter;
static MedianFilter51 s_ai2_filter;

// PDM driver for LED_AI1 — runs on a 150µs hardware repeating timer,
// independent of the main loop.  pdm_w is updated in read_inputs().
static float pdm_y   = 0;
static float pdm_err = 0;
static volatile float pdm_w = 0;

static bool pdm_timer_callback(repeating_timer_t*) {
    pdm_y   = pdm_w > pdm_err ? 1 : 0;
    pdm_err = pdm_y - pdm_w + pdm_err;
    digitalWrite(LED_AI1, pdm_y == 1 ? HIGH : LOW);
    return true;
}

static void start_pdm_timer() {
    static repeating_timer_t pdm_timer;
    add_repeating_timer_us(150, pdm_timer_callback, NULL, &pdm_timer);
}

// Gate input ISR state — latches rising edges so short pulses aren't missed.
static volatile uint8_t s_gate1_state   = 0;
static volatile uint8_t s_gate2_state   = 0;
static volatile uint8_t s_gate1_latched = 0;
static volatile uint8_t s_gate2_latched = 0;

static void gate1_isr() {
    uint8_t val = 1 - digitalRead(PIN_I1);
    s_gate1_state = val;
    if (val) s_gate1_latched = 1;
    digitalWrite(LED_I1, val);
}

static void gate2_isr() {
    uint8_t val = 1 - digitalRead(PIN_I2);
    s_gate2_state = val;
    if (val) s_gate2_latched = 1;
    digitalWrite(LED_I2, val);
}

#endif

#if defined(ARDUINO) && defined(USEQHARDWARE_0_2)
// Rotary encoder state
static uint8_t s_rotary_prev_next_code = 0;
static uint16_t s_rotary_store = 0;

static int8_t read_rotary() {
    static const int8_t rot_enc_table[] = {
        0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0
    };
    s_rotary_prev_next_code <<= 2;
    if (digitalRead(PIN_ROTARYENC_B))
        s_rotary_prev_next_code |= 0x02;
    if (digitalRead(PIN_ROTARYENC_A))
        s_rotary_prev_next_code |= 0x01;
    s_rotary_prev_next_code &= 0x0f;

    if (rot_enc_table[s_rotary_prev_next_code]) {
        s_rotary_store <<= 4;
        s_rotary_store |= s_rotary_prev_next_code;
        if ((s_rotary_store & 0xff) == 0x2b) return -1;
        if ((s_rotary_store & 0xff) == 0x17) return  1;
    }
    return 0;
}
#endif

// ═══════════════════════════════════════════════════════════════════════════
// init()
// ═══════════════════════════════════════════════════════════════════════════

void HardwareIO::init()
{
#ifdef ARDUINO

    // ── Configure output counts per variant ──────────────────────────────
#if defined(MUSICTHING)
    num_continuous_outs = 4;
    num_binary_outs     = 2;
    num_serial_outs     = 9;
    num_hw_inputs       = 16;  // I1, I2, M1, M2, T1, T2, RS1, R1, AI1, AI2, MAIN, X, Y, Z, AUDIO_L, AUDIO_R
#elif defined(USEQHARDWARE_1_0)
    num_continuous_outs = 3;
    num_binary_outs     = 3;
    num_serial_outs     = 9;
    num_hw_inputs       = 10;  // I1, I2, M1, -, T1, -, -, -, AI1, AI2
#elif defined(USEQHARDWARE_0_2)
    num_continuous_outs = 2;
    num_binary_outs     = 4;
    num_serial_outs     = 9;
    num_hw_inputs       = 8;   // I1, I2, M1, M2, T1, T2, RS1, R1
#elif defined(USEQHARDWARE_EXPANDER_OUT_0_1)
    num_continuous_outs = 8;
    num_binary_outs     = 0;
    num_serial_outs     = 9;
    num_hw_inputs       = 0;
#else
    num_continuous_outs = 3;
    num_binary_outs     = 3;
    num_serial_outs     = 9;
    num_hw_inputs       = 0;
#endif

    // ── Zero state ───────────────────────────────────────────────────────
    for (size_t i = 0; i < MAX_HW_INPUTS; i++)  inputs[i]  = 0.0;
    for (size_t i = 0; i < sig::MAX_OUTPUTS; i++) outputs[i] = 0.0;

    // ── Configure output pin modes ──────────────────────────────────────
    for (int i = 0; i < HW_NUM_OUTPUTS; i++) {
        if (OUTPUT_PINS[i] >= 0)
            pinMode(OUTPUT_PINS[i], OUTPUT_2MA);
    }

    // ── Analog output setup (PWM freq / resolution) ─────────────────────
    analogWriteFreq(100000);
    analogWriteResolution(11);

    // ── LED PIO PWM setup ───────────────────────────────────────────────
#ifdef ENABLE_LED_CONTROL
#if !defined(USEQHARDWARE_EXPANDER_OUT_0_1) && !defined(MUSICTHING)
    // PIO PWM for LED outputs (MUSICTHING and Expander use analogWrite instead)
    uint offset  = pio_add_program(pio0, &pwm_program);
    uint offset2 = pio_add_program(pio1, &pwm_program);
    for (int i = 0; i < HW_NUM_CONTINUOUS; i++) {
        auto pio_inst = (i < 4) ? pio0 : pio1;
        uint pio_off  = (i < 4) ? offset : offset2;
        auto sm       = static_cast<uint>(i % 4);
        pwm_program_init(pio_inst, sm, pio_off, OUTPUT_LED_PINS[i]);
        hw_pio_pwm_set_period(pio_inst, sm, (1u << 11) - 1);
    }
#endif
#endif // ENABLE_LED_CONTROL

    // ── Configure input pin modes ───────────────────────────────────────
#if defined(MUSICTHING)
    pinMode(PIN_I1, INPUT_PULLUP);
    pinMode(PIN_I2, INPUT_PULLUP);

    // SPI DAC init for aL/aR
    pinMode(DAC_CS_PIN, OUTPUT);
    digitalWrite(DAC_CS_PIN, HIGH);
    SPI.begin();
    dac_write(0, 0);
    dac_write(1, 0);

    analogReadResolution(12);
    pinMode(MUX_IN_1_PIN, INPUT);
    pinMode(MUX_IN_2_PIN, INPUT);
    pinMode(AUDIO_IN_L_PIN, INPUT);
    pinMode(AUDIO_IN_R_PIN, INPUT);
    pinMode(MUX_LOGIC_A_PIN, OUTPUT);
    pinMode(MUX_LOGIC_B_PIN, OUTPUT);

#elif defined(USEQHARDWARE_1_0)
#ifdef ENABLE_DIGITAL_IO
    pinMode(PIN_I1, INPUT_PULLUP);
    pinMode(PIN_I2, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_I1), gate1_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_I2), gate2_isr, CHANGE);
#endif
#ifdef ENABLE_ANALOG_INPUTS
    analogReadResolution(11);
    pinMode(PIN_AI1, INPUT);
    pinMode(PIN_AI2, INPUT);
#endif
    pinMode(PIN_SWITCH_M1, INPUT_PULLUP);
    pinMode(PIN_SWITCH_T1, INPUT_PULLUP);
    pinMode(PIN_SWITCH_T2, INPUT_PULLUP);

#elif defined(USEQHARDWARE_0_2)
#ifdef ENABLE_DIGITAL_IO
    pinMode(PIN_I1, INPUT_PULLUP);
    pinMode(PIN_I2, INPUT_PULLUP);
#endif
    pinMode(PIN_SWITCH_M1, INPUT_PULLUP);
    pinMode(PIN_SWITCH_M2, INPUT_PULLUP);
    pinMode(PIN_SWITCH_T1, INPUT_PULLUP);
    pinMode(PIN_SWITCH_T2, INPUT_PULLUP);
#ifdef ENABLE_ENCODER_INPUT
    pinMode(PIN_SWITCH_R1, INPUT_PULLUP);
    pinMode(PIN_ROTARYENC_A, INPUT_PULLUP);
    pinMode(PIN_ROTARYENC_B, INPUT_PULLUP);
#endif
#endif

    // ── Configure LED pin modes ─────────────────────────────────────────
    // On variants where continuous-output LEDs are PIO-driven (v1.0, v0.2),
    // pwm_program_init above already set the pin function to PIO via
    // pio_gpio_init. Calling pinMode here would call gpio_init, which resets
    // the function back to SIO and steals the pin from PIO — leaving the
    // analog LEDs stuck low. Skip those indices; only configure non-PIO LED
    // pins (digital LEDs on v1.0/v0.2, all LEDs on MUSICTHING/Expander).
#ifdef ENABLE_LED_CONTROL
    for (int i = 0; i < HW_NUM_OUTPUTS; i++) {
#if !defined(USEQHARDWARE_EXPANDER_OUT_0_1) && !defined(MUSICTHING)
        if (i < HW_NUM_CONTINUOUS) {
            // PIO-managed; only adjust slew rate (function stays PIO).
            gpio_set_slew_rate(OUTPUT_LED_PINS[i], GPIO_SLEW_RATE_SLOW);
            continue;
        }
#endif
        pinMode(OUTPUT_LED_PINS[i], OUTPUT_2MA);
        gpio_set_slew_rate(OUTPUT_LED_PINS[i], GPIO_SLEW_RATE_SLOW);
    }

#if defined(USEQHARDWARE_1_0)
    pinMode(LED_AI1, OUTPUT_2MA);
    pinMode(LED_AI2, OUTPUT_2MA);
    pinMode(LED_I1, OUTPUT_2MA);
    pinMode(LED_I2, OUTPUT_2MA);
    start_pdm_timer();
#endif
#endif // ENABLE_LED_CONTROL

#else
    // ── Desktop build — set counts only ──────────────────────────────────
    num_continuous_outs = 3;
    num_binary_outs     = 3;
    num_serial_outs     = 9;
    num_hw_inputs       = 0;

    for (size_t i = 0; i < MAX_HW_INPUTS; i++)  inputs[i]  = 0.0;
    for (size_t i = 0; i < sig::MAX_OUTPUTS; i++) outputs[i] = 0.0;
#endif // ARDUINO
}

// ═══════════════════════════════════════════════════════════════════════════
// read_inputs()
// ═══════════════════════════════════════════════════════════════════════════

void HardwareIO::read_inputs()
{
#ifdef ARDUINO

#if defined(MUSICTHING)
    // ── Digital gate inputs ──────────────────────────────────────────────
    inputs[INP_I1] = 1.0 - digitalRead(PIN_I1);
    inputs[INP_I2] = 1.0 - digitalRead(PIN_I2);

    // ── Mux-based analog inputs with oversampling + smoothing ────────────
    constexpr size_t muxdelay = 8;

    // Audio inputs (always available, direct)
    s_responsive[6].update(analogRead(AUDIO_IN_L_PIN));
    s_responsive[7].update(analogRead(AUDIO_IN_R_PIN));

    // Mux ch 0: Main knob + CV1
    digitalWrite(MUX_LOGIC_A_PIN, 0);
    digitalWrite(MUX_LOGIC_B_PIN, 0);
    delayMicroseconds(muxdelay);
    s_responsive[0].update(oversample_adc(MUX_IN_1_PIN));
    s_responsive[4].update(4095 - oversample_adc(MUX_IN_2_PIN));

    // Mux ch 1: Y knob
    digitalWrite(MUX_LOGIC_A_PIN, 0);
    digitalWrite(MUX_LOGIC_B_PIN, 1);
    delayMicroseconds(muxdelay);
    s_responsive[1].update(oversample_adc(MUX_IN_1_PIN));

    // Mux ch 2: X knob + CV2
    digitalWrite(MUX_LOGIC_A_PIN, 1);
    digitalWrite(MUX_LOGIC_B_PIN, 0);
    delayMicroseconds(muxdelay);
    s_responsive[2].update(oversample_adc(MUX_IN_1_PIN));
    s_responsive[5].update(4095 - oversample_adc(MUX_IN_2_PIN));

    // Mux ch 3: Switch
    digitalWrite(MUX_LOGIC_A_PIN, 1);
    digitalWrite(MUX_LOGIC_B_PIN, 1);
    delayMicroseconds(muxdelay);
    s_responsive[3].update(oversample_adc(MUX_IN_1_PIN));

    // ── Normalize to [0,1] ───────────────────────────────────────────────
    inputs[INP_MAINKNOB] = s_responsive[0].getValue() * RECP_4096;
    inputs[INP_YKNOB]    = s_responsive[1].getValue() * RECP_4096;
    inputs[INP_XKNOB]    = s_responsive[2].getValue() * RECP_4096;
    inputs[INP_AI1]      = s_responsive[4].getValue() * RECP_4096;
    inputs[INP_AI2]      = s_responsive[5].getValue() * RECP_4096;

    // Audio direct inputs
    inputs[INP_AUDIO_L] = s_responsive[6].getValue() * RECP_4096;
    inputs[INP_AUDIO_R] = s_responsive[7].getValue() * RECP_4096;

    // Switch: threshold to 0/1/2
    int sw = s_responsive[3].getValue();
    if (sw < 100)       inputs[INP_ZSWITCH] = 0.0;
    else if (sw > 3500) inputs[INP_ZSWITCH] = 2.0;
    else                inputs[INP_ZSWITCH] = 1.0;

#elif defined(USEQHARDWARE_1_0)
    // ── Digital gate inputs (interrupt-driven with edge latching) ────────
#ifdef ENABLE_DIGITAL_IO
    {
        uint8_t g1 = s_gate1_state;
        if (s_gate1_latched) { g1 = 1; s_gate1_latched = 0; }
        inputs[INP_I1] = static_cast<double>(g1);

        uint8_t g2 = s_gate2_state;
        if (s_gate2_latched) { g2 = 1; s_gate2_latched = 0; }
        inputs[INP_I2] = static_cast<double>(g2);
    }
#endif

    // ── Toggle switch (3-way) ────────────────────────────────────────────
    int ts_a = 1 - digitalRead(PIN_SWITCH_T1);
    int ts_b = 1 - digitalRead(PIN_SWITCH_T2);
    if (ts_a == 0 && ts_b == 0)  inputs[INP_T1] = 1.0;
    else if (ts_a == 1)          inputs[INP_T1] = 2.0;
    else                         inputs[INP_T1] = 0.0;

    // ── Momentary ────────────────────────────────────────────────────────
    inputs[INP_M1] = 1.0 - digitalRead(PIN_SWITCH_M1);

    // ── Analog inputs with median filter ─────────────────────────────────
#ifdef ENABLE_ANALOG_INPUTS
    int raw_ai1 = analogRead(PIN_AI1);
    int raw_ai2 = analogRead(PIN_AI2);
    inputs[INP_AI1] = s_ai1_filter.process(raw_ai1 * RECP_2048);
    inputs[INP_AI2] = s_ai2_filter.process(raw_ai2 * RECP_2048);

#ifdef ENABLE_LED_CONTROL
    int ai1_sq = (raw_ai1 * raw_ai1) >> 11;
    pdm_w = ai1_sq / 2048.0f;
    int ai2_sq = (raw_ai2 * raw_ai2) >> 11;
    analogWrite(LED_AI2, ai2_sq);
#endif
#endif // ENABLE_ANALOG_INPUTS

#elif defined(USEQHARDWARE_0_2)
    // ── Digital gate inputs ──────────────────────────────────────────────
#ifdef ENABLE_DIGITAL_IO
    inputs[INP_I1] = 1.0 - digitalRead(PIN_I1);
    inputs[INP_I2] = 1.0 - digitalRead(PIN_I2);
#endif

    // ── Switches ─────────────────────────────────────────────────────────
    inputs[INP_T1]  = 1.0 - digitalRead(PIN_SWITCH_T1);
    inputs[INP_M1]  = 1.0 - digitalRead(PIN_SWITCH_M1);
    inputs[INP_M2]  = 1.0 - digitalRead(PIN_SWITCH_M2);
    inputs[INP_T2]  = 1.0 - digitalRead(PIN_SWITCH_T2);

    // ── Rotary encoder ───────────────────────────────────────────────────
#ifdef ENABLE_ENCODER_INPUT
    inputs[INP_RS1] = 1.0 - digitalRead(PIN_SWITCH_R1);
    int8_t rot = read_rotary();
    if (rot) {
        inputs[INP_R1] += rot;
    }
#endif
#endif

#endif // ARDUINO
}

// ═══════════════════════════════════════════════════════════════════════════
// write_outputs()
// ═══════════════════════════════════════════════════════════════════════════

void HardwareIO::write_outputs()
{
#ifdef ARDUINO

    // ── Continuous outputs (indices 0 .. num_continuous_outs-1) ──────────
    for (int i = 0; i < num_continuous_outs; i++) {
        double val = outputs[i];

        // Clamp to [0,1]
        if (val < 0.0) val = 0.0;
        if (val > 1.0) val = 1.0;

        int scaled = static_cast<int>(val * MAX_PWM);
        if (scaled > MAX_PWM_I) scaled = MAX_PWM_I;
        if (scaled < 0) scaled = 0;

        if constexpr (INVERT_ANALOG) {
            scaled = MAX_PWM_I - scaled;
        }

#ifdef ENABLE_LED_CONTROL
        // LED (exponential curve)
        // For inverted hardware, LED shows the logical value (what the user
        // specified), not the inverted physical value. Use the un-inverted
        // output value for the brightness curve.
        int led_val;
        if constexpr (INVERT_ANALOG) {
            // Use original output value, not inverted scaled
            led_val = static_cast<int>(val * MAX_PWM);
            if (led_val > MAX_PWM_I) led_val = MAX_PWM_I;
        } else {
            led_val = scaled;
        }
        led_val = (led_val * led_val) >> 11;

        // Write LED
#if defined(MUSICTHING) || defined(USEQHARDWARE_EXPANDER_OUT_0_1)
        analogWrite(OUTPUT_LED_PINS[i], led_val);
#else
        // Standard hardware (v1.0, v0.2): use PIO PWM
        PIO pio_inst = (i < 4) ? pio0 : pio1;
        uint sm = static_cast<uint>(i % 4);
        hw_pio_pwm_set_level(pio_inst, sm, static_cast<uint32_t>(led_val));
#endif
#endif // ENABLE_LED_CONTROL

        // Write output pin
#if defined(MUSICTHING)
        if (i < 2) {
            // aL/aR: SPI DAC (12-bit, scale from 11-bit PWM range)
            uint16_t dac_val = static_cast<uint16_t>((scaled * 4095) / MAX_PWM_I);
            dac_write(static_cast<uint8_t>(i), dac_val);
        } else {
            analogWrite(OUTPUT_PINS[i], scaled);
        }
#else
        analogWrite(OUTPUT_PINS[i], scaled);
#endif
    }

    // ── Binary outputs ────────────────────────────────────────────────────
    // Signal engine indices: d1=8, d2=9, ... (see graph_builder resolve_output_index)
    // Hardware pins: OUTPUT_PINS[num_continuous_outs + i]
#ifdef ENABLE_DIGITAL_IO
    for (int i = 0; i < num_binary_outs; i++) {
        double val  = outputs[8 + i];

        uint8_t dv = (val > 0.0) ? 1 : 0;
        if constexpr (INVERT_DIGITAL) {
            dv = 1 - dv;
        }

        int pin_idx = num_continuous_outs + i;
        if (pin_idx < HW_NUM_OUTPUTS) {
            digitalWrite(OUTPUT_PINS[pin_idx], dv);

#ifdef ENABLE_LED_CONTROL
            // LED proportional
            int led_val = static_cast<int>(val * MAX_PWM);
            if (led_val > MAX_PWM_I) led_val = MAX_PWM_I;
            if (led_val < 0) led_val = 0;
            analogWrite(OUTPUT_LED_PINS[pin_idx], led_val);
#endif
        }
    }
#endif // ENABLE_DIGITAL_IO

    // ── Serial outputs (indices starting at num_continuous_outs + num_binary_outs)
    // Serial outputs are virtual — they are sent over the serial wire, not
    // written to hardware pins.  This is handled by SerialProtocol, so
    // write_outputs() intentionally skips them.

#endif // ARDUINO
}

// ═══════════════════════════════════════════════════════════════════════════
// update_leds()
// ═══════════════════════════════════════════════════════════════════════════

void HardwareIO::update_leds()
{
#if defined(ARDUINO) && defined(ENABLE_LED_CONTROL)
    // v1.0: gate input LEDs are driven by ISR (gate1_isr/gate2_isr)
    // for instant response — no work needed here.
#endif // ARDUINO && ENABLE_LED_CONTROL
}

// ═══════════════════════════════════════════════════════════════════════════
// Boot LED sequence
// ═══════════════════════════════════════════════════════════════════════════
// Uses the output LEDs to signal boot state, since these are the only LEDs
// present on all hardware variants.  The onboard LED (LED_BUILTIN) is also
// used as a secondary indicator where available.

// Helper: set an output LED value using the correct peripheral.
// Continuous output LEDs (indices < HW_NUM_CONTINUOUS) on standard hardware
// use PIO PWM — analogWrite would steal the pin from PIO.
static inline void set_output_led(int i, int value)
{
#ifdef ARDUINO
#if !defined(USEQHARDWARE_EXPANDER_OUT_0_1) && !defined(MUSICTHING)
    if (i < HW_NUM_CONTINUOUS) {
        PIO pio_inst = (i < 4) ? pio0 : pio1;
        uint sm = static_cast<uint>(i % 4);
        hw_pio_pwm_set_level(pio_inst, sm, static_cast<uint32_t>(value));
    } else {
        analogWrite(OUTPUT_LED_PINS[i], value);
    }
#else
    analogWrite(OUTPUT_LED_PINS[i], value);
#endif
#else
    (void)i; (void)value;
#endif
}

void HardwareIO::boot_led_amber()
{
#ifdef ARDUINO
#ifdef ENABLE_LED_CONTROL
    for (int i = 0; i < HW_NUM_OUTPUTS; i++) {
        set_output_led(i, MAX_PWM_I / 2);
    }
#endif
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
#endif
}

void HardwareIO::boot_led_green()
{
#ifdef ARDUINO
#ifdef ENABLE_LED_CONTROL
    for (int i = 0; i < HW_NUM_OUTPUTS; i++) {
        set_output_led(i, 0);
    }
#endif
    digitalWrite(LED_BUILTIN, HIGH);
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);
#endif
}

void HardwareIO::boot_led_error_flash()
{
#ifdef ARDUINO
    for (int blink = 0; blink < 3; blink++) {
#ifdef ENABLE_LED_CONTROL
        for (int i = 0; i < HW_NUM_OUTPUTS; i++) {
            set_output_led(i, MAX_PWM_I);
        }
#endif
        digitalWrite(LED_BUILTIN, HIGH);
        delay(80);

#ifdef ENABLE_LED_CONTROL
        for (int i = 0; i < HW_NUM_OUTPUTS; i++) {
            set_output_led(i, 0);
        }
#endif
        digitalWrite(LED_BUILTIN, LOW);
        delay(80);
    }
#endif
}

// ═══════════════════════════════════════════════════════════════════════════
// boot_led_animation()
// ═══════════════════════════════════════════════════════════════════════════
// Variant-specific LED chase animation matching the original production
// firmware.  Runs early during boot as a visual "I'm alive" indicator.

void HardwareIO::boot_led_animation()
{
#ifdef ARDUINO
#ifdef ENABLE_LED_CONTROL

#if defined(USEQHARDWARE_1_0)
    int d = 30;
    for (int i = 0; i < 8; i++) {
        digitalWrite(LED_AI1, HIGH);
        delay(d);
        digitalWrite(LED_AI2, HIGH);
        delay(d);
        set_output_led(0, MAX_PWM_I);   // A1
        digitalWrite(LED_AI1, LOW);
        delay(d);
        set_output_led(1, MAX_PWM_I);   // A2
        digitalWrite(LED_AI2, LOW);
        delay(d);
        set_output_led(2, MAX_PWM_I);   // A3
        set_output_led(0, 0);           // A1 off
        delay(d);
        set_output_led(5, MAX_PWM_I);   // D3
        set_output_led(1, 0);           // A2 off
        delay(d);
        set_output_led(4, MAX_PWM_I);   // D2
        set_output_led(2, 0);           // A3 off
        delay(d);
        set_output_led(3, MAX_PWM_I);   // D1
        set_output_led(5, 0);           // D3 off
        delay(d);
        digitalWrite(LED_I2, HIGH);
        set_output_led(4, 0);           // D2 off
        delay(d);
        digitalWrite(LED_I1, HIGH);
        set_output_led(3, 0);           // D1 off
        delay(d);
        digitalWrite(LED_I2, LOW);
        delay(d);
        digitalWrite(LED_I1, LOW);
        delay(d);
        d -= 3;
    }

#elif defined(USEQHARDWARE_0_2)
    // v0.2 output_led_pins: A1=0, A2=1, D1=2, D2=3, D3=4, D4=5
    int d = 30;
    for (int i = 0; i < 8; i++) {
        digitalWrite(LED_I1, HIGH);
        delay(d);
        set_output_led(0, MAX_PWM_I);   // A1
        delay(d);
        set_output_led(2, MAX_PWM_I);   // D1
        digitalWrite(LED_I1, LOW);
        delay(d);
        set_output_led(4, MAX_PWM_I);   // D3
        set_output_led(0, 0);           // A1 off
        delay(d);
        set_output_led(5, MAX_PWM_I);   // D4
        set_output_led(2, 0);           // D1 off
        delay(d);
        set_output_led(3, MAX_PWM_I);   // D2
        set_output_led(4, 0);           // D3 off
        delay(d);
        set_output_led(1, MAX_PWM_I);   // A2
        set_output_led(5, 0);           // D4 off
        delay(d);
        digitalWrite(LED_I2, HIGH);
        set_output_led(3, 0);           // D2 off
        delay(d);
        set_output_led(1, 0);           // A2 off
        delay(d);
        digitalWrite(LED_I2, LOW);
        delay(d);
        d -= 3;
    }

#else
    // Fallback: amber glow
    for (int i = 0; i < HW_NUM_OUTPUTS; i++) {
        set_output_led(i, MAX_PWM_I / 2);
    }
#endif

#endif // ENABLE_LED_CONTROL

    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
#endif // ARDUINO
}

} // namespace firmware
