#include "uds/srv_download.h"
#include "uds/uds_codes.h"
#include "isotp/isotp.h"
#include "config/flash_config.h"
#include "drivers/flash_driver.h"

sr_errno_t x34_download_start_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf) {
    if (rx_length < 5)
    return SR_OK;
}

// this will combine the flash write and erase depending on PAGE/SECTOR_SIZE / num_bytes_transfered_per_x36
// e.g. it should know that for a certain sector/page, if it needs to write more than once to the same sector/page
// because one buffer of data < sector/page size, it will know to erase ONCE per sector/page
sr_errno_t x36_trnsfr_data_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf) {
    return SR_OK;
}

sr_errno_t x37_download_exit_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf) {
    return SR_OK;
}

sr_errno_t x3e_hbt_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf) {
    return SR_OK;
}