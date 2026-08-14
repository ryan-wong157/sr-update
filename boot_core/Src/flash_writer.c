// Flash write session layer sitting above flash_driver.
// Handles page-erase-once-per-page tracking and doubleword buffering so callers
// can write an arbitrary amount of data at an address without worrying about
// flash page/doubleword stuff.

#include <stdint.h>
#include "flash_writer.h"
#include "mcu_interface/flash_driver.h"
#include "config/flash_config.h"

static uint32_t session_active;
static uint32_t next_write_addr;
static uint32_t current_bank;
static uint32_t current_page;
static uint8_t carry[8];
static uint8_t carry_len;

static uint64_t pack_doubleword(const uint8_t* bytes) {
    // relies on little endian Cortex-M byte order: bytes[0] (lowest address)
    uint64_t value = 0;
    for (int i = 0; i < 8; i++) {
        value |= ((uint64_t)bytes[i]) << (8 * i);
    }
    return value;
}

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
Does a thing where if len is not doubleword (8 byte) aligned, the leftover bytes are buffered 
then prepended into the next double word write. If it's last write, then sr_flash_writer_finish flushes this buffer
and pads to make up double word with 0xFF.
Erases as many pages as needed to write "len" bytes from "address"
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
    // while amt of bytes to write >= doubleword
    while (carry_len + (len - data_index) >= 8) {
        uint8_t doubleword[8];

        // handle prepending buffered carry bytes if any
        uint8_t from_carry = carry_len;
        for (uint8_t i = 0; i < from_carry; i++) {
            doubleword[i] = carry[i];
        }
        for (uint8_t i = from_carry; i < 8; i++) {
            doubleword[i] = data[data_index++];
        }

        err = sr_flash_write64(next_write_addr, pack_doubleword(doubleword));
        if (err != SR_OK) {
            return err;
        }

        next_write_addr += 8;
        carry_len = 0;
    }

    // add leftover <8 bytes to carry buffer
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
        for (uint8_t i = carry_len; i < 8; i++) {
            carry[i] = 0xFF;
        }
        err = sr_flash_write64(next_write_addr, pack_doubleword(carry));
        if (err == SR_OK) {
            next_write_addr += 8;
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
