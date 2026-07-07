#include "main.h"
#include "isotplib.h"
#include "drivers/can_driver.h"

/**
 * @brief Saves a pointer to the can handle, creates and inits isotp session, registers all callbacks
 * 
 * @param handle - fdcan handle
 * @param frame_format - either ISOTP_FORMAT_NORMAL/FD 
 */
void sr_isotp_init(sr_fdcan_handle_t* handle, isotp_format_t frame_format);

/**
 * @brief User gives a 16 byte max sized tx buffer (FOR NOW), 
 * this function handles the entire back and forth at the CAN level, 
 * BLOCKING until the transfer is done or fails
 * 
 * TODO: Implement a timeout if other party doesn't finish transaction for some reason
 * 
 * @param tx_data - buffer of data
 * @param length - length of data
  * @return sr_errno_t
 */
sr_errno_t sr_isotp_tx(const uint8_t* tx_data, size_t length);

/**
 * @brief User gives an empty buffer at least as big as 2048 to recv firmware data.
 * Blocks until full isotp message is transferred from client, then copies into rx_buff
 * amount copied is put into recv_length
 * 
 * TODO: Implement a timeout if other party doesn't finish transaction for some reason
 * 
 * @param rx_buff 
 * @param length 
 * @param recv_length 
 * @return sr_errno_t 
 */
sr_errno_t sr_isotp_rx(uint8_t* rx_buff, size_t length, uint32_t* recv_length);

/**
 * @brief function called by can ISR
 * 
 * @param data 
 * @param length 
 */
void sr_isotp_can_isr_callback(uint8_t* data, size_t length);

