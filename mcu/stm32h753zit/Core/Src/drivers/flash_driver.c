// Flash driver functions for stm32H753ZIT
// Written by Ryan Wong

#include <string.h>
#include "main.h"
#include "mcu_interface/flash_driver.h"
#include "config/flash_config.h"

// error flags are per bank on the H7
static void clear_error_flags(uint32_t bank) {
    if (bank == FLASH_BANK_1) {
        __HAL_FLASH_CLEAR_FLAG_BANK1(FLASH_FLAG_ALL_ERRORS_BANK1);
    } else {
        __HAL_FLASH_CLEAR_FLAG_BANK2(FLASH_FLAG_ALL_ERRORS_BANK2);
    }
}

sr_errno_t sr_flash_write(uint32_t address, const uint8_t* data) {
    HAL_StatusTypeDef retval;
    uint32_t bank;
    uint32_t page;
    if (sr_flash_get_page_info(address, &bank, &page) != SR_OK || address % FLASH_WRITE_GRANULARITY_BYTES != 0) {
        return ERR_FLASH_INVALID_INPUT;
    }

    // have to pass in a 4 byte aligned address, so make our own
    uint32_t flash_word[FLASH_WRITE_GRANULARITY_BYTES / 4];
    memcpy(flash_word, data, FLASH_WRITE_GRANULARITY_BYTES);

    retval = HAL_FLASH_Unlock();
    if (retval != HAL_OK) {
        return ERR_FLASH_UNLOCK;
    }

    // need to clear error flags to prevent previous errors from affecting current write
    clear_error_flags(bank);

    retval = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, address, (uint32_t)flash_word);
    if (retval != HAL_OK) {
        HAL_FLASH_Lock();
        return ERR_FLASH_WRITE;
    }

    // readback check
    if (memcmp((const void*)address, data, FLASH_WRITE_GRANULARITY_BYTES) != 0) {
        HAL_FLASH_Lock();
        return ERR_FLASH_READBACK;
    }

    HAL_FLASH_Lock();
    return SR_OK;
}

sr_errno_t sr_flash_erase_page(uint32_t bank, uint32_t start_page, uint32_t num_pages) {
    // can't erase across banks in one go
    if ((bank != FLASH_BANK_1 && bank != FLASH_BANK_2) || start_page > FLASH_PAGES_PER_BANK - 1 || num_pages > (FLASH_PAGES_PER_BANK - start_page)) {
        return ERR_FLASH_INVALID_INPUT;
    }

    HAL_StatusTypeDef retval;
    // 0xFFFF FFFF on success, offending sector num on erase fail
    uint32_t page_error;
    FLASH_EraseInitTypeDef erase_struct;

    erase_struct.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_struct.Banks = bank;
    erase_struct.Sector = start_page;
    erase_struct.NbSectors = num_pages;
    erase_struct.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    retval = HAL_FLASH_Unlock();
    if (retval != HAL_OK) {
        return ERR_FLASH_UNLOCK;
    }

    clear_error_flags(bank);

    retval = HAL_FLASHEx_Erase(&erase_struct, &page_error);
    if (retval != HAL_OK || page_error != 0xFFFFFFFF) {
        HAL_FLASH_Lock();
        return ERR_FLASH_ERASE;
    }

    HAL_FLASH_Lock();
    return SR_OK;
}

sr_errno_t sr_flash_get_page_info(uint32_t address, uint32_t* bank, uint32_t* page) {
    if (address < FLASH_BEGIN_ADDRESS || address > FLASH_END_ADDRESS) {
        return ERR_FLASH_INVALID_INPUT;
    }

    uint32_t index = (address - FLASH_BEGIN_ADDRESS) / FLASH_PAGE_SIZE_BYTES;
    *bank = (index < FLASH_PAGES_PER_BANK) ? FLASH_BANK_1 : FLASH_BANK_2;
    *page = index % FLASH_PAGES_PER_BANK;
    return SR_OK;
}

sr_errno_t sr_flash_get_page_address(uint32_t bank, uint32_t page, uint32_t* address) {
    if ((bank != FLASH_BANK_1 && bank != FLASH_BANK_2) || page >= FLASH_PAGES_PER_BANK) {
        return ERR_FLASH_INVALID_INPUT;
    }

    uint32_t bank_base = (bank == FLASH_BANK_1) ? FLASH_BANK1_BASE : FLASH_BANK2_BASE;
    *address = bank_base + page * FLASH_PAGE_SIZE_BYTES;
    return SR_OK;
}
