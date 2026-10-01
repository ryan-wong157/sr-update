#include "main.h"
#include "can_helper.h"
#include "config/can_config.h"

// external
#include "isotp/isotp.h"

// =================================================================================================
// INTERRUPT SERVICE ROUTINES
// =================================================================================================
// CAN FD ISR for FIFO0
void HAL_FDCAN_RxFifo0Callback (FDCAN_HandleTypeDef* hfdcan, uint32_t RxFifo0ITs) {
    if (RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) {
        FDCAN_RxHeaderTypeDef head;
        uint8_t data[64];

        while (HAL_FDCAN_GetRxFifoFillLevel(hfdcan, FDCAN_RX_FIFO0) > 0) {
            if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &head, data) != HAL_OK) {
                break;
            }
            sr_isotp_can_isr_callback(data, dlc_to_bytes(head.DataLength));
        }
    }
}