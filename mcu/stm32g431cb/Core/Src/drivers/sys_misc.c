// Setup the data watchpoint and trace (DWT) to track cycle counts 
// why? so isotp.c can accurately meet the separation timing requirements without using a whole timer
// code based on https://www.hesliplabs.com/blog/swo-and-cycle-counting-on-stm32
// Adapted by Ryan Wong

#include "main.h"
#include "mcu_interface/sys_misc.h"

void sr_counter_start() {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t sr_cyccnt() {
    return DWT->CYCCNT;
}

uint32_t sr_millis() {
    return HAL_GetTick();
}

void sr_reset_mcu() {
    HAL_NVIC_SystemReset();
}