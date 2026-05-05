#include "utils/string.h"

// NOTE: this file is not being used yet

enum class HardwareInputType
{
    Gate,
    CV,
    Audio
};

struct HardwareInput
{
    String name;
    int pin;
    std::optional<int> led_pin;
    bool inverted;
    HardwareInputType type;
};