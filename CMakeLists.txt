cmake_minimum_required(VERSION 3.15)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Debug")
endif()


#
# Project details
#

project(
  "uSEQ"
  VERSION 0.0.1
  LANGUAGES CXX
)

# set(SOURCE_DIR "./src")

# Interpreter static library
add_executable(interpreter #STATIC
  src/main.cpp
  src/interpreter.h
  src/interpreter/environment.h
  src/interpreter/environment.cpp
  src/interpreter/LispLibrary.cpp
  # src/interpreter/LispLibrary.lisp
  src/interpreter/value.h
  src/interpreter/io.h
  src/string.h
  src/useq.h
  src/useq.cpp
  # add other source files as needed
)


# useq static library, depends on interpreter
# add_library(uSEQ STATIC
#   useq.hpp
#   # add other source files as needed
# )

# target_link_libraries(uSEQ PRIVATE interpreter)

# pico_firmware executable, depends on useq and interpreter
# add_executable(pico_firmware
#   src/pico_firmware/main.cpp
#   src/pico_firmware/some_file.cpp
#   # add other source files as needed
# )
# target_link_libraries(pico_firmware PRIVATE useq interpreter)

# Set C++ standard
#
set_target_properties(interpreter PROPERTIES LINKER_LANGUAGE CXX)
# set_target_properties(uSEQ PROPERTIES LINKER_LANGUAGE CXX)
# set_property(TARGET interpreter PROPERTY CXX_STANDARD 17)
# set_property(TARGET uSEQ PROPERTY CXX_STANDARD 17)
# set_property(TARGET pico_firmware PROPERTY CXX_STANDARD 17)
