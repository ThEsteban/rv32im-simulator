cmake_minimum_required(VERSION 3.20)
project(rv32i_simulator LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(rv32_core
    src/alu.cpp
    src/bus.cpp
    src/cpu.cpp
    src/decoder.cpp
    src/loader.cpp
    src/memory.cpp
    src/register_file.cpp
)

target_include_directories(rv32_core PUBLIC include)
target_compile_features(rv32_core PUBLIC cxx_std_20)

add_executable(rv32i src/main.cpp)
target_link_libraries(rv32i PRIVATE rv32_core)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rv32_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(rv32i PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()

add_executable(rv32i_tests tests/test_cpu.cpp)
target_link_libraries(rv32i_tests PRIVATE rv32_core)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rv32i_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME rv32i_tests COMMAND rv32i_tests)
