cmake_minimum_required(VERSION 3.6)
project(code_handler)

# Compiler flags -O3
# add_definitions("-Werror -Wall -Wextra")

set(CMAKE_CXX_STANDARD 17)

set(SOURCE_FILES
        main.cpp
        code_handler/general/bit_manipulations.cpp
        code_handler/operating_system/operating_system_utilities.cpp
        code_handler/code_handler.cpp
        code_handler/code_handler_functions.cpp
        code_handler/architecture_specific.cpp
        code_handler/general/endian.cpp
        code_handler/code_handler_enumerations.cpp
        code_handler/general/rounding.cpp
        code_handler/code_handler_gecko_registers.cpp
        code_handler/testing/benchmarking.cpp
        code_handler/testing/benchmarking.h)
add_executable(code_handler ${SOURCE_FILES})