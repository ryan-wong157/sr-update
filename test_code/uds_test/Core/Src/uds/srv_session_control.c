#include <stdint.h>
#include "uds/srv_session_control.h"
#include "uds/srv_security_access.h"
#include "uds/uds_codes.h"
#include "config/uds_config.h"
#include "isotp/isotp.h"

static session_state_t session_state = SESSION_DEFAULT;

/* TODO:
- s3server timeout check (probably done in uds.c layer hmm, but then uds.c will have to know the session state)
*/

void set_session(session_state_t sesn) {
    session_state = sesn;
}

session_state_t get_session() {
    return session_state;
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

    // Reset security no matter what - every session change re-locks
    x27_on_session_change();
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
