// Benchmark for uSEQ tick() performance
#include <chrono>
#include <iostream>
#include <vector>
#include "uSEQ/src/uSEQ.h"
#include "uSEQ/src/ports/desktop_clock.h"
#include "uSEQ/src/utils/default_logger.h"

// Mock IO for benchmarking
class MockIo : public IIo {
public:
    void digital_write(int pin, int val) override { (void)pin; (void)val; }
    void analog_write(int pin, int val) override { (void)pin; (void)val; }
    int digital_read(int pin) override { (void)pin; return 0; }
    int analog_read(int pin) override { (void)pin; return 512; }
    void pin_mode(int pin, int mode) override { (void)pin; (void)mode; }
};

int main() {
    // Setup
    DesktopClock clock;
    DefaultLogger logger;
    MockIo io;

    uSEQ useq(&clock, &logger, &io);

    // Initialize with some simple expressions
    useq.eval("(def a1 (sin (* 2 pi (phasor 1))))");
    useq.eval("(def a2 (cos (* 2 pi (phasor 2))))");
    useq.eval("(def d1 (> (phasor 0.5) 0.5))");
    useq.eval("(def d2 (> (phasor 0.25) 0.5))");

    // Warm up
    for (int i = 0; i < 100; i++) {
        useq.tick();
    }

    // Benchmark
    const int num_iterations = 10000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_iterations; i++) {
        useq.tick();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double avg_tick_time = duration.count() / (double)num_iterations;
    double fps = 1000000.0 / avg_tick_time;

    std::cout << "=== uSEQ tick() Performance Benchmark ===" << std::endl;
    std::cout << "Iterations: " << num_iterations << std::endl;
    std::cout << "Total time: " << duration.count() << " microseconds" << std::endl;
    std::cout << "Average tick time: " << avg_tick_time << " microseconds" << std::endl;
    std::cout << "Effective FPS: " << fps << " Hz" << std::endl;

    return 0;
}