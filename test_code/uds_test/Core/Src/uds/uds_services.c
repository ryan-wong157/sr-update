#include <string.h>
#include "drivers/rng_driver.h"
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
static session_state_t session_state = SESSION_DEFAULT;
static uint8_t security_access = 0; // 0 means no auth, 1 means auth

// globals for 0x27 state
static uint32_t curr_seed = 0;
static uint8_t unlock_state = 0; // 0 is waiting for seed req, 1 is waiting for key response
static uint8_t num_attempts = 0;
static uint32_t timeout_start = 0;

// =================================================================================================
// Helpers
// =================================================================================================
sr_errno_t uds_send_nrc(uint8_t* tx_buf, uint8_t nrc) {
    tx_buf[0] = SID_NEG_RES;
    tx_buf[1] = nrc;
    return sr_isotp_tx(tx_buf, 2);
}

void reset_session_state() {
    curr_seed = 0;
    unlock_state = 0;
    security_access = 0;
    return;
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
    reset_session_state();
    // PROGRAMMING_BACK_TO_IDLE()
    session_state = (session_state_t)sfb;

    if (suppress) {
        return SR_OK;
    }

    // Respond
    uint16_t p2 = (sfb == SESSION_DEFAULT) ? CFG_DEFAULT_P2_SERVER_MAX : CFG_PROG_P2_SERVER_MAX;
    uint16_t p2star = (sfb == SESSION_DEFAULT) ? CFG_DEFAULT_P2STAR_SERVER_MAX : CFG_PROG_P2STAR_SERVER_MAX;

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
    if (rx_length < 2) {
        return uds_send_nrc(tx_buf, NRC_INCORRECT_MSG_LENGTH_OR_INVALID_FORMAT);
    }

    if (session_state != SESSION_PROGRAMMING) {
        return uds_send_nrc(tx_buf, NRC_SERVICE_NOT_SUPPORTED_IN_CURR_SESS);
    }

    // retry check
    if (HAL_GetTick() - timeout_start < CFG_RETRY_TIMEOUT && num_attempts >= 3) {
        return uds_send_nrc(tx_buf, NRC_TIME_DELAY_NOT_EXPIRED);
    } else if (HAL_GetTick() - timeout_start >= CFG_RETRY_TIMEOUT && num_attempts >= 3) {
        num_attempts = 0;
    }

    uint8_t sfb = rx_buf[1];
    uint8_t suppress = sfb >> 7;

    if ((sfb & 0x7F) == 0x01) {
        // seed request, always generate and reply with a seed
        sr_generate_number(&curr_seed);
        unlock_state = 1;

        tx_buf[0] = SID_SEC_ACCESS_RES;
        tx_buf[1] = sfb;
        tx_buf[2] = (uint8_t)(curr_seed >> 24);
        tx_buf[3] = (uint8_t)(curr_seed >> 16); 
        tx_buf[4] = (uint8_t)(curr_seed >> 8); 
        tx_buf[5] = (uint8_t)curr_seed;

        return sr_isotp_tx(tx_buf, 6);
    } else if ((sfb & 0x7F) == 0x02) {
        if (unlock_state == 0) {
            // if not expecting a key
            return uds_send_nrc(tx_buf, NRC_REQUEST_SEQUENCE_ERROR);
        }

        if (rx_length != 66) {
            // expected length must be SID, SFB, 64 byte signature
            return uds_send_nrc(tx_buf, NRC_INCORRECT_MSG_LENGTH_OR_INVALID_FORMAT);
        }

        uint8_t signature[68];
        for (int i = 2; i < 66; i++) {
            signature[i - 2] = rx_buf[i];
        }
        // append seed to verify for
        signature[64] = (uint8_t)(curr_seed >> 24);
        signature[65] = (uint8_t)(curr_seed >> 16);
        signature[66] = (uint8_t)(curr_seed >> 8);
        signature[67] = (uint8_t)curr_seed;

        // validate using ed25519 specifically for curr_seed
        uint8_t og_msg[68];
        uint64_t og_msg_len;
        if (crypto_sign_open(og_msg, &og_msg_len, signature, 68, x27_PUB_KEY) != 0) {
            unlock_state = 0;
            num_attempts++;
            if (num_attempts >= CFG_MAX_x27_ATTEMPTS) {
                timeout_start = HAL_GetTick();
            }
            return uds_send_nrc(tx_buf, NRC_INVALID_KEY);
        }
        unlock_state = 0;
        security_access = 1;

        if (!suppress) {
            tx_buf[0] = SID_SEC_ACCESS_RES;
            tx_buf[1] = sfb;
            return sr_isotp_tx(tx_buf, 2);
        }
        return SR_OK;
    } else {
        // unknown sfb
        return uds_send_nrc(tx_buf, NRC_SUB_FUNCTION_NOT_SUPPORTED);
    }
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
