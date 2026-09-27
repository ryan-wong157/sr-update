# example_ecu config file
# Which physical MCU this ECU builds against
set(ECU_MCU stm32g431cb)

# ECU ID (ISO-TP Tx/Rx CAN IDs are derived from this)
set(ECU_COMPILE_DEFS
    ECU_ID=0x10
)
