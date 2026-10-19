// TEMP benchmarking for thesis, uses the HAL UART directly

#include <string.h>
#include "bench.h"
#include "usart.h"
#include "mcu_interface/sys_misc.h"

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

// appends " key=value" to buf at *len
static void append_field(char* buf, uint32_t* len, const char* key, uint32_t value) {
    buf[(*len)++] = ' ';
    while (*key) buf[(*len)++] = *key++;
    buf[(*len)++] = '=';

    char digits[10];
    uint32_t n = 0;
    do {
        digits[n++] = '0' + (value % 10);
        value /= 10;
    } while (value);
    while (n) buf[(*len)++] = digits[--n];
}

static void send_line(char* buf, uint32_t len) {
    buf[len++] = '\r';
    buf[len++] = '\n';
    HAL_UART_Transmit(&huart2, (uint8_t*)buf, len, 100);
}

void bench_send_transport(void) {
    char buf[160] = "BENCH_TX";
    uint32_t len = strlen(buf);
    append_field(buf, &len, "erase_us", cycles_to_us(bench_transport.erase_cycles));
    append_field(buf, &len, "erase_n", bench_transport.erase_count);
    append_field(buf, &len, "erase_avg_ns", avg_ns(bench_transport.erase_cycles, bench_transport.erase_count));
    append_field(buf, &len, "write_us", cycles_to_us(bench_transport.write_cycles));
    append_field(buf, &len, "write_n", bench_transport.write_count);
    append_field(buf, &len, "write_avg_ns", avg_ns(bench_transport.write_cycles, bench_transport.write_count));
    send_line(buf, len);
}

void bench_send_boot(void) {
    char buf[128] = "BENCH_BOOT";
    uint32_t len = strlen(buf);
    append_field(buf, &len, "hash_us", cycles_to_us(bench_boot.hash_cycles));
    append_field(buf, &len, "sig_us", cycles_to_us(bench_boot.sig_cycles));
    append_field(buf, &len, "swap_us", cycles_to_us(bench_boot.swap_cycles));
    send_line(buf, len);
}
