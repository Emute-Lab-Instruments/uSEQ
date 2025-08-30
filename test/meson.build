# Test configuration for uSEQ builtin functions and API tests

# Test source files
test_src = [
  'test_generated_builtins.cpp'
]

# Parser test source files
parser_test_src = [
  'test_parser.cpp'
]

# API test source files
value_api_test_src = [
  'test_value_api.cpp'
]

environment_api_test_src = [
  'test_environment_api.cpp'
]

parser_api_test_src = [
  'test_parser_api.cpp'
]

interpreter_api_test_src = [
  'test_interpreter_api.cpp'
]

modulisp_api_test_src = [
  'test_modulisp_api.cpp'
]

# Time/DI tests
time_injection_test_src = [
  'test_time_injection.cpp'
]

signals_test_src = [
  'test_signals.cpp'
]

automation_analysis_test_src = [
  'test_automation_analysis.cpp'
]

smoothness_test_src = [
  'test_just_smoothness.cpp'
]

# IO bridge tests
io_bridge_test_src = [
  'test_io_bridge.cpp'
]

# Logging bridge tests
logging_bridge_test_src = [
  'test_logging_bridge.cpp'
]

# I2C bus integration tests
i2c_bus_test_src = [
  'test_i2c_bus.cpp'
]

# Storage environment tests
storage_env_test_src = [
  'test_storage_env.cpp'
]

# Common source files needed for tests (from parent directory)
test_common_src = [
  # UTILS
  '../uSEQ/src/utils/string.h',
  '../uSEQ/src/utils/string.cpp',
  '../uSEQ/src/utils/common.h',
  '../uSEQ/src/utils/common.cpp',
  '../uSEQ/src/utils/itoa.h',
  '../uSEQ/src/utils/itoa.cpp',
  '../uSEQ/src/utils/dtostrf.h',
  '../uSEQ/src/utils/log.h',
  '../uSEQ/src/utils/log.cpp',
  '../uSEQ/src/utils/error_messages.h',
  '../uSEQ/src/utils/error_messages.cpp',
  '../uSEQ/src/utils/flags.h',
  '../uSEQ/src/utils/flags.cpp',
  '../uSEQ/src/utils/logger_bridge.cpp',
  '../uSEQ/src/utils/default_logger.h',
  '../uSEQ/src/utils/default_logger.cpp',
  '../uSEQ/src/utils.h',
  '../uSEQ/src/utils.cpp',
  # LISP
  '../uSEQ/src/modulisp/lisp/parser.h',
  '../uSEQ/src/modulisp/lisp/parser.cpp',
  '../uSEQ/src/modulisp/lisp/value.h',
  '../uSEQ/src/modulisp/lisp/value.cpp',
  '../uSEQ/src/modulisp/lisp/environment.h',
  '../uSEQ/src/modulisp/lisp/environment.cpp',
  '../uSEQ/src/modulisp/lisp/interpreter.h',
  '../uSEQ/src/modulisp/lisp/interpreter.cpp',
  '../uSEQ/src/modulisp/lisp/configure.h',
  '../uSEQ/src/modulisp/lisp/generated_builtins.h',
  '../uSEQ/src/modulisp/lisp/generated_builtins.cpp',
  # MODULISP
  '../uSEQ/src/modulisp/modulisp.h',
  '../uSEQ/src/modulisp/modulisp.cpp',
  '../uSEQ/src/modulisp/modulisp_api.cpp',
  '../uSEQ/src/modulisp/modulisp_eval.cpp',
  '../uSEQ/src/modulisp/modulisp_time.cpp',
]

# Test compiler arguments (similar to standalone)
test_args = [
  '-DUSE_OWN_ARDUINO_STR',
  '-DUSE_STD_IO',
  '-DNO_ETL',
  '-D__not_in_flash(section)=',
  '-D__not_in_flash_func(x)=',
  '-std=c++17'
]

# Include directories
test_inc = include_directories('../uSEQ', '../uSEQ/src')

# Test executable
test_builtin_exe = executable(
  'test_generated_builtins',
  sources : test_src + test_common_src,
  include_directories : test_inc,
  cpp_args : test_args,
  dependencies : [
    # Add math library dependency
    declare_dependency(link_args : ['-lm'])
  ]
)

# Parser test executable
test_parser_exe = executable(
  'test_parser',
  sources : parser_test_src + test_common_src,
  include_directories : test_inc,
  cpp_args : test_args,
  dependencies : [
    # Add math library dependency
    declare_dependency(link_args : ['-lm'])
  ]
)

# Value API test executable
test_value_api_exe = executable(
  'test_value_api',
  sources : value_api_test_src + test_common_src,
  include_directories : test_inc,
  cpp_args : test_args,
  dependencies : [
    # Add math library dependency
    declare_dependency(link_args : ['-lm'])
  ]
)

# Environment API test executable
test_environment_api_exe = executable(
  'test_environment_api',
  sources : environment_api_test_src + test_common_src,
  include_directories : test_inc,
  cpp_args : test_args,
  dependencies : [
    # Add math library dependency
    declare_dependency(link_args : ['-lm'])
  ]
)

# Parser API test executable
test_parser_api_exe = executable(
  'test_parser_api',
  sources : parser_api_test_src + test_common_src,
  include_directories : test_inc,
  cpp_args : test_args,
  dependencies : [
    # Add math library dependency
    declare_dependency(link_args : ['-lm'])
  ]
)

# Interpreter API test executable
test_interpreter_api_exe = executable(
  'test_interpreter_api',
  sources : interpreter_api_test_src + test_common_src,
  include_directories : test_inc,
  cpp_args : test_args,
  dependencies : [
    # Add math library dependency
    declare_dependency(link_args : ['-lm'])
  ]
)

# ModuLisp API test executable
test_modulisp_api_exe = executable(
  'test_modulisp_api',
  sources : modulisp_api_test_src + test_common_src,
  include_directories : test_inc,
  cpp_args : test_args,
  dependencies : [
    # Add math library dependency
    declare_dependency(link_args : ['-lm'])
  ]
)

# Signals test executable - DISABLED - uses removed DSP methods
# test_signals_exe = executable(
#   'test_signals',
#   sources : signals_test_src + test_common_src,
#   include_directories : test_inc,
#   cpp_args : test_args,
#   dependencies : [
#     # Add math library dependency
#     declare_dependency(link_args : ['-lm'])
#   ]
# )

# Automation analysis test executable
test_automation_analysis_exe = executable(
  'test_automation_analysis',
  sources : automation_analysis_test_src + test_common_src,
  include_directories : test_inc,
  cpp_args : test_args,
  dependencies : [
    # Add math library dependency
    declare_dependency(link_args : ['-lm'])
  ]
)

# Smoothness test executable - DISABLED - uses removed DSP methods
# test_smoothness_exe = executable(
#   'test_smoothness',
#   sources : smoothness_test_src + test_common_src,
#   include_directories : test_inc,
#   cpp_args : test_args,
#   dependencies : [
#     # Add math library dependency
#     declare_dependency(link_args : ['-lm'])
#   ]
# )

# Register the tests with Meson
test('builtin_functions_test', test_builtin_exe)
test('parser_test', test_parser_exe)
test('value_api_test', test_value_api_exe)
test('environment_api_test', test_environment_api_exe)
test('parser_api_test', test_parser_api_exe)
test('interpreter_api_test', test_interpreter_api_exe)
test('modulisp_api_test', test_modulisp_api_exe)
# test('signals_test', test_signals_exe)  # Temporarily disabled - uses removed DSP methods
test('automation_analysis_test', test_automation_analysis_exe)
# test('smoothness_test', test_smoothness_exe)  # Temporarily disabled - uses removed DSP methods

# Time injection test executable
time_injection_exe = executable(
  'test_time_injection',
  sources : time_injection_test_src + test_common_src,
  include_directories : test_inc,
  cpp_args : test_args,
  dependencies : [
    declare_dependency(link_args : ['-lm'])
  ]
)

test('time_injection_test', time_injection_exe)

# IO bridge test executable
io_bridge_exe = executable(
  'test_io_bridge',
  sources : io_bridge_test_src + test_common_src + [
    '../uSEQ/src/uSEQ.cpp',
    '../uSEQ/src/uSEQ_io.cpp',
    '../uSEQ/src/uSEQ_api.cpp',
    '../uSEQ/src/uSEQ/output_manager.cpp',
    '../uSEQ/src/uSEQ_update.cpp',
    '../uSEQ/src/uSEQ_led.cpp',
    '../uSEQ/src/uSEQ_i2c.cpp'
  ],
  include_directories : test_inc,
  cpp_args : test_args,
  dependencies : [
    declare_dependency(link_args : ['-lm'])
  ]
)

test('io_bridge_test', io_bridge_exe)

# Logging bridge test executable
logging_bridge_exe = executable(
  'test_logging_bridge',
  sources : logging_bridge_test_src + test_common_src,
  include_directories : test_inc,
  cpp_args : test_args,
  dependencies : [
    declare_dependency(link_args : ['-lm'])
  ]
)

test('logging_bridge_test', logging_bridge_exe)

# I2C bus integration test executable
i2c_bus_exe = executable(
  'test_i2c_bus',
  sources : i2c_bus_test_src + test_common_src + [
    '../uSEQ/src/uSEQ.cpp',
    '../uSEQ/src/uSEQ_io.cpp',
    '../uSEQ/src/uSEQ_api.cpp',
    '../uSEQ/src/uSEQ_i2c.cpp',
    '../uSEQ/src/uSEQ/output_manager.cpp',
    '../uSEQ/src/uSEQ_update.cpp',
    '../uSEQ/src/uSEQ_led.cpp'
  ],
  include_directories : test_inc,
  cpp_args : test_args,
  dependencies : [
    declare_dependency(link_args : ['-lm'])
  ]
)

test('i2c_bus_test', i2c_bus_exe)

# Storage environment test executable  
storage_env_exe = executable(
  'test_storage_env',
  sources : storage_env_test_src + test_common_src + [
    '../uSEQ/src/uSEQ.cpp',
    '../uSEQ/src/uSEQ_flash.cpp',
    '../uSEQ/src/uSEQ_io.cpp',
    '../uSEQ/src/uSEQ_api.cpp',
    '../uSEQ/src/uSEQ/output_manager.cpp',
    '../uSEQ/src/uSEQ_update.cpp',
    '../uSEQ/src/uSEQ_i2c.cpp',
    '../uSEQ/src/uSEQ_led.cpp'
  ],
  include_directories : test_inc,
  cpp_args : test_args,
  dependencies : [
    declare_dependency(link_args : ['-lm'])
  ]
)

test('storage_env_test', storage_env_exe)
