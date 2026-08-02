#ifndef SRV_DOWNLOAD_H
#define SRV_DOWNLOAD_H

#include <stdint.h>
#include "sr_errno.h"

typedef enum {
    DOWNLOAD_IDLE = 0,
    DOWNLOAD_IN_PROGRESS
} download_state_t;

sr_errno_t x34_download_start_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf);
sr_errno_t x36_trnsfr_data_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf);
sr_errno_t x37_download_exit_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf);
sr_errno_t x3e_hbt_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf);

// Cancels any in-progress download. Aborts the flash writer session and
// resets download state back to idle.
void reset_programming(void);

#endif
