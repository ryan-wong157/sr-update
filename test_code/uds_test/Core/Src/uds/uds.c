#include "uds/uds.h"
#include "uds/uds_services.h"
#include "uds/uds_codes.h"
#include "isotp/isotp.h"
#include "config/uds_config.h"
#include "sr_errno.h"

// tx and rx buffers used by isotp layer to put stuff in 
static uint8_t isotp_tx_buf[CFG_UDS_TX_BUF_SIZE];
static uint8_t isotp_rx_buf[CFG_UDS_RX_BUF_SIZE];

static sr_errno_t uds_dispatch(uint32_t length);

sr_errno_t sr_uds_server_start(FDCAN_HandleTypeDef* hfdcan) {
    sr_errno_t retval = sr_isotp_start(
        hfdcan, 
        ISOTP_FORMAT_NORMAL, 
        isotp_tx_buf, 
        sizeof(isotp_tx_buf), 
        isotp_rx_buf, 
        sizeof(isotp_rx_buf)
    );
    if (retval != SR_OK) {
        return retval;
    }

    while (1) {
        uint32_t bytes_received;
        retval = sr_isotp_rx(&bytes_received);

        switch (retval) {
            case SR_OK:
                uds_dispatch(bytes_received);
                break;

            case ERR_ISOTP_TIMEOUT:
            case ERR_ISOTP_TX_LEN:
            case ERR_ISOTP_INVALID_FRAME:
            case ERR_ISOTP_ABORTED_TRANSFER:
            case ERR_ISOTP_TRANSMISSION_TOO_LARGE:
            case ERR_ISOTP_OUT_OF_ORDER:
            case ERR_ISOTP_UNEXPECTED_FRAME_TYPE:
            case ERR_ISOTP_RX_BUFF_TOO_SMALL:
            case ERR_ISOTP_TX_INTERRUPTED_BY_RX:                
                break;

            default:
                for (int i = 0; i < 5; i++) {
                    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
                    HAL_Delay(1000);
                    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
                    HAL_Delay(1000);
                }
                return retval;
        }
    }
}

static sr_errno_t uds_dispatch(uint32_t length) {
    uint8_t serv_id_byte = isotp_rx_buf[0];

    switch (serv_id_byte) {
        case 0x80:
            return test_handler(isotp_rx_buf, length);
        case SID_SESS_CTRL_RQ:
            return x10_sess_ctrl_handler(isotp_rx_buf, length);
        case SID_ECU_RST_RQ:
            return x11_ecu_rst_handler(isotp_rx_buf, length);
        case SID_READ_DATA_RQ:
            return x22_read_data_handler(isotp_rx_buf, length);
        case SID_SEC_ACCESS_RQ:
            return x27_sec_access_handler(isotp_rx_buf, length);
        case SID_DOWNLOAD_START_RQ:
            return x34_download_start_handler(isotp_rx_buf, length);
        case SID_TRNSFR_DATA_RQ:
            return x36_trnsfr_data_handler(isotp_rx_buf, length);
        case SID_DOWNLOAD_EXIT_RQ:
            return x37_download_exit_handler(isotp_rx_buf, length);
        case SID_TESTER_HBT_RQ:
            return x3e_hbt_handler(isotp_rx_buf, length);
        default: {
            return uds_send_nrc(NRC_SERVICE_NOT_SUPPORTED);
        }
    }
}
