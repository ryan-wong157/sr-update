#ifndef UDS_SERVICES_H
#define UDS_SERVICES_H

#include <stdint.h>
#include "config/errno.h"

/**
 * Service handlers. Each receives the isotp rx buffer holding the request
 * (req[0] is the SID) and the number of bytes received, and is responsible
 * for sending its own response via sr_isotp_tx.
 */
sr_errno_t test_handler(const uint8_t* req, uint32_t length);
sr_errno_t x10_sess_ctrl_handler(const uint8_t* req, uint32_t length);
sr_errno_t x11_ecu_rst_handler(const uint8_t* req, uint32_t length);
sr_errno_t x22_read_data_handler(const uint8_t* req, uint32_t length);
sr_errno_t x27_sec_access_handler(const uint8_t* req, uint32_t length);
sr_errno_t x34_download_start_handler(const uint8_t* req, uint32_t length);
sr_errno_t x36_trnsfr_data_handler(const uint8_t* req, uint32_t length);
sr_errno_t x37_download_exit_handler(const uint8_t* req, uint32_t length);
sr_errno_t x3e_hbt_handler(const uint8_t* req, uint32_t length);

#endif
