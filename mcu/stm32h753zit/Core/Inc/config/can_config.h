#ifndef CAN_CONFIG_H
#define CAN_CONFIG_H

#include "main.h"
#include "common_config/can_bus_config.h"

// This value MUST BE < 32 bit counter overflow time where counter increments every cpu cycle
#define CFG_FDCAN_TIMEOUT_MS 1000

// Fixed FDCAN settings
// Values are FDCAN_* macros from stm32h7xx_hal_fdcan.h
#define CFG_FDCAN_FIFO_OVERWRITE      FDCAN_RX_FIFO_BLOCKING
#define CFG_FDCAN_TX_ID_TYPE          FDCAN_EXTENDED_ID
#define CFG_FDCAN_TX_EVENT_FIFO_CTRL  FDCAN_NO_TX_EVENTS
#if CFG_CAN_FD
#define CFG_FDCAN_TX_BRS              FDCAN_BRS_ON
#define CFG_FDCAN_TX_FRAME_FORMAT     FDCAN_FD_CAN
#else
#define CFG_FDCAN_TX_BRS              FDCAN_BRS_OFF
#define CFG_FDCAN_TX_FRAME_FORMAT     FDCAN_CLASSIC_CAN
#endif

// Fills an FD frame up to its DLC when the length isn't one of 12, 16, 20, 24, 32, 48, 64 (ISO 15765-2 default)
#define CFG_FDCAN_PAD_BYTE            0xCC

#endif