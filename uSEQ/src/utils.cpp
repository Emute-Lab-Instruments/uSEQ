#include "utils.h"
#include <algorithm>
// #include <iterator>

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
