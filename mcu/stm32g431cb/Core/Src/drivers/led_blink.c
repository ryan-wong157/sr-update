#include "main.h"
#include "mcu_interface/led_blink.h"
#include "sr_errno.h"

sr_errno_t sr_led_init() {
    (void)1;
    return HAL_OK;
}

sr_errno_t sr_led_blink() {
    for (int i = 0; i < 10; i++) {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
        HAL_Delay(100);
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
        HAL_Delay(100);
    }
    return HAL_OK;
}

void sr_led_set(uint8_t on) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}