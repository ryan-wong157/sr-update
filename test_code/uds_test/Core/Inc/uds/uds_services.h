#ifndef UDS_SERVICES_H
#define UDS_SERVICES_H

#include <stdint.h>
#include "sr_errno.h"
#include "config/uds_config.h"

// =================================================================================================
// Random defines for per-service stuff
// =================================================================================================

typedef enum {
    SESSION_DEFAULT = 0,
    SESSION_PROGRAMMING
} session_state_t;

// Supported Data IDs
#define DID_BOOTLOADER_VER 0xF180
#define DID_APP_VER 0xF181
// #define DID_LAST_UPDATE_INFO 0xF184
#define DID_CURR_DIAG_SESS 0xF186
#define DID_ECU_NODE_NUMBER 0xF191

// =================================================================================================
// Handlers
// =================================================================================================

// Sends negative response code to client. Use NRC in uds_codes.h.
sr_errno_t uds_send_nrc(uint8_t* tx_buf, uint8_t nrc);

/**
 * @brief UDS service dispatch handlers. Will parse rx buf and respond using tx buf
 * 
 * @param rx_buf - ptr to static rx_buf in uds.c
 * @param rx_length - length of message contained in rx_buf
 * @param tx_buf - ptr to static tx_buf in uds.c
 * @return sr_errno_t 
 */
sr_errno_t test_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf);
sr_errno_t x10_sess_ctrl_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf);
sr_errno_t x11_ecu_rst_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf);
sr_errno_t x22_read_data_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf);
sr_errno_t x27_sec_access_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf);
sr_errno_t x34_download_start_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf);
sr_errno_t x36_trnsfr_data_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf);
sr_errno_t x37_download_exit_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf);
sr_errno_t x3e_hbt_handler(const uint8_t* rx_buf, uint32_t rx_length, uint8_t* tx_buf);

#endif
