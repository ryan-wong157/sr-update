#include "uds/srv_download.h"
#include "uds/srv_security_access.h"
#include "uds/srv_session_control.h"
#include "uds/uds_codes.h"
#include "isotp/isotp.h"
#include "config/uds_config.h"
#include "config/flash_config.h"
#include "drivers/flash_driver.h"

static uint32_t curr_write_addr = 0; // TODO: THIS SHOULD BE START OF SECOND SLOT
static uint32_t num_bytes_to_download = 0;

sr_errno_t x34_download_start_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf) {
    if (rx_length < 3) {
        // can we at least read SID, dataFormatIdentifier and addressAndLengthFormatIdentifier?
        return uds_send_nrc(tx_buf, NRC_INCORRECT_MSG_LENGTH_OR_INVALID_FORMAT);
    }

    if (get_security_access() != SECURITY_UNLOCKED || get_session() != SESSION_PROGRAMMING) {
        return uds_send_nrc(tx_buf, NRC_SECURITY_ACCESS_DENIED);
    }

    uint8_t data_format_id = rx_buf[1];
    uint8_t address_and_length_format_id = rx_buf[2];
    uint8_t mem_size_len = address_and_length_format_id >> 4;
    uint8_t mem_addr_len = address_and_length_format_id & 0x0F;

    if (data_format_id != 0x00) {
        // no compression nor encryption supported
        return uds_send_nrc(tx_buf, NRC_REQUEST_OUT_OF_RANGE);
    }

    if (mem_size_len == 0 || mem_size_len > 4 || mem_addr_len != 4) {
        // 1. len of memSize section can't be 0 
        // 2. total num bytes to transfer must fit into a uint32_t
        // 2. memAddress field must be 4 bytes (since we are 32 bit addressed)
        return uds_send_nrc(tx_buf, NRC_REQUEST_OUT_OF_RANGE);
    }

    if (rx_length < 3 + mem_addr_len + mem_size_len) {
        // is the buffer as big as they claim?
        return uds_send_nrc(tx_buf, NRC_INCORRECT_MSG_LENGTH_OR_INVALID_FORMAT);
    }

    // get memorySize
    uint32_t mem_size = 0;
    uint8_t index_offset = 3 + mem_addr_len;
    for (int i = 0; i < mem_size_len; i++) {
        mem_size = (mem_size << 8) | rx_buf[index_offset + i];
    }
    num_bytes_to_download = mem_size;
    if (num_bytes_to_download == 0) {
        return uds_send_nrc(tx_buf, NRC_INCORRECT_MSG_LENGTH_OR_INVALID_FORMAT);
    }

    // form response
    tx_buf[0] = SID_DOWNLOAD_START_RES;

    uint8_t bytes_needed_for_maxblocklength = 0;
    uint32_t tmp = CFG_UDS_x36_MAX_BLOCK_LEN;
    do {
        // this handles if CFG_UDS_x36_MAX_BLOCK_LEN = 0, still need 1 byte to represent
        bytes_needed_for_maxblocklength++;
        tmp >>= 8;
    } while (tmp);
    tx_buf[1] = bytes_needed_for_maxblocklength << 4; // LS nibble is reserved 0x0

    for (int i = 0; i < bytes_needed_for_maxblocklength; i++) {
        // parse each byte of the max block length into the tx array
        tx_buf[2 + i] = (CFG_UDS_x36_MAX_BLOCK_LEN >> (8 * (bytes_needed_for_maxblocklength - 1 - i))) & 0xFF;
    }
    
    return sr_isotp_tx(tx_buf, 2 + bytes_needed_for_maxblocklength);
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