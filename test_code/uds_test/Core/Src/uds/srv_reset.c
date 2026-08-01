#include "main.h"
#include "uds/srv_reset.h"
#include "uds/uds_codes.h"
#include "isotp/isotp.h"

sr_errno_t x11_ecu_rst_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf) {
    if (rx_length != 2) {
        return uds_send_nrc(tx_buf, SID_ECU_RST_RQ, NRC_INCORRECT_MSG_LENGTH_OR_INVALID_FORMAT);
    }

    uint8_t sfb = rx_buf[1];
    uint8_t suppress = sfb >> 7;

    if (sfb != 0x01) {
        return uds_send_nrc(tx_buf, SID_ECU_RST_RQ, NRC_SUB_FUNCTION_NOT_SUPPORTED);
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