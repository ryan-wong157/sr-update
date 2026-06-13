// Flash driver functions for stm32G431CB
// Written by Ryan Wong

#include "flash_driver.h"

sr_flash_status_t write_flash_64(uint32_t address, uint64_t data) {
    HAL_StatusTypeDef retval;
    
    retval = HAL_FLASH_Unlock();
    if (retval != HAL_OK) {
        return SR_FLASH_UNLOCK_ERR;
    }
    
    // need to clear error flags to prevent previous errors from affecting current write
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

    retval = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address, data);
    if (retval != HAL_OK) {
        //TODO: return actual error code
        // uint32_t flash_err = HAL_FLASH_GetError();
        // switch (flash_err) {
           
        // }
        HAL_FLASH_Lock();
        return SR_FLASH_WRITE_ERR;
    }

    // readback check
    uint64_t readback_value = *(volatile uint64_t *)address;
    if (readback_value != data) {
        HAL_FLASH_Lock();
        return SR_FLASH_READBACK_ERR;
    }

    HAL_FLASH_Lock();
    return SR_FLASH_OK;
}

sr_flash_status_t erase_flash_page(uint32_t bank, uint32_t start_page, uint32_t num_pages) {
    if (bank != FLASH_BANK_1 || start_page < 0 || start_page > NUM_FLASH_PAGES - 1 || num_pages > (NUM_FLASH_PAGES - start_page + 1)) {
        return SR_FLASH_INVALID_INPUT;
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
        return SR_FLASH_UNLOCK_ERR;
    }

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);

    retval = HAL_FLASHEx_Erase(&erase_struct, &page_error);
    if (retval != HAL_OK || page_error != 0xFFFFFFFF) {
        HAL_FLASH_Lock();
        return SR_FLASH_ERASE_ERR;
    }

    HAL_FLASH_Lock();
    return SR_FLASH_OK;
}    
