#include "main.h"
#include "isotplib.h"

/**
 * @brief Saves a pointer to the can handle, creates and inits isotp session, registers all callbacks
 * 
 * @param handle - fdcan handle
 * @param frame_format - either ISOTP_FORMAT_NORMAL/FD 
 */
void sr_isotp_init(sr_fdcan_handle_t* handle, isotp_format_t frame_format);

/**
 * @brief User gives a 4096 byte max sized tx buffer, 
 * this function handles the entire back and forth at the CAN level, 
 * BLOCKING until the transfer is done or fails
 * 
 * @param tx_data - buffer of data (4096 max)
 * @param length - length of data
  * @return sr_errno_t
 */
sr_errno_t sr_isotp_tx(const uint8_t* tx_data, size_t length);

// BLOCKS until new complete ISOTP message, returns a pointer to buffer. Handles FC and response CFs automatically
const uint8_t* sr_isotp_rx();