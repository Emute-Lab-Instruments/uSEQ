#ifndef RANDOM_GENERATOR_H_
#define RANDOM_GENERATOR_H_

#include <cstdint>

// Interface for random number generation (allows mocking in tests)
class IRandomGenerator {
public:
    virtual ~IRandomGenerator() = default;
    virtual double generate() = 0;
    virtual double generate_with_index(uint32_t index) = 0;
    virtual void set_seed(uint32_t seed) = 0;
};

// Default implementation using simple hashing
class SimpleRandomGenerator : public IRandomGenerator {
public:
    SimpleRandomGenerator(uint32_t seed = 0x9E3779B9) : m_seed(seed) {}
    
    double generate() override;
    double generate_with_index(uint32_t index) override;
    void set_seed(uint32_t seed) override { m_seed = seed; }
    
private:
    uint32_t m_seed;
    double simple_hashing_function(uint32_t value);
};

#endif // RANDOM_GENERATOR_H_