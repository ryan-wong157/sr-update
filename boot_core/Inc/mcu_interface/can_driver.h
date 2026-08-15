#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H

#include <stdint.h>
#include "sr_errno.h"

// =================================================================================================
// Functions
// =================================================================================================
/**
 * @brief Configures the FD CAN peripheral using settings in config/can_config.h
 *
 * @return sr_errno_t - error code defined in errno.h
 */
sr_errno_t sr_fdcan_configure();

/**
 * @brief start can
 * 
 * @return sr_errno_t 
 */
sr_errno_t sr_fdcan_start();

/**
 * @brief
 *
 * @param can_id id of can message
 * @param data pointer to byte array of data
 * @param length length of byte array
 * @return uint32_t - error code defined in errno.h
 */
sr_errno_t sr_fdcan_tx(uint32_t can_id, uint8_t* data, uint32_t length);

/**
 * @brief Exact same as above but blocks until confirmed message has been sent
 *
 * @param can_id
 * @param data
 * @param length
 * @return sr_errno_t - error code defined in errno.h
 */
sr_errno_t sr_fdcan_tx_blocking(uint32_t can_id, uint8_t* data, uint32_t length);

#endif