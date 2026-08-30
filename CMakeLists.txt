cmake_minimum_required(VERSION 3.16)
project(rv32i_simulator LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(rv32i_core
    src/alu.cpp
    src/cpu.cpp
    src/decoder.cpp
    src/memory.cpp
    src/register_file.cpp
)

target_include_directories(rv32i_core PUBLIC include)
target_compile_features(rv32i_core PUBLIC cxx_std_17)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rv32i_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()

add_executable(test_cpu tests/test_cpu.cpp)
target_link_libraries(test_cpu PRIVATE rv32i_core)
add_test(NAME cpu_pipeline COMMAND test_cpu)
