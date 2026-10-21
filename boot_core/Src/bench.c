// TEMP benchmarking for thesis, see bench.h for the CAN frame format

#include <string.h>
#include "main.h"
#include "bench.h"
#include "mcu_interface/sys_misc.h"
#include "mcu_interface/can_driver.h"

bench_transport_t bench_transport;
bench_boot_t bench_boot;

// Reset_Handler calls SystemInit before .data/.bss are set up, -Wl,--wrap=SystemInit sends it here first.
// Paints all of RAM with 0xAA so bench_stack_used() can find how deep the stack got. Naked because it
// overwrites the stack it would otherwise be using, nothing has been pushed yet since Reset_Handler
// only did a bl. Tail calls the real SystemInit with lr untouched.
__attribute__((naked)) void __wrap_SystemInit(void) {
    __asm volatile(
        "ldr r0, =_sdata\n" // start of RAM (.data is the first RAM section)
        "ldr r1, =_estack\n" // end of RAM
        "ldr r2, =0xAAAAAAAA\n"
        "1:\n"
        "cmp r0, r1\n"
        "bhs 2f\n"
        "str r2, [r0], #4\n"
        "b 1b\n"
        "2:\n"
        "b __real_SystemInit\n"
        ".ltorg\n"
    );
}

// Walks down from the top of RAM to the end of .bss and returns how many bytes below _estack the lowest
// non 0xAA word is, i.e. the peak stack use since reset. Keeps going past 0xAA words instead of stopping
// at the first one, so an untouched local in the middle of the stack can't cut it short. Naked so the
// scan doesn't push anything onto the stack it is measuring.
__attribute__((naked)) uint32_t bench_stack_used(void) {
    __asm volatile(
        "ldr r1, =_estack\n" // scan pointer, walks down
        "ldr r2, =_ebss\n" // stop here, below this is .data/.bss
        "ldr r3, =0xAAAAAAAA\n"
        "mov r0, r1\n" // lowest dirty address so far, _estack = nothing used
        "1:\n"
        "cmp r1, r2\n"
        "bls 2f\n"
        "ldr r12, [r1, #-4]!\n"
        "cmp r12, r3\n"
        "it ne\n"
        "movne r0, r1\n"
        "b 1b\n"
        "2:\n"
        "ldr r1, =_estack\n"
        "sub r0, r1, r0\n"
        "bx lr\n"
        ".ltorg\n"
    );
}

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
    send_field(BENCH_STACK_TX_BYTES, bench_stack_used());
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
    send_field(BENCH_STACK_BOOT_BYTES, bench_stack_used());
}
