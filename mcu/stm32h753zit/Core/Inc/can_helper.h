#ifndef CAN_HELPER_H
#define CAN_HELPER_H
#include <stdint.h>

/**
 * @brief Converts a HAL FDCAN_DLC_BYTES_x macro (as read from FDCAN_RxHeaderTypeDef.DataLength)
 * into the actual number of data bytes.
 * 
 * @param dlc - one of the FDCAN_DLC_BYTES_x macros
 * @return uint32_t - number of data bytes, or 0 if dlc is not a valid code
 */
uint32_t dlc_to_bytes(uint32_t dlc);

/**
 * @brief Converts a byte count into the smallest HAL FDCAN_DLC_BYTES_x macro that can hold it, rounding up
 * 
 * @param length - number of data bytes (0-64)
 * @return uint32_t - FDCAN_DLC_BYTES_x macro, or UINT32_MAX if length > 64
 */
uint32_t bytes_to_dlc(uint32_t length);

#endif