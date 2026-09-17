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
#define BOOTLOADER_SIZE_PAGES 10U
#define BOOTLOADER_SIZE_BYTES (BOOTLOADER_SIZE_PAGES * FLASH_PAGE_SIZE_BYTES)  // 20 KB, pages 0-9
#define FW_SLOT_SIZE_PAGES 27U
#define FW_SLOT_SIZE_BYTES (FW_SLOT_SIZE_PAGES * FLASH_PAGE_SIZE_BYTES) // 54 KB, 27 pages each

#define BOOTLOADER_START_ADDRESS FLASH_BEGIN_ADDRESS
#define FW_SLOT_A_START_ADDRESS (BOOTLOADER_START_ADDRESS + BOOTLOADER_SIZE_BYTES)
#define FW_SLOT_B_START_ADDRESS (FW_SLOT_A_START_ADDRESS + FW_SLOT_SIZE_BYTES)

// TRAILER structures ================================================================
// Specific to this MCU only
#define TRAILER_SIZE_BYTES sizeof(image_trailer_t)
#define FW_MAX_IMAGE_SIZE_BYTES (FW_SLOT_SIZE_BYTES - TRAILER_SIZE_BYTES)
static const uint32_t trailer_magic_const[4] = {
    0xf395c277,
    0x7fefd260,
    0x0f505235,
    0x8079b62c,
};

// 8 byte aligned
typedef struct {
    uint8_t status;
    uint8_t padding[7]; // pad to FLASH_WRITE_GRANULARITY_BYTES
} record_t;

// 54 * 8 + 3 * 8 + 16 = 472 bytes...
typedef struct {
    record_t trailer_swap_status[2 * FW_SLOT_SIZE_PAGES]; // 2 records per page in this slot
    record_t trailer_swap_info;
    record_t trailer_copy_done;
    record_t trailer_image_ok;
    uint32_t trailer_magic[4];
} image_trailer_t;

#endif