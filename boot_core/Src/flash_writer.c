// Flash write session layer sitting above the mcu-provided flash_driver.
// Handles page-erase-once-per-page tracking and write-granularity buffering so callers
// can write an arbitrary amount of data at an address without worrying about
// flash page/write-granularity stuff. Generic across any MCU's write granularity
// and page size, as declared by mcu_interface/flash_driver.h.

#include <string.h>
#include "flash_writer.h"
#include "mcu_interface/flash_driver.h"

static uint32_t session_active;
static uint32_t next_write_addr;
static uint32_t current_bank;
static uint32_t current_page;
static uint8_t carry[FLASH_WRITE_GRANULARITY_BYTES];
static uint32_t carry_len;

// erases every not-yet-erased page from the current erase position up to
// and including the page containing (address + total_len - 1)
static sr_errno_t erase_up_to(uint32_t address, uint32_t total_len) {
    sr_errno_t err;
    uint32_t end_bank;
    uint32_t end_page;

    // -1 because address + total len is one byte past actual address to erase up to
    err = sr_flash_get_page_info(address + total_len - 1, &end_bank, &end_page);
    if (err != SR_OK) {
        return err;
    }

    while (!(current_bank == end_bank && current_page == end_page)) {
        // we do this in a very complicated way because arithmetic will change depending on how many banks the MCU flash has
        uint32_t current_page_addr;
        err = sr_flash_get_page_address(current_bank, current_page, &current_page_addr);
        if (err != SR_OK) {
            return err;
        }

        uint32_t next_page_addr = current_page_addr + FLASH_PAGE_SIZE_BYTES;
        err = sr_flash_get_page_info(next_page_addr, &current_bank, &current_page);
        if (err != SR_OK) {
            return err;
        }
        err = sr_flash_erase_page(current_bank, current_page, 1);
        if (err != SR_OK) {
            return err;
        }
    }

    return SR_OK;
}

sr_errno_t sr_flash_writer_begin(uint32_t start_address) {
    uint32_t bank;
    uint32_t page;
    sr_errno_t err = sr_flash_get_page_info(start_address, &bank, &page);
    if (err != SR_OK) {
        return err;
    }

    if (start_address % FLASH_PAGE_SIZE_BYTES != 0) {
        return ERR_FLASH_INVALID_INPUT;
    }

    // erase this first starting page
    err = sr_flash_erase_page(bank, page, 1);
    if (err != SR_OK) {
        return err;
    }

    carry_len = 0;
    next_write_addr = start_address;
    current_bank = bank;
    current_page = page;
    session_active = 1;
    return SR_OK;
}

/*
Does a thing where if len is not write-granularity aligned, the leftover bytes are buffered
then prepended into the next granularity-sized write. If it's the last write,
sr_flash_writer_finish flushes this buffer and pads with 0xFF.
Erases as many pages as needed to write "len" bytes from the current write cursor.
*/
sr_errno_t sr_flash_writer_write(const uint8_t* data, uint32_t len) {
    if (!session_active) {
        return ERR_FLASH_WRITER_NOT_STARTED;
    }

    sr_errno_t err = erase_up_to(next_write_addr, carry_len + len);
    if (err != SR_OK) {
        return err;
    }

    uint32_t data_index = 0;
    // while amt of bytes to write >= write granularity
    while (carry_len + (len - data_index) >= FLASH_WRITE_GRANULARITY_BYTES) {
        uint8_t chunk[FLASH_WRITE_GRANULARITY_BYTES];

        // handle prepending buffered carry bytes if any
        uint32_t from_carry = carry_len;
        memcpy(chunk, carry, from_carry);
        for (uint32_t i = from_carry; i < FLASH_WRITE_GRANULARITY_BYTES; i++) {
            chunk[i] = data[data_index++];
        }

        err = sr_flash_write(next_write_addr, chunk);
        if (err != SR_OK) {
            return err;
        }

        next_write_addr += FLASH_WRITE_GRANULARITY_BYTES;
        carry_len = 0;
    }

    // add leftover bytes (< write granularity) to carry buffer
    for (uint32_t i = data_index; i < len; i++) {
        carry[carry_len++] = data[i];
    }

    return SR_OK;
}

sr_errno_t sr_flash_writer_finish(void) {
    if (!session_active) {
        return ERR_FLASH_WRITER_NOT_STARTED;
    }

    sr_errno_t err = SR_OK;
    if (carry_len > 0) {
        for (uint32_t i = carry_len; i < FLASH_WRITE_GRANULARITY_BYTES; i++) {
            carry[i] = 0xFF;
        }
        err = sr_flash_write(next_write_addr, carry);
        if (err == SR_OK) {
            next_write_addr += FLASH_WRITE_GRANULARITY_BYTES;
        }
    }

    session_active = 0;
    carry_len = 0;
    return err;
}

void sr_flash_writer_abort(void) {
    session_active = 0;
    carry_len = 0;
}
