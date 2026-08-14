#ifndef CAN_CONFIG_H
#define CAN_CONFIG_H

#include "main.h"

// This value MUST BE < 32 bit counter overflow time where counter increments every cpu cycle
#define CFG_FDCAN_TIMEOUT_MS 1000

// Fixed FDCAN settings
// Values are FDCAN_* macros from stm32g4xx_hal_fdcan.h
#define CFG_FDCAN_FIFO_OVERWRITE      FDCAN_RX_FIFO_BLOCKING
#define CFG_FDCAN_TX_ID_TYPE          FDCAN_STANDARD_ID
#define CFG_FDCAN_TX_BRS              FDCAN_FRAME_FD_NO_BRS
#define CFG_FDCAN_TX_FRAME_FORMAT     FDCAN_CLASSIC_CAN
#define CFG_FDCAN_TX_EVENT_FIFO_CTRL  FDCAN_NO_TX_EVENTS

#endif