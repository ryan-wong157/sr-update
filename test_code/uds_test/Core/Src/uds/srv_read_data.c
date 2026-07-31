#include "uds/srv_read_data.h"
#include "uds/uds_codes.h"
#include "uds/srv_session_control.h"
#include "isotp/isotp.h"
#include "config/metadata.h"
#include "config/uds_config.h"

sr_errno_t x22_read_data_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf) {
    if (rx_length < 3 || !(rx_length % 2)) {
        return uds_send_nrc(tx_buf, NRC_INCORRECT_MSG_LENGTH_OR_INVALID_FORMAT);
    }
    
    // Build response buffer
    tx_buf[0] = SID_READ_DATA_RES;

    uint32_t tx_index = 1;
    uint32_t rx_index = 1;
    uint8_t any_supported = 0;
    while (rx_index < rx_length) {
        uint16_t did = ((uint16_t)rx_buf[rx_index] << 8) | (uint16_t)rx_buf[rx_index + 1];

        // record_len = 2 DID bytes + num data bytes
        uint32_t record_len = 0;
        switch (did) {
            case DID_BOOTLOADER_VER:   
                record_len = 2 + 3; break;
            case DID_APP_VER:          
                record_len = 2 + 3; break;
            case DID_CURR_DIAG_SESS:   
                record_len = 2 + 1; break;
            case DID_ECU_NODE_NUMBER:  
                record_len = 2 + 2; break;
        }

        if (record_len != 0 && tx_index + record_len > CFG_UDS_TX_BUF_SIZE) {
            return uds_send_nrc(tx_buf, NRC_RESPONSE_TOO_LONG);
        }

        switch (did) {
            case DID_BOOTLOADER_VER:
                tx_buf[tx_index++] = rx_buf[rx_index];
                tx_buf[tx_index++] = rx_buf[rx_index + 1];
                tx_buf[tx_index++] = BOOTLOADER_VERS_MAJOR;
                tx_buf[tx_index++] = BOOTLOADER_VERS_MINOR;
                tx_buf[tx_index++] = BOOTLOADER_VERS_PATCH;
                any_supported = 1;
                break;
            case DID_APP_VER:
                tx_buf[tx_index++] = rx_buf[rx_index];
                tx_buf[tx_index++] = rx_buf[rx_index + 1];
                // TODO: replace this with current app major.minor.patch, or 0.0.0 if no app exists
                tx_buf[tx_index++] = 0;
                tx_buf[tx_index++] = 0;
                tx_buf[tx_index++] = 0;
                any_supported = 1;
                break;
            case DID_CURR_DIAG_SESS:
                tx_buf[tx_index++] = rx_buf[rx_index];
                tx_buf[tx_index++] = rx_buf[rx_index + 1];
                tx_buf[tx_index++] = (uint8_t)get_session();
                any_supported = 1;
                break;
            case DID_ECU_NODE_NUMBER:
                tx_buf[tx_index++] = rx_buf[rx_index];
                tx_buf[tx_index++] = rx_buf[rx_index + 1];
                tx_buf[tx_index++] = (NODE_ID >> 8) & 0xFF;
                tx_buf[tx_index++] = (NODE_ID & 0xFF);
                any_supported = 1;
                break;
            default:
                break;
        }
        rx_index += 2;
    }

    if (!any_supported) {
        return uds_send_nrc(tx_buf, NRC_REQUEST_OUT_OF_RANGE);
    }

    return sr_isotp_tx(tx_buf, tx_index);
}