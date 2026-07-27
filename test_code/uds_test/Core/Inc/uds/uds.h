#ifndef UDS_H
#define UDS_H

#include "main.h"
#include "sr_errno.h"

/**
 * @brief Brings up the whole can -> isotp -> uds stack and runs the server loop.
 * Owns the isotp tx/rx buffers and hands them down to the isotp layer.
 * Only returns if the stack fails to come up - otherwise loops forever serving requests.
 *
 * @param hfdcan - HAL handle, must already be HAL-initialised (MX_FDCANx_Init'd) but not yet started
 * @return sr_errno_t - error code defined in errno.h, only returns on start-up failure
 */
sr_errno_t sr_uds_server_start(FDCAN_HandleTypeDef* hfdcan);

#endif