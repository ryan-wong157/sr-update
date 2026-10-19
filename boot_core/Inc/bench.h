#ifndef BENCH_H
#define BENCH_H

// TEMP benchmarking for thesis. Times are accumulated in CPU cycles and
// converted to microseconds when sent over USART2.

#include <stdint.h>

// payload flash ops only (flash_writer.c), sent before the 0x11 reset
typedef struct {
    uint64_t erase_cycles;
    uint32_t erase_count;
    uint64_t write_cycles;
    uint32_t write_count;
} bench_transport_t;

// on the boot after the download, sent before booting/rejecting
typedef struct {
    uint32_t hash_cycles;
    uint32_t sig_cycles;
    uint32_t swap_cycles;
} bench_boot_t;

extern bench_transport_t bench_transport;
extern bench_boot_t bench_boot;

void bench_transport_reset(void);
// start_cyccnt is the sr_cyccnt() value taken just before the erase/write
void bench_add_erase(uint32_t start_cyccnt);
void bench_add_write(uint32_t start_cyccnt);

void bench_send_transport(void);
void bench_send_boot(void);

#endif
