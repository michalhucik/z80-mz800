/*
 * Copyright (c) 2026 Michal Hucik
 * SPDX-License-Identifier: MIT
 * https://github.com/michalhucik/z80-mz800
 */
/**
 * @file zexall_runner.c
 * @brief ZEXALL/ZEXDOC runner pro cpu-z80 multi-v0.2.
 *
 * Minimalni CP/M harness pro spusteni Z80 Instruction Exerciser
 * (Frank Cringle / J.G. Harston).
 *
 * Princip: na BDOS entry point (0xFFF0) je instrukce HALT.
 * Kdyz CPU zahlati na teto adrese, runner zpracuje BDOS funkci,
 * nastavi PC za HALT a pokracuje. Na adrese 0x0000 je take HALT
 * pro detekci ukonceni (WBOOT).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cpu/z80.h"

/** 64KB RAM pro CP/M */
static u8 ram[65536];

/** BDOS entry point adresa */
#define BDOS_ENTRY 0xFFF0

/* ========== Callbacky ========== */

static u8 mem_read(z80_t *cpu, u16 addr, int m1_state, void *data) {
    (void)cpu; (void)m1_state; (void)data;
    return ram[addr];
}
static void mem_write(z80_t *cpu, u16 addr, u8 value, void *data) {
    (void)cpu; (void)data;
    ram[addr] = value;
}
static u8 io_read(z80_t *cpu, u16 port, void *data) {
    (void)cpu; (void)port; (void)data;
    return 0xFF;
}
static void io_write(z80_t *cpu, u16 port, u8 value, void *data) {
    (void)cpu; (void)port; (void)value; (void)data;
}

/* ========== CP/M BDOS emulace ========== */

/**
 * @brief Zpracovani BDOS volani.
 */
static void handle_bdos(z80_t *cpu) {
    u8 func = cpu->bc.l;

    switch (func) {
        case 2:
            putchar((char)cpu->de.l);
            break;
        case 9: {
            u16 addr = cpu->de.w;
            while (ram[addr] != '$') {
                putchar((char)ram[addr]);
                addr++;
            }
            break;
        }
        default:
            break;
    }
    fflush(stdout);

    cpu->pc = BDOS_ENTRY + 1;
    cpu->halted = false;
}

/* ========== Main ========== */

int main(int argc, char *argv[]) {
    const char *filename = "zexall.com";
    if (argc > 1) filename = argv[1];

    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Nelze otevrit '%s'\n", filename);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    memset(ram, 0, sizeof(ram));

    if (fread(&ram[0x0100], 1, (size_t)size, f) != (size_t)size) {
        fprintf(stderr, "Chyba cteni '%s'\n", filename);
        fclose(f);
        return 1;
    }
    fclose(f);

    /* CP/M setup */
    ram[0x0000] = 0x76;
    ram[0x0005] = 0xC3;
    ram[0x0006] = (u8)(BDOS_ENTRY & 0xFF);
    ram[0x0007] = (u8)(BDOS_ENTRY >> 8);
    ram[BDOS_ENTRY]     = 0x76;
    ram[BDOS_ENTRY + 1] = 0xC9;

    printf("=== ZEXALL Runner (cpu-z80 " CPU_Z80_VERSION ") ===\n");
    printf("Soubor: %s (%ld bajtu)\n\n", filename, size);

    z80_t *cpu = z80_create(
        mem_read, NULL, mem_write, NULL,
        io_read, NULL, io_write, NULL,
        NULL, NULL
    );
    cpu->pc = 0x0100;

    clock_t start = clock();

    while (1) {
        z80_execute(cpu, 100000);

        if (!cpu->halted) continue;

        u16 halt_pc = cpu->pc - 1;

        if (halt_pc == 0x0000) break;
        if (halt_pc == BDOS_ENTRY) {
            handle_bdos(cpu);
            continue;
        }

        fprintf(stderr, "\nNeocekavany HALT na 0x%04X\n", halt_pc);
        break;
    }

    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;

    printf("\n=== Dokonceno za %.1f s ===\n", elapsed);
    printf("Celkem T-stavu: %u\n", cpu->total_cycles);

    z80_destroy(cpu);
    return 0;
}
