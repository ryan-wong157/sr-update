#include "isotp/isotp.h"
#include "drivers/can_driver.h"
#include "config/can_config.h"

void HAL_FDCAN_RxFifo0Callback (FDCAN_HandleTypeDef* hfdcan, uint32_t RxFifo0ITs) {
    if (RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) {
        FDCAN_RxHeaderTypeDef head;
        uint8_t data[8];
        HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &head, data);

        sr_isotp_can_isr_callback(data, sr_fdcan_dlc_to_bytes(head.DataLength));
    }
}