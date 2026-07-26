#ifndef ISOTP_H
#define ISOTP_H

#include "main.h"
#include "isotplib.h"
#include "drivers/can_driver.h"

/**
 * @brief Initialises and configures fd-can driver layer, starts fd-can periperhal.
 *
 * @param hfdcan - HAL handle, must already be HAL-initialised (MX_FDCANx_Init) but not yet started
 * @param frame_format - either ISOTP_FORMAT_NORMAL/FD
 * @param tx_buf - buffer isotp uses to build outgoing frames, owned by caller
 * @param tx_len - size of tx_buf
 * @param rx_buf - buffer isotp uses to build incoming frames, owned by caller
 * @param rx_len - size of rx_buf
 * @return sr_errno_t
 */
sr_errno_t sr_isotp_start(FDCAN_HandleTypeDef* hfdcan, isotp_format_t frame_format, uint8_t* tx_buf, uint32_t tx_len, uint8_t* rx_buf, uint32_t rx_len);

/**
 * @brief User gives a bytes to send over CAN. Should be <= tx_len from above.
 * This function handles the entire back and forth at the CAN level, 
 * BLOCKING until the transfer is done or fails or timeout
 * 
 * @param tx_data - buffer of data
 * @param length - length of data
  * @return sr_errno_t
 */
sr_errno_t sr_isotp_tx(const uint8_t* tx_data, size_t length);

/**
 * @brief Blocks until a full isotp message has been transferred from the partner into the
 * rx buffer the caller provided to sr_isotp_start.
 * On SR_OK the message is sitting in that buffer and recv_length holds how many bytes are valid.
 * The contents stay valid until the next sr_isotp_rx call, which re-arms the CAN ISR to
 * write into the buffer again. So consume/dispatch before calling again.
 *
 * @param recv_length - out, number of bytes of the rx buffer that hold the received message
 * @return sr_errno_t
 */
sr_errno_t sr_isotp_rx(uint32_t* recv_length);

/**
 * @brief function called by can ISR
 * 
 * @param data 
 * @param length 
 */
void sr_isotp_can_isr_callback(uint8_t* data, size_t length);

#endif