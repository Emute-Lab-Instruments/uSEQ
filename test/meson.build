# Test configuration for uSEQ builtin functions

# Test source files
test_src = [
  'test_generated_builtins.cpp'
]

# Parser test source files
parser_test_src = [
  'test_parser.cpp'
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
  '../uSEQ/src/utils.h',
  '../uSEQ/src/utils.cpp',
  # LISP
  '../uSEQ/src/lisp/parser.h',
  '../uSEQ/src/lisp/parser.cpp',
  '../uSEQ/src/lisp/value.h',
  '../uSEQ/src/lisp/value.cpp',
  '../uSEQ/src/lisp/environment.h',
  '../uSEQ/src/lisp/environment.cpp',
  '../uSEQ/src/lisp/interpreter.h',
  '../uSEQ/src/lisp/interpreter.cpp',
  '../uSEQ/src/lisp/configure.h',
  '../uSEQ/src/lisp/generated_builtins.h',
  '../uSEQ/src/lisp/generated_builtins.cpp',
]

# Test compiler arguments (similar to standalone)
test_args = [
  '-DUSE_OWN_ARDUINO_STR',
  '-DUSE_STD_IO',
  '-DNO_ETL',
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

# Register the tests with Meson
test('builtin_functions_test', test_builtin_exe)
test('parser_test', test_parser_exe)