cmake_minimum_required(VERSION 3.10)
project(ARM_M_TLM)

# Set C++ standard
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ARM Architecture Configuration
# Set default to Cortex-M0+ if not specified
if(NOT DEFINED ARM_CORE_TYPE)
    set(ARM_CORE_TYPE "CORTEX_M0_PLUS")
endif()

# Define the core type for compilation
add_compile_definitions(ARM_CORE_TYPE=${ARM_CORE_TYPE})

message(STATUS "Building for ARM Core: ${ARM_CORE_TYPE}")

# Find SystemC
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMC REQUIRED systemc)

# Include directories
include_directories(${SYSTEMC_INCLUDE_DIRS})
include_directories(src)
include_directories(src/cpu)
include_directories(src/memory)
include_directories(src/bus)
include_directories(src/peripherals)
include_directories(src/helpers)
include_directories(src/debug)

# Source files
set(SOURCES
    src/main.cpp
    src/Simulator.cpp
    src/cpu/CPU.cpp
    src/cpu/Registers.cpp
    src/cpu/Instruction.cpp
    src/cpu/Execute.cpp
    src/memory/Memory.cpp
    src/bus/BusCtrl.cpp
    src/peripherals/Trace.cpp
    src/peripherals/NVIC.cpp
    src/helpers/Performance.cpp
    src/helpers/Log.cpp
    src/debug/GDBServer.cpp
)

# Create executable
add_executable(arm_m_tlm ${SOURCES})

# Link libraries
target_link_libraries(arm_m_tlm ${SYSTEMC_LIBRARIES})

# Compiler flags
target_compile_options(arm_m_tlm PRIVATE -g ${SYSTEMC_CFLAGS_OTHER})
target_compile_definitions(arm_m_tlm PRIVATE SC_INCLUDE_DYNAMIC_PROCESSES)

# Set output directory
set_target_properties(arm_m_tlm PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Create directories
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/logs)