#ifndef SR_APP_H
#define SR_APP_H

#include "stm32g4xx_hal.h"

// Call once FDCAN is initialised and started (HAL_FDCAN_Start), before your main loop.
// Your filters must allow 0x1F000FFD (extended ID) into RX FIFO0
void sr_app_confirm_boot(FDCAN_HandleTypeDef* hfdcan);

#endif
