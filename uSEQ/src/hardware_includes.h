#ifndef HARDWARE_INCLUDES_H_
#define HARDWARE_INCLUDES_H_

#ifndef ARDUINO
#include <chrono>
#endif

#ifdef ARDUINO
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/pio.h"
#include "pico/bootrom.h"
#include "uSEQ/piopwm.h"

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

#else
// Desktop builds - Arduino stubs and hardware function stubs

#include <cstdint>
#include <iostream>
#include <thread>
#include <chrono>

// Arduino core types and constants
typedef uint8_t byte;
typedef bool boolean;
// Note: uint is already defined by glibc, don't redefine it

// Pin modes
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define OUTPUT_2MA 3

// Digital values
#define HIGH 1
#define LOW 0

// Arduino interrupt constants
#define CHANGE 1
#define RISING 2
#define FALLING 3

// GPIO constants for Pico SDK stubs
#define GPIO_SLEW_RATE_SLOW 0
#define GPIO_SLEW_RATE_FAST 1

// PIO type stub
typedef void* PIO;

// Flash memory constants for desktop
#ifndef XIP_BASE
#define XIP_BASE ((uintptr_t)0x10000000)
#endif

#ifndef PICO_FLASH_SIZE_BYTES  
#define PICO_FLASH_SIZE_BYTES (2 * 1024 * 1024)
#endif

#ifndef FLASH_SECTOR_SIZE
#define FLASH_SECTOR_SIZE (4 * 1024)
#endif

#ifndef FLASH_PAGE_SIZE
#define FLASH_PAGE_SIZE 256
#endif

#ifndef PPB_BASE
#define PPB_BASE 0xE0000000
#endif

// MUX pin constants - only define if not already defined by pinmap.h
#ifndef MUX_LOGIC_A
#define MUX_LOGIC_A 0
#endif
#ifndef MUX_IN_1  
#define MUX_IN_1 1
#endif

// Serial class stub
class SerialStub {
public:
    void begin(int baud) {}
    void write(uint8_t data) {}
    void write(const char* str) {}
    void print(const char* str) { std::cout << str; }
    void println(const char* str) { std::cout << str << std::endl; }
    void print(int val) { std::cout << val; }
    void println(int val) { std::cout << val << std::endl; }
    void print(double val) { std::cout << val; }
    void println(double val) { std::cout << val << std::endl; }
    bool available() { return false; }
    int read() { return -1; }
    void flush() {}
};

// PIO program structure stub
struct pwm_program_struct {
    // Stub structure for desktop
};

// Extern declarations for PIO instances
extern PIO pio0;
extern PIO pio1;
extern const pwm_program_struct pwm_program;
extern SerialStub Serial;

// Arduino core function stubs
inline void pinMode(int pin, int mode) {}
inline void digitalWrite(int pin, int value) {}
inline int digitalRead(int pin) { return LOW; }
inline int analogRead(int pin) { return 2048; }
inline void analogWrite(int pin, int value) {}
inline void delay(unsigned long ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
inline void delayMicroseconds(unsigned int us) { std::this_thread::sleep_for(std::chrono::microseconds(us)); }

// Arduino analog and PWM function stubs
inline void analogWriteFreq(uint32_t freq) {}
inline void analogWriteResolution(int bits) {}
inline void analogReadResolution(int bits) {}

// Arduino interrupt function stubs
inline int digitalPinToInterrupt(int pin) { return pin; }
inline void attachInterrupt(int interrupt, void (*isr)(void), int mode) {}

// Pico SDK GPIO function stubs
inline void gpio_set_slew_rate(int pin, int rate) {}
inline void gpio_set_drive_strength(int pin, int strength) {}
inline void gpio_init(int pin) {}
inline void gpio_set_dir(int pin, bool out) {}
inline void gpio_put(int pin, bool value) {}
inline bool gpio_get(int pin) { return false; }

// RP2040 PIO function stubs
inline uint pio_add_program(PIO pio, const void* program) { return 0; }
inline void pwm_program_init(PIO pio, uint sm, uint offset, uint pin) {}
inline void pio_pwm_set_level(PIO pio, uint sm, uint32_t level) {}
inline void pio_pwm_set_period(PIO pio, uint sm, uint32_t period) {}

// Bootrom function stubs
inline void reset_usb_boot(uint32_t gpio_activity_pin_mask, uint32_t disable_interface_mask) {
    std::cout << "Bootloader reset requested (desktop stub)" << std::endl;
}

// Flash memory function stubs
inline uint32_t save_and_disable_interrupts() { return 0; }
inline void restore_interrupts(uint32_t state) {}
inline void flash_range_erase(uint32_t flash_offs, size_t count) {}
inline void flash_range_program(uint32_t flash_offs, const uint8_t* data, size_t count) {}

// I2C function stubs
inline int i2c_write_blocking(void* i2c, uint8_t addr, const uint8_t* src, size_t len, bool nostop) { return len; }
inline int i2c_read_blocking(void* i2c, uint8_t addr, uint8_t* dst, size_t len, bool nostop) {
    for (size_t i = 0; i < len; i++) dst[i] = 0;
    return len;
}

// Hardware-specific function stubs
inline void setup_leds() {}
inline void start_pdm() {}
inline void setup_i2cHOST() {}
inline void i2cWriteString(int expander, String msg) {}

// Pin mapping stubs (return dummy values)
inline int analog_out_LED_pin(int out) { return 0; }
inline int analog_out_pin(int out) { return 0; }
inline int digital_out_LED_pin(int out) { return 0; }
inline int digital_out_pin(int out) { return 0; }

#endif

#endif // HARDWARE_INCLUDES_H_
