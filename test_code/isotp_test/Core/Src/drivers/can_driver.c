// FD-CAN driver functions
// Written by Ryan Wong

#include "drivers/can_driver.h"
#include "drivers/dwt.h"
#include "config/can_config.h"

sr_errno_t sr_fdcan_config(sr_fdcan_handle_t* handle, FDCAN_HandleTypeDef* hfdcan, sr_fdcan_config_t* config_struct) {
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

    handle->hfdcan = hfdcan;
    handle->tx_id_type = config_struct->tx_id_type;
    handle->tx_brs = config_struct->tx_brs;
    handle->tx_frame_format = config_struct->tx_frame_format;
    handle->tx_event_fifo_control = config_struct->tx_event_fifo_control;
    handle->filter_index = 0;
    return SR_OK;
}

sr_errno_t sr_fdcan_filter_add(sr_fdcan_handle_t* handle, uint32_t filter_type, uint32_t filter_config, uint32_t id1, uint32_t id2) {
    uint32_t num_filters = handle->hfdcan->Init.StdFiltersNbr;

    if (handle->filter_index < num_filters) {
        FDCAN_FilterTypeDef filter = {
            .IdType=handle->tx_id_type,
            .FilterIndex=handle->filter_index,
            .FilterType=filter_type,
            .FilterConfig=filter_config,
            .FilterID1=id1,
            .FilterID2=id2
        };
        if (HAL_FDCAN_ConfigFilter(handle->hfdcan, &filter) != HAL_OK) {
            return SR_FDCAN_CFG_FILTER_ERR;
        }
        handle->filter_index++;
        return SR_OK;
    } else {
        return SR_FDCAN_OVER_MAX_FILTERS;
    }
}

sr_errno_t sr_fdcan_tx(sr_fdcan_handle_t* handle, uint32_t can_id, uint8_t* data, uint32_t length) {
    uint32_t dlc = sr_fdcan_bytes_to_dlc(length);
    if (dlc == UINT32_MAX || (handle->tx_frame_format == FDCAN_CLASSIC_CAN && dlc > FDCAN_DLC_BYTES_8)) {
        return SR_FDCAN_TX_MSG_TOO_BIG;
    }

    FDCAN_TxHeaderTypeDef header = {
        .Identifier=can_id,
        .IdType=handle->tx_id_type,
        .TxFrameType=FDCAN_DATA_FRAME,
        .DataLength=dlc,
        .ErrorStateIndicator=FDCAN_ESI_ACTIVE,
        .BitRateSwitch=handle->tx_brs,
        .FDFormat=handle->tx_frame_format,
        .TxEventFifoControl=handle->tx_event_fifo_control,
        .MessageMarker=0
    };

    if (HAL_FDCAN_AddMessageToTxFifoQ(handle->hfdcan, &header, data) != HAL_OK) {
        return SR_FDCAN_TX_ERR;
    }
    return SR_OK;
}

sr_errno_t sr_fdcan_tx_blocking(sr_fdcan_handle_t* handle, uint32_t can_id, uint8_t* data, uint32_t length) {
    uint32_t cycles_per_us = SystemCoreClock / 1000000U;
    uint32_t timeout_start_cycl = sr_cyccnt();
    uint32_t timeout_cycls = FDCAN_TIMEOUT_MS * 1000 * cycles_per_us;
    sr_errno_t retval = sr_fdcan_tx(handle, can_id, data, length);
    if (retval != SR_OK) {
        return retval;
    }
    // FIFO holds 3 messages max
    while (HAL_FDCAN_GetTxFifoFreeLevel(handle->hfdcan) < 3) {
        // do nothing
        if (sr_cyccnt() - timeout_start_cycl >= timeout_cycls) {
            return SR_FDCAN_TIMEOUT;
        }
    }

    return SR_OK;
}

static const uint32_t dlc_to_bytes_table[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 32, 48, 64
};

uint32_t sr_fdcan_dlc_to_bytes(uint32_t dlc) {
    if (dlc >= sizeof(dlc_to_bytes_table) / sizeof(dlc_to_bytes_table[0])) {
        return 0;
    }
    return dlc_to_bytes_table[dlc];
}

uint32_t sr_fdcan_bytes_to_dlc(uint32_t length) {
    if (length > 64) {
        return UINT32_MAX;
    }
    for (uint32_t dlc = 0; dlc < sizeof(dlc_to_bytes_table) / sizeof(dlc_to_bytes_table[0]); dlc++) {
        if (dlc_to_bytes_table[dlc] >= length) {
            return dlc;
        }
    }
    return UINT32_MAX;
}