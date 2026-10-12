#include <string.h>
#include "image_handler.h"
#include "image_format.h"
#include "mcu_interface/flash_driver.h"


static const image_header_t* get_header(slot_type_t slot) {
    if (slot == SLOT_A) return (const image_header_t*)FW_SLOT_A_START_ADDRESS;
    const image_header_t* update = (const image_header_t*)FW_SLOT_B_UPDATE_START_ADDRESS;
    if (update->ih_magic == IMAGE_MAGIC) return update;
    return (const image_header_t*)FW_SLOT_B_START_ADDRESS;
}
static image_trailer_t* get_trailer(slot_type_t slot) {
    if (slot == SLOT_A) return (image_trailer_t*)TRAILER_A_START_ADDRESS;
    return (image_trailer_t*)TRAILER_B_START_ADDRESS;
}

// HEADER functions ==================================================================
uint8_t is_header_magic_set(slot_type_t slot) {
    const image_header_t* header = get_header(slot);
    return (header->ih_magic == IMAGE_MAGIC) ? 1 : 0;
}

uint16_t get_header_size(slot_type_t slot) {
    const image_header_t* header = get_header(slot);
    if (!is_header_magic_set(slot)) return 0;
    return header->ih_hdr_size;
}

// Checks the image's ECU ID against the ECU_ID this bootloader was built with
uint8_t is_ecu_id_match(slot_type_t slot) {
    const image_header_t* header = get_header(slot);
    return (header->ih_ecu_id == ECU_ID) ? 1 : 0;
}

// Size of the binary payload only, Returns 0 if the slot has no valid header
uint32_t get_image_size(slot_type_t slot) {
    const image_header_t* header = get_header(slot);
    if (!is_header_magic_set(slot)) return 0;
    return header->ih_img_size;
}

static uint32_t get_total_image_size(slot_type_t slot) {
    uint32_t hdr_size = get_header_size(slot);
    uint32_t img_size = get_image_size(slot);
    if (hdr_size == 0 || img_size > FW_ACTUAL_SIZE_BYTES) return 0;

    uint32_t total = hdr_size + img_size;
    if (total + sizeof(image_tlv_header_t) > FW_ACTUAL_SIZE_BYTES) return 0;

    const image_tlv_header_t* tlv_header = (const image_tlv_header_t*)((uint32_t)get_header(slot) + total);
    if (tlv_header->th_tlv_magic == IMAGE_TLV_INFO_MAGIC) {
        total += tlv_header->th_tlv_total_size;
    }
    return total;
}

// Returned pointer points directly into flash
const image_version_t* get_image_version(slot_type_t slot) {
    const image_header_t* header = get_header(slot);
    return &(header->ih_img_ver);
}

// TRAILER functions =================================================================
sr_errno_t set_trailer_magic(slot_type_t slot) {
    image_trailer_t* trailer = get_trailer(slot);
    magic_record_t data;
    memset(&data, 0xFF, sizeof(data));
    data.magic = TRAILER_MAGIC;
    for (uint32_t offset = 0; offset < sizeof(data); offset += FLASH_WRITE_GRANULARITY_BYTES) {
        sr_errno_t retval = sr_flash_write((uint32_t)&(trailer->trailer_magic) + offset, data.raw + offset);
        if (retval != SR_OK) return retval;
    }
    return SR_OK;
}
uint8_t is_trailer_magic_set(slot_type_t slot) {
    image_trailer_t* trailer = get_trailer(slot);
    return (trailer->trailer_magic.magic == TRAILER_MAGIC) ? 1 : 0;
}

sr_errno_t set_copy_done(slot_type_t slot) {
    image_trailer_t* trailer = get_trailer(slot);
    record_t data;
    memset(&data, 0xFF, sizeof(data));
    data.status = 0x01;
    return sr_flash_write((uint32_t)&(trailer->trailer_copy_done), data.raw);
}
uint8_t is_copy_done(slot_type_t slot) {
    image_trailer_t* trailer = get_trailer(slot);
    return (trailer->trailer_copy_done.status == 0x01) ? 1 : 0;
}

sr_errno_t set_image_ok(slot_type_t slot) {
    image_trailer_t* trailer = get_trailer(slot);
    record_t data;
    memset(&data, 0xFF, sizeof(data));
    data.status = 0x01;
    return sr_flash_write((uint32_t)&(trailer->trailer_image_ok), data.raw);
}
uint8_t is_image_ok(slot_type_t slot) {
    image_trailer_t* trailer = get_trailer(slot);
    return (trailer->trailer_image_ok.status == 0x01) ? 1 : 0;
}

sr_errno_t set_swap_info(slot_type_t slot, swap_type_t swap_type) {
    image_trailer_t* trailer = get_trailer(slot);
    record_t data;
    memset(&data, 0xFF, sizeof(data));
    data.status = (uint8_t)swap_type;
    return sr_flash_write((uint32_t)&(trailer->trailer_swap_info), data.raw);
}
swap_type_t get_swap_info(slot_type_t slot) {
    image_trailer_t* trailer = get_trailer(slot);
    if (SWAP_TYPE_TEST <= trailer->trailer_swap_info.status && trailer->trailer_swap_info.status <= SWAP_TYPE_PERM) {
        return (swap_type_t)trailer->trailer_swap_info.status;
    } else {
        return SWAP_TYPE_NONE;
    }
}

sr_errno_t set_swap_size_pages(slot_type_t slot) {
    image_trailer_t* trailer = get_trailer(slot);
    uint32_t size_a = get_total_image_size(SLOT_A);
    uint32_t size_b = get_total_image_size(SLOT_B);
    uint32_t bigger = size_a > size_b ? size_a : size_b;
    uint32_t pages = (bigger + FLASH_PAGE_SIZE_BYTES - 1) / FLASH_PAGE_SIZE_BYTES;
    if (pages > FW_ACTUAL_SIZE_PAGES) pages = FW_ACTUAL_SIZE_PAGES;
    record_t data;
    memset(&data, 0xFF, sizeof(data));
    data.status = (uint8_t)pages;
    return sr_flash_write((uint32_t)&(trailer->trailer_swap_size), data.raw);
}

// Returns 0xFF if the swap size has not been set
uint32_t get_swap_size_pages(slot_type_t slot) {
    image_trailer_t* trailer = get_trailer(slot);
    return trailer->trailer_swap_size.status;
}

sr_errno_t erase_trailer(slot_type_t slot) {
    uint32_t bank;
    uint32_t page;
    sr_errno_t retval;
    if (slot == SLOT_A) {
        retval = sr_flash_get_page_info(TRAILER_A_START_ADDRESS, &bank, &page);
    } else {
        retval = sr_flash_get_page_info(TRAILER_B_START_ADDRESS, &bank, &page);
    }
    if (retval != SR_OK) return retval;
    return sr_flash_erase_page(bank, page, 1);
}

// SWAP functions ====================================================================
// Swap progress is recorded in slot A's trailer, 2 records per page
static sr_errno_t set_swap_status(uint32_t record_idx) {
    image_trailer_t* trailer = get_trailer(SLOT_A);
    record_t data;
    memset(&data, 0xFF, sizeof(data));
    data.status = 0x01;
    return sr_flash_write((uint32_t)&(trailer->trailer_swap_status[record_idx]), data.raw);
}

// Erases the page containing address
static sr_errno_t erase_page(uint32_t address) {
    uint32_t bank;
    uint32_t page;
    sr_errno_t retval = sr_flash_get_page_info(address, &bank, &page);
    if (retval != SR_OK) return retval;
    return sr_flash_erase_page(bank, page, 1);
}

// Erases the page at dst_address then copies the page at src_address into it
// Note that dst_address and src_address has to be the first byte of a page
static sr_errno_t copy_page(uint32_t dst_address, uint32_t src_address) {
    sr_errno_t retval = erase_page(dst_address);
    if (retval != SR_OK) return retval;

    for (uint32_t offset = 0; offset < FLASH_PAGE_SIZE_BYTES; offset += FLASH_WRITE_GRANULARITY_BYTES) {
        retval = sr_flash_write(dst_address + offset, (const uint8_t*)(src_address + offset));
        if (retval != SR_OK) return retval;
    }
    return SR_OK;
}

// writes p(idx) to s(idx) then s(idx + 1) to p(idx) and erases s(idx + 1)
// If is_halfway_done is set, only does the second write.
// Note that i use 0-indexing and you end up with filled s(0) page
sr_errno_t swap_forward(uint32_t index, uint32_t total_pages, uint8_t is_halfway_done) {
    if (total_pages > FW_ACTUAL_SIZE_PAGES || index > total_pages) return ERR_FLASH_INVALID_INPUT;

    for (uint32_t idx = index; idx < total_pages; idx++) {
        uint32_t primary = FW_SLOT_A_START_ADDRESS + idx * FLASH_PAGE_SIZE_BYTES;
        uint32_t secondary = FW_SLOT_B_START_ADDRESS + idx * FLASH_PAGE_SIZE_BYTES;
        sr_errno_t retval;

        if (!is_halfway_done) {
            retval = copy_page(secondary, primary);
            if (retval != SR_OK) return retval;
            retval = set_swap_status(2 * idx);
            if (retval != SR_OK) return retval;
        }
        is_halfway_done = 0;

        retval = copy_page(primary, secondary + FLASH_PAGE_SIZE_BYTES);
        if (retval != SR_OK) return retval;
        retval = set_swap_status(2 * idx + 1);
        if (retval != SR_OK) return retval;
    }

    // erase slot 2 final page to prevent leftover header being present in page 1 for 1 page swaps
    return erase_page(FW_SLOT_B_START_ADDRESS + total_pages * FLASH_PAGE_SIZE_BYTES);
}

// writes p(idx) to s(idx + 1) and s(idx) to p(idx), counting down from index to 0
// You end up with empty s(0) page. This is used for swapping back
sr_errno_t swap_revert(uint32_t index, uint32_t total_pages, uint8_t is_halfway_done) {
    if (total_pages > FW_ACTUAL_SIZE_PAGES || index >= total_pages) return ERR_FLASH_INVALID_INPUT;

    for (uint32_t idx = index + 1; idx-- > 0;) {
        uint32_t primary = FW_SLOT_A_START_ADDRESS + idx * FLASH_PAGE_SIZE_BYTES;
        uint32_t secondary = FW_SLOT_B_START_ADDRESS + idx * FLASH_PAGE_SIZE_BYTES;
        sr_errno_t retval;

        if (!is_halfway_done) {
            retval = copy_page(secondary + FLASH_PAGE_SIZE_BYTES, primary);
            if (retval != SR_OK) return retval;
            retval = set_swap_status(2 * idx);
            if (retval != SR_OK) return retval;
        }
        is_halfway_done = 0;

        retval = copy_page(primary, secondary);
        if (retval != SR_OK) return retval;
        retval = set_swap_status(2 * idx + 1);
        if (retval != SR_OK) return retval;
    }
    return SR_OK;
}
