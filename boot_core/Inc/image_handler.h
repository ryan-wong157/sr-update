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
    SWAP_TYPE_NONE = 0xFF
} swap_type_t;

// HEADER functions ==================================================================
uint8_t is_header_magic_set(slot_type_t slot);
uint16_t get_header_size(slot_type_t slot);
uint8_t is_ecu_id_match(slot_type_t slot);
uint32_t get_image_size(slot_type_t slot);
const image_version_t* get_image_version(slot_type_t slot);
uint8_t is_image_valid(slot_type_t slot, uint8_t check_crypto);

// TRAILER functions =================================================================
// Setting a field to the value it already holds does nothing and returns SR_OK.
// Setting it to a different value fails, call erase_trailer() first
sr_errno_t set_trailer_magic(slot_type_t slot);
uint8_t is_trailer_magic_set(slot_type_t slot);

sr_errno_t set_copy_done(slot_type_t slot);
uint8_t is_copy_done(slot_type_t slot);

sr_errno_t set_image_ok(slot_type_t slot);
uint8_t is_image_ok(slot_type_t slot);

sr_errno_t set_swap_info(slot_type_t slot, swap_type_t swap_type);
swap_type_t get_swap_info(slot_type_t slot);

sr_errno_t set_swap_size_pages(slot_type_t slot);
// Returns 0xFF if the swap size has not been set
uint32_t get_swap_size_pages(slot_type_t slot);

sr_errno_t erase_trailer(slot_type_t slot);
// Erases every page of the slot, trailer last
sr_errno_t erase_slot(slot_type_t slot);

// Read trailer A swap status and returns index and is_halfway done for index that swap is up to
sr_errno_t get_swap_status(uint32_t* index, uint8_t* is_halfway_done);
// SWAP functions ====================================================================
sr_errno_t swap_forward(uint32_t index, uint32_t total_pages, uint8_t is_halfway_done);
sr_errno_t swap_revert(uint32_t index, uint32_t total_pages, uint8_t is_halfway_done);

#endif