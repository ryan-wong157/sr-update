#ifndef FLASH_CONFIG_H
#define FLASH_CONFIG_H

#include <stdint.h>
// flash constants for STM32H753ZIT ==================================================
// "page" here means a 128K sector. Addresses are contiguous
#define NUM_FLASH_BANKS 2U
#define NUM_FLASH_PAGES 16U
#define FLASH_PAGES_PER_BANK 8U
#define FLASH_PAGE_SIZE_BYTES 0x20000U
#define FLASH_BEGIN_ADDRESS 0x08000000U
#define FLASH_END_ADDRESS 0x081FFFFFU

// STM32H7 is 256 bit (flash word) writes minimum
#define FLASH_WRITE_GRANULARITY_BYTES 32U

// flash regions
// bootloader = bank 1 sector 0, slot A = bank 1 sectors 1-7, slot B = all of bank 2
#define BOOTLOADER_SIZE_PAGES 1U
#define FW_SLOT_A_SIZE_PAGES 7U
#define FW_SLOT_B_SIZE_PAGES 8U

#endif
