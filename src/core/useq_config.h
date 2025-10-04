#ifndef USEQ_CORE_CONFIG_H
#define USEQ_CORE_CONFIG_H

/**
 * Core uSEQ Configuration Constants
 *
 * These are platform-independent configuration values that apply to all uSEQ builds.
 */

namespace useq {

// === Reserved Serial Outputs ===

/**
 * Serial output 0 (s0) is ALWAYS reserved for time output
 * This outputs time_since_boot in seconds and cannot be reassigned by user code.
 */
constexpr size_t SERIAL_OUT_TIME_INDEX = 0;

/**
 * First user-assignable serial output index
 * User code can assign to s1, s2, s3, ... (indices 1, 2, 3, ...)
 */
constexpr size_t SERIAL_OUT_FIRST_USER = 1;

// === Default Values ===

/**
 * Default value for uninitialized continuous outputs (analog CV)
 * Middle of the range [0.0, 1.0]
 */
constexpr float DEFAULT_CONTINUOUS_VALUE = 0.5f;

/**
 * Default value for uninitialized binary outputs (gates)
 */
constexpr float DEFAULT_BINARY_VALUE = 0.0f;

// === Timing Constants ===

/**
 * Serial output rate limit in microseconds
 * Prevents flooding the serial port with too many messages
 */
constexpr unsigned long SERIAL_MESSAGE_RATE_LIMIT_US = 100000; // 100ms

/**
 * Default delay between update cycles in microseconds
 * Small delay to allow for interrupts and other system tasks
 */
constexpr unsigned int UPDATE_LOOP_DELAY_US = 100;

} // namespace useq

#endif // USEQ_CORE_CONFIG_H
