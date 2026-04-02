/*
 * Copyright (c) 2026 Michal Hucik
 * SPDX-License-Identifier: MIT
 * https://github.com/michalhucik/z80-mz800
 */
/**
 * @file zexall_runner.c
 * @brief ZEXALL/ZEXDOC runner pro vsechny varianty cpu-z80.
 *
 * Minimalni CP/M harness pro spusteni Z80 Instruction Exerciser
 * (Frank Cringle / J.G. Harston).
 *
 * Kompilace s ruznymi jadry:
 *   -DCORE_V01     -> cpu-z80 v0.1 (stare API)
 *   (default)      -> cpu-z80 multi-v0.2 (nove API)
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

#ifdef CORE_V01

/* cpu-z80 v0.1: jednoducha signatura bez cpu a user_data */
static u8 mem_read(u16 addr) { return ram[addr]; }
static void mem_write(u16 addr, u8 value) { ram[addr] = value; }
static u8 io_read(u16 port) { (void)port; return 0xFF; }
static void io_write(u16 port, u8 value) { (void)port; (void)value; }

#else

/* cpu-z80 multi-v0.2: signatura s cpu + user_data */
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

#endif

/* ========== CP/M BDOS emulace ========== */

/**
 * @brief Zpracovani BDOS volani.
 *
 * C = cislo funkce, DE = parametr.
 * Po zpracovani nastavi PC za HALT aby se provedl RET.
 */
static void handle_bdos(z80_t *cpu) {
    u8 func = cpu->bc.l;

    switch (func) {
        case 2:
            /* C_WRITE: vypis znak z E */
            putchar((char)cpu->de.l);
            break;
        case 9: {
            /* C_WRITESTR: vypis retezec z DE az do '$' */
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

    /* Preskocit HALT, dalsi instrukce je RET */
    cpu->pc = BDOS_ENTRY + 1;
    cpu->halted = false;
}

/* ========== Main ========== */

int main(int argc, char *argv[]) {
    const char *filename = "zexall.com";
    if (argc > 1) filename = argv[1];

    /* Nacteni .com souboru */
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Nelze otevrit '%s'\n", filename);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    memset(ram, 0, sizeof(ram));

    /* Nacteni na 0x0100 (CP/M .com load address) */
    if (fread(&ram[0x0100], 1, (size_t)size, f) != (size_t)size) {
        fprintf(stderr, "Chyba cteni '%s'\n", filename);
        fclose(f);
        return 1;
    }
    fclose(f);

    /* === CP/M setup === */
    ram[0x0000] = 0x76;              /* WBOOT: HALT */
    ram[0x0005] = 0xC3;              /* BDOS:  JP BDOS_ENTRY */
    ram[0x0006] = (u8)(BDOS_ENTRY & 0xFF);
    ram[0x0007] = (u8)(BDOS_ENTRY >> 8);
    ram[BDOS_ENTRY]     = 0x76;      /* HALT - zachytime v hlavni smycce */
    ram[BDOS_ENTRY + 1] = 0xC9;      /* RET  - navrat po zpracovani */

    printf("=== ZEXALL Runner (cpu-z80 " CPU_Z80_VERSION ") ===\n");
    printf("Soubor: %s (%ld bajtu)\n\n", filename, size);

    /* Vytvoreni CPU */
#ifdef CORE_V01
    z80_t cpu_inst;
    z80_t *cpu = &cpu_inst;
    z80_init(cpu);
    z80_set_mem_read(mem_read);
    z80_set_mem_write(mem_write);
    z80_set_mem_fetch(mem_read);
    z80_set_io_read(io_read);
    z80_set_io_write(io_write);
#else
    z80_t *cpu = z80_create(
        mem_read, NULL, mem_write, NULL,
        io_read, NULL, io_write, NULL,
        NULL, NULL
    );
#endif

    cpu->pc = 0x0100;

    clock_t start = clock();

    /* Hlavni emulacni smycka */
    while (1) {
        z80_execute(cpu, 100000);

        if (!cpu->halted) continue;

        u16 halt_pc = cpu->pc - 1;

        if (halt_pc == 0x0000) break;            /* WBOOT */
        if (halt_pc == BDOS_ENTRY) {              /* BDOS */
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

#ifndef CORE_V01
    z80_destroy(cpu);
#endif

    return 0;
}
