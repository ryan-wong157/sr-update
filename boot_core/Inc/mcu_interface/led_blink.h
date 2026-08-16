#ifndef LED_BLINK_H
#define LED_BLINK_H

#include <stdint.h>
#include "sr_errno.h"

sr_errno_t sr_led_init();

sr_errno_t sr_led_blink();

#endif