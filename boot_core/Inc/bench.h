#ifndef BENCH_H
#define BENCH_H

// TEMP benchmarking for thesis. Times are accumulated in CPU cycles and converted
// to microseconds when sent. Results go out as raw CAN frames on CFG_BENCH_TX_ID, one
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
#define BENCH_HASH_US       0x11u
#define BENCH_SIG_US        0x12u
#define BENCH_SWAP_US       0x13u

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

// CAN must already be running (called from the UDS server)
void bench_send_transport(void);
// (re)starts CAN itself
void bench_send_boot(void);

#endif
