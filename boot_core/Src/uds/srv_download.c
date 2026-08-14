#include "uds/srv_download.h"
#include "uds/srv_security_access.h"
#include "uds/srv_session_control.h"
#include "uds/uds_codes.h"
#include "isotp/isotp.h"
#include "common_config/uds_config.h"
#include "mcu_interface/flash_writer.h"

static download_state_t curr_state = DOWNLOAD_IDLE;
static uint32_t num_bytes_to_download = 0;
static uint8_t expected_seq_counter = 1;

sr_errno_t x34_download_start_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf) {
    if (rx_length < 3) {
        // can we at least read SID, dataFormatIdentifier and addressAndLengthFormatIdentifier?
        return uds_send_nrc(tx_buf, SID_DOWNLOAD_START_RQ, NRC_INCORRECT_MSG_LENGTH_OR_INVALID_FORMAT);
    }

    if (get_security_access() != SECURITY_UNLOCKED || get_session() != SESSION_PROGRAMMING) {
        return uds_send_nrc(tx_buf, SID_DOWNLOAD_START_RQ, NRC_SECURITY_ACCESS_DENIED);
    }

    uint8_t data_format_id = rx_buf[1];
    uint8_t address_and_length_format_id = rx_buf[2];
    uint8_t mem_size_len = address_and_length_format_id >> 4;
    uint8_t mem_addr_len = address_and_length_format_id & 0x0F;

    if (data_format_id != 0x00) {
        // no compression nor encryption supported
        return uds_send_nrc(tx_buf, SID_DOWNLOAD_START_RQ, NRC_REQUEST_OUT_OF_RANGE);
    }

    if (mem_size_len == 0 || mem_size_len > 4 || mem_addr_len != 4) {
        // 1. len of memSize section can't be 0 
        // 2. total num bytes to transfer must fit into a uint32_t
        // 2. memAddress field must be 4 bytes (since we are 32 bit addressed)
        return uds_send_nrc(tx_buf, SID_DOWNLOAD_START_RQ, NRC_REQUEST_OUT_OF_RANGE);
    }

    if (rx_length < 3 + mem_addr_len + mem_size_len) {
        // is the buffer as big as they claim?
        return uds_send_nrc(tx_buf, SID_DOWNLOAD_START_RQ, NRC_INCORRECT_MSG_LENGTH_OR_INVALID_FORMAT);
    }

    // get memorySize
    uint32_t mem_size = 0;
    uint8_t index_offset = 3 + mem_addr_len;
    for (int i = 0; i < mem_size_len; i++) {
        mem_size = (mem_size << 8) | rx_buf[index_offset + i];
    }
    num_bytes_to_download = mem_size;
    if (num_bytes_to_download == 0 || num_bytes_to_download > sr_flash_get_slot_capacity(FLASH_SLOT_B)) {
        return uds_send_nrc(tx_buf, SID_DOWNLOAD_START_RQ, NRC_REQUEST_OUT_OF_RANGE);
    }

    if (sr_flash_writer_begin(FLASH_SLOT_B) != SR_OK) {
        return uds_send_nrc(tx_buf, SID_DOWNLOAD_START_RQ, NRC_PROGRAMMING_FAILURE);
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
    
    curr_state = DOWNLOAD_IN_PROGRESS;
    return sr_isotp_tx(tx_buf, 2 + bytes_needed_for_maxblocklength);
}

sr_errno_t x36_trnsfr_data_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf) {
    if (rx_length < 3 || rx_length > (2 + CFG_UDS_x36_MAX_BLOCK_LEN)) {
        // Must adhere to block length limit as well
        return uds_send_nrc(tx_buf, SID_TRNSFR_DATA_RQ, NRC_INCORRECT_MSG_LENGTH_OR_INVALID_FORMAT);
    }

    if (curr_state == DOWNLOAD_IDLE || num_bytes_to_download == 0) {
        return uds_send_nrc(tx_buf, SID_TRNSFR_DATA_RQ, NRC_REQUEST_SEQUENCE_ERROR);
    }

    uint8_t seq_counter = rx_buf[1];
    uint32_t num_bytes_sent = rx_length - 2;

    if (num_bytes_sent > num_bytes_to_download) {
        return uds_send_nrc(tx_buf, SID_TRNSFR_DATA_RQ, NRC_TRANSFER_DATA_SUSPENDED);
    }

    if (seq_counter == expected_seq_counter - 1) {
        // already handeled block, but response was lost so client re-tried 
        tx_buf[0] = SID_TRNSFR_DATA_RES;
        tx_buf[1] = seq_counter;
        return sr_isotp_tx(tx_buf, 2);
    }

    if (seq_counter < expected_seq_counter - 1 || seq_counter > expected_seq_counter) {
        return uds_send_nrc(tx_buf, SID_TRNSFR_DATA_RQ, NRC_WRONG_BLOCK_SEQUENCE_COUNTER);
    }

    // Finally, seq_counter == expected_seq_counter
    sr_errno_t result = uds_send_nrc(tx_buf, SID_TRNSFR_DATA_RQ, NRC_REQUEST_RECEIVED_RESPONSE_PENDING);
    if (result != SR_OK) {
        // if isotp is cooked, abort
        return result;
    }


    if (sr_flash_writer_write(&rx_buf[2], num_bytes_sent) != SR_OK) {
        return uds_send_nrc(tx_buf, SID_TRNSFR_DATA_RQ, NRC_PROGRAMMING_FAILURE);
    }

    num_bytes_to_download -= num_bytes_sent;
    expected_seq_counter++;

    tx_buf[0] = SID_TRNSFR_DATA_RES;
    tx_buf[1] = seq_counter;
    return sr_isotp_tx(tx_buf, 2);
}

/*
This should: flush the flash write buffer, verify the metadata (signature) and do bootloader-related stuff
*/
sr_errno_t x37_download_exit_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf) {
    if (rx_length != 1) {
        return uds_send_nrc(tx_buf, SID_DOWNLOAD_EXIT_RQ, NRC_INCORRECT_MSG_LENGTH_OR_INVALID_FORMAT);
    }

    if (num_bytes_to_download != 0 || curr_state == DOWNLOAD_IDLE) {
        return uds_send_nrc(tx_buf, SID_DOWNLOAD_EXIT_RQ, NRC_REQUEST_SEQUENCE_ERROR);
    }

    // flush final write to flash
    if (sr_flash_writer_finish() != SR_OK) {
        return uds_send_nrc(tx_buf, SID_DOWNLOAD_EXIT_RQ, NRC_PROGRAMMING_FAILURE);
    }

    // TODO: CHECK INTEGRITY AND OTHER THINGS.
    // IF FAIL, SEND BACK 0X72 GENERAL PROGRAMMING FAILURE

    tx_buf[0] = SID_DOWNLOAD_EXIT_RES;

    return sr_isotp_tx(tx_buf, 1);
}

sr_errno_t x3e_hbt_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf) {
    if (rx_length != 2) {
        return uds_send_nrc(tx_buf, SID_TESTER_HBT_RQ, NRC_INCORRECT_MSG_LENGTH_OR_INVALID_FORMAT);
    }
    uint8_t sfb = rx_buf[1];
    if (!(sfb == 0x00 || sfb == 0x80)) {
        return uds_send_nrc(tx_buf, SID_TESTER_HBT_RQ, NRC_SUB_FUNCTION_NOT_SUPPORTED);
    }

    if (sfb == 0x00) {
        tx_buf[0] = SID_TESTER_HBT_RES;
        tx_buf[1] = sfb;
        return sr_isotp_tx(tx_buf, 2);
    }

    return SR_OK;
}

void reset_programming(void) {
    if (curr_state != DOWNLOAD_IDLE) {
        sr_flash_writer_abort();
    }
    curr_state = DOWNLOAD_IDLE;
    num_bytes_to_download = 0;
    expected_seq_counter = 1;
}