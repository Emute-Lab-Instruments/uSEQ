#ifndef HARDWARE_INCLUDES_H_
#define HARDWARE_INCLUDES_H_

#ifdef ARDUINO
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/pio.h"
#include "pico/bootrom.h"
#include "uSEQ/piopwm.h"
#endif

// Forward declarations for hardware functions
void setup_leds();
void start_pdm();
bool timer_callback(repeating_timer_t* mst);

// I2C functions
void setup_i2cHOST();
void i2cWriteString(int expander, String msg);

// Pin mapping functions
int analog_out_LED_pin(int out);
int analog_out_pin(int out);
int digital_out_LED_pin(int out);
int digital_out_pin(int out);

// PIO PWM functions
void pio_pwm_set_level(PIO pio, uint sm, uint32_t level);
void pio_pwm_set_period(PIO pio, uint sm, uint32_t period);

#endif // HARDWARE_INCLUDES_H_
