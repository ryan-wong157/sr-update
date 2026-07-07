#include "drivers/isotp.h"
#include "drivers/can_driver.h"
#include "drivers/dwt.h"
#include "drivers/errno.h"
#include "drivers/can_config.h"

// TEMP: SUPPORT 1 SESSION and 1 can peripheral. (probably no need at all to support more than 1)
static isotp_session_t isotp_session;
static sr_fdcan_handle_t* fdcan_handle;

// Per session details
static uint8_t rx_buf[2048];
static uint8_t tx_buf[16]; // just for a test
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

// =================================================================================================
// PUBLIC INTERFACE FUNCS
// =================================================================================================
void sr_isotp_init(sr_fdcan_handle_t* handle, isotp_format_t frame_format) {
    fdcan_handle = handle;
    isotp_session_init(&isotp_session, ISOTP_FORMAT_NORMAL, tx_buf, sizeof(tx_buf), rx_buf, sizeof(rx_buf));
    sr_dwt_init();
    
    isotp_session.callback_transmission_rx = rx_done_callback;
    isotp_session.callback_entire_tx_done = tx_done_callback;
    isotp_session.callback_error_invalid_frame = err_invalid_frame_callback;
    isotp_session.callback_error_partner_aborted_transfer = err_partner_aborted_transfer_callback;
    isotp_session.callback_error_transmission_too_large = err_transmission_too_large_callback;
    isotp_session.callback_error_consecutive_out_of_order = err_consecutive_out_of_order_callback;
    isotp_session.callback_error_unexpected_frame_type = err_unexpected_frame_type_callback;
}

sr_errno_t sr_isotp_tx(const uint8_t* tx_data, size_t length) {    
    size_t bytes_sent = isotp_session_send(&isotp_session, tx_data, length);
    if (bytes_sent != length) {
        isotp_session_idle(&isotp_session);
        return SR_ISOTP_TX_LEN_ERR;
    }
    // For tx timing requirements. Store in number of clock cycles, not time
    uint32_t cycles_per_us = SystemCoreClock / 1000000U;
    uint32_t req_separation_cycl = 0;
    uint32_t last_send_cycl = 0;
    tx_done_flag = 0;
    while (!tx_done_flag) {
        if (last_err != SR_OK) {
            sr_errno_t bruh = last_err;
            last_err = SR_OK;
            return bruh;
        }
        if (sr_cyccnt() - last_send_cycl >= req_separation_cycl) {
            // start sending, wait for response, update timestamp
            // TEMP: 8 BYTES MAX FOR NOW
            uint8_t send_buf[8];
            uint32_t req_separation_us;
            size_t single_len = isotp_session_can_tx(&isotp_session, send_buf, sizeof(send_buf), &req_separation_us);
            if (single_len > 0) {
                sr_errno_t retval = sr_fdcan_tx_blocking(fdcan_handle, ISOTP_TX_ID, send_buf, single_len);
                if (retval != SR_OK) {
                    // error!!! do something
                }
                last_send_cycl = sr_cyccnt();
                req_separation_cycl = req_separation_us * cycles_per_us; // try to avoid fp math
            }
        }
        // separation time not met, keep trying
    }
    tx_done_flag = 0;
    isotp_session_idle(&isotp_session);
    return SR_OK;
}

sr_errno_t sr_isotp_rx(uint8_t* rx_buff, size_t length, uint32_t* recv_length) {
    // in case any stale transmission which could be half-overwritten already
    rx_done_flag = 0;
    while (1) {
        // critical section cuz the receive buffer should not be overwritten while reading from it 
        HAL_NVIC_DisableIRQ(FDCAN1_IT0_IRQn);
        if (rx_done_flag) {
            if (isotp_session.full_transmission_length > length) {
                rx_done_flag = 0;
                HAL_NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
                return SR_ISOTP_RX_BUFF_TOO_SMALL;
            }
            memcpy(rx_buff, isotp_session.rx_buffer, isotp_session.full_transmission_length);
            *recv_length = isotp_session.full_transmission_length;
            rx_done_flag = 0;
            HAL_NVIC_EnableIRQ(FDCAN1_IT0_IRQn);
            break;
        }
        HAL_NVIC_EnableIRQ(FDCAN1_IT0_IRQn);

        // Do flow control response if needed
        // TEMP: 8 BYTES MAX FOR NOW
        uint8_t frame[8];
        size_t n = isotp_session_can_tx(&isotp_session, frame, sizeof(frame), NULL);
        if (n > 0) {
            sr_fdcan_tx_blocking(fdcan_handle, ISOTP_TX_ID, frame, n);
        }
    }
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
    last_err = SR_ISOTP_INVALID_FRAME;
}

static void err_partner_aborted_transfer_callback(void* context, const uint8_t* msg_data, const size_t msg_length) {
    isotp_session_idle((isotp_session_t*)context);
    last_err = SR_ISOTP_ABORTED_TRANSFER;
}

static void err_transmission_too_large_callback(void* context, const uint8_t* data, const size_t length, const size_t requested_size) {
    isotp_session_idle((isotp_session_t*)context);
    last_err = SR_ISOTP_TRANSMISSION_TOO_LARGE;
}

static void err_consecutive_out_of_order_callback(void* context, const uint8_t* data, const size_t length, const uint8_t expected_index, const uint8_t received_index) {
    isotp_session_idle((isotp_session_t*)context);
    last_err = SR_ISOTP_OUT_OF_ORDER;
}

static void err_unexpected_frame_type_callback(void* context, const uint8_t* msg_data, const size_t msg_length) {
    isotp_session_idle((isotp_session_t*)context);
    last_err = SR_ISOTP_UNEXPECTED_FRAME_TYPE;
}