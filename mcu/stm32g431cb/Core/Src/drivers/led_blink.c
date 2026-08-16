#include "main.h"
#include "mcu_interface/led_blink.h"
#include "sr_errno.h"

sr_errno_t sr_led_init() {
    (void)1;
    return HAL_OK;
}

sr_errno_t sr_led_blink() {
    for (int i = 0; i < 5; i++) {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
        HAL_Delay(1000);
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
        HAL_Delay(1000);
    }
    return HAL_OK;
}