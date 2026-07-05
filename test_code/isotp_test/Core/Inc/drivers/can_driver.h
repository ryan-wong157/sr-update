#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H

#include "main.h"
#include "drivers/errno.h"

typedef struct {
    uint32_t fifo_overwrite;
    uint32_t tx_id_type;
    uint32_t tx_brs;
    uint32_t tx_frame_format;
    uint32_t tx_event_fifo_control;
} sr_fdcan_config_t;

/**
 * @brief Configures the FD CAN peripheral
 * Handles configuration of things like the global filter, fifo overwrite etc.
 * Initiates tx header data which should be constant per CAN peripheral, stores in global var for now
 * Should also activate notification for relevant FIFOs if using interrupts
 * 
 * @param config_struct - Struct as defined above
 * @return sr_errno_t - error code defined in errno.h
 */

sr_errno_t sr_fdcan_config(FDCAN_HandleTypeDef* hfdcan, sr_fdcan_config_t* config_struct);

/**
 * @brief Adds a filter element 
 * If filter_type == range, then accepts IDs between id1 and id2
 * If filter_type == mask, id1 = filter and id2 = mask
 * If filter_type == dual, accept only id1 and id2 exactly
 * 
 * @param hfdcan 
 * @param filter_type - FDCAN_FILTER_RANGE or FDCAN_FILTER_MASK or FDCAN_FILTER_DUAL
 * @param filter_config - What to do with successful message, like FDCAN_FILTER_TO_RXFIFO0
 * @param id1 - 11 bit hex for standard id or 29 for extended
 * @param id2 - 11 bit hex for standard id or 29 for extended
 * @return uint32_t - error code defined in errno.h
 */
sr_errno_t sr_fdcan_filter_add(FDCAN_HandleTypeDef* hfdcan, uint32_t filter_type, uint32_t filter_config, uint32_t id1, uint32_t id2);

/**
 * @brief 
 * 
 * @param hfdcan can periph handle
 * @param can_id id of can message
 * @param data pointer to byte array of data
 * @param length length of byte array
 * @return uint32_t - error code defined in errno.h
 */
sr_errno_t sr_fdcan_tx(FDCAN_HandleTypeDef* hfdcan, uint32_t can_id, uint8_t* data, uint32_t length);

/**
 * @brief Exact same as above but blocks until confirmed message has been sent
 * 
 * @param hfdcan 
 * @param can_id 
 * @param data 
 * @param length 
 * @return sr_errno_t - error code defined in errno.h
 */
sr_errno_t sr_fdcan_tx_blocking(FDCAN_HandleTypeDef* hfdcan, uint32_t can_id, uint8_t* data, uint32_t length);

#endif