# ECU definition: ecu_front
# Which physical MCU this ECU builds against
set(ECU_MCU stm32g431cb)

# ECU identity + per-ECU CAN arbitration IDs, exposed to the firmware as compile definitions
set(ECU_COMPILE_DEFS
    ECU_ID=0x10
    CFG_ISOTP_TX_ID=0x7A0
    CFG_ISOTP_RX_ID=0x7A8
)
