// FD-CAN driver functions
// Written by Ryan Wong

#include "drivers/can_driver.h"

static uint32_t tx_id_type;
static uint32_t tx_brs;
static uint32_t tx_frame_format;
static uint32_t tx_event_fifo_control;

sr_errno_t sr_fdcan_config(FDCAN_HandleTypeDef* hfdcan, sr_fdcan_config_t* config_struct) {
    HAL_StatusTypeDef retval;
    // always always reject messages that don't pass through filter
    retval = HAL_FDCAN_ConfigGlobalFilter(hfdcan, FDCAN_REJECT, FDCAN_REJECT, FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE);
    if (retval != HAL_OK) {
        return SR_FDCAN_CFG_GLOBAL_FILTER_ERR;
    }

    // TEMP: CONFIGURE BOTH??
    // Either FDCAN_RX_FIFO_OVERWRITE or FDCAN_RX_FIFO_BLOCKING, let user configure
    retval = HAL_FDCAN_ConfigRxFifoOverwrite(hfdcan, FDCAN_RX_FIFO0, config_struct->fifo_overwrite);
    if (retval != HAL_OK) {
        return SR_FDCAN_CFG_FIFO_OVERWRITE_ERR;
    }
    retval = HAL_FDCAN_ConfigRxFifoOverwrite(hfdcan, FDCAN_RX_FIFO1, config_struct->fifo_overwrite);
    if (retval != HAL_OK) {
        return SR_FDCAN_CFG_FIFO_OVERWRITE_ERR;
    }

    // TEMP: ALWAYS ONLY ACTIVATE FIF0 IRQ for now
    retval = HAL_FDCAN_ActivateNotification(hfdcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
    if (retval != HAL_OK) {
        return SR_FDCAN_ACTIVATE_NOTIF_ERR;
    }

    tx_id_type = config_struct->tx_id_type;
    tx_brs = config_struct->tx_brs;
    tx_frame_format = config_struct->tx_frame_format;
    tx_event_fifo_control = config_struct->tx_event_fifo_control;
    return SR_OK;
}

sr_errno_t sr_fdcan_filter_add(FDCAN_HandleTypeDef* hfdcan, uint32_t filter_type, uint32_t filter_config, uint32_t id1, uint32_t id2) {
    static uint32_t filter_index = 0;
    uint32_t num_filters = hfdcan->Init.StdFiltersNbr;

    if (filter_index < num_filters) {
        FDCAN_FilterTypeDef filter = {
            .IdType=tx_id_type,
            .FilterIndex=filter_index,
            .FilterType=filter_type,
            .FilterConfig=filter_config,
            .FilterID1=id1,
            .FilterID2=id2
        };
        if (HAL_FDCAN_ConfigFilter(hfdcan, &filter) != HAL_OK) {
            for (int i = 0; i < 10; i++) {
                HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
                HAL_Delay(500);
                HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
                HAL_Delay(500);
            }
            return SR_FDCAN_CFG_FILTER_ERR;
        }
        filter_index++;
        return SR_OK;
    } else {
        return SR_FDCAN_OVER_MAX_FILTERS;
    }
}

sr_errno_t sr_fdcan_tx(FDCAN_HandleTypeDef* hfdcan, uint32_t can_id, uint8_t* data, uint32_t length) {
    if (length > FDCAN_DLC_BYTES_8) {
        return SR_FDCAN_TX_MSG_TOO_BIG;
    }

    FDCAN_TxHeaderTypeDef header = {
        .Identifier=can_id,
        .IdType=tx_id_type,
        .TxFrameType=FDCAN_DATA_FRAME,
        .DataLength=length,
        .ErrorStateIndicator=FDCAN_ESI_ACTIVE,
        .BitRateSwitch=tx_brs,
        .FDFormat=tx_frame_format,
        .TxEventFifoControl=tx_event_fifo_control,
        .MessageMarker=0
    };

    if (HAL_FDCAN_AddMessageToTxFifoQ(hfdcan, &header, data) != HAL_OK) {
        return SR_FDCAN_TX_ERR;
    }
    return SR_OK;
}

sr_errno_t sr_fdcan_tx_blocking(FDCAN_HandleTypeDef* hfdcan, uint32_t can_id, uint8_t* data, uint32_t length) {
    sr_errno_t retval = sr_fdcan_tx(hfdcan, can_id, data, length);
    if (retval != SR_OK) {
        return retval;
    }
    // FIFO holds 3 messages max
    while (HAL_FDCAN_GetTxFifoFreeLevel(hfdcan) < 3) {
        // do nothing
    }

    return SR_OK;
}