# Test configuration for uSEQ builtin functions and API tests

# Define precompiled header path (relative to test directory)
test_pch_file = '../uSEQ/src/pch.h'

# Test compiler arguments (similar to standalone)
test_args = [
  '-DUSE_OWN_ARDUINO_STR',
  '-DUSE_STD_IO',
  '-DNO_ETL',
  '-D__not_in_flash(section)=',
  '-D__not_in_flash_func(x)=',
  '-std=c++17'
]

# Add conditional compilation flags based on options
if get_option('enable_dsp')
  test_args += '-DENABLE_DSP_ENGINE'
  test_args += '-DENABLE_TEMPO_ESTIMATOR'
endif

if get_option('enable_i2c')
  test_args += '-DENABLE_I2C_NETWORKING'
  test_args += '-DENABLE_I2C_HOST'
  test_args += '-DENABLE_I2C_CLIENT'
endif

if get_option('enable_flash')
  test_args += '-DENABLE_FLASH_STORAGE'
endif

if get_option('enable_leds')
  test_args += '-DENABLE_LED_CONTROL'
  test_args += '-DENABLE_RGB_LED'
endif

# Include directories
test_inc = include_directories('../uSEQ', '../uSEQ/src')

# Math library dependency
math_dep = declare_dependency(link_args : ['-lm'])

python3 = find_program('python3')

# ===== ModuLisp Core Tests (LISP interpreter) =====
test_src = [
  'modulisp/core/test_builtins.cpp'
]

parser_test_src = [
  'modulisp/core/test_parser.cpp'
]

value_api_test_src = [
  'modulisp/core/test_value_api.cpp'
]

environment_api_test_src = [
  'modulisp/core/test_environment_api.cpp'
]

parser_api_test_src = [
  'modulisp/core/test_parser_api.cpp'
]

parser_error_reporting_test_src = [
  'modulisp/core/test_parser_error_reporting.cpp'
]

interpreter_api_test_src = [
  'modulisp/core/test_interpreter_api.cpp'
]

error_handling_test_src = [
  'modulisp/core/test_error_handling.cpp'
]

# ===== ModuLisp API Tests (module-specific) =====
modulisp_api_test_src = [
  'modulisp/api/test_modulisp_api.cpp'
]

time_injection_test_src = [
  'modulisp/api/test_time_injection.cpp'
]

output_sync_test_src = [
  'modulisp/api/test_output_sync.cpp'
]

dependency_invalidation_test_src = [
  'modulisp/api/test_dependency_invalidation.cpp'
]

output_sampling_test_src = [
  'modulisp/api/test_output_sampling.cpp'
]

bytecode_vm_test_src = [
  'modulisp/api/test_bytecode_vm.cpp'
]

bytecode_vm_probe_src = [
  'modulisp/bytecode_vm/bytecode_vm_probe.cpp'
]

bytecode_vm_benchmark_src = [
  'modulisp/bytecode_vm/bytecode_vm_benchmark.cpp'
]

vm_fallback_detection_src = [
  'modulisp/bytecode_vm/test_vm_fallback_detection.cpp'
]

# Signal semantics removed (signal metadata deleted)
# signal_semantics_test_src = [
#   'modulisp/api/test_signal_semantics.cpp'
# ]

signals_test_src = [
  'test_signals.cpp'
]

automation_analysis_test_src = [
  'test_automation_analysis.cpp'
]

smoothness_test_src = [
  'test_just_smoothness.cpp'
]

# ===== Hardware/Integration Tests =====
io_bridge_test_src = [
  'hardware/test_io_bridge.cpp'
]

logging_bridge_test_src = [
  'hardware/test_logging_bridge.cpp'
]

i2c_bus_test_src = [
  'hardware/test_i2c_bus.cpp'
]

storage_env_test_src = [
  'hardware/test_storage_env.cpp'
]

output_manager_test_src = [
  'hardware/test_output_manager.cpp'
]

json_protocol_test_src = [
  'hardware/test_json_protocol.cpp'
]

# IOManager tests - COMMENTED OUT: test_io_manager.cpp doesn't exist
# io_manager_test_src = [
#   'test_io_manager.cpp'
# ]
io_manager_test_src = []

# Dependencies from parent build are available here
# No need to duplicate source files - use the static libraries instead!

# Test executable - uses modulisp_dep from parent project
test_builtin_exe = executable(
  'test_builtins',
  sources : test_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [modulisp_dep, math_dep]
)

# Parser test executable
test_parser_exe = executable(
  'test_parser',
  sources : parser_test_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [modulisp_dep, math_dep]
)

# Value API test executable
test_value_api_exe = executable(
  'test_value_api',
  sources : value_api_test_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [modulisp_dep, math_dep]
)

# Environment API test executable
test_environment_api_exe = executable(
  'test_environment_api',
  sources : environment_api_test_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [modulisp_dep, math_dep]
)

# Parser API test executable
test_parser_api_exe = executable(
  'test_parser_api',
  sources : parser_api_test_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [modulisp_dep, math_dep]
)

# Parser error reporting test executable
test_parser_error_reporting_exe = executable(
  'test_parser_error_reporting',
  sources : parser_error_reporting_test_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [modulisp_dep, math_dep]
)

# Interpreter API test executable
test_interpreter_api_exe = executable(
  'test_interpreter_api',
  sources : interpreter_api_test_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [modulisp_dep, math_dep]
)

# ModuLisp API test executable
test_modulisp_api_exe = executable(
  'test_modulisp_api',
  sources : modulisp_api_test_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [modulisp_dep, math_dep]
)

test_bytecode_vm_exe = executable(
  'test_bytecode_vm',
  sources : bytecode_vm_test_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [modulisp_dep, math_dep]
)

# Error handling test executable
test_error_handling_exe = executable(
  'test_error_handling',
  sources : error_handling_test_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [modulisp_dep, math_dep]
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
# test_automation_analysis_exe = executable(
#   'test_automation_analysis',
#   sources : automation_analysis_test_src,
#   include_directories : test_inc,
#   cpp_args : test_args,
#   cpp_pch : test_pch_file,
#   dependencies : [modulisp_dep, math_dep]
# )

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
test('parser_error_reporting_test', test_parser_error_reporting_exe)
test('interpreter_api_test', test_interpreter_api_exe)
test('modulisp_api_test', test_modulisp_api_exe)
test('bytecode_vm_test', test_bytecode_vm_exe)
test('error_handling_test', test_error_handling_exe)
# test('signals_test', test_signals_exe)  # Temporarily disabled - uses removed DSP methods
# test('automation_analysis_test', test_automation_analysis_exe)
# test('smoothness_test', test_smoothness_exe)  # Temporarily disabled - uses removed DSP methods

# Time injection test executable
time_injection_exe = executable(
  'test_time_injection',
  sources : time_injection_test_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [modulisp_dep, math_dep]
)

test('time_injection_test', time_injection_exe)

# Output synchronization test executable
output_sync_exe = executable(
  'test_output_sync',
  sources : output_sync_test_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [modulisp_dep, math_dep]
)

test('output_sync_test', output_sync_exe)

# Dependency invalidation test executable
dependency_invalidation_exe = executable(
  'test_dependency_invalidation',
  sources : dependency_invalidation_test_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [modulisp_dep, math_dep]
)

test('dependency_invalidation_test', dependency_invalidation_exe)

# Output sampling test executable
output_sampling_exe = executable(
  'test_output_sampling',
  sources : output_sampling_test_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [modulisp_dep, math_dep]
)

test('output_sampling_test', output_sampling_exe)

bytecode_vm_probe_exe = executable(
  'bytecode_vm_probe',
  sources : bytecode_vm_probe_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [modulisp_dep, math_dep]
)

test('bytecode_vm_probe_smoke', bytecode_vm_probe_exe)

test(
  'bytecode_vm_golden_fixture',
  python3,
  args : [
    files('../scripts/run_bytecode_vm_golden.py'),
    files('modulisp/bytecode_vm/golden/basic_semantics.yaml')
  ],
  depends : bytecode_vm_probe_exe
)

vm_fallback_detection_exe = executable(
  'test_vm_fallback_detection',
  sources : vm_fallback_detection_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [modulisp_dep, math_dep]
)

test('test_vm_fallback_detection', vm_fallback_detection_exe)

# Benchmark executable -- NOT a test; run manually with ./build/test/bytecode_vm_benchmark
bytecode_vm_benchmark_exe = executable(
  'bytecode_vm_benchmark',
  sources : bytecode_vm_benchmark_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [modulisp_dep, math_dep]
)

# Signal semantics test removed (signal metadata deleted)

# Execution semantics characterization test executable
execution_semantics_test_src = [
  'modulisp/test_execution_semantics.cpp'
]

execution_semantics_exe = executable(
  'test_execution_semantics',
  sources : execution_semantics_test_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [modulisp_dep, math_dep]
)

test('execution_semantics_test', execution_semantics_exe)

# Execution API test executable (Commit 2)
execution_api_test_src = [
  'modulisp/test_execution_api.cpp'
]

execution_api_exe = executable(
  'test_execution_api',
  sources : execution_api_test_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [modulisp_dep, math_dep]
)

test('execution_api_test', execution_api_exe)

# IO bridge test executable - needs full hardware dependency
io_bridge_exe = executable(
  'test_io_bridge',
  sources : io_bridge_test_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [useq_hw_dep, math_dep]
)

test('io_bridge_test', io_bridge_exe)

# Logging bridge test executable
logging_bridge_exe = executable(
  'test_logging_bridge',
  sources : logging_bridge_test_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [modulisp_dep, math_dep]
)

test('logging_bridge_test', logging_bridge_exe)

# I2C bus integration test executable - needs full hardware dependency
i2c_bus_exe = executable(
  'test_i2c_bus',
  sources : i2c_bus_test_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [useq_hw_dep, math_dep]
)

test('i2c_bus_test', i2c_bus_exe)

# Storage environment test executable - needs full hardware dependency
storage_env_exe = executable(
  'test_storage_env',
  sources : storage_env_test_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [useq_hw_dep, math_dep]
)

test('storage_env_test', storage_env_exe)

# OutputManager test executable - needs full hardware dependency
output_manager_exe = executable(
  'test_output_manager',
  sources : output_manager_test_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [useq_hw_dep, math_dep]
)

test('output_manager_test', output_manager_exe)

# JSON protocol parsing test executable - needs full hardware dependency
json_protocol_exe = executable(
  'test_json_protocol',
  sources : json_protocol_test_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [useq_hw_dep, math_dep]
)

test('json_protocol_test', json_protocol_exe)

# IOManager test executable - COMMENTED OUT: test file doesn't exist
# io_manager_exe = executable(
#   'test_io_manager',
#   sources : io_manager_test_src + [
#     '../uSEQ/src/uSEQ/io_manager.cpp'
#   ],
#   include_directories : test_inc,
#   cpp_args : test_args,
#   cpp_pch : test_pch_file,
#   dependencies : [math_dep]
# )
#
# test('io_manager_test', io_manager_exe)

# Test with minimal interfaces/helpers - demonstrates faster compilation
test_with_helpers_src = [
  'test_with_helpers.cpp'
]

test_with_helpers_exe = executable(
  'test_with_helpers',
  sources : test_with_helpers_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [test_helpers_dep, math_dep]
)

test('test_with_helpers', test_with_helpers_exe)

# ModuLisp end-to-end test
modulisp_end_to_end_src = [
  'ModuLisp/end_to_end.cpp'
]

modulisp_end_to_end_exe = executable(
  'test_modulisp_end_to_end',
  sources : modulisp_end_to_end_src,
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [modulisp_dep, math_dep]
)

test('modulisp_end_to_end_test', modulisp_end_to_end_exe)
