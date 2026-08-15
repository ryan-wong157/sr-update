# MCU port: stm32g431cb
# this file is added by the root CMakeLists.txt.

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Our custom driver sources (implements boot_core/Inc/mcu_interface/*.h)
file(GLOB MCU_DRIVER_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Core/Src/drivers/*.c")
target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${MCU_DRIVER_SOURCES})

# Typical include path
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Core/Inc")

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
)