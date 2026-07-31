#ifndef UDS_H
#define UDS_H

#include "main.h"
#include "sr_errno.h"

/**
 * @brief Brings up the whole can -> isotp -> uds stack and runs the server loop.
 * Only returns if the stack fails to come up. Otherwise loops forever serving requests.
 *
 * @param hfdcan - HAL handle, must already be HAL-initialised (MX_FDCANx_Init'd) but not yet started
 * @return sr_errno_t
 */
sr_errno_t sr_uds_server_start(FDCAN_HandleTypeDef* hfdcan);

#endif