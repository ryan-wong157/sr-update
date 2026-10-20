// TEMP benchmarking for thesis, see bench.h for the CAN frame format

#include <string.h>
#include "main.h"
#include "bench.h"
#include "mcu_interface/sys_misc.h"
#include "mcu_interface/can_driver.h"

bench_transport_t bench_transport;
bench_boot_t bench_boot;

void bench_transport_reset(void) {
    memset(&bench_transport, 0, sizeof(bench_transport));
}

void bench_add_erase(uint32_t start_cyccnt) {
    bench_transport.erase_cycles += sr_cyccnt() - start_cyccnt;
    bench_transport.erase_count++;
}

void bench_add_write(uint32_t start_cyccnt) {
    bench_transport.write_cycles += sr_cyccnt() - start_cyccnt;
    bench_transport.write_count++;
}

static uint32_t cycles_to_us(uint64_t cycles) {
    return (uint32_t)(cycles / (SystemCoreClock / 1000000U));
}

// average per op in ns, ns rather than us so a ~80 us write doesn't lose precision
static uint32_t avg_ns(uint64_t cycles, uint32_t count) {
    if (count == 0) return 0;
    return (uint32_t)(cycles * 1000U / (SystemCoreClock / 1000000U) / count);
}

static void send_field(uint8_t field_id, uint32_t value) {
    uint8_t data[8] = {field_id, 0, 0, 0,
                       (uint8_t)value, (uint8_t)(value >> 8), (uint8_t)(value >> 16), (uint8_t)(value >> 24)};
    sr_fdcan_tx_blocking(CFG_BENCH_TX_ID, data, sizeof(data));
}

void bench_send_transport(void) {
    send_field(BENCH_ERASE_US, cycles_to_us(bench_transport.erase_cycles));
    send_field(BENCH_ERASE_N, bench_transport.erase_count);
    send_field(BENCH_ERASE_AVG_NS, avg_ns(bench_transport.erase_cycles, bench_transport.erase_count));
    send_field(BENCH_WRITE_US, cycles_to_us(bench_transport.write_cycles));
    send_field(BENCH_WRITE_N, bench_transport.write_count);
    send_field(BENCH_WRITE_AVG_NS, avg_ns(bench_transport.write_cycles, bench_transport.write_count));
}

void bench_send_boot(void) {
    // CAN may already be running from send_rejection, configure only works while stopped
    sr_fdcan_stop();
    if (sr_fdcan_configure() != SR_OK || sr_fdcan_start() != SR_OK) {
        return;
    }
    send_field(BENCH_HASH_US, cycles_to_us(bench_boot.hash_cycles));
    send_field(BENCH_SIG_US, cycles_to_us(bench_boot.sig_cycles));
    send_field(BENCH_SWAP_US, cycles_to_us(bench_boot.swap_cycles));
}
