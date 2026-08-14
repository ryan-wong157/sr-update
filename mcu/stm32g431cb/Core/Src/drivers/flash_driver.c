// Flash driver functions for stm32G431CB
// Written by Ryan Wong

#include "main.h"
#include "mcu_interface/flash_driver.h"
#include "config/flash_config.h"

sr_errno_t sr_flash_write64(uint32_t address, uint64_t data) {
    HAL_StatusTypeDef retval;
    
    retval = HAL_FLASH_Unlock();
    if (retval != HAL_OK) {
        return ERR_FLASH_UNLOCK;
    }
    
    // need to clear error flags to prevent previous errors from affecting current write
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

    retval = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address, data);
    if (retval != HAL_OK) {
        HAL_FLASH_Lock();
        return ERR_FLASH_WRITE;
    }

    // readback check
    uint64_t readback_value = *(volatile uint64_t *)address;
    if (readback_value != data) {
        HAL_FLASH_Lock();
        return ERR_FLASH_READBACK;
    }

    HAL_FLASH_Lock();
    return SR_OK;
}

sr_errno_t sr_flash_erase_page(uint32_t bank, uint32_t start_page, uint32_t num_pages) {
    if (bank != FLASH_BANK_1 || start_page < 0 || start_page > NUM_FLASH_PAGES - 1 || num_pages > (NUM_FLASH_PAGES - start_page + 1)) {
        return ERR_FLASH_INVALID_INPUT;
    }

    HAL_StatusTypeDef retval;
    // 0xFFFF FFFF on success, offending page num on erase fail 
    uint32_t page_error;
    FLASH_EraseInitTypeDef erase_struct;

    erase_struct.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_struct.Banks = bank;
    erase_struct.Page = start_page;
    erase_struct.NbPages = num_pages;

    retval = HAL_FLASH_Unlock();
    if (retval != HAL_OK) {
        return ERR_FLASH_UNLOCK;
    }

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

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

    // Change if different MCU
    *bank = FLASH_BANK_1;
    *page = (address - FLASH_BEGIN_ADDRESS) / FLASH_PAGE_SIZE_BYTES;
    return SR_OK;
}

sr_errno_t sr_flash_get_page_address(uint32_t bank, uint32_t page, uint32_t* address) {
    // Change if different MCU
    if (bank != FLASH_BANK_1 || page >= NUM_FLASH_PAGES) {
        return ERR_FLASH_INVALID_INPUT;
    }

    *address = FLASH_BEGIN_ADDRESS + page * FLASH_PAGE_SIZE_BYTES;
    return SR_OK;
}
