#ifndef FLASH_DRIVER_H
#define FLASH_DRIVER_H

#include <stdint.h>
#include "sr_errno.h"
#include "config/flash_config.h"

// 1. "Page" means the smallest erasable unit of flash on this MCU (some vendors call this a sector).

// 2. config/flash_config.h must supply:
//  FLASH_WRITE_GRANULARITY_BYTES
//  FLASH_PAGE_SIZE_BYTES
//  NUM_FLASH_BANKS,
//  and the flash slot layout (FW_SLOT_A/B_START_ADDRESS, FW_SLOT_SIZE_BYTES).

/**
 * @brief Writes exactly FLASH_WRITE_GRANULARITY_BYTES bytes into flash at address.
 *
 * Blocks until flash operation completes.
 * Address must be aligned to FLASH_WRITE_GRANULARITY_BYTES and within a previously erased region.
 * Attempting to write to an unaligned or not-yet-erased region will error.
 * Handles lock/unlock internally.
 *
 * @param address - target 32 bit flash address, must be aligned to FLASH_WRITE_GRANULARITY_BYTES
 * @param data - buffer of exactly FLASH_WRITE_GRANULARITY_BYTES bytes to write
 * @return sr_errno_t - status enum defined in errno.h
 */
sr_errno_t sr_flash_write(uint32_t address, const uint8_t* data);

/**
 * @brief Erases one or more pages of flash memory from the specified bank
 *
 * Start page will need to be within 0 and max pages - 1
 * num pages + start page must not exceed max pages
 *
 * @param bank - flash bank index (0 .. NUM_FLASH_BANKS - 1)
 * @param start_page - page to start erasing from (inclusive, 0 indexed)
 * @param num_pages - number of pages to erase from start_page
 * @return sr_errno_t - status enum defined in errno.h
 */
sr_errno_t sr_flash_erase_page(uint32_t bank, uint32_t start_page, uint32_t num_pages);

/**
 * @brief Resolves an absolute flash address to its bank and page index.
 *
 * @param address - absolute flash address
 * @param bank - out: bank containing address
 * @param page - out: 0-indexed page number within that bank
 * @return sr_errno_t
 */
sr_errno_t sr_flash_get_page_info(uint32_t address, uint32_t* bank, uint32_t* page);

/**
 * @brief Resolves a bank/page pair to the absolute address of that page's first byte.
 *
 * Inverse of sr_flash_get_page_info().
 *
 * @param bank - target bank
 * @param page - 0-indexed page number within that bank
 * @param address - out: absolute address of the start of that page
 * @return sr_errno_t
 */
sr_errno_t sr_flash_get_page_address(uint32_t bank, uint32_t page, uint32_t* address);

#endif
