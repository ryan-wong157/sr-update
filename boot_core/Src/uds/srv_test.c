#include <stdint.h>
#include <string.h>
#include "sr_errno.h"
#include "uds/srv_test.h"
#include "uds/uds_codes.h"
#include "isotp/isotp.h"

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