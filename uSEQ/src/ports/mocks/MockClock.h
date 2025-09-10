// Simple controllable clock for tests
#pragma once

#include "../IClock.h"

struct MockClock : public IClock
{
    uint64_t now_us = 0;

    void set_micros(uint64_t us) { now_us = us; }
    void advance_micros(uint64_t delta) { now_us += delta; }

    uint64_t micros() override { return now_us; }
    uint64_t millis() override { return now_us / 1000ULL; }
};
