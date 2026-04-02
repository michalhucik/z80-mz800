/**
 * @file bench_z80ex.c
 * @brief Benchmark z80ex emulatoru (official i mz800 varianta).
 *
 * z80ex nema batch execute, pouziva se z80ex_step() ve smycce.
 * Kompiluje se s -DZ80EX_DIR=cesta pro vyber varianty.
 */

#include "bench_common.h"

/* z80ex include */
#include "include/z80ex.h"

static Z80EX_BYTE mem_read(Z80EX_CONTEXT *cpu, Z80EX_WORD addr,
                            int m1_state, void *ud) {
    (void)cpu; (void)m1_state; (void)ud;
    return ram[addr];
}

static void mem_write(Z80EX_CONTEXT *cpu, Z80EX_WORD addr,
                       Z80EX_BYTE data, void *ud) {
    (void)cpu; (void)ud;
    ram[addr] = data;
}

static Z80EX_BYTE io_read(Z80EX_CONTEXT *cpu, Z80EX_WORD port, void *ud) {
    (void)cpu; (void)port; (void)ud;
    return 0xFF;
}

static void io_write(Z80EX_CONTEXT *cpu, Z80EX_WORD port,
                      Z80EX_BYTE data, void *ud) {
    (void)cpu; (void)port; (void)data; (void)ud;
}

static Z80EX_BYTE int_read(Z80EX_CONTEXT *cpu, void *ud) {
    (void)cpu; (void)ud;
    return 0xFF;
}

int main(void) {
    load_test_program();

    Z80EX_CONTEXT *cpu = z80ex_create(
        mem_read, NULL,
        mem_write, NULL,
        io_read, NULL,
        io_write, NULL,
        int_read, NULL
    );

    clock_t start = clock();
    unsigned long long total = 0;

    while (!z80ex_doing_halt(cpu)) {
        total += z80ex_step(cpu);
    }

    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;

    printf(BENCH_LABEL ": %5.3f s | %12llu cyklu | %7.1f MHz\n",
           elapsed, total, (total / elapsed) / 1e6);

    z80ex_destroy(cpu);
    return 0;
}
