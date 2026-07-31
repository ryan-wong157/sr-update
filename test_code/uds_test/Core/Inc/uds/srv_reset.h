#ifndef SRV_RESET_H
#define SRV_RESET_H

#include <stdint.h>
#include "sr_errno.h"

sr_errno_t x11_ecu_rst_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf);

#endif
