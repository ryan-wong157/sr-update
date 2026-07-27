#include "uds/uds_services.h"
#include "uds/uds_codes.h"
#include "config/uds_config.h"
#include "isotp/isotp.h"

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
sr_errno_t uds_send_nrc(uint8_t nrc) {
    uint8_t ret_buf[2] = {SID_NEG_RES, nrc};
    return sr_isotp_tx(ret_buf, sizeof(ret_buf));
}

// =================================================================================================
// UDS SID handlers
// =================================================================================================

sr_errno_t test_handler(const uint8_t* req, uint32_t length) {
    // for test, we expect a singular byte, being 0x80 (the SID)
    const char response[] = "brochachowacho";
    const char bad_res[] = "bad :(";
    if (length != 1) {
        return sr_isotp_tx((const uint8_t*)bad_res, sizeof(bad_res) - 1);
    }
    return sr_isotp_tx((const uint8_t*)response, sizeof(response) - 1);
}

sr_errno_t x10_sess_ctrl_handler(const uint8_t* req, uint32_t length) {
    if (length != 2) {
        return uds_send_nrc(NRC_INCORRECT_MSG_LENGTH_OR_INVALID_FORMAT);
    }
    uint8_t sid = req[0];
    uint8_t sfb = req[1];
    uint8_t suppress = sfb >> 7;

    if (sfb != SESSION_DEFAULT && sfb != SESSION_PROGRAMMING) {
        return uds_send_nrc(NRC_SUB_FUNCTION_NOT_SUPPORTED);
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

    uint8_t res_buf[6] = {
        SID_SESS_CTRL_RES, 
        sfb, 
        (p2 >> 8) & 0xFF, 
        p2 & 0xFF,
        (p2star >> 8) & 0xFF,
        p2star & 0xFF
    };

    return sr_isotp_tx(res_buf, sizeof(res_buf));
}

sr_errno_t x11_ecu_rst_handler(const uint8_t* req, uint32_t length) {
    if (length != 2) {
        return uds_send_nrc(NRC_INCORRECT_MSG_LENGTH_OR_INVALID_FORMAT);
    }

    uint8_t sid = req[0];
    uint8_t sfb = req[1];
    uint8_t suppress = sfb >> 7;

    if (sfb != 0x01) {
        return uds_send_nrc(NRC_SUB_FUNCTION_NOT_SUPPORTED);
    }

    if (!suppress) {
        uint8_t res_buf[2] = {SID_ECU_RST_RES, sfb};
        sr_isotp_tx(res_buf, sizeof(res_buf));
    }

    NVIC_SystemReset();

    // dead branch
    return SR_OK;
}

sr_errno_t x22_read_data_handler(const uint8_t* req, uint32_t length) {
    
}

sr_errno_t x27_sec_access_handler(const uint8_t* req, uint32_t length) {

}

sr_errno_t x34_download_start_handler(const uint8_t* req, uint32_t length) {

}

sr_errno_t x36_trnsfr_data_handler(const uint8_t* req, uint32_t length) {

}

sr_errno_t x37_download_exit_handler(const uint8_t* req, uint32_t length) {

}

sr_errno_t x3e_hbt_handler(const uint8_t* req, uint32_t length) {

}
