#ifndef SYS_MISC_H
#define SYS_MISC_H

#include <stdint.h>
#include "sr_errno.h"

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

// These two functions handle accessing the persistent value which tells the bootloader to 
// wait in UDS server for new firmware rather than jumping to app

// boot_magic = 0xB007C0DE -> hold in bootloader
// boot_magic != 0xB007C0DE -> boot regularly
#define BOOT_HOLD_MAGIC 0xB007C0DE

// write to backup reg
void sr_write_boot_magic(uint32_t data);

// read from backup reg
uint32_t sr_read_boot_magic();

#endif