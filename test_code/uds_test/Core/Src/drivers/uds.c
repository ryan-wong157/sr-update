#include "drivers/uds.h"
#include "drivers/isotp.h"
#include "config/uds_config.h"

// tx and rx buffers used by isotp layer to put stuff in 
static uint8_t isotp_tx_buf[CFG_UDS_TX_BUF_SIZE];
static uint8_t isotp_rx_buf[CFG_UDS_RX_BUF_SIZE];

static sr_errno_t sr_uds_dispatch(uint32_t length);

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
                sr_uds_dispatch(bytes_received);
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


static sr_errno_t sr_uds_dispatch(uint32_t length) {
    (void)length;

    const char response[] = "brochachowacho";
    return sr_isotp_tx((const uint8_t*)response, sizeof(response) - 1);
}
