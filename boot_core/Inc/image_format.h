#ifndef IMAGE_FORAMT_H
#define IMAGE_FORMAT_H

#include <stdint.h>


// HEADER structures =================================================================
#define IMAGE_MAGIC 0xb0fe673d
#define IMAGE_HEADER_SIZE 16

// 4 bytes
typedef struct __attribute__((packed)) {
    uint8_t iv_major;
    uint8_t iv_minor;
    uint16_t iv_revision;
} image_version_t;

// 16 bytes
typedef struct __attribute__((packed)) {
    uint32_t ih_magic;
    uint16_t ih_hdr_size;
    uint16_t ih_ecu_id;
    uint32_t ih_img_size;
    image_version_t ih_img_ver;
} image_header_t;

// TLV structures ====================================================================
#define IMAGE_TLV_INFO_MAGIC 0xcd34
#define IMAGE_TLV_SHA256 0x01
#define IMAGE_TLV_ED25519_SIG 0x02

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

#endif