// FD-CAN driver functions
// Written by Ryan Wong

#include "main.h"
#include "fdcan.h"
#include "mcu_interface/can_driver.h"
#include "mcu_interface/sys_misc.h"
#include "can_helper.h"
#include "config/can_config.h"

// =================================================================================================
// Defines and Globals
// =================================================================================================
/**
 * @brief static fd-can stuff for the can periperal
 */
typedef struct {
    uint32_t tx_id_type;
    uint32_t tx_brs;
    uint32_t tx_frame_format;
    uint32_t tx_event_fifo_control;
    uint32_t filter_index;
} fdcan_data_t;

// adjust this if not using FD CAN 1
static FDCAN_HandleTypeDef* peripheral_handle = &hfdcan1;
static fdcan_data_t fdcan_data;

// =================================================================================================
// Helpers
// =================================================================================================
static HAL_StatusTypeDef filter_add(uint32_t filter_type, uint32_t filter_config, uint32_t id1, uint32_t id2) {
    HAL_StatusTypeDef ret;
    uint32_t num_filters = peripheral_handle->Init.StdFiltersNbr;

    if (fdcan_data.filter_index < num_filters) {
        FDCAN_FilterTypeDef filter = {
            .IdType=fdcan_data.tx_id_type,
            .FilterIndex=fdcan_data.filter_index,
            .FilterType=filter_type,
            .FilterConfig=filter_config,
            .FilterID1=id1,
            .FilterID2=id2
        };
        ret = HAL_FDCAN_ConfigFilter(peripheral_handle, &filter);
        if (ret != HAL_OK) {
            return ret;
        }
        fdcan_data.filter_index++;
        return SR_OK;
    } else {
        return ERR_FDCAN_OVER_MAX_FILTERS;
    }
}

// =================================================================================================
// Imeplementation
// =================================================================================================
sr_errno_t sr_fdcan_configure() {
    HAL_StatusTypeDef retval;
    fdcan_data.tx_id_type = CFG_FDCAN_TX_ID_TYPE;
    fdcan_data.tx_brs = CFG_FDCAN_TX_BRS;
    fdcan_data.tx_frame_format = CFG_FDCAN_TX_FRAME_FORMAT;
    fdcan_data.tx_event_fifo_control = CFG_FDCAN_TX_EVENT_FIFO_CTRL;
    fdcan_data.filter_index = 0;

    // always always reject messages that don't pass through filter
    retval = HAL_FDCAN_ConfigGlobalFilter(peripheral_handle, FDCAN_REJECT, FDCAN_REJECT, FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE);
    if (retval != HAL_OK) {
        return ERR_FDCAN_CFG_GLOBAL_FILTER;
    }

    // config individual filters
    retval = filter_add(FDCAN_FILTER_RANGE, FDCAN_FILTER_TO_RXFIFO0, CFG_ISOTP_RX_ID, CFG_ISOTP_RX_ID);
    if (retval != HAL_OK) {
        return ERR_FDCAN_CFG_FILTER;
    }

    // only FIFO0 is used
    retval = HAL_FDCAN_ConfigRxFifoOverwrite(peripheral_handle, FDCAN_RX_FIFO0, CFG_FDCAN_FIFO_OVERWRITE);
    if (retval != HAL_OK) {
        return ERR_FDCAN_CFG_FIFO_OVERWRITE;
    }

    // Activate FIFO0 interrupt on new msg
    retval = HAL_FDCAN_ActivateNotification(peripheral_handle, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
    if (retval != HAL_OK) {
        return ERR_FDCAN_ACTIVATE_NOTIF;
    }

    return SR_OK;
}

sr_errno_t sr_fdcan_start() {
    if (HAL_FDCAN_Start(peripheral_handle) != HAL_OK) {
        return ERR_FDCAN_START;
    }
    return SR_OK;
}

sr_errno_t sr_fdcan_tx(uint32_t can_id, uint8_t* data, uint32_t length) {
    uint32_t dlc = sr_fdcan_bytes_to_dlc(length);
    if (dlc == UINT32_MAX || (fdcan_data.tx_frame_format == FDCAN_CLASSIC_CAN && dlc > FDCAN_DLC_BYTES_8)) {
        return ERR_FDCAN_TX_MSG_TOO_BIG;
    }

    FDCAN_TxHeaderTypeDef header = {
        .Identifier=can_id,
        .IdType=fdcan_data.tx_id_type,
        .TxFrameType=FDCAN_DATA_FRAME,
        .DataLength=dlc,
        .ErrorStateIndicator=FDCAN_ESI_ACTIVE,
        .BitRateSwitch=fdcan_data.tx_brs,
        .FDFormat=fdcan_data.tx_frame_format,
        .TxEventFifoControl=fdcan_data.tx_event_fifo_control,
        .MessageMarker=0
    };

    if (HAL_FDCAN_AddMessageToTxFifoQ(peripheral_handle, &header, data) != HAL_OK) {
        return ERR_FDCAN_TX;
    }
    return SR_OK;
}

sr_errno_t sr_fdcan_tx_blocking(uint32_t can_id, uint8_t* data, uint32_t length) {
    uint32_t timeout_start_ms = sr_millis();
    uint32_t timeout_ms = CFG_FDCAN_TIMEOUT_MS;
    sr_errno_t retval = sr_fdcan_tx(can_id, data, length);
    if (retval != SR_OK) {
        return retval;
    }
    // FIFO holds 3 messages max
    while (HAL_FDCAN_GetTxFifoFreeLevel(peripheral_handle) < 3) {
        // do nothing
        if (sr_millis() - timeout_start_ms >= timeout_ms) {
            return ERR_FDCAN_TIMEOUT;
        }
    }

    return SR_OK;
}