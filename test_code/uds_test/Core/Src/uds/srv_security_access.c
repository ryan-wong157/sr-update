#include "main.h"
#include "tweetnacl.h"
#include "uds/srv_security_access.h"
#include "uds/srv_session_control.h"
#include "uds/uds_codes.h"
#include "config/uds_config.h"
#include "isotp/isotp.h"
#include "drivers/rng_driver.h"

// globals for 0x27 state
static security_access_t security_access = SECURITY_LOCKED;

static uint32_t curr_seed = 0;
static x27_state_t unlock_state = EXPECTING_REQUEST;
static uint8_t num_attempts = 0;
static uint32_t timeout_start = 0;

security_access_t get_security_access() {
    return security_access;
}

void x27_on_session_change(void) {
    curr_seed = 0;
    unlock_state = EXPECTING_REQUEST;
    security_access = SECURITY_LOCKED;
}

sr_errno_t x27_sec_access_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf) {
    if (rx_length < 2) {
        return uds_send_nrc(tx_buf, NRC_INCORRECT_MSG_LENGTH_OR_INVALID_FORMAT);
    }

    if (get_session() != SESSION_PROGRAMMING) {
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
        unlock_state = EXPECTING_SIGNATURE;

        tx_buf[0] = SID_SEC_ACCESS_RES;
        tx_buf[1] = sfb;
        tx_buf[2] = (uint8_t)(curr_seed >> 24);
        tx_buf[3] = (uint8_t)(curr_seed >> 16);
        tx_buf[4] = (uint8_t)(curr_seed >> 8);
        tx_buf[5] = (uint8_t)curr_seed;

        return sr_isotp_tx(tx_buf, 6);
    } else if ((sfb & 0x7F) == 0x02) {
        if (unlock_state == EXPECTING_REQUEST) {
            // if not expecting a signature
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
            unlock_state = EXPECTING_REQUEST;
            num_attempts++;
            if (num_attempts >= CFG_MAX_x27_ATTEMPTS) {
                timeout_start = HAL_GetTick();
            }
            return uds_send_nrc(tx_buf, NRC_INVALID_KEY);
        }
        unlock_state = EXPECTING_REQUEST;
        security_access = SECURITY_UNLOCKED;

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
