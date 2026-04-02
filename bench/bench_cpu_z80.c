/**
 * @file bench_cpu_z80.c
 * @brief Benchmark cpu-z80 emulatoru.
 *
 * Spousti testovaci Z80 program a meri vykon v MHz.
 */

#include "bench_common.h"
#include "cpu/z80.h"

/**
 * Callback pro cteni pameti.
 *
 * @param addr Adresa v pameti.
 * @return Bajt na dane adrese.
 */
static u8 mem_read(u16 addr) { return ram[addr]; }

/**
 * Callback pro zapis do pameti.
 *
 * @param addr Adresa v pameti.
 * @param data Zapisovany bajt.
 */
static void mem_write(u16 addr, u8 data) { ram[addr] = data; }

int main(void) {
    load_test_program();

    z80_t cpu;
    z80_init(&cpu);
    z80_set_mem_read(mem_read);
    z80_set_mem_write(mem_write);
    z80_set_mem_fetch(mem_read);

    /* Batch execution benchmark - pouziva z80_execute s velkym chunkem */
    clock_t start = clock();

    while (!cpu.halted) {
        z80_execute(&cpu, 100000);
    }

    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    unsigned long long total_cycles = cpu.total_cycles;

    printf("cpu-z80:  %8.3f s | %12llu cyklu | %7.1f MHz\n",
           elapsed, total_cycles,
           (total_cycles / elapsed) / 1000000.0);

    return 0;
}
