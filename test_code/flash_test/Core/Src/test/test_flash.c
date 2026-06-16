#include "test/test_flash.h"

void test_flash() {
    uint32_t write_addr = 0x0801F800;
    uint64_t data = 0xDEADBEEF;
    sr_flash_status_t retval;

    retval = sr_flash_erase_page(FLASH_BANK_1, 63, 1);
    if (retval != SR_FLASH_OK) {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
        HAL_Delay(1000);
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
    }

    retval = sr_flash_write64(write_addr, data);
    if (retval != SR_FLASH_OK) {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
        HAL_Delay(1000);
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
    }

    if (*(volatile uint32_t*)write_addr != (uint32_t)data) {
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
        HAL_Delay(1000);
        HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_5);
    }
}