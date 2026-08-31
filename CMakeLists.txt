cmake_minimum_required(VERSION 3.15)
project(RV32I_Emulator VERSION 1.0 DESCRIPTION "32-bit RISC-V Emulator" LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


add_executable(sim
    src/alu.cpp
    src/bus.cpp
    src/cpu.cpp
    src/csrs.cpp
    src/decoder.cpp
    src/guest_fault.cpp
    src/loader.cpp
    src/main.cpp
    src/memory.cpp
    src/register_file.cpp
)


target_include_directories(sim PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)


if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(sim PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
endif()