#include <string.h>
#include "isotp/isotp.h"
#include "mcu_interface/can_driver.h"
#include "mcu_interface/sys_misc.h"
#include "sr_errno.h"
#include "common_config/isotp_config.h"

static isotp_session_t isotp_session;

static volatile sr_errno_t last_err = SR_OK;
static volatile uint8_t rx_done_flag = 0; // 0 no, 1 yes
static volatile uint8_t tx_done_flag = 0;
static volatile uint32_t rx_cf_count = 0; // consecutive frames accepted, used to restart the rx timeout
static volatile uint32_t rx_fc_count = 0; // flow controls accepted, used to restart the tx timeout

// Callback forward decls
static void rx_done_callback(void* context);
static void tx_done_callback(void* context);
static void peek_consecutive_frame_callback(void* context, const uint8_t* data, const size_t length, const size_t start_idx);
static void peek_flow_control_frame_callback(void* context);
static void err_invalid_frame_callback(void* context, const isotp_spec_frame_type_t rx_frame_type, const uint8_t* msg_data, const size_t msg_length);
static void err_partner_aborted_transfer_callback(void* context, const uint8_t* msg_data, const size_t msg_length);
static void err_transmission_too_large_callback(void* context, const uint8_t* data, const size_t length, const size_t requested_size);
static void err_consecutive_out_of_order_callback(void* context, const uint8_t* data, const size_t length, const uint8_t expected_index, const uint8_t received_index);
static void err_unexpected_frame_type_callback(void* context, const uint8_t* msg_data, const size_t msg_length);
static void err_tx_interrupted_by_rx_callback(void* context, const isotp_spec_frame_type_t rx_frame_type, const uint8_t* msg_data, const size_t msg_length);

// =================================================================================================
// PUBLIC INTERFACE FUNCS
// =================================================================================================
sr_errno_t sr_isotp_start(uint8_t* tx_buf, uint32_t tx_len, uint8_t* rx_buf, uint32_t rx_len) {
    sr_errno_t retval;

    retval = sr_fdcan_configure();
    if (retval != SR_OK) {
        return retval;
    }

    retval = sr_fdcan_start();
    if (retval != SR_OK) {
        return retval;
    }

    isotp_session_init(&isotp_session, ISOTP_FORMAT, tx_buf, tx_len, rx_buf, rx_len);
    sr_counter_start();

    isotp_session.callback_transmission_rx = rx_done_callback;
    isotp_session.callback_entire_tx_done = tx_done_callback;
    isotp_session.callback_peek_consecutive_frame = peek_consecutive_frame_callback;
    isotp_session.callback_peek_flow_control_frame = peek_flow_control_frame_callback;
    isotp_session.callback_error_invalid_frame = err_invalid_frame_callback;
    isotp_session.callback_error_partner_aborted_transfer = err_partner_aborted_transfer_callback;
    isotp_session.callback_error_transmission_too_large = err_transmission_too_large_callback;
    isotp_session.callback_error_consecutive_out_of_order = err_consecutive_out_of_order_callback;
    isotp_session.callback_error_unexpected_frame_type = err_unexpected_frame_type_callback;
    isotp_session.callback_error_tx_interrupted_by_rx = err_tx_interrupted_by_rx_callback;
    return SR_OK;
}

sr_errno_t sr_isotp_tx(const uint8_t* tx_data, size_t length) {
    last_err = SR_OK;
    size_t bytes_sent = isotp_session_send(&isotp_session, tx_data, length);
    if (bytes_sent != length) {
        isotp_session_idle(&isotp_session);
        return ERR_ISOTP_TX_LEN;
    }
    // For tx timing requirements
    uint32_t req_separation_us = 0;
    uint32_t last_send_cyc = sr_cyccnt();
    uint32_t timeout_start_cyc = last_send_cyc;
    uint32_t timeout_us = CFG_ISOTP_TIMEOUT_MS * 1000U;
    uint32_t last_fc_count = rx_fc_count;
    tx_done_flag = 0;

    while (!tx_done_flag) {
        if (last_err != SR_OK) {
            sr_errno_t retval = last_err;
            last_err = SR_OK;
            return retval;
        }
        // every flow control restarts the wait for the next one
        uint32_t fc_count = rx_fc_count;
        if (fc_count != last_fc_count) {
            last_fc_count = fc_count;
            timeout_start_cyc = sr_cyccnt();
        }
        if (sr_micros_since(timeout_start_cyc) >= timeout_us) {
            isotp_session_idle(&isotp_session);
            return ERR_ISOTP_TIMEOUT;
        }
        if (sr_micros_since(last_send_cyc) >= req_separation_us) {
            // TEMP: 8 BYTES MAX FOR NOW (change to 64 later for fd can)
            uint8_t send_buf[8];
            size_t single_len = isotp_session_can_tx(&isotp_session, send_buf, sizeof(send_buf), &req_separation_us);
            if (single_len > 0) {
                sr_errno_t retval = sr_fdcan_tx_blocking(CFG_ISOTP_TX_ID, send_buf, single_len);
                if (retval != SR_OK) {
                    isotp_session_idle(&isotp_session);
                    return retval;
                }
                last_send_cyc = sr_cyccnt();
                timeout_start_cyc = last_send_cyc;
            }
        }
        // separation time not met, keep trying
    }
    tx_done_flag = 0;
    isotp_session_idle(&isotp_session);
    return SR_OK;
}

// Note: rx_done_flag being set STOPS the CAN ISR from touching the receive buffer.
// This means a single frame msg will be waiting to be claimed, but multi-frame will timeout on client side
sr_errno_t sr_isotp_rx(uint8_t* out_buf, uint32_t out_len, uint32_t* recv_length) {
    uint32_t timeout_start_cyc = sr_cyccnt();
    uint32_t timeout_us = CFG_ISOTP_TIMEOUT_MS * 1000U;
    uint32_t last_cf_count = rx_cf_count;
    while (!rx_done_flag) {
        if (last_err != SR_OK) {
            sr_errno_t retval = last_err;
            last_err = SR_OK;
            if (retval == ERR_ISOTP_TRANSMISSION_TOO_LARGE) {
                // TEMP: 8 BYTES MAX FOR NOW
                uint8_t fc_overflow[8] = {0x32, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
                sr_fdcan_tx_blocking(CFG_ISOTP_TX_ID, fc_overflow, sizeof(fc_overflow));
            }
            return retval;
        }
        uint32_t cf_count = rx_cf_count;
        if (cf_count != last_cf_count) {
            last_cf_count = cf_count;
            timeout_start_cyc = sr_cyccnt();
        }
        if (sr_micros_since(timeout_start_cyc) >= timeout_us) {
            isotp_session_idle(&isotp_session);
            return ERR_ISOTP_TIMEOUT;
        }
        // respond with any FC frames if there's any
        // TEMP: 8 BYTES MAX FOR NOW
        uint8_t frame[8];
        size_t n = isotp_session_can_tx(&isotp_session, frame, sizeof(frame), NULL);
        if (n > 0 && last_err == SR_OK && !rx_done_flag) {
            sr_errno_t retval = sr_fdcan_tx_blocking(CFG_ISOTP_TX_ID, frame, n);
            if (retval != SR_OK) {
                isotp_session_idle(&isotp_session);
                return retval;
            }
            timeout_start_cyc = sr_cyccnt();
        }
    }

    // Note if incoming msg > rx buf, error in callback, so no need to check here
    if (isotp_session.full_transmission_length == 0) {
        rx_done_flag = 0;
        isotp_session_idle(&isotp_session);
        return ERR_ISOTP_INVALID_FRAME;
    }

    if (isotp_session.full_transmission_length > out_len) {
        rx_done_flag = 0;
        isotp_session_idle(&isotp_session);
        return ERR_ISOTP_RX_BUFF_TOO_SMALL;
    }

    memcpy(out_buf, isotp_session.rx_buffer, isotp_session.full_transmission_length);
    *recv_length = isotp_session.full_transmission_length;
    rx_done_flag = 0;
    isotp_session_idle(&isotp_session);
    return SR_OK;
}

// =================================================================================================
// PRIVATE CALLBACKS
// =================================================================================================
static void rx_done_callback(void* context) {
    rx_done_flag = 1;
    return;
}

static void tx_done_callback(void* context) {
    tx_done_flag = 1;
    return;
}

static void peek_consecutive_frame_callback(void* context, const uint8_t* data, const size_t length, const size_t start_idx) {
    rx_cf_count++;
}

static void peek_flow_control_frame_callback(void* context) {
    rx_fc_count++;
}

static void err_invalid_frame_callback(void* context, const isotp_spec_frame_type_t rx_frame_type, const uint8_t* msg_data, const size_t msg_length) {
    isotp_session_idle((isotp_session_t*)context);
    last_err = ERR_ISOTP_INVALID_FRAME;
}

static void err_partner_aborted_transfer_callback(void* context, const uint8_t* msg_data, const size_t msg_length) {
    isotp_session_idle((isotp_session_t*)context);
    last_err = ERR_ISOTP_ABORTED_TRANSFER;
}

static void err_transmission_too_large_callback(void* context, const uint8_t* data, const size_t length, const size_t requested_size) {
    isotp_session_idle((isotp_session_t*)context);
    last_err = ERR_ISOTP_TRANSMISSION_TOO_LARGE;
}

static void err_consecutive_out_of_order_callback(void* context, const uint8_t* data, const size_t length, const uint8_t expected_index, const uint8_t received_index) {
    isotp_session_idle((isotp_session_t*)context);
    last_err = ERR_ISOTP_OUT_OF_ORDER;
}

static void err_unexpected_frame_type_callback(void* context, const uint8_t* msg_data, const size_t msg_length) {
    isotp_session_idle((isotp_session_t*)context);
    last_err = ERR_ISOTP_UNEXPECTED_FRAME_TYPE;
}

static void err_tx_interrupted_by_rx_callback(void* context, const isotp_spec_frame_type_t rx_frame_type, const uint8_t* msg_data, const size_t msg_length) {
    isotp_session_idle((isotp_session_t*)context);
    last_err = ERR_ISOTP_TX_INTERRUPTED_BY_RX;
}

// =================================================================================================
// Callback for new CAN message
// =================================================================================================
// Frames ISO 15765-2 says to ignore, which the library would otherwise act on
static uint8_t frame_is_malformed(const uint8_t* data, size_t length) {
    if (length == 0) {
        return 1;
    }
    switch ((data[ISOTP_SPEC_FRAME_TYPE_IDX] & ISOTP_SPEC_FRAME_TYPE_MASK) >> ISOTP_SPEC_FRAME_TYPE_SHIFT) {
        case ISOTP_SPEC_FRAME_FIRST: {
            // TEMP: 8 BYTES MAX FOR NOW
            // a first frame fills the CAN frame and announces more than a single frame can carry
            if (length < 8) {
                return 1;
            }
            size_t ff_dl = ((size_t)(data[ISOTP_SPEC_FRAME_FIRST_LEN_MSB_IDX] & ISOTP_SPEC_FRAME_FIRST_LEN_MSB_MASK) << 8)
                         | (data[ISOTP_SPEC_FRAME_FIRST_LEN_LSB_IDX] & ISOTP_SPEC_FRAME_FIRST_LEN_LSB_MASK);
            return ff_dl < 8;
        }
        case ISOTP_SPEC_FRAME_FLOW_CONTROL:
            // without block size and STmin the library would take it as "send everything"
            return length < ISOTP_SPEC_FRAME_FLOWCONTROL_HEADER_END;
        default:
            return 0;
    }
}

// CAN ISR should call this
void sr_isotp_can_isr_callback(uint8_t* data, size_t length) {
    if (!rx_done_flag && !frame_is_malformed(data, length)) {
        isotp_session_can_rx(&isotp_session, data, length);
    }   
    // don't touch buffer if rx not done
}