/**
 * @file bench_cpu_z80.c
 * @brief Benchmark cpu-z80 - batch / per-step / per-step + RAM fast-path.
 *
 * Rezim se voli pri prekladu:
 *   (vychozi)                       batch     - z80_execute(cpu, 100000)
 *   -DBENCH_PER_STEP                per-step  - z80_step(cpu) po jedne instrukci
 *   -DBENCH_PER_STEP -DCPU_Z80_RAM_FASTPATH
 *                                   per-step + inline RAM fast-path
 *
 * Per-step rezim odpovida zpusobu, jakym cycle-accurate emulator skutecne
 * pohani CPU (boundary po kazde instrukci) - na rozdil od batch, ktery je
 * nejrychlejsi, ale realny emulator s mid-instruction synchronizaci periferii
 * ho pouzit nemuze. BENCH_LABEL odlisuje vystup jednotlivych rezimu.
 */

#include "bench_common.h"
#include "cpu/z80.h"

#ifndef BENCH_LABEL
#define BENCH_LABEL "cpu-z80 batch "
#endif

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

#ifdef CPU_Z80_RAM_FASTPATH
    /*
     * Benchmark ma celou 64 KiB jako plochou RAM (zadny banking/IO mapping),
     * takze vsech 16 stranek ukazuje primo na ram[]. dbus_latch nepouzivame.
     */
    u8 *fp[16];
    for (int i = 0; i < 16; i++) fp[i] = ram + (i * 0x1000);
    z80_set_ram_fastpath(cpu, fp, fp, NULL, true);
#endif

    clock_t start = clock();
#ifdef BENCH_PER_STEP
    while (!z80_is_halted(cpu)) z80_step(cpu);
#else
    while (!z80_is_halted(cpu)) z80_execute(cpu, 100000);
#endif
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    unsigned long long tc = cpu->total_cycles;
    printf("%s  %5.3f s | %12llu cyklu | %7.1f MHz\n",
           BENCH_LABEL, elapsed, tc, (tc / elapsed) / 1e6);
    z80_destroy(cpu);
    return 0;
}
