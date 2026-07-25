// Setup the data watchpoint and trace (DWT) to track cycle counts 
// why? so isotp.c can accurately meet the separation timing requirements without using a whole timer
// code based on https://www.hesliplabs.com/blog/swo-and-cycle-counting-on-stm32
// Adapted by Ryan Wong

#include "drivers/dwt.h"

void sr_dwt_init() {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t sr_cyccnt() {
    return DWT->CYCCNT;
}