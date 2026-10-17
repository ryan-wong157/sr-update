#ifndef UDS_H
#define UDS_H

#include <stdint.h>
#include "sr_errno.h"

/**
 * @brief Starts UDS server. This starts isotp layer underneath. Blocking
 *
 * @param broadcast_distress 1 to send a distress frame every CFG_DISTRESS_PERIOD_MS, used when
 *                           there is no app to boot. 0 when the app asked us to hold in the bootloader
 * @return sr_errno_t
 */
sr_errno_t sr_uds_server_start(uint8_t broadcast_distress);

#endif