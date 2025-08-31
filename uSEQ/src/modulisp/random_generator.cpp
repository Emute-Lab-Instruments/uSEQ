#include "random_generator.h"

double SimpleRandomGenerator::generate() {
    // Simple LCG-based random number generator
    m_seed = (1103515245 * m_seed + 12345) & 0x7fffffff;
    return static_cast<double>(m_seed) / 0x7fffffff;
}

double SimpleRandomGenerator::generate_with_index(uint32_t index) {
    return simple_hashing_function(index);
}

double SimpleRandomGenerator::simple_hashing_function(uint32_t value) {
    // Simple hash function for deterministic pseudo-random values
    value = ((value >> 16) ^ value) * 0x45d9f3b;
    value = ((value >> 16) ^ value) * 0x45d9f3b;
    value = (value >> 16) ^ value;
    return static_cast<double>(value & 0x7fffffff) / 0x7fffffff;
}