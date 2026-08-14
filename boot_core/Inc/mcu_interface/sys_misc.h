#ifndef SYS_MISC_H
#define SYS_MISC_H

#include <stdint.h>

// start a cycle counter
void sr_counter_start();

// return cycle count
uint32_t sr_cyccnt();

// return microseconds elapsed since counter start
uint32_t sr_micros();

// return millis count since start
uint32_t sr_millis();

// reset mcu
void sr_reset_mcu();

#endif