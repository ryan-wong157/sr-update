#ifndef IMAGE_FORMAT_H
#define IMAGE_FORMAT_H

#include <stdint.h>
#include "config/flash_config.h" // FROM MCU-specific header

// FLASH LOCATION DEFINES ============================================================
#define BOOTLOADER_SIZE_BYTES (BOOTLOADER_SIZE_PAGES * FLASH_PAGE_SIZE_BYTES)
#define FW_SLOT_A_SIZE_BYTES (FW_SLOT_A_SIZE_PAGES * FLASH_PAGE_SIZE_BYTES)
#define FW_SLOT_B_SIZE_BYTES (FW_SLOT_B_SIZE_PAGES * FLASH_PAGE_SIZE_BYTES)

#define FW_ACTUAL_SIZE_PAGES (FW_SLOT_A_SIZE_PAGES - 1)
#define FW_ACTUAL_SIZE_BYTES (FW_ACTUAL_SIZE_PAGES * FLASH_PAGE_SIZE_BYTES)

#define BOOTLOADER_START_ADDRESS FLASH_BEGIN_ADDRESS
#define FW_SLOT_A_START_ADDRESS (BOOTLOADER_START_ADDRESS + BOOTLOADER_SIZE_BYTES)
#define FW_SLOT_B_START_ADDRESS (FW_SLOT_A_START_ADDRESS + FW_SLOT_A_SIZE_BYTES)
#define FW_SLOT_B_UPDATE_START_ADDRESS (FW_SLOT_B_START_ADDRESS + FLASH_PAGE_SIZE_BYTES)

// HEADER structures =================================================================
#define IMAGE_MAGIC 0xb0fe673d
// Vector table must be aligned to power of 2 >= number of vectors * 4. on h743 it's 1024 
#define IMAGE_HEADER_SIZE 1024

// 4 bytes
typedef struct __attribute__((packed)) {
    uint8_t iv_major;
    uint8_t iv_minor;
    uint8_t iv_revision;
    uint8_t iv_reserved;
} image_version_t;

// 16 bytes
typedef struct __attribute__((packed)) {
    uint32_t ih_magic;
    uint16_t ih_hdr_size;
    uint16_t ih_ecu_id;
    uint32_t ih_img_size;
    image_version_t ih_img_ver;
} image_header_t;
_Static_assert(sizeof(image_header_t) <= IMAGE_HEADER_SIZE, "image header doesn't fit in its padded size");

// TLV structures ====================================================================
#define IMAGE_TLV_INFO_MAGIC 0xcd34
#define IMAGE_TLV_SHA512 0x01
#define IMAGE_TLV_ED25519_SIG 0x02
#define IMAGE_TLV_SHA512_SIZE 64
#define IMAGE_TLV_ED25519_SIG_SIZE 64

// 4 bytes
typedef struct __attribute__((packed)) {
    uint16_t th_tlv_magic;
    uint16_t th_tlv_total_size; // including header
} image_tlv_header_t;

// 4 bytes + tlv_len for each TLV
typedef struct  __attribute__((packed)) {
    uint16_t tlv_type;
    uint16_t tlv_len;
} image_tlv_t;

// TRAILER structures ================================================================
// Put trailer at start of final page of each slot
#define TRAILER_SIZE_BYTES sizeof(image_trailer_t)
#define TRAILER_A_START_ADDRESS (FW_SLOT_B_START_ADDRESS - FLASH_PAGE_SIZE_BYTES)
#define TRAILER_B_START_ADDRESS (FW_SLOT_B_START_ADDRESS + FW_SLOT_B_SIZE_BYTES - FLASH_PAGE_SIZE_BYTES)
#define TRAILER_MAGIC 0x56951c69

// 8 byte aligned
typedef union {
    uint8_t status;
    uint8_t raw[FLASH_WRITE_GRANULARITY_BYTES]; // pad to FLASH_WRITE_GRANULARITY_BYTES
} record_t;

// padded to FLASH_WRITE_GRANULARITY_BYTES
typedef union {
    uint32_t magic;
    uint8_t raw[FLASH_WRITE_GRANULARITY_BYTES];
} magic_record_t;

typedef struct {
    record_t trailer_swap_status[2 * FW_ACTUAL_SIZE_PAGES]; // 2 records per page in this slot
    record_t trailer_swap_size;
    record_t trailer_swap_info;
    record_t trailer_copy_done;
    record_t trailer_image_ok;
    magic_record_t trailer_magic;
} image_trailer_t;
#endif