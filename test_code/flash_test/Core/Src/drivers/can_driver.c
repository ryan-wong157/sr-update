// FD-CAN driver functions
// Written by Ryan Wong

#include "drivers/can_driver.h"

static uint32_t tx_id_type;
static uint32_t tx_brs;
static uint32_t tx_frame_format;
static uint32_t tx_event_fifo_control;

void sr_fdcan_config(FDCAN_HandleTypeDef* hfdcan, uint32_t overwrite) {
    HAL_StatusTypeDef retval;
    retval = HAL_FDCAN_ConfigGlobalFilter(hfdcan, FDCAN_REJECT, FDCAN_REJECT, FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE);

    // Either FDCAN_RX_FIFO_OVERWRITE or FDCAN_RX_FIFO_BLOCKING, let user configure
    retval = HAL_FDCAN_ConfigRxFifoOverwrite(hfdcan, FDCAN_RX_FIFO0, overwrite);
    retval = HAL_FDCAN_ConfigRxFifoOverwrite(hfdcan, FDCAN_RX_FIFO1, overwrite);

    retval = HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
    retval = HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_RX_FIFO1_NEW_MESSAGE, 0);
}

void sr_fdcan_start(FDCAN_HandleTypeDef* hfdcan) {
    HAL_StatusTypeDef retval;
    retval = HAL_FDCAN_Start(hfdcan);
}