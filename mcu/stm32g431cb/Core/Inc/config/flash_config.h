#ifndef FLASH_CONFIG_H
#define FLASH_CONFIG_H

#include <stdint.h>
// flash constants for STM32G431CB ===================================================
#define NUM_FLASH_BANKS 1U
#define NUM_FLASH_PAGES 64U
#define FLASH_PAGE_SIZE_BYTES 2048U
#define FLASH_BEGIN_ADDRESS 0x08000000U
#define FLASH_END_ADDRESS 0x0801FFFFU

// STM32g4 is 64 bit writes minimum
#define FLASH_WRITE_GRANULARITY_BYTES 8U

// flash regions
#define BOOTLOADER_SIZE_PAGES 11U
#define FW_SLOT_A_SIZE_PAGES 26U
#define FW_SLOT_B_SIZE_PAGES 27U

#endif