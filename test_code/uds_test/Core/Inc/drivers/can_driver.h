#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H

#include "main.h"
#include "config/errno.h"

// =================================================================================================
// Structs
// =================================================================================================
/**
 * @brief Per-peripheral CAN driver state. One instance per FDCAN peripheral in use.
 */
typedef struct {
    FDCAN_HandleTypeDef* hfdcan;
    uint32_t tx_id_type;
    uint32_t tx_brs;
    uint32_t tx_frame_format;
    uint32_t tx_event_fifo_control;
    uint32_t filter_index;
} sr_fdcan_handle_t;


// =================================================================================================
// Functions
// =================================================================================================
/**
 * @brief Configures the FD CAN peripheral using settings in config/can_config.h
 *
 * @param handle - Driver handle for this peripheral, filled in by this call
 * @param hfdcan - HAL handle for the peripheral this handle will drive
 * @return sr_errno_t - error code defined in errno.h
 */
sr_errno_t sr_fdcan_configure(sr_fdcan_handle_t* handle, FDCAN_HandleTypeDef* hfdcan);

/**
 * @brief Adds a filter element
 * If filter_type == range, then accepts IDs between id1 and id2
 * If filter_type == mask, id1 = filter and id2 = mask
 * If filter_type == dual, accept only id1 and id2 exactly
 *
 * @param handle
 * @param filter_type - FDCAN_FILTER_RANGE or FDCAN_FILTER_MASK or FDCAN_FILTER_DUAL
 * @param filter_config - What to do with successful message, like FDCAN_FILTER_TO_RXFIFO0
 * @param id1 - 11 bit hex for standard id or 29 for extended
 * @param id2 - 11 bit hex for standard id or 29 for extended
 * @return uint32_t - error code defined in errno.h
 */
sr_errno_t sr_fdcan_filter_add(sr_fdcan_handle_t* handle, uint32_t filter_type, uint32_t filter_config, uint32_t id1, uint32_t id2);

/**
 * @brief
 *
 * @param handle
 * @param can_id id of can message
 * @param data pointer to byte array of data
 * @param length length of byte array
 * @return uint32_t - error code defined in errno.h
 */
sr_errno_t sr_fdcan_tx(sr_fdcan_handle_t* handle, uint32_t can_id, uint8_t* data, uint32_t length);

/**
 * @brief Exact same as above but blocks until confirmed message has been sent
 *
 * @param handle
 * @param can_id
 * @param data
 * @param length
 * @return sr_errno_t - error code defined in errno.h
 */
sr_errno_t sr_fdcan_tx_blocking(sr_fdcan_handle_t* handle, uint32_t can_id, uint8_t* data, uint32_t length);

/**
 * @brief Converts a HAL FDCAN_DLC_BYTES_x code (as read from FDCAN_RxHeaderTypeDef.DataLength)
 * into the actual number of data bytes. Only 0-8 map 1:1. 
 * DLC codes 9-15 map to 12,16,20,24,32,48,64.
 *
 * @param dlc - one of the FDCAN_DLC_BYTES_x values
 * @return uint32_t - number of data bytes, or 0 if dlc is not a valid code
 */
uint32_t sr_fdcan_dlc_to_bytes(uint32_t dlc);

/**
 * @brief Converts a byte count into the smallest HAL FDCAN_DLC_BYTES_x code that can hold it.
 * For FD lengths that fall between valid steps (e.g. 9-11), rounds up to the next valid code (12).
 *
 * @param length - number of data bytes (0-64)
 * @return uint32_t - FDCAN_DLC_BYTES_x code, or UINT32_MAX if length > 64
 */
uint32_t sr_fdcan_bytes_to_dlc(uint32_t length);

#endif