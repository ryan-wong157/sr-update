#ifndef ISOTP_CONFIG_H
#define ISOTP_CONFIG_H

#include <stdint.h>
#include "isotp/isotplib/isotplib.h"
#include "common_config/can_bus_config.h"

#define CFG_ISOTP_TIMEOUT_MS 1000

#if CFG_CAN_FD
static const isotp_format_t ISOTP_FORMAT = ISOTP_FORMAT_FD;
#define CFG_ISOTP_CAN_DL 64
#else
static const isotp_format_t ISOTP_FORMAT = ISOTP_FORMAT_NORMAL;
#define CFG_ISOTP_CAN_DL 8
#endif

// ECU_ID is the "ECU Hardware ID" (pedalbox, drive UEN etc), defined per-ECU via cmake.
// 12 bits!!!! (4 MSB should be 0)
#ifndef ECU_ID
#error "ECU_ID must be defined via cmake -DECU=<name>"
#endif
_Static_assert(ECU_ID <= 0xFFF, "ECU_ID must fit in 12 bits (0x000 - 0xFFF)");

// ISO-TP CAN IDs are derived from ECU_ID and are always 29-bit extended IDs.
// Priority is 0x1F, msg ID = 0xFFE (according to the Sunswift id scheme)
#define CFG_ISOTP_TX_ID (0x1F000000u | ((uint32_t)(ECU_ID) << 12) | 0xFFEu)
#define CFG_ISOTP_RX_ID 0x1F000FFEu

#endif