#ifndef DWT_H
#define DWT_H

#include "main.h"

// sets up data watchpoint and trace to begin cycle counting
void sr_dwt_init();

// return curr cycle count in DWT->CYCCNT
uint32_t sr_cyccnt();

#endif