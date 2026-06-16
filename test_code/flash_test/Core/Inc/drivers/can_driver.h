#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H

// Handles state of FD CAN 

#include "main.h"

/*
Functions i need:
CAN configuration
    - Configure global filter to auto reject whatever isn't in the supplied filters
    - configure rxfifo overwrite
    - HAL_FDCAN activate notification (interrupts on)
CAN start
    - HAL_FDCAN_Start
Filter configuration
    - Check number of filters declared - if index is greater, then error
    - keep track of filter index as a static global 
    - You can configure as long as CAN is ready/busy
    - allow people to add range or bitmask filters (maybe one func each)
CAN Tx
    - configures header
    - fills out data struct
    - adds message to Tx queue
CAN Rx (meant to be called in the interrupt handler)
    - first check if IT was called because of new message in fifo
    - All it does is take in the message, writes msg to proper global OR to queue if freeRTOS
*/

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
 */
void sr_fdcan_config(const FDCAN_HandleTypeDef* hfdcan, const sr_fdcan_config_t* config_struct);

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
 */
void sr_fdcan_filter_add(FDCAN_HandleTypeDef* hfdcan, uint32_t filter_type, uint32_t filter_config, uint32_t id1, uint32_t id2);

/**
 * @brief 
 * 
 * @param hfdcan 
 * @param can_id
 * @param data 
 * @param length 
 */
void sr_fdcan_tx(FDCAN_HandleTypeDef* hfdcan, uint32_t can_id, uint8_t* data, uint32_t length);

#endif