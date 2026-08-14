#include <string.h>
#include "isotp/isotp.h"
#include "drivers/dwt.h"
#include "sr_errno.h"
#include "config/can_config.h"

// own 1 session and 1 can peripheral
static isotp_session_t isotp_session;
static sr_fdcan_handle_t fdcan_handle;

static volatile sr_errno_t last_err = SR_OK;
static volatile uint8_t rx_done_flag = 0; // 0 no, 1 yes
static volatile uint8_t tx_done_flag = 0;

// Callback forward decls
static void rx_done_callback(void* context);
static void tx_done_callback(void* context);
static void err_invalid_frame_callback(void* context, const isotp_spec_frame_type_t rx_frame_type, const uint8_t* msg_data, const size_t msg_length);
static void err_partner_aborted_transfer_callback(void* context, const uint8_t* msg_data, const size_t msg_length);
static void err_transmission_too_large_callback(void* context, const uint8_t* data, const size_t length, const size_t requested_size);
static void err_consecutive_out_of_order_callback(void* context, const uint8_t* data, const size_t length, const uint8_t expected_index, const uint8_t received_index);
static void err_unexpected_frame_type_callback(void* context, const uint8_t* msg_data, const size_t msg_length);
static void err_tx_interrupted_by_rx_callback(void* context, const isotp_spec_frame_type_t rx_frame_type, const uint8_t* msg_data, const size_t msg_length);

// =================================================================================================
// PUBLIC INTERFACE FUNCS
// =================================================================================================
sr_errno_t sr_isotp_start(FDCAN_HandleTypeDef* hfdcan, isotp_format_t frame_format, uint8_t* tx_buf, uint32_t tx_len, uint8_t* rx_buf, uint32_t rx_len) {
    sr_errno_t retval;

    retval = sr_fdcan_configure(&fdcan_handle, hfdcan);
    if (retval != SR_OK) {
        return retval;
    }
    retval = sr_fdcan_filter_add(&fdcan_handle, FDCAN_FILTER_RANGE, FDCAN_FILTER_TO_RXFIFO0, CFG_ISOTP_RX_ID, CFG_ISOTP_RX_ID);
    if (retval != SR_OK) {
        return retval;
    }
    if (HAL_FDCAN_Start(hfdcan) != HAL_OK) {
        return ERR_FDCAN_START;
    }

    isotp_session_init(&isotp_session, frame_format, tx_buf, tx_len, rx_buf, rx_len);
    sr_dwt_init();

    isotp_session.callback_transmission_rx = rx_done_callback;
    isotp_session.callback_entire_tx_done = tx_done_callback;
    isotp_session.callback_error_invalid_frame = err_invalid_frame_callback;
    isotp_session.callback_error_partner_aborted_transfer = err_partner_aborted_transfer_callback;
    isotp_session.callback_error_transmission_too_large = err_transmission_too_large_callback;
    isotp_session.callback_error_consecutive_out_of_order = err_consecutive_out_of_order_callback;
    isotp_session.callback_error_unexpected_frame_type = err_unexpected_frame_type_callback;
    isotp_session.callback_error_tx_interrupted_by_rx = err_tx_interrupted_by_rx_callback;
    return SR_OK;
}

sr_errno_t sr_isotp_tx(const uint8_t* tx_data, size_t length) {
    size_t bytes_sent = isotp_session_send(&isotp_session, tx_data, length);
    if (bytes_sent != length) {
        isotp_session_idle(&isotp_session);
        return ERR_ISOTP_TX_LEN;
    }
    // For tx timing requirements. Store in number of clock cycles instead of time
    uint32_t cycles_per_us = SystemCoreClock / 1000000U;
    uint32_t req_separation_cycls = 0;
    uint32_t last_send_cycl = 0;
    uint32_t timeout_start_cycl = sr_cyccnt();
    uint32_t timeout_cycls = CFG_ISOTP_TIMEOUT_MS * 1000U * cycles_per_us;
    tx_done_flag = 0;

    while (!tx_done_flag) {
        if (last_err != SR_OK) {
            sr_errno_t retval = last_err;
            last_err = SR_OK;
            return retval;
        }
        if (sr_cyccnt() - timeout_start_cycl >= timeout_cycls) {
            isotp_session_idle(&isotp_session);
            return ERR_ISOTP_TIMEOUT;
        }
        if (sr_cyccnt() - last_send_cycl >= req_separation_cycls) {
            // TEMP: 8 BYTES MAX FOR NOW (change to 64 later for fd can)
            uint8_t send_buf[8];
            uint32_t req_separation_us;
            size_t single_len = isotp_session_can_tx(&isotp_session, send_buf, sizeof(send_buf), &req_separation_us);
            if (single_len > 0) {
                sr_errno_t retval = sr_fdcan_tx_blocking(&fdcan_handle, CFG_ISOTP_TX_ID, send_buf, single_len);
                if (retval != SR_OK) {
                    isotp_session_idle(&isotp_session);
                    return retval;
                }
                last_send_cycl = sr_cyccnt();
                timeout_start_cycl = last_send_cycl;
                req_separation_cycls = req_separation_us * cycles_per_us;
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
sr_errno_t sr_isotp_rx(uint32_t* recv_length) {
    uint32_t cycles_per_us = SystemCoreClock / 1000000U;
    uint32_t timeout_start_cycl = sr_cyccnt();
    uint32_t timeout_cycls = CFG_ISOTP_TIMEOUT_MS * 1000U * cycles_per_us;
    while (!rx_done_flag) {
        if (last_err != SR_OK) {
            sr_errno_t retval = last_err;
            last_err = SR_OK;
            return retval;
        }
        if (sr_cyccnt() - timeout_start_cycl >= timeout_cycls) {
            isotp_session_idle(&isotp_session);
            return ERR_ISOTP_TIMEOUT;
        }
        // respond with any FC frames if there's any
        // TEMP: 8 BYTES MAX FOR NOW
        uint8_t frame[8];
        size_t n = isotp_session_can_tx(&isotp_session, frame, sizeof(frame), NULL);
        if (n > 0) {
            sr_errno_t retval = sr_fdcan_tx_blocking(&fdcan_handle, CFG_ISOTP_TX_ID, frame, n);
            if (retval != SR_OK) {
                isotp_session_idle(&isotp_session);
                return retval;
            }
            timeout_start_cycl = sr_cyccnt();
        }
    }

    // Note if incoming msg > rx buf, error in callback, so no need to check here
    if (isotp_session.full_transmission_length == 0) {
        rx_done_flag = 0;
        isotp_session_idle(&isotp_session);
        return ERR_ISOTP_INVALID_FRAME;
    }

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
// CAN ISR should call this
void sr_isotp_can_isr_callback(uint8_t* data, size_t length) {
    if (!rx_done_flag) {
        isotp_session_can_rx(&isotp_session, data, length);
    }   
    // don't touch buffer if rx not done
}