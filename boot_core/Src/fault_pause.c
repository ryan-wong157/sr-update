// TEMP fault tolerance testing, see fault_pause.h

#include "fault_pause.h"
#include "mcu_interface/led_blink.h"
#include "mcu_interface/sys_misc.h"

static void wait_ms(uint32_t ms) {
    uint32_t start = sr_millis();
    while (sr_millis() - start < ms) {}
}

void fault_pause(void) {
    sr_led_set(1);
    wait_ms(CFG_FAULT_PAUSE_ON_MS);
    sr_led_set(0);
    wait_ms(CFG_FAULT_PAUSE_OFF_MS);
}
