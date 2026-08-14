#ifndef UDS_H
#define UDS_H

#include "main.h"
#include "sr_errno.h"

/**
 * @brief Starts UDS server. This starts isotp layer underneath. Blocking
 *
 * @return sr_errno_t
 */
sr_errno_t sr_uds_server_start();

#endif