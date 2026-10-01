#ifndef ISOTP_H
#define ISOTP_H

#include <stdint.h>
#include <stddef.h>
#include "sr_errno.h"
#include "isotp/isotplib/isotplib.h"

/**
 * @brief Initialises can fd layer and isotp layer
 *
 * @param tx_buf - buffer isotp uses to build outgoing frames, owned by caller
 * @param tx_len - size of tx_buf
 * @param rx_buf - buffer isotp uses to build incoming frames, owned by caller
 * @param rx_len - size of rx_buf
 * @return sr_errno_t
 */
sr_errno_t sr_isotp_start(uint8_t* tx_buf, uint32_t tx_len, uint8_t* rx_buf, uint32_t rx_len);

/**
 * @brief User should use and re-pass the pointer to tx_buf from above into this function
 * The reason for this is to prevent unecessary copy and allocation of another buffer. length should be how much of this tx_buf we actually want to transfer
 * This function handles the entire back and forth at the CAN level, 
 * BLOCKING until the transfer is done or fails or timeout
 * 
 * @param tx_data - buffer of data
 * @param length - length of data
  * @return sr_errno_t
 */
sr_errno_t sr_isotp_tx(const uint8_t* tx_data, size_t length);

/**
 * @brief Blocks until a full isotp message has been transferred from the partner, then copies
 * it into out_buf.
 * On SR_OK the message is sitting in out_buf and recv_length holds how many bytes are valid (>0 guarantee).
 *
 * @param out_buf - buffer the received message is copied into, owned by caller
 * @param out_len - size of out_buf
 * @param recv_length - out, number of bytes of out_buf that hold the received message
 * @return sr_errno_t
 */
sr_errno_t sr_isotp_rx(uint8_t* out_buf, uint32_t out_len, uint32_t* recv_length);

/**
 * @brief function called by can ISR
 * 
 * @param data 
 * @param length 
 */
void sr_isotp_can_isr_callback(uint8_t* data, size_t length);

#endif