#ifndef FLASH_DRIVER_H
#define FLASH_DRIVER_H

#include <stdint.h>
#include "sr_errno.h"

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

/**
 * @brief Resolves an absolute flash address to its containing bank and page index.
 *
 * This is the single place that knows the chip's bank/page geometry
 * (bank count, pages per bank, address ranges) so callers above this driver
 * never need to hardcode a bank or compute page numbers themselves.
 *
 * @param address - absolute flash address
 * @param bank - out: bank containing address (e.g. FLASH_BANK_1)
 * @param page - out: 0-indexed page number within that bank
 * @return sr_errno_t - ERR_FLASH_INVALID_INPUT if address is out of range
 */
sr_errno_t sr_flash_get_page_info(uint32_t address, uint32_t* bank, uint32_t* page);

/**
 * @brief Resolves a bank/page pair to the absolute address of that page's first byte.
 *
 * Inverse of sr_flash_get_page_info().
 *
 * @param bank - target bank (e.g. FLASH_BANK_1)
 * @param page - 0-indexed page number within that bank
 * @param address - out: absolute address of the start of that page
 * @return sr_errno_t - ERR_FLASH_INVALID_INPUT if bank/page is out of range
 */
sr_errno_t sr_flash_get_page_address(uint32_t bank, uint32_t page, uint32_t* address);

#endif