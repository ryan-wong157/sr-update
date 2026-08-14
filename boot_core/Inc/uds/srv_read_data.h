#ifndef SRV_READ_DATA_H
#define SRV_READ_DATA_H

#include <stdint.h>
#include "sr_errno.h"

// Supported Data IDs
#define DID_BOOTLOADER_VER 0xF180
#define DID_APP_VER 0xF181
// #define DID_LAST_UPDATE_INFO 0xF184
#define DID_CURR_DIAG_SESS 0xF186
#define DID_ECU_NODE_NUMBER 0xF191

sr_errno_t x22_read_data_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf);

#endif
