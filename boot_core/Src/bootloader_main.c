// Bootloader entry point. The CubeMX-generated Core/Src/main.c in the mcu layer
// is never used

#include "mcu_interface/peripherals.h"
#include "mcu_interface/sys_misc.h"
#include "uds/uds.h"

int main(void) {
    if (sr_peripherals_init() != SR_OK) {
        sr_reset_mcu();
    }

    sr_uds_server_start();

    // sr_uds_server_start()
    return 0;
}
