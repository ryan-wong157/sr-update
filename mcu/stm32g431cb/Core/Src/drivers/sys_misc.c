// Setup the data watchpoint and trace (DWT) to track cycle counts 
// why? so isotp.c can accurately meet the separation timing requirements without using a whole timer
// code based on https://www.hesliplabs.com/blog/swo-and-cycle-counting-on-stm32
// Adapted by Ryan Wong

#include "main.h"
#include "rtc.h"
#include "mcu_interface/sys_misc.h"

static RTC_HandleTypeDef* rtc_handle = &hrtc;

void sr_counter_start() {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t sr_cyccnt() {
    return DWT->CYCCNT;
}

uint32_t sr_micros() {
    return DWT->CYCCNT / (SystemCoreClock / 1000000U);
}

uint32_t sr_millis() {
    return HAL_GetTick();
}

void sr_reset_mcu() {
    HAL_NVIC_SystemReset();
}

sr_errno_t sr_write_boot_magic() {
    HAL_PWR_EnableBkUpAccess();
    HAL_RTCEx_BKUPWrite(rtc_handle, RTC_BKP_DR0, (uint32_t)BOOT_HOLD_MAGIC);
    HAL_PWR_DisableBkUpAccess();
}

sr_errno_t sr_read_boot_magic(uint32_t* data) {
    return HAL_RTCEx_BKUPRead(rtc_handle, RTC_BKP_DR0);
}