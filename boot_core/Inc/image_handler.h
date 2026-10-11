#ifndef IMAGE_HANDLER_H
#define IMAGE_HANDLER_H 

#include <stdint.h>
#include "sr_errno.h"
#include "image_format.h"

typedef enum {
    SLOT_A,
    SLOT_B
} slot_type_t;

typedef enum {
    SWAP_TYPE_TEST = 0x01,
    SWAP_TYPE_REVERT = 0x02,
    SWAP_TYPE_PERM = 0x03,
    SWAP_TYPE_NONE = 0xFF
} swap_type_t;

// HEADER functions ==================================================================
uint8_t is_header_magic_set(slot_type_t slot);
uint16_t get_header_size(slot_type_t slot);
uint8_t is_ecu_id_match(slot_type_t slot);
uint32_t get_image_size(slot_type_t slot);
const image_version_t* get_image_version(slot_type_t slot);

// TRAILER functions =================================================================
// Each trailer field can only be set once, call erase_trailer() before setting it again
sr_errno_t set_trailer_magic(slot_type_t slot);
uint8_t is_trailer_magic_set(slot_type_t slot);

sr_errno_t set_copy_done(slot_type_t slot);
uint8_t is_copy_done(slot_type_t slot);

sr_errno_t set_image_ok(slot_type_t slot);
uint8_t is_image_ok(slot_type_t slot);

sr_errno_t set_swap_info(slot_type_t slot, swap_type_t swap_type);
swap_type_t get_swap_info(slot_type_t slot);

// Auto calculates and sets max swap size (in pages) to the slot
sr_errno_t set_swap_size_pages(slot_type_t slot);
// Returns 0xFF if the swap size has not been set
uint32_t get_swap_size_pages(slot_type_t slot);

sr_errno_t erase_trailer(slot_type_t slot);

// TODO: swap function
#endif