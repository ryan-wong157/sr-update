// implementation of getting the time in microseconds since init called.
// why? so isotp.c can accurately meet the separation timing requirements
// code based on https://www.hesliplabs.com/blog/swo-and-cycle-counting-on-stm32
#include "drivers/dwt.h"

void dwt_init() {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; // Enable trace and debug
	DWT->CYCCNT = 0;                                // Reset counter
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;            // Enable the cycle counter
}

uint32_t micros() {
    // timeUS = cycles * period * 10^6 for micro
    return (DWT->CYCCNT / SystemCoreClock) * 1000000U;
}