cmake_minimum_required(VERSION 3.22)

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

message("Building ECU '${ECU}' for MCU '${ECU_MCU}'")

add_subdirectory("${ECU_MCU_DIR}")

target_compile_definitions(${ECU_MCU} PRIVATE ${ECU_COMPILE_DEFS})
