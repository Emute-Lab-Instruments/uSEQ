#include "utils.h"
#include <algorithm>
// #include <iterator>

float scale_value(float x, float in_min, float in_max, float out_min, float out_max)
{
    // Check to prevent division by zero
    if (in_max == in_min)
    {
        // Handle the error appropriately, here returning out_min as a default
        return out_min;
    }

    // Check for potential overflow before multiplication
    if ((x - in_min) > (std::numeric_limits<float>::max() / (out_max - out_min)))
    {
        // Handle overflow, here returning out_max as a default
        return out_max;
    }

    // Perform the scaling
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

float lerp(float a, float b, float t)
{
    // Use fma (fused multiply-add) for potentially more precise and performant
    // computation
    return std::fma(t, (b - a), a);
}

template <typename T>
std::set<T> set_intersection(const std::set<T>& set1, const std::set<T>& set2)
{
    std::set<T> resultSet;
    std::set_intersection(set1.begin(), set1.end(), set2.begin(), set2.end(),
                          std::inserter(resultSet, resultSet.begin()));
    return resultSet;
}

template <typename T>
std::set<T> set_difference(const std::set<T>& set1, const std::set<T>& set2)
{
    std::set<T> resultSet;
    std::set_difference(set1.begin(), set1.end(), set2.begin(), set2.end(),
                        std::inserter(resultSet, resultSet.begin()));
    return resultSet;
}

#if defined(ARDUINO)
void flash_builtin(int sleep, int times = 10)
{
    for (int i = 0; i < times; i++)
    {
        digitalWrite(LED_BUILTIN, HIGH);
        // turn the LED on (HIGH is the voltage level)
        delay(sleep);
        // wait for a second
        digitalWrite(LED_BUILTIN, LOW);
        // turn the LED off by making the voltage LOW
        delay(sleep);
    }
}

#else

void flash_builtin(int sleep, int times = 10) {}
double millis() { return 0.0; }
double micros() { return 0.0; }

void delay(int x) {}
#endif
