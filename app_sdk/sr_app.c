#include <string.h>
#include "stm32g4xx_hal.h"
#include "sr_app.h"
#include "image_format.h"

#define SWAP_TYPE_TEST 0x01u
#define SWAP_TYPE_REVERT 0x02u

static volatile const image_trailer_t* const trailer = (volatile const image_trailer_t*)TRAILER_A_START_ADDRESS;
static const image_header_t* const header = (const image_header_t*)FW_SLOT_A_START_ADDRESS;

#define HANDSHAKE_TX_ID(ecu_id) (0x1F000000u | ((uint32_t)(ecu_id) << 12) | 0xFFDu)
#define HANDSHAKE_RX_ID 0x1F000FFDu
#define HANDSHAKE_PERIOD_MS 100u

#define STATUS_WAITING 0x00u
#define STATUS_CONFIRMED 0x01u
#define STATUS_FAILED 0x02u


static uint8_t is_unconfirmed_boot(void) {
    return trailer->trailer_magic.magic == TRAILER_MAGIC
        && (trailer->trailer_swap_info.status == SWAP_TYPE_TEST || trailer->trailer_swap_info.status == SWAP_TYPE_REVERT)
        && trailer->trailer_copy_done.status == 0x01
        && trailer->trailer_image_ok.status != 0x01;
}

static void send_handshake(FDCAN_HandleTypeDef* hfdcan, uint16_t ecu_id, uint8_t status) {
    uint8_t data[4] = {
        status,
        header->ih_img_ver.iv_major,
        header->ih_img_ver.iv_minor,
        header->ih_img_ver.iv_revision
    };
    FDCAN_TxHeaderTypeDef tx_header = {
        .Identifier = HANDSHAKE_TX_ID(ecu_id),
        .IdType = FDCAN_EXTENDED_ID,
        .TxFrameType = FDCAN_DATA_FRAME,
        .DataLength = FDCAN_DLC_BYTES_4,
        .ErrorStateIndicator = FDCAN_ESI_ACTIVE,
        // same frame type the app configured the peripheral for (FrameFormat in fdcan.c)
        .BitRateSwitch = hfdcan->Init.FrameFormat == FDCAN_FRAME_FD_BRS ? FDCAN_BRS_ON : FDCAN_BRS_OFF,
        .FDFormat = hfdcan->Init.FrameFormat == FDCAN_FRAME_CLASSIC ? FDCAN_CLASSIC_CAN : FDCAN_FD_CAN,
        .TxEventFifoControl = FDCAN_NO_TX_EVENTS,
        .MessageMarker = 0
    };
    // a full TX FIFO just drops this one, the next period sends again
    HAL_FDCAN_AddMessageToTxFifoQ(hfdcan, &tx_header, data);
}

// Check for handshake response
static uint8_t is_confirm_received(FDCAN_HandleTypeDef* hfdcan, uint16_t ecu_id) {
    uint8_t confirmed = 0;
    FDCAN_RxHeaderTypeDef rx_header;
    uint8_t data[64];
    while (HAL_FDCAN_GetRxFifoFillLevel(hfdcan, FDCAN_RX_FIFO0) > 0) {
        if (HAL_FDCAN_GetRxMessage(hfdcan, FDCAN_RX_FIFO0, &rx_header, data) != HAL_OK) {
            break;
        }
        if (rx_header.IdType == FDCAN_EXTENDED_ID && rx_header.Identifier == HANDSHAKE_RX_ID
            && rx_header.DataLength >= FDCAN_DLC_BYTES_2
            && (((uint16_t)data[0] << 8) | data[1]) == ecu_id) {
            confirmed = 1;
        }
    }
    return confirmed;
}

static int write_image_ok(void) {
    uint32_t address = (uint32_t)&trailer->trailer_image_ok;
    record_t record;
    memset(&record, 0xFF, sizeof(record));
    record.status = 0x01;
    uint64_t doubleword;
    memcpy(&doubleword, record.raw, sizeof(doubleword));

    if (HAL_FLASH_Unlock() != HAL_OK) return 1;
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    HAL_StatusTypeDef retval = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address, doubleword);
    HAL_FLASH_Lock();
    return retval != HAL_OK || memcmp((const void*)address, record.raw, sizeof(record)) != 0;
}

void sr_app_confirm_boot(FDCAN_HandleTypeDef* hfdcan) {
    if (!is_unconfirmed_boot()) {
        return;
    }

    HAL_FDCAN_DeactivateNotification(hfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE);

    uint16_t ecu_id = header->ih_ecu_id;
    uint32_t last_sent_ms = HAL_GetTick() - HANDSHAKE_PERIOD_MS;
    while (!is_confirm_received(hfdcan, ecu_id)) {
        if (HAL_GetTick() - last_sent_ms >= HANDSHAKE_PERIOD_MS) {
            last_sent_ms = HAL_GetTick();
            send_handshake(hfdcan, ecu_id, STATUS_WAITING);
        }
    }

    // reset IWDG
    IWDG->KR = 0x0000AAAAu;
    int failed = write_image_ok();
    send_handshake(hfdcan, ecu_id, failed ? STATUS_FAILED : STATUS_CONFIRMED);

    // wait until last handshake sent
    uint32_t start_ms = HAL_GetTick();
    while (HAL_GetTick() - start_ms < 10) {}
    HAL_NVIC_SystemReset();
}
