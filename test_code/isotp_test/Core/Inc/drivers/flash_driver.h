#ifndef FLASH_DRIVER_H
#define FLASH_DRIVER_H

#include "main.h"
#include "errno.h"
#include "flash_config.h"
#include <stdint.h>

/**
 * @brief Writes a 64 bit value into flash at "address"
 * 
 * Blocks until flash operation completes.
 * Address must be 8-byte aligned and within a previously erased version.
 * Attempting to write to an unaligned or not-yet-erased region will error.
 * Handles lock/unlock internally.
 * 
 * @param address - target 32 bit flash address, must be 8-byte aligned
 * @param data - 64 bit value to write
 * @return sr_errno_t - status enum defined in errno.h
 */
sr_errno_t sr_flash_write64(uint32_t address, uint64_t data);

/**
 * @brief Erases one or more pages of flash memory from specificed bank
 * 
 * Start page will need to be within 0 and max pages - 1
 * num pages + start page must not exceed max pages
 * bank must be either FLASH_BANK_1, or FLASH_BANK_2, not both
 * 
 * @param bank - flash bank number: FLASH_BANK_1, or FLASH_BANK_2
 * @param start_page - page to start erasing from (inclusive, 0 indexed)
 * @param num_pages - number of pages to erase from start page
 * @return sr_errno_t - status enum defined in errno.h
 */
sr_errno_t sr_flash_erase_page(uint32_t bank, uint32_t start_page, uint32_t num_pages);

#endif