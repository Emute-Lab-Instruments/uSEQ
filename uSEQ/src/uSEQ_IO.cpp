#include "uSEQ_IO.h"
#include "uSEQ.h"
#include "utils/log.h"
#include "uSEQ_Modules.h"

#if USE_NEW_MODULES && USE_IO_MODULE
// IO utility functions - only defined if the module is enabled
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
// IO module implementations
void uSEQ::update_inputs()
{
#if USEQ_DEBUG
    debug("uSEQ::update_inputs");
#endif

    update_analog_inputs();
    update_digital_inputs();
    
#ifdef USEQHARDWARE_0_2
    read_rotary_encoders();
#endif
}

void uSEQ::update_analog_inputs()
{
#ifdef ANALOG_INPUTS
    // Define analog input pins for hardware v1.0
    const int useq_analog_in_pins[] = { USEQ_PIN_AI1, USEQ_PIN_AI2 };
    
    for (int i = 0; i < NUM_ANALOG_INS; i++)
    {
        int raw = analogRead(useq_analog_in_pins[i]);
        m_analog_inputs[i] = cvInFilter[i].lopass(raw, 0.5);
    }
#endif
}

void uSEQ::update_digital_inputs()
{
#ifdef USEQHARDWARE_1_0
    // Define digital input pins for hardware v1.0
    const int useq_digital_in_pins[] = { 
        USEQ_PIN_SWITCH_M1, 
        USEQ_PIN_SWITCH_T1, 
        USEQ_PIN_SWITCH_T2, 
        USEQ_PIN_I1
    };
#elif defined(USEQHARDWARE_0_2)
    // Define digital input pins for hardware v0.2
    const int useq_digital_in_pins[] = { 
        USEQ_PIN_SWITCH_M1, 
        USEQ_PIN_SWITCH_M2,
        USEQ_PIN_SWITCH_T1, 
        USEQ_PIN_SWITCH_T2 
    };
#else
    // Define a default set of pins
    const int useq_digital_in_pins[] = { 0, 0, 0, 0 };
#endif

    for (int i = 0; i < NUM_DIGITAL_INS; i++)
    {
        int pin = useq_digital_in_pins[i];
        if (pin > 0) {
            m_digital_inputs[i] = digitalRead(pin) == LOW ? 1 : 0; // Input pullup, so invert
        }
    }
}

void uSEQ::update_outs()
{
    update_continuous_outs();
    update_binary_outs();
    update_serial_outs();
}

void uSEQ::update_continuous_outs()
{
    for (int i = 0; i < NUM_CONTINUOUS_OUTS; i++)
    {
        // In the refactored version we use the m_continuous_vals directly
        // This is a transitional function that will be updated during 
        // full refactoring when m_continuous_outputs is fully implemented
        double value = 0;
        if (i < m_continuous_vals.size()) {
            value = m_continuous_vals[i];
        }
        analog_write_with_led(i+1, value);
    }
}

void uSEQ::update_binary_outs()
{
    for (int i = 0; i < NUM_BINARY_OUTS; i++)
    {
        // In the refactored version we use the m_binary_vals directly
        // This is a transitional function that will be updated during 
        // full refactoring when m_binary_outputs is fully implemented
        int value = 0;
        if (i < m_binary_vals.size()) {
            value = m_binary_vals[i];
        }
        digital_write_with_led(i+1, value);
    }
}

void uSEQ::update_serial_outs()
{
    for (int i = 0; i < NUM_SERIAL_OUTS; i++)
    {
        // In the refactored version we use the m_serial_vals directly
        // This is a transitional function that will be updated during 
        // full refactoring when m_serial_outputs is fully implemented
        double value = 0;
        if (i < m_serial_vals.size() && m_serial_vals[i].has_value()) {
            value = m_serial_vals[i].value();
        }
        serial_write(i+1, value);
    }
}

void uSEQ::analog_write_with_led(int out, CONTINUOUS_OUTPUT_VALUE_TYPE value)
{
    analogWrite(analog_out_pin(out), value);
    analogWrite(analog_out_LED_pin(out), value);
}

void uSEQ::digital_write_with_led(int out, BINARY_OUTPUT_VALUE_TYPE value)
{
    digitalWrite(digital_out_pin(out), value);
    digitalWrite(digital_out_LED_pin(out), value);
}

void uSEQ::serial_write(int out, SERIAL_OUTPUT_VALUE_TYPE value)
{
    // Implementation depends on serial protocol chosen
}

#ifdef USEQHARDWARE_0_2
// Rotary encoder implementation
static uint8_t prevNextCode = 0;
static uint16_t store = 0;

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
        if ((store & 0xff) == 0x2b)
            return -1;
        if ((store & 0xff) == 0x17)
            return 1;
    }
    return 0;
}

void uSEQ::read_rotary_encoders()
{
    int8_t rotary = read_rotary();
    if (rotary != 0)
    {
        // Handle rotary encoder input
        m_encoder_delta += rotary;
    }
}

void uSEQ::setup_rotary_encoder()
{
    pinMode(USEQ_PIN_ROTARYENC_A, INPUT);
    pinMode(USEQ_PIN_ROTARYENC_B, INPUT);
    pinMode(USEQ_PIN_ROTARYENC_SW, INPUT_PULLUP);
}
#endif

#endif // USE_NEW_MODULES && USE_IO_MODULE