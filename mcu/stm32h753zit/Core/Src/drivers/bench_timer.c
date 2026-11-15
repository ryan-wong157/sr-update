// TEMP benchmarking for thesis, tick source for boot_core/Src/bench.c
// TIM2 is a free running 32 bit up counter at 20 MHz (120 MHz / (PSC 5 + 1), ARR 0xFFFFFFFF), started in
// sr_peripherals_init(). 50 ns per tick and wraps every ~214 s, so a whole swap fits in one span unlike CYCCNT

#include "main.h"
#include "tim.h"
#include "bench.h"

uint32_t bench_ticks(void) {
    return htim2.Instance->CNT;
}

uint32_t bench_ticks_per_us(void) {
    return 20U;
}
