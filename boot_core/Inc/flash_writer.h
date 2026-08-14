#ifndef FLASH_WRITER_H
#define FLASH_WRITER_H

#include <stdint.h>
#include "sr_errno.h"

/**
 * @brief Begins a new flash write session starting at start_address.
 *
 * Resets internal carry buffer and erase-tracking state.
 * start_address must be page-aligned (FLASH_PAGE_SIZE_BYTES) and within a valid
 * flash region
 *
 * @param start_address - first address the session will write to
 * @return sr_errno_t
 */
sr_errno_t sr_flash_writer_begin(uint32_t start_address);

/**
 * @brief Writes len bytes of data, continuing from wherever the current session left off.
 *
 * The write cursor is tracked internally (starting from the address passed to
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