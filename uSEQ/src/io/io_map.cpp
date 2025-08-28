// Implementation of the uSEQ IO mapping registry.

#include "io_map.h"

namespace useq {
namespace io {

static std::vector<int> s_cont_pwm; // 1-based indexing; element 0 unused
static std::vector<int> s_cont_led; // 1-based indexing
static std::vector<int> s_bin_pin;  // 1-based indexing
static std::vector<int> s_bin_led;  // 1-based indexing
static std::vector<std::string> s_cont_names; // 1-based indexing
static std::vector<std::string> s_bin_names;  // 1-based indexing

void init_from_hardware()
{
    // Pre-size with a dummy element 0 so we can use 1-based indices directly
    s_cont_pwm.assign(NUM_CONTINUOUS_OUTS + 1, -1);
    s_cont_led.assign(NUM_CONTINUOUS_OUTS + 1, -1);
    s_cont_names.assign(NUM_CONTINUOUS_OUTS + 1, std::string());
    for (int i = 1; i <= NUM_CONTINUOUS_OUTS; ++i)
    {
        s_cont_pwm[i] = analog_out_pin(i);
        s_cont_led[i] = analog_out_LED_pin(i);
        s_cont_names[i] = std::string("a") + std::to_string(i);
    }

    s_bin_pin.assign(NUM_BINARY_OUTS + 1, -1);
    s_bin_led.assign(NUM_BINARY_OUTS + 1, -1);
    s_bin_names.assign(NUM_BINARY_OUTS + 1, std::string());
    for (int i = 1; i <= NUM_BINARY_OUTS; ++i)
    {
        s_bin_pin[i] = digital_out_pin(i);
        s_bin_led[i] = digital_out_LED_pin(i);
        s_bin_names[i] = std::string("d") + std::to_string(i);
    }
}

size_t num_continuous_outs() { return NUM_CONTINUOUS_OUTS; }
size_t num_binary_outs() { return NUM_BINARY_OUTS; }

int continuous_out_pwm_pin(int index)
{
    if (index < 1 || index > NUM_CONTINUOUS_OUTS) return -1;
    // Fall back to direct mapping if not initialised yet
    if (s_cont_pwm.empty()) return analog_out_pin(index);
    return s_cont_pwm[index];
}

int continuous_out_led_pin(int index)
{
    if (index < 1 || index > NUM_CONTINUOUS_OUTS) return -1;
    if (s_cont_led.empty()) return analog_out_LED_pin(index);
    return s_cont_led[index];
}

int binary_out_pin(int index)
{
    if (index < 1 || index > NUM_BINARY_OUTS) return -1;
    if (s_bin_pin.empty()) return digital_out_pin(index);
    return s_bin_pin[index];
}

int binary_out_led_pin(int index)
{
    if (index < 1 || index > NUM_BINARY_OUTS) return -1;
    if (s_bin_led.empty()) return digital_out_LED_pin(index);
    return s_bin_led[index];
}

const char* continuous_out_name(int index)
{
    if (index < 1 || index > NUM_CONTINUOUS_OUTS) return "";
    if (s_cont_names.empty()) return "";
    return s_cont_names[index].c_str();
}

const char* binary_out_name(int index)
{
    if (index < 1 || index > NUM_BINARY_OUTS) return "";
    if (s_bin_names.empty()) return "";
    return s_bin_names[index].c_str();
}

} // namespace io
} // namespace useq
