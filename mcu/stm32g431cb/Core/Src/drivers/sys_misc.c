// Setup the data watchpoint and trace (DWT) to track cycle counts 
// why? so isotp.c can accurately meet the separation timing requirements without using a whole timer
// code based on https://www.hesliplabs.com/blog/swo-and-cycle-counting-on-stm32
// Adapted by Ryan Wong

#include "main.h"
#include "rtc.h"
#include "config/flash_config.h"
#include "mcu_interface/sys_misc.h"

static RTC_HandleTypeDef* rtc_handle = &hrtc;

// Switching MSP and branching must happen with no stack touching after MSP change
// NOTE: this was a bug I found in the RELEASE build. it tries to pop after MSP load and before branch leading to hard fault
// This makes sure that doesn't happen
__attribute__((naked, noreturn))
static void branch_to(uint32_t msp, uint32_t reset_handler) {
    __asm volatile(
        "msr msp, r0\n"
        "bx  r1\n"
    );
}

void jump_to_app(uint32_t app_start) {
    uint32_t msp = *(volatile uint32_t*)app_start;
    uint32_t reset_handler = *(volatile uint32_t*)(app_start + 4);

    // need to disable irqs before jumping and setting msp
    __disable_irq();

    // stop systick timer from firing
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    // clear any interrupts left over
    for (uint32_t i = 0; i < sizeof(NVIC->ICER) / sizeof(NVIC->ICER[0]); i++) {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }

    SCB->VTOR = app_start;
    __DSB();
    __ISB();

    branch_to(msp, reset_handler);
}

void sr_counter_start() {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t sr_cyccnt() {
    return DWT->CYCCNT;
}

uint32_t sr_micros_since(uint32_t start_cyccnt) {
    // given start cycles, how many microseconds have passed since then?
    return (DWT->CYCCNT - start_cyccnt) / (SystemCoreClock / 1000000U);
}

uint32_t sr_millis() {
    return HAL_GetTick();
}

void sr_reset_mcu() {
    HAL_NVIC_SystemReset();
}

void sr_write_boot_magic(uint32_t data) {
    HAL_PWR_EnableBkUpAccess();
    HAL_RTCEx_BKUPWrite(rtc_handle, RTC_BKP_DR0, data);
    HAL_PWR_DisableBkUpAccess();
}

uint32_t sr_read_boot_magic() {
    return HAL_RTCEx_BKUPRead(rtc_handle, RTC_BKP_DR0);
}