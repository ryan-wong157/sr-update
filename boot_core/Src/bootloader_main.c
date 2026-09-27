// Bootloader entry point. The CubeMX-generated Core/Src/main.c in the mcu layer
// is never used

#include <string.h>
#include "mcu_interface/peripherals.h"
#include "mcu_interface/sys_misc.h"
#include "uds/uds.h"
#include "image_format.h"

int main(void) {
    if (sr_peripherals_init() != SR_OK) {
        sr_reset_mcu();
    }

    if (sr_read_boot_magic() == BOOT_HOLD_MAGIC) {
        sr_write_boot_magic(0);
        sr_uds_server_start();
    } else {
        image_header_t header;
        memcpy(&header, *(volatile uint32_t*)FW_SLOT_A_START_ADDRESS, sizeof(header));
        uint32_t bin_start = FW_SLOT_A_START_ADDRESS + IMAGE_HEADER_SIZE;
        jump_to_app(bin_start);
    }
    return 0;
}
