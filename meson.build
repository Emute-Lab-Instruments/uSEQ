project('uSEQ', 'cpp')

# Define include directories
src_inc = include_directories('src')

# hardware_src = [
#   'src/hardware/pinmap.h'
# ]

# Common source files
common_src = [
  # UTILS
  'src/utils/string.h',
  'src/utils/string.cpp',
  'src/utils/common.h',
  'src/utils/common.cpp',
  'src/utils/itoa.h',
  'src/utils/itoa.cpp',
  'src/utils/dtostrf.h',
  'src/utils/log.h',
  'src/utils/log.cpp',
  'src/utils/error_messages.h',
  'src/utils/error_messages.cpp',
  'src/utils/flags.h',
  'src/utils/flags.cpp',
  # LISP
  'src/lisp/parser.h',
  'src/lisp/parser.cpp',
  'src/lisp/value.h',
  'src/lisp/value.cpp',
  'src/lisp/environment.h',
  'src/lisp/environment.cpp',
  'src/lisp/interpreter.h',
  'src/lisp/interpreter.cpp',
  'src/lisp/configure.h',
  # uSEQ
  'src/uSEQ.h',
  'src/uSEQ.cpp',
  # TODO should these be moved separately?
  'src/uSEQ/io.h',
  'src/uSEQ/io.cpp',
  # EXTERN
  'src/extern/MAFilter.hpp',
  'src/extern/tempoEstimator.h',
  'src/extern/tempoEstimator.cpp',
  # 'src/io/pinmap.h',
  # 'src/io/hardware.h',
  # 'src/io/hardware.cpp',
]

standalone_src = common_src + [
  'src/standalone_interpreter.cpp'
]

arduino_src = common_src

# Define compiler arguments for different targets
standalone_args = [
  '-DDESKTOP',
  # '-DUSE_STD_STR',
  '-DUSE_ARDUINO_STR',
  '-DUSE_STD_IO'
]

arduino_args = [
  '-DARDUINO',
  '-DUSE_ARDUINO_STR',
  '-DUSE_SERIAL_IO'
]

# Specify executables with source files, include directories, and compiler arguments

standalone_interpreter = executable(
  'standalone',
  sources : standalone_src,
  include_directories : src_inc,
  cpp_args : standalone_args
)

# arduino_interpreter = executable(
#   'arduino',
#   sources : arduino_src,
#   include_directories : src_inc,
#   cpp_args : arduino_args
# )
