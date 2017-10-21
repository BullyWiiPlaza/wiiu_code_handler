cmake_minimum_required(VERSION 3.6)
project(code_handler)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        main.c
        code_handler/general/bit_manipulations.c
        code_handler/operating_system/operating_system_utilities.c
        code_handler/code_handler.c
        code_handler/code_handler_functions.c
        code_handler/architecture_specific.c
        code_handler/general/endian.c
        code_handler/code_handler_enumerations.c
        code_handler/general/rounding.c
        code_handler/code_handler_gecko_registers.c)
add_executable(code_handler ${SOURCE_FILES})