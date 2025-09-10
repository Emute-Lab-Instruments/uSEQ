#ifndef COMPILER_CONFIG_H_
#define COMPILER_CONFIG_H_

/**
 * @file compiler_config.h
 * @brief Centralized compiler configuration and warning management
 *
 * This header provides a controlled way to manage compiler warnings and diagnostics
 * across the uSEQ codebase, replacing scattered pragma directives with a centralized
 * approach.
 *
 * Philosophy: Enable maximum useful warnings by default, but provide targeted
 * suppressions only where absolutely necessary (e.g., generated code, legacy APIs).
 */

// =============================================================================
// COMPILER DETECTION
// =============================================================================

#if defined(__GNUC__) && !defined(__clang__)
#define USEQ_COMPILER_GCC 1
#elif defined(__clang__)
#define USEQ_COMPILER_CLANG 1
#else
#define USEQ_COMPILER_OTHER 1
#endif

// =============================================================================
// WARNING SUPPRESSION MACROS
// =============================================================================

/**
 * For generated code that we cannot modify
 * Use sparingly and document why it's needed
 */
#define USEQ_SUPPRESS_WARNINGS_PUSH                                                 \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wall\"")      \
        _Pragma("GCC diagnostic ignored \"-Wextra\"")                               \
            _Pragma("GCC diagnostic ignored \"-Wpedantic\"")

#define USEQ_SUPPRESS_WARNINGS_POP _Pragma("GCC diagnostic pop")

/**
 * For format string functions that need specific warning suppressions
 */
#define USEQ_SUPPRESS_FORMAT_WARNINGS_PUSH                                          \
    _Pragma("GCC diagnostic push")                                                  \
        _Pragma("GCC diagnostic ignored \"-Wformat-nonliteral\"")                   \
            _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")

#define USEQ_SUPPRESS_FORMAT_WARNINGS_POP _Pragma("GCC diagnostic pop")

/**
 * For Arduino/hardware abstraction where we need to interface with
 * third-party libraries that may not be warning-clean
 */
#define USEQ_SUPPRESS_EXTERNAL_WARNINGS_PUSH                                        \
    _Pragma("GCC diagnostic push")                                                  \
        _Pragma("GCC diagnostic ignored \"-Wconversion\"")                          \
            _Pragma("GCC diagnostic ignored \"-Wsign-conversion\"")                 \
                _Pragma("GCC diagnostic ignored \"-Wold-style-cast\"")

#define USEQ_SUPPRESS_EXTERNAL_WARNINGS_POP _Pragma("GCC diagnostic pop")

// =============================================================================
// MEMORY ATTRIBUTES
// =============================================================================

#ifdef ARDUINO
#define USEQ_FAST_MEM __not_in_flash("USEQDATA")
#define USEQ_FAST_FUNC __not_in_flash_func()
#else
#define USEQ_FAST_MEM  // Empty for desktop builds
#define USEQ_FAST_FUNC // Empty for desktop builds
#endif

// =============================================================================
// DOCUMENTATION NOTES
// =============================================================================

/**
 * MIGRATION GUIDE:
 *
 * Replace old patterns:
 *
 * OLD:
 *   #pragma GCC diagnostic push
 *   #pragma GCC diagnostic ignored "-Wall"
 *   #pragma GCC diagnostic ignored "-Wextra"
 *   #pragma GCC diagnostic ignored "-Wpedantic"
 *   ... code ...
 *   #pragma GCC diagnostic pop
 *
 * NEW:
 *   USEQ_SUPPRESS_WARNINGS_PUSH
 *   ... code ...
 *   USEQ_SUPPRESS_WARNINGS_POP
 *
 * But FIRST try to fix the actual warnings before suppressing them!
 *
 * Only use these macros when:
 * 1. Working with generated code (like generated_builtins.cpp)
 * 2. Interfacing with third-party libraries with unavoidable warnings
 * 3. Using Arduino/embedded APIs with different conventions
 *
 * DO NOT use for regular source code - fix the warnings instead!
 */

#endif // COMPILER_CONFIG_H_