project('uSEQ', 'cpp')

# Optional build options are defined in meson_options.txt

# Configure ccache if requested and available
if get_option('use_ccache')
  ccache = find_program('ccache', required: false)
  if ccache.found()
    message('ccache found and enabled for compilation caching')
    # Meson automatically uses ccache if it's in PATH and CXX/CC are not set
    # We can also explicitly set it via environment variables or wrapper scripts
    # Export these before running meson setup:
    #   export CXX="ccache g++"
    #   export CC="ccache gcc"
    # Or use: meson setup build --wrap-mode=default
    message('Note: Ensure ccache is in PATH or set CXX="ccache g++" before meson setup')
  else
    message('ccache not found, continuing without compilation caching')
    message('Install ccache for 80-90% speedup on repeated builds')
  endif
endif

# Configure sanitizer and coverage flags for desktop builds
san_flags = []
cov_flags = []
if get_option('sanitizers')
  san_flags = ['-fsanitize=address,undefined', '-fno-omit-frame-pointer']
endif
if get_option('coverage')
  cov_flags = ['--coverage', '-O0', '-g']
endif

# Add global compiler arguments with sensible warning configuration
common_warning_args = [
  '-Wall',           # Enable most warnings
  '-Wextra',         # Enable extra warnings
  '-Wpedantic',      # Pedantic warnings for ISO C++ compliance
  # '-Werror',         # Treat warnings as errors (exits on first error) - DISABLED
  # '-Wfatal-errors',  # Stop after first error (for faster builds) - DISABLED
  # Note: Removed contradictory -Wconversion/-Wno-conversion pair
  '-Wno-conversion', # Disable type conversion warnings (embedded code needs flexibility)
  '-Wshadow',        # Warn about variable shadowing
  '-Wunused',        # Warn about unused variables/functions
  '-Wno-unused-parameter', # Disable unused parameter warnings (common in embedded callbacks)
  '-Wuninitialized', # Warn about uninitialized variables
  '-Wdouble-promotion', # Warn about float to double promotions
  '-Wformat=2',      # Strict format string checking
  '-Wnull-dereference', # Warn about null pointer dereferences
]

add_global_arguments(common_warning_args + san_flags + cov_flags, language : 'cpp')

# Define include directories
src_inc = include_directories('uSEQ')

# Define precompiled header for faster compilation
pch_file = 'uSEQ/src/pch.h'

# Define compiler arguments for different targets (early for libraries)
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

# Add conditional compilation flags based on options
if get_option('minimal_build')
  standalone_args += '-DMINIMAL_BUILD'
  message('Building minimal configuration without optional features')
endif

if get_option('enable_dsp')
  standalone_args += '-DENABLE_DSP_ENGINE'
  standalone_args += '-DENABLE_TEMPO_ESTIMATOR'
  message('DSP engine support enabled')
endif

if get_option('enable_i2c')
  standalone_args += '-DENABLE_I2C_NETWORKING'
  standalone_args += '-DENABLE_I2C_HOST'
  standalone_args += '-DENABLE_I2C_CLIENT'
  message('I2C networking support enabled')
endif

if get_option('enable_flash')
  standalone_args += '-DENABLE_FLASH_STORAGE'
  message('Flash storage support enabled')
endif

if get_option('enable_leds')
  standalone_args += '-DENABLE_LED_CONTROL'
  standalone_args += '-DENABLE_RGB_LED'
  message('LED control support enabled')
endif

arduino_args = [
  # '-DARDUINO',
  # '-DUSE_ARDUINO_STR',
  # '-DUSE_SERIAL_IO'
]

# === STATIC LIBRARIES FOR PROPER DEPENDENCY TRACKING ===

# Get unity build option and configure override options
unity_build_enabled = get_option('unity_build')
unity_override_options = unity_build_enabled ? ['unity=on'] : ['unity=off']

# Utils library - lowest level, no dependencies
utils_lib = static_library('useq_utils',
  'uSEQ/src/utils/string.cpp',
  'uSEQ/src/utils/common.cpp',
  'uSEQ/src/utils/itoa.cpp',
  'uSEQ/src/utils/log.cpp',
  'uSEQ/src/utils/error_messages.cpp',
  'uSEQ/src/utils/flags.cpp',
  'uSEQ/src/utils/logger_bridge.cpp',
  'uSEQ/src/utils/default_logger.cpp',
  'uSEQ/src/utils.cpp',
  include_directories : src_inc,
  cpp_args : standalone_args,
  cpp_pch : pch_file,
  override_options : unity_override_options
)

# Create a dependency object for utils
utils_dep = declare_dependency(
  link_with : utils_lib,
  include_directories : src_inc
)

# LISP core library - depends on utils
lisp_core_lib = static_library('useq_lisp_core',
  'uSEQ/src/modulisp/lisp/parser.cpp',
  'uSEQ/src/modulisp/lisp/value.cpp',
  'uSEQ/src/modulisp/lisp/environment.cpp',
  'uSEQ/src/modulisp/lisp/interpreter.cpp',
  'uSEQ/src/modulisp/lisp/signal_metadata.cpp',
  # 'uSEQ/src/modulisp/lisp/value_signal_processing.cpp',
  'uSEQ/src/modulisp/lisp/builtins.cpp',
  'uSEQ/src/modulisp/lisp/error_context.cpp',
  'uSEQ/src/template_instantiations.cpp',
  include_directories : src_inc,
  cpp_args : standalone_args,
  cpp_pch : pch_file,
  dependencies : utils_dep,
  override_options : unity_override_options
)

# Create a dependency object for lisp core
lisp_core_dep = declare_dependency(
  link_with : lisp_core_lib,
  include_directories : src_inc,
  dependencies : utils_dep
)

# ModuLisp library - depends on lisp_core
modulisp_lib = static_library('useq_modulisp',
  'uSEQ/src/modulisp/modulisp.cpp',
  'uSEQ/src/modulisp/modulisp_interpreter.cpp',
  'uSEQ/src/modulisp/modulisp_time.cpp',
  'uSEQ/src/modulisp/modulisp_api.cpp',
  'uSEQ/src/modulisp/modulisp_eval.cpp',
  'uSEQ/src/modulisp/time_manager.cpp',
  'uSEQ/src/modulisp/scheduler.cpp',
  'uSEQ/src/modulisp/random_generator.cpp',
  include_directories : src_inc,
  cpp_args : standalone_args,
  cpp_pch : pch_file,
  dependencies : lisp_core_dep,
  override_options : unity_override_options
)

# Test helpers library - minimal interfaces for faster test compilation
test_helpers_lib = static_library('useq_test_helpers',
  'uSEQ/src/interfaces/test_helpers.cpp',
  include_directories : src_inc,
  cpp_args : standalone_args,
  cpp_pch : pch_file,
  dependencies : lisp_core_dep,
  override_options : unity_override_options
)

# Create a dependency object for test helpers
test_helpers_dep = declare_dependency(
  link_with : test_helpers_lib,
  include_directories : src_inc,
  dependencies : lisp_core_dep
)

# Create a dependency object for modulisp
modulisp_dep = declare_dependency(
  link_with : modulisp_lib,
  include_directories : src_inc,
  dependencies : lisp_core_dep
)

# DSP library - depends on utils
dsp_lib = static_library('useq_dsp',
  'uSEQ/src/dsp/tempoEstimator.cpp',
  include_directories : src_inc,
  cpp_args : standalone_args,
  cpp_pch : pch_file,
  dependencies : utils_dep,
  override_options : unity_override_options
)

# Create a dependency object for dsp
dsp_dep = declare_dependency(
  link_with : dsp_lib,
  include_directories : src_inc,
  dependencies : utils_dep
)

# uSEQ hardware library - depends on modulisp and dsp
useq_hw_lib = static_library('useq_hw',
  'uSEQ/src/uSEQ.cpp',
  'uSEQ/src/uSEQ_io.cpp',
  'uSEQ/src/uSEQ_i2c.cpp',
  'uSEQ/src/uSEQ_led.cpp',
  'uSEQ/src/uSEQ_flash.cpp',
  'uSEQ/src/uSEQ_api.cpp',
  'uSEQ/src/uSEQ_output_registration.cpp',
  'uSEQ/src/uSEQ/output_manager.cpp',
  'uSEQ/src/uSEQ/io_manager.cpp',
  'uSEQ/src/uSEQ/i2cHost.cpp',
  'uSEQ/src/uSEQ/i2cClient.cpp',
  'uSEQ/src/uSEQ/i2cUtils.cpp',
  'uSEQ/src/uSEQ_update.cpp',
  include_directories : src_inc,
  cpp_args : standalone_args,
  cpp_pch : pch_file,
  dependencies : [modulisp_dep, dsp_dep],
  override_options : unity_override_options
)

# Create a dependency object for useq hardware
useq_hw_dep = declare_dependency(
  link_with : useq_hw_lib,
  include_directories : src_inc,
  dependencies : [modulisp_dep, dsp_dep]
)

# === EXECUTABLES USE STATIC LIBRARIES ===

# Standalone interpreter executable - links against all needed libraries
standalone_interpreter = executable(
  'standalone',
  'uSEQ/src/standalone_interpreter.cpp',
  include_directories : src_inc,
  cpp_args : standalone_args,
  cpp_pch : pch_file,
  dependencies : useq_hw_dep,
  override_options : unity_override_options
)

# arduino_interpreter = executable(
#   'arduino',
#   sources : arduino_src,
#   include_directories : src_inc,
#   cpp_args : arduino_args
# )

# Add test subdirectory
subdir('test')
