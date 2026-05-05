#include "modulisp/lisp/value.h"
#include "utils/string.h"
#include <variant>

// NOTE: this file is not being used yet

#define DEFAULT_OUTPUT_GATE 0
#define DEFAULT_OUTPUT_CV 0.5

enum class HardwareOutputType
{
    Gate,
    CV,
    Audio
};

struct HardwareOutput
{
    String name;
    int pin;
    std::optional<int> led_pin;
    bool inverted;
    Value expr;
    // union of int or float
    std::variant<int, float> value;
    HardwareOutputType type;
};