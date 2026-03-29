# Test configuration for uSEQ signal engine

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

# Include directories
test_inc = include_directories('../uSEQ', '../uSEQ/src')

# Math library dependency
math_dep = declare_dependency(link_args : ['-lm'])

python3 = find_program('python3')

# ===== Signal Engine Tests =====

signal_engine_test_exe = executable(
  'test_signal_engine',
  sources : ['signal_engine/test_signal_engine.cpp'],
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [signal_engine_dep, math_dep]
)

test('signal_engine_test', signal_engine_test_exe)

signal_engine_probe_exe = executable(
  'signal_engine_probe',
  sources : ['signal_engine/signal_engine_probe.cpp'],
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [signal_engine_dep, math_dep]
)

test('signal_engine_probe_smoke', signal_engine_probe_exe)

# ===== Flash Storage Tests =====

flash_storage_test_exe = executable(
  'test_flash_storage',
  sources : ['signal_engine/test_flash_storage.cpp'],
  include_directories : test_inc,
  cpp_args : test_args + ['-DENABLE_FLASH_STORAGE'],
  cpp_pch : test_pch_file,
  dependencies : [firmware_dep, math_dep]
)

test('flash_storage_test', flash_storage_test_exe)

test(
  'signal_engine_golden_fixture',
  python3,
  args : [
    files('../scripts/run_bytecode_vm_golden.py'),
    files('signal_engine/golden/basic_semantics.yaml'),
    '--probe', signal_engine_probe_exe.full_path()
  ],
  depends : signal_engine_probe_exe
)

# ===== Firmware E2E Tests =====

firmware_e2e_test_exe = executable(
  'test_firmware_e2e',
  sources : ['firmware/test_firmware_e2e.cpp'],
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [firmware_dep, math_dep]
)

test('firmware_e2e_test', firmware_e2e_test_exe, timeout: 60)

firmware_e2e2_test_exe = executable(
  'test_firmware_e2e_part2',
  sources : ['firmware/test_firmware_e2e_part2.cpp'],
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [firmware_dep, math_dep]
)

test('firmware_e2e_test_part2', firmware_e2e2_test_exe, timeout: 60)

# ===== Firmware Fuzz Tests =====

firmware_fuzz_test_exe = executable(
  'test_firmware_fuzz',
  sources : ['firmware/test_firmware_fuzz.cpp'],
  include_directories : test_inc,
  cpp_args : test_args,
  cpp_pch : test_pch_file,
  dependencies : [firmware_dep, math_dep]
)

test('firmware_fuzz_test', firmware_fuzz_test_exe, timeout: 120)
