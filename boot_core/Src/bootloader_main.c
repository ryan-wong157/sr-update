// Bootloader entry point. The CubeMX-generated Core/Src/main.c in the mcu layer
// is never used

#include <string.h>
#include "mcu_interface/peripherals.h"
#include "mcu_interface/sys_misc.h"
#include "mcu_interface/led_blink.h"
#include "uds/uds.h"
#include "image_handler.h" 

// Sets up slot A's trailer for a revert, clears the revert marker in slot B's trailer, then swaps back.
static sr_errno_t start_revert(void) {
    sr_errno_t retval = erase_trailer(SLOT_A);
    if (retval == SR_OK) retval = set_swap_info(SLOT_A, SWAP_TYPE_REVERT);
    if (retval == SR_OK) retval = set_swap_size_pages(SLOT_A);
    if (retval == SR_OK) retval = set_trailer_magic(SLOT_A);
    if (retval == SR_OK) retval = erase_trailer(SLOT_B);
    if (retval != SR_OK) return retval;

    uint32_t swap_size = get_swap_size_pages(SLOT_A);
    return swap_revert(swap_size - 1, swap_size, 0);
}

int main(void) {
    if (sr_peripherals_init() != SR_OK) {
        sr_reset_mcu();
    }

    sr_led_blink();

    uint8_t confirm_revert = 0;
    uint8_t boot_allowed = 1;
    sr_errno_t retval;
    if (is_trailer_magic_set(SLOT_A) && !is_copy_done(SLOT_A) && !is_trailer_magic_set(SLOT_B)) {
        // resume swap in progress
        uint32_t index;
        uint8_t is_halfway_done;
        swap_type_t swap_type = get_swap_info(SLOT_A);
        uint32_t total_pages = get_swap_size_pages(SLOT_A);
        retval = get_swap_status(&index, &is_halfway_done);

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
        // revert but powered-off before slot B is erased in the normal revert path
        retval = start_revert();
        if (retval != SR_OK) {
            sr_reset_mcu();
        }
        confirm_revert = 1;
    } else if (is_trailer_magic_set(SLOT_B) && !is_image_ok(SLOT_B)) {
        // test swap
    } else if (is_trailer_magic_set(SLOT_B) && is_image_ok(SLOT_B)) {
        // perm swap
    } else if (is_trailer_magic_set(SLOT_A) && is_copy_done(SLOT_A) && !is_image_ok(SLOT_A)) {
        // revert
        if (is_image_valid(SLOT_B, 0)) {
            // need to write copy_done and magic in B trailer to handle power off if tralier A hasn't been fully written yet
            // If that is the case, it falls back to the revert path above
            retval = erase_trailer(SLOT_B);
            if (retval == SR_OK) retval = set_copy_done(SLOT_B);
            if (retval == SR_OK) retval = set_trailer_magic(SLOT_B);
            if (retval == SR_OK) retval = start_revert();
            if (retval != SR_OK) {
                sr_reset_mcu();
            }
            confirm_revert = 1;
        } else {
            // nothing valid to revert to, don't boot the unconfirmed image
            boot_allowed = 0;
        }
    } else if (sr_read_boot_magic() == BOOT_HOLD_MAGIC) {
        sr_write_boot_magic(0);
        sr_led_blink();
        sr_uds_server_start();
    }

    // TODO: start IWDG
    if (boot_allowed && is_image_valid(SLOT_A, 0)) {
        if (confirm_revert) {
            // image ok goes first, copy done without it would get reverted again
            retval = set_image_ok(SLOT_A);
            if (retval == SR_OK) retval = set_copy_done(SLOT_A);
            if (retval != SR_OK) sr_reset_mcu();
        }
        sr_peripherals_deinit();
        jump_to_app(FW_SLOT_A_START_ADDRESS + IMAGE_HEADER_SIZE);
    }

    // Slot A's image is not valid or a revert had nothing to go back to, fallback to UDS. TODO: broadcast error msg
    sr_uds_server_start();
    return 0;
}
