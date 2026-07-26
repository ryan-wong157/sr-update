#include "uds/uds_services.h"
#include "isotp/isotp.h"

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

}

sr_errno_t x11_ecu_rst_handler(const uint8_t* req, uint32_t length) {

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
