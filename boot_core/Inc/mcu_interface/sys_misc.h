#ifndef SYS_MISC_H
#define SYS_MISC_H

#include <stdint.h>
#include "sr_errno.h"

// Must disable IRQs, then jump to Reset Handler for slot 1
// app start is the very first byte address of the actual binary payload, after the image header
__attribute__((noreturn)) void jump_to_app(uint32_t app_start);

// start a cycle counter
void sr_counter_start();

// return cycle count
uint32_t sr_cyccnt();

// return microseconds elapsed since start_cyccnt (a value from sr_cyccnt())
uint32_t sr_micros_since(uint32_t start_cyccnt);

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