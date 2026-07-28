#include <string.h>
#include "uds/uds_services.h"
#include "uds/uds_codes.h"
#include "config/uds_config.h"
#include "config/metadata.h"
#include "isotp/isotp.h"
#include "tweetnacl.h"

/* TODO:
- s3server timeout check (probably done in uds.c layer hmm, but then uds.c will have to know the session state)
*/
// =================================================================================================
// Stuff
// =================================================================================================
session_state_t session_state = SESSION_DEFAULT;
uint8_t security_access = 0;

// =================================================================================================
// Helpers
// =================================================================================================
sr_errno_t uds_send_nrc(uint8_t* tx_buf, uint8_t nrc) {
    tx_buf[0] = SID_NEG_RES;
    tx_buf[1] = nrc;
    return sr_isotp_tx(tx_buf, 2);
}

// =================================================================================================
// UDS SID handlers
// =================================================================================================

sr_errno_t test_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf) {
    // for test, we expect a singular byte, being 0x80 (the SID)
    const char response[] = "brochachowacho";
    const char bad_res[] = "bad :(";
    if (rx_length != 1) {
        memcpy(tx_buf, bad_res, sizeof(bad_res) - 1);
        return sr_isotp_tx(tx_buf, sizeof(bad_res) - 1);
    }
    memcpy(tx_buf, response, sizeof(response) - 1);
    return sr_isotp_tx(tx_buf, sizeof(response) - 1);
}

sr_errno_t x10_sess_ctrl_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf) {
    if (rx_length != 2) {
        return uds_send_nrc(tx_buf, NRC_INCORRECT_MSG_LENGTH_OR_INVALID_FORMAT);
    }
    uint8_t sfb = rx_buf[1];
    uint8_t suppress = sfb >> 7;

    if (sfb != SESSION_DEFAULT && sfb != SESSION_PROGRAMMING) {
        return uds_send_nrc(tx_buf, NRC_SUB_FUNCTION_NOT_SUPPORTED);
    }

    // TODO: Reset security and abort programming no matter what
    // RESET_SECURITY()
    // PROGRAMMING_BACK_TO_IDLE()
    session_state = (session_state_t)sfb;

    if (suppress) {
        return SR_OK;
    }

    // Respond
    uint16_t p2 = (sfb == SESSION_DEFAULT) ? DEFAULT_P2_SERVER_MAX : PROG_P2_SERVER_MAX;
    uint16_t p2star = (sfb == SESSION_DEFAULT) ? DEFAULT_P2STAR_SERVER_MAX : PROG_P2STAR_SERVER_MAX;

    tx_buf[0] = SID_SESS_CTRL_RES;
    tx_buf[1] = sfb;
    tx_buf[2] = (p2 >> 8) & 0xFF;
    tx_buf[3] = p2 & 0xFF;
    tx_buf[4] = (p2star >> 8) & 0xFF;
    tx_buf[5] = p2star & 0xFF;

    return sr_isotp_tx(tx_buf, 6);
}

sr_errno_t x11_ecu_rst_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf) {
    if (rx_length != 2) {
        return uds_send_nrc(tx_buf, NRC_INCORRECT_MSG_LENGTH_OR_INVALID_FORMAT);
    }

    uint8_t sfb = rx_buf[1];
    uint8_t suppress = sfb >> 7;

    if (sfb != 0x01) {
        return uds_send_nrc(tx_buf, NRC_SUB_FUNCTION_NOT_SUPPORTED);
    }

    if (!suppress) {
        tx_buf[0] = SID_ECU_RST_RES;
        tx_buf[1] = sfb;
        sr_isotp_tx(tx_buf, 2);
    }

    NVIC_SystemReset();

    // dead branch
    return SR_OK;
}

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
            case DID_BOOTLOADER_VER:   record_len = 2 + 3; break;
            case DID_APP_VER:          record_len = 2 + 3; break;
            case DID_CURR_DIAG_SESS:   record_len = 2 + 1; break;
            case DID_ECU_NODE_NUMBER:  record_len = 2 + 2; break;
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
                tx_buf[tx_index++] = (uint8_t)session_state;
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

sr_errno_t x27_sec_access_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf) {
    // handle this
    return SR_OK;
}

sr_errno_t x34_download_start_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf) {
    return SR_OK;
}

sr_errno_t x36_trnsfr_data_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf) {
    return SR_OK;
}

sr_errno_t x37_download_exit_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf) {
    return SR_OK;
}

sr_errno_t x3e_hbt_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf) {
    return SR_OK;
}
