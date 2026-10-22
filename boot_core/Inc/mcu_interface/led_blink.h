#ifndef LED_BLINK_H
#define LED_BLINK_H

#include <stdint.h>
#include "sr_errno.h"

sr_errno_t sr_led_init();

sr_errno_t sr_led_blink();

// on = 1 lights the LED, 0 turns it off
void sr_led_set(uint8_t on);

#endif