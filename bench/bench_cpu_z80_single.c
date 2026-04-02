/**
 * @file bench_cpu_z80_single.c
 * @brief Benchmark cpu-z80 single-v0.2.
 */

#include "bench_common.h"
#include "cpu/z80.h"

static u8 mem_read(z80_t *cpu, u16 addr, int m1_state, void *ud) {
    (void)cpu; (void)m1_state; (void)ud;
    return ram[addr];
}

static void mem_write(z80_t *cpu, u16 addr, u8 data, void *ud) {
    (void)cpu; (void)ud;
    ram[addr] = data;
}

static u8 io_read(z80_t *cpu, u16 port, void *ud) {
    (void)cpu; (void)port; (void)ud;
    return 0xFF;
}

static void io_write(z80_t *cpu, u16 port, u8 data, void *ud) {
    (void)cpu; (void)port; (void)data; (void)ud;
}

int main(void) {
    load_test_program();
    z80_t *cpu = z80_create(mem_read, NULL, mem_write, NULL,
                             io_read, NULL, io_write, NULL, NULL, NULL);

    clock_t start = clock();
    while (!z80_is_halted(cpu)) z80_execute(cpu, 100000);
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    unsigned long long tc = cpu->total_cycles;
    printf("cpu-z80-single: %5.3f s | %12llu cyklu | %7.1f MHz\n",
           elapsed, tc, (tc / elapsed) / 1e6);
    z80_destroy(cpu);
    return 0;
}
