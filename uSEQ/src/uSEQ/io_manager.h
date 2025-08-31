#ifndef USEQ_IO_MANAGER_H
#define USEQ_IO_MANAGER_H

#include "../utils/compiler_config.h"
#include "configure.h"
#include "../ports/IIo.h"
#include "../dsp/MedianFilter.h"
#include <cstdint>
#include <memory>

#ifdef ARDUINO
USEQ_SUPPRESS_EXTERNAL_WARNINGS_PUSH
#include <Arduino.h>
USEQ_SUPPRESS_EXTERNAL_WARNINGS_POP
#endif

// Forward declarations
class uSEQ;

// Simple low-pass filter (moved from global scope)
class maxiFilter {
private:
    double z = 0;
    double output = 0;

public:
    maxiFilter() {}
#ifdef ARDUINO
    double __force_inline lopass(double input, double cutoff);
#else
    double lopass(double input, double cutoff);
#endif
};

/**
 * IOManager - Encapsulates all hardware I/O operations for uSEQ
 * 
 * This class manages:
 * - Digital and analog inputs/outputs
 * - LED control and animations
 * - Pin configuration and setup
 * - Interrupt handling for inputs
 * - Platform abstraction (Arduino vs Desktop)
 */
class IOManager {
public:
    // Constructor/Destructor
    explicit IOManager(uSEQ* parent, IIo* io_adapter = nullptr);
    ~IOManager() = default;

    // === Initialization ===
    void init();
    void setup_io();
    
    // === Input Operations ===
    void update_inputs();
    void set_input_value(size_t index, double value);
    double get_input_value(size_t index) const;
    
    // Input interrupt handlers (static for Arduino interrupts)
    static void gpio_irq_gate1();
    static void gpio_irq_gate2();
    
    // === Output Operations ===
    // Analog/PWM outputs
    void analog_write_with_led(int output, CONTINUOUS_OUTPUT_VALUE_TYPE val);
    
    // Digital outputs
    void digital_write_with_led(int output, BINARY_OUTPUT_VALUE_TYPE val);
    
    // Serial stream outputs
    void serial_write(int output, SERIAL_OUTPUT_VALUE_TYPE val);
    
    // === LED Control ===
    void led_animation();
    void set_led(uint8_t led_pin, bool state);
    void set_led_pwm(uint8_t led_pin, int value);
    
    // === Platform-specific operations ===
#ifdef ARDUINO
    void setup_pio_pwm();
    void pio_pwm_set_level(PIO pio, uint sm, uint32_t level);
    void pio_pwm_set_period(PIO pio, uint sm, uint32_t period);
#endif

#ifdef USEQHARDWARE_0_2
    void setup_rotary_encoder();
    void read_rotary_encoders();
#endif

    // === Static instance management (for interrupt callbacks) ===
    static void set_instance(IOManager* instance) { s_instance = instance; }
    static IOManager* get_instance() { return s_instance; }

private:
    // Parent uSEQ instance
    uSEQ* m_parent;
    
    // Optional I/O adapter for testing/simulation
    IIo* m_io_adapter;
    
    // Static instance for interrupt callbacks
    static IOManager* s_instance;
    
    // Input value storage
    double m_input_vals[14];
    
    // Input filtering
    MedianFilter m_filter1;
    MedianFilter m_filter2;
    maxiFilter m_cv_filters[2];
    
    // === Setup functions ===
    void setup_outputs();
    void setup_analog_outputs();
    void setup_digital_outputs();
    void setup_inputs();
    void setup_digital_inputs();
    void setup_analog_inputs();
    void setup_switches();
    void setup_leds();
    
    // === Helper functions ===
    int get_analog_out_pin(int output) const;
    int get_analog_out_led_pin(int output) const;
    int get_digital_out_pin(int output) const;
    int get_digital_out_led_pin(int output) const;
    
    // === Platform-specific helpers ===
#ifdef MUSICTHING
    void read_musicthing_inputs();
#endif

#ifdef USEQHARDWARE_1_0
    void read_hardware_1_0_inputs();
#endif

#ifdef USEQHARDWARE_0_2
    int8_t read_rotary();
    // Rotary encoder state
    uint8_t m_prev_next_code = 0;
    uint16_t m_store = 0;
#endif

#ifdef MIDIOUT
    void setup_midi();
#endif
};

// Helper functions for pin mapping
#ifdef ARDUINO
int analog_out_pin(int out);
int analog_out_LED_pin(int out);
int digital_out_pin(int out);
int digital_out_LED_pin(int out);
#endif

// PDM-related globals (hardware 1.0 specific)
#ifdef USEQHARDWARE_1_0
extern float pdm_y;
extern float pdm_err;
extern float pdm_w;
bool timer_callback(repeating_timer_t* rt);
void start_pdm();
#endif

#endif // USEQ_IO_MANAGER_H