#ifndef COMPILER_CONFIG_H_
#define COMPILER_CONFIG_H_

// Warning suppression macros — used by dtostrf.h and third-party headers.

#define USEQ_SUPPRESS_WARNINGS_PUSH                                                 \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wall\"")      \
        _Pragma("GCC diagnostic ignored \"-Wextra\"")                               \
            _Pragma("GCC diagnostic ignored \"-Wpedantic\"")

#define USEQ_SUPPRESS_WARNINGS_POP _Pragma("GCC diagnostic pop")

#define USEQ_SUPPRESS_FORMAT_WARNINGS_PUSH                                          \
    _Pragma("GCC diagnostic push")                                                  \
        _Pragma("GCC diagnostic ignored \"-Wformat-nonliteral\"")                   \
            _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")

#define USEQ_SUPPRESS_FORMAT_WARNINGS_POP _Pragma("GCC diagnostic pop")

#endif // COMPILER_CONFIG_H_
