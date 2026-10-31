# example_h753 config file
# Which physical MCU this ECU builds against
set(ECU_MCU stm32h753zit)

# ECU ID (ISO-TP Tx/Rx CAN IDs are derived from this)
set(ECU_COMPILE_DEFS
    ECU_ID=0x001
)
