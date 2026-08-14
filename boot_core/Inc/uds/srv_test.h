#ifndef SRV_TEST_H
#define SRV_TEST_H

#include <stdint.h>
#include "sr_errno.h"

sr_errno_t test_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf);

#endif
