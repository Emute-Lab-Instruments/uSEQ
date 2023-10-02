cmake_minimum_required(VERSION 3.24)
project(virtual)


set(CMAKE_CXX_STANDARD 17)
add_executable(virtual itoa.h itoa.cpp main.cpp String.cpp String.h Common.cpp Common.h  ../../uSEQ.ino ../../piopwm.h ../../uSEQ.ino ../../uSEQ.ino)
target_include_directories(virtual PRIVATE .)
