#ifndef BENCH_H
#define BENCH_H

// TEMP benchmarking for thesis. Times are accumulated in bench timer ticks (bench_ticks()) and
// converted to microseconds when sent. Results go out as raw CAN frames on CFG_BENCH_TX_ID, one
// field per frame: [field_id, 0, 0, 0, value (u32 little endian)]. ota_client prints them.

#include <stdint.h>
#include "common_config/boot_config.h"

#define CFG_BENCH_TX_ID (0x1F000000u | ((uint32_t)(ECU_ID) << 12) | 0xFFCu)

// field ids, mirrored in host_scripts/ota_client/src/ota_client/cli.py
#define BENCH_ERASE_US      0x01u
#define BENCH_ERASE_N       0x02u
#define BENCH_ERASE_AVG_NS  0x03u
#define BENCH_WRITE_US      0x04u
#define BENCH_WRITE_N       0x05u
#define BENCH_WRITE_AVG_NS  0x06u
#define BENCH_STACK_TX_BYTES 0x07u
#define BENCH_HASH_US       0x11u
#define BENCH_SIG_US        0x12u
#define BENCH_SWAP_US       0x13u
#define BENCH_STACK_BOOT_BYTES 0x14u

// payload flash ops only (flash_writer.c), sent before the 0x11 reset
typedef struct {
    uint64_t erase_ticks;
    uint32_t erase_count;
    uint64_t write_ticks;
    uint32_t write_count;
} bench_transport_t;

// on the boot after the download, sent before booting/rejecting
typedef struct {
    uint32_t hash_ticks;
    uint32_t sig_ticks;
    uint32_t swap_ticks;
} bench_boot_t;

extern bench_transport_t bench_transport;
extern bench_boot_t bench_boot;

// Free running 32 bit tick counter, implemented per MCU port (drivers/bench_timer.c). A single
// timed span must be shorter than one wrap (2^32 / ticks per us)
uint32_t bench_ticks(void);
uint32_t bench_ticks_per_us(void);

void bench_transport_reset(void);
// start_ticks is the bench_ticks() value taken just before the erase/write
void bench_add_erase(uint32_t start_ticks);
void bench_add_write(uint32_t start_ticks);

// peak stack use in bytes since reset, RAM is painted 0xAA by __wrap_SystemInit
uint32_t bench_stack_used(void);

// CAN must already be running (called from the UDS server)
void bench_send_transport(void);
// (re)starts CAN itself
void bench_send_boot(void);

#endif
