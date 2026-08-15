cmake_minimum_required(VERSION 3.22)

# ECU config cmake stuff =================================================
if(NOT ECU)
    message(FATAL_ERROR "No ECU specified. Configure with -DECU=<name>, e.g. -DECU=ecu_front (see ecus/*.cmake for available ECUs)")
endif()

set(ECU_DEF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/ecus/${ECU}.cmake")
if(NOT EXISTS "${ECU_DEF_FILE}")
    message(FATAL_ERROR "Unknown ECU '${ECU}': ${ECU_DEF_FILE} does not exist")
endif()
include("${ECU_DEF_FILE}")

if(NOT ECU_MCU)
    message(FATAL_ERROR "ecus/${ECU}.cmake did not set ECU_MCU")
endif()

set(ECU_MCU_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mcu/${ECU_MCU}")
if(NOT EXISTS "${ECU_MCU_DIR}")
    message(FATAL_ERROR "Unknown ECU_MCU '${ECU_MCU}': ${ECU_MCU_DIR} does not exist")
endif()


# Actual build stuff =================================================
# Binary name is derived from the ECU + MCU combination, e.g. ecu_front_stm32g431cb
set(CMAKE_PROJECT_NAME "${ECU}_${ECU_MCU}_bl")

message("Building ECU '${ECU}' for MCU '${ECU_MCU}' as target '${CMAKE_PROJECT_NAME}'")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
project(${CMAKE_PROJECT_NAME})
enable_language(C ASM)

add_executable(${CMAKE_PROJECT_NAME})

# Add the ecu.cmake macros
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE ${ECU_COMPILE_DEFS})

# Add boot core sources and include dirs
file(GLOB_RECURSE BOOT_CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/boot_core/Src/*.c")
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${BOOT_CORE_SOURCES})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/boot_core/Inc")

# Include the target MCU deps
add_subdirectory("${ECU_MCU_DIR}")
