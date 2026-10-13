// Bootloader entry point. The CubeMX-generated Core/Src/main.c in the mcu layer
// is never used

#include <string.h>
#include "mcu_interface/peripherals.h"
#include "mcu_interface/sys_misc.h"
#include "mcu_interface/led_blink.h"
#include "uds/uds.h"
#include "image_handler.h" 

int main(void) {
    if (sr_peripherals_init() != SR_OK) {
        sr_reset_mcu();
    }
    
    sr_led_blink();

    uint8_t confirm_revert = 0;
    if (is_trailer_magic_set(SLOT_A) && !is_copy_done(SLOT_A) && !is_trailer_magic_set(SLOT_B)) {
        // resume swap in progress
        uint32_t index;
        uint8_t is_halfway_done;
        swap_type_t swap_type = get_swap_info(SLOT_A);
        uint32_t total_pages = get_swap_size_pages(SLOT_A);
        sr_errno_t retval = get_swap_status(&index, &is_halfway_done);

        if (retval == SR_OK) {
            if (swap_type != SWAP_TYPE_REVERT) {
                retval = swap_forward(index, total_pages, is_halfway_done);
            } else if (index < total_pages) {
                // index == total_pages means every page was already done
                retval = swap_revert(index, total_pages, is_halfway_done);
            }
        }

        if (retval == SR_OK && swap_type == SWAP_TYPE_TEST) {
            retval = set_copy_done(SLOT_A);
        } else if (swap_type == SWAP_TYPE_REVERT) {
            // image ok and copy done get set below once the reverted image is validated
            confirm_revert = 1;
        }

        if (retval != SR_OK) {
            sr_reset_mcu();
        }
    } else if (is_trailer_magic_set(SLOT_B) && !is_image_ok(SLOT_B) && is_copy_done(SLOT_B)) {
        // revert
    } else if (is_trailer_magic_set(SLOT_B) && !is_image_ok(SLOT_B)) {
        // test swap
    } else if (is_trailer_magic_set(SLOT_B) && is_image_ok(SLOT_B)) {
        // perm swap
    } else if (is_trailer_magic_set(SLOT_A) && is_copy_done(SLOT_A) && !is_image_ok(SLOT_A)) {
        // revert
    } else if (sr_read_boot_magic() == BOOT_HOLD_MAGIC) {
        sr_write_boot_magic(0);
        sr_led_blink();
        sr_uds_server_start();
    }

    // TODO: start IWDG
    if (is_image_valid(SLOT_A, 0)) {
        if (confirm_revert) {
            sr_errno_t retval = SR_OK;
            if (!is_image_ok(SLOT_A)) retval = set_image_ok(SLOT_A);
            if (retval == SR_OK && !is_copy_done(SLOT_A)) retval = set_copy_done(SLOT_A);
            if (retval != SR_OK) sr_reset_mcu();
        }
        sr_peripherals_deinit();
        jump_to_app(FW_SLOT_A_START_ADDRESS + IMAGE_HEADER_SIZE);
    }

    // Slot A's image is not valid, fallback to UDS. TODO: broadcast error msg
    sr_uds_server_start();
    return 0;
}
