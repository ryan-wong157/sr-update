#include "main.h"
#include "stdint.h"
#include "bl_jump.h"
#include "flash_layout.h"

void jump_to_app(void) {
    // get MSP
    uint32_t msp = *(volatile uint32_t*)APP_START_ADDR;

    // function pointer to reset handler
    void (*app_reset_handler)(void);
    app_reset_handler = (void (*)(void))(*(volatile uint32_t*)(APP_START_ADDR + 4));

    // need to disable irqs before jumping and setting msp
    __disable_irq();

    // stop systick timer from firing
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    // set new MSP
    __set_MSP(msp);

    // jump!
    app_reset_handler();
}