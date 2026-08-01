#include <stdint.h>
#include "uds/uds_codes.h"
#include "isotp/isotp.h"

sr_errno_t uds_send_nrc(uint8_t* tx_buf, uint8_t original_sid, uint8_t nrc) {
    tx_buf[0] = SID_NEG_RES;
    tx_buf[1] = original_sid;
    tx_buf[2] = nrc;
    return sr_isotp_tx(tx_buf, 3);
}
