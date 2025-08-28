project('uSEQ', 'cpp')

# Define include directories
src_inc = include_directories('uSEQ')

# hardware_src = [
#   'src/hardware/pinmap.h'
# ]

# Common source files
common_src = [
  # UTILS
  'uSEQ/src/utils/string.h',
  'uSEQ/src/utils/string.cpp',
  'uSEQ/src/utils/common.h',
  'uSEQ/src/utils/common.cpp',
  'uSEQ/src/utils/itoa.h',
  'uSEQ/src/utils/itoa.cpp',
  'uSEQ/src/utils/dtostrf.h',
  'uSEQ/src/utils/log.h',
  'uSEQ/src/utils/log.cpp',
  'uSEQ/src/utils/error_messages.h',
  'uSEQ/src/utils/error_messages.cpp',
  'uSEQ/src/utils/flags.h',
  'uSEQ/src/utils/flags.cpp',
  'uSEQ/src/utils.h',
  'uSEQ/src/utils.cpp',
  # LISP
  'uSEQ/src/lisp/parser.h',
  'uSEQ/src/lisp/parser.cpp',
  'uSEQ/src/lisp/value.h',
  'uSEQ/src/lisp/value.cpp',
  'uSEQ/src/lisp/environment.h',
  'uSEQ/src/lisp/environment.cpp',
  'uSEQ/src/lisp/interpreter.h',
  'uSEQ/src/lisp/interpreter.cpp',
  'uSEQ/src/lisp/configure.h',
  'uSEQ/src/lisp/generated_builtins.h',
  'uSEQ/src/lisp/generated_builtins.cpp',
  # uSEQ
  'uSEQ/src/uSEQ.h',
  'uSEQ/src/uSEQ.cpp',
  'uSEQ/src/uSEQ_integration.h',
  'uSEQ/src/uSEQ_eval.cpp',
  'uSEQ/src/uSEQ_io.cpp',
  'uSEQ/src/uSEQ_i2c.cpp',
  'uSEQ/src/uSEQ_led.cpp',
  'uSEQ/src/uSEQ_flash.cpp',
  'uSEQ/src/uSEQ_time.cpp',
  'uSEQ/src/uSEQ_api.cpp',
  'uSEQ/src/io/io_map.cpp',
  # 'uSEQ/src/uSEQ_update_refactored.cpp',
  'uSEQ/src/uSEQ_update.cpp',  # Keep this version
  # TODO should these be moved separately?
  # 'uSEQ/src/uSEQ/io.h',
  # 'uSEQ/src/uSEQ/io.cpp',
  # EXTERN
  'uSEQ/src/dsp/MAFilter.h',
  'uSEQ/src/dsp/tempoEstimator.h',
  'uSEQ/src/dsp/tempoEstimator.cpp',
  # 'uSEQ/src/io/pinmap.h',
  # 'uSEQ/src/io/hardware.h',
  # 'uSEQ/src/io/hardware.cpp',
]

standalone_src = common_src + [
  'uSEQ/src/standalone_interpreter.cpp'
]

arduino_src = common_src

# Define compiler arguments for different targets
standalone_args = [
  # '-DDESKTOP',
  # '-DUSE_STD_STR',
  '-DUSE_OWN_ARDUINO_STR',
  '-DUSE_STD_IO',
  '-DNO_ETL',
  # '-DPICO_NO_HARDWARE'
  '-D__not_in_flash(section)=',
  '-D__not_in_flash_func(x)=',
]

arduino_args = [
  # '-DARDUINO',
  # '-DUSE_ARDUINO_STR',
  # '-DUSE_SERIAL_IO'
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

# Add test subdirectory
subdir('test')
