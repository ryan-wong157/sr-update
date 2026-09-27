#ifndef ISOTP_CONFIG_H
#define ISOTP_CONFIG_H

#include <stdint.h>
#include "isotp/isotplib/isotplib.h"

#define CFG_ISOTP_TIMEOUT_MS 1000
static const isotp_format_t ISOTP_FORMAT = ISOTP_FORMAT_NORMAL;

// ECU_ID is the "ECU Hardware ID" (pedalbox, drive UEN etc), defined per-ECU via cmake.
// 12 bits!!!! (4 MSB should be 0)
#ifndef ECU_ID
#error "ECU_ID must be defined via cmake -DECU=<name>"
#endif
_Static_assert(ECU_ID <= 0xFFF, "ECU_ID must fit in 12 bits (0x000 - 0xFFF)");

// ISO-TP CAN IDs are derived from ECU_ID and are always 29-bit extended IDs.
// Tx (ECU -> tool):  0x1F<ECU_ID>000, e.g. ECU_ID=0x10  ->  0x1F010000
// Rx (tool -> ECU):  fixed broadcast address, same for every ECU
#define CFG_ISOTP_TX_ID (0x1F000000u | ((uint32_t)(ECU_ID) << 12))
#define CFG_ISOTP_RX_ID 0x1F000000u

#endif