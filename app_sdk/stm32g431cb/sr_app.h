#ifndef SR_APP_H
#define SR_APP_H

#include "stm32g4xx_hal.h"

// 1 = send the handshake as CAN FD with bit rate switching, 0 = classic CAN.
// Match your FDCAN FrameFormat in CubeMX (FD_BRS or Classic)
#define SR_APP_CAN_FD 0

// Call once FDCAN is initialised and started (HAL_FDCAN_Start), before your main loop.
// Your filters must allow 0x1F000FFD (extended ID) into RX FIFO0
void sr_app_confirm_boot(FDCAN_HandleTypeDef* hfdcan);

#endif
