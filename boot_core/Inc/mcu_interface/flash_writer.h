#ifndef FLASH_WRITER_H
#define FLASH_WRITER_H

#include <stdint.h>
#include "sr_errno.h"

// Firmware flash slots. The mcu layer owns the address/size each slot maps to.
typedef enum {
    FLASH_SLOT_A,
    FLASH_SLOT_B,
} flash_slot_t;

/**
 * @brief Returns the number of bytes available in a flash slot.
 *
 * @param slot - target flash slot
 * @return uint32_t - capacity of the slot in bytes
 */
uint32_t sr_flash_get_slot_capacity(flash_slot_t slot);

/**
 * @brief Begins a new flash write session targeting the given slot.
 *
 * Resets internal carry buffer and erase-tracking state.
 *
 * @param slot - flash slot the session will write to
 * @return sr_errno_t
 */
sr_errno_t sr_flash_writer_begin(flash_slot_t slot);

/**
 * @brief Writes len bytes of data, continuing from wherever the current session left off.
 *
 * The write cursor is tracked internally (starting from the beginning of the slot passed to
 * sr_flash_writer_begin()) and advances automatically as data is written, so the
 * caller never needs to supply or track an address itself.
 *
 * @param data - bytes to write
 * @param len - number of bytes in data
 * @return sr_errno_t
 */
sr_errno_t sr_flash_writer_write(const uint8_t* data, uint32_t len);

/**
 * @brief Flushes any buffered leftover bytes (padding with 0xFF) and ends the session.
 *
 * @return sr_errno_t
 */
sr_errno_t sr_flash_writer_finish(void);

/**
 * @brief Abandons the current write session without flushing buffered bytes.
 * Used to cancel an in-progress download. Any pages already erased/written remain
 */
void sr_flash_writer_abort(void);

#endif