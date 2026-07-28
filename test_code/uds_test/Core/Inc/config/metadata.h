#ifndef METADATA_H
#define METADATA_H

#include <stdint.h>

const static uint8_t BOOTLOADER_VERS_MAJOR = 1;
const static uint8_t BOOTLOADER_VERS_MINOR = 0;
const static uint8_t BOOTLOADER_VERS_PATCH = 0;

// This is the "ECU Hardware ID". Defines what this board is (pedalbox, drive UEN etc)
// Must be defined during compile time.
// 12 bits!!!! (4 MSB should be 0)
#ifndef NODE_ID
#error "NODE_ID must be defined via cmake -DNODE_ID=..."
#endif

_Static_assert(NODE_ID <= 0xFFF, "NODE_ID must fit in 12 bits (0x000 - 0xFFF)");

#endif