/*
 * Copyright (c) 2026 Michal Hucik
 * SPDX-License-Identifier: MIT
 * https://github.com/michalhucik/z80-mz800
 */
/**
 * @file test_dasm_prefix.c
 * @brief Regresní testy zobrazení ignorovaného prefixu DD/FD a neplatné ED.
 *
 * Prefix DD nebo FD, za kterým následuje další prefix (DD, FD) nebo ED,
 * CPU vykoná (4 T) a další bajt zpracuje jako novou instrukci; jádro
 * cpu-z80 obojí provede v jednom kroku. Disassembler ho ukazuje jako
 * jednobajtovou instrukci "DD*" / "FD*" (do v0.1.5 "NOP*"), nedefinovaná
 * ED xx zůstává "NOP*" (2 bajty, 8 T).
 *
 * Pokrývá:
 *   - DD DD 21 34 12: první řádek DD* (1 B, 4 T), další DD 21 = LD IX,1234h
 *   - FD DD, FD FD, DD FD, DD ED, FD ED: DD* / FD*
 *   - ED 00 (nedefinovaná): NOP*, 2 B, 8 T
 *
 * Standalone test - linkuje jen libdasm_z80.a.
 * Inline mini-framework ve stylu tests/test_framework.h.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "z80_dasm.h"

/* ========== Inline test framework ========== */

/** Počet spuštěných testů. */
static int g_tests_run = 0;
/** Počet úspěšných testů. */
static int g_tests_passed = 0;
/** Počet neúspěšných testů. */
static int g_tests_failed = 0;
/** Název aktuálního testu. */
static const char *g_current = "";
/** Příznak selhání aktuálního testu. */
static int g_current_failed = 0;

/** Začátek testu - nastaví název a vynuluje příznak selhání. */
#define TEST_BEGIN(name) do { \
    g_current = name; \
    g_current_failed = 0; \
    g_tests_run++; \
} while (0)

/** Konec testu - započítá výsledek. */
#define TEST_END() do { \
    if (g_current_failed) g_tests_failed++; \
    else                  g_tests_passed++; \
} while (0)

/** Ověří shodu dvou řetězců. */
#define EXPECT_STR_EQ(actual, expected) do { \
    const char *_a = (actual); \
    const char *_e = (expected); \
    if (_a == NULL || strcmp(_a, _e) != 0) { \
        printf("  FAIL [%s]: got \"%s\", expected \"%s\" (line %d)\n", \
               g_current, _a ? _a : "(null)", _e, __LINE__); \
        g_current_failed = 1; \
    } \
} while (0)

/** Ověří shodu dvou celých čísel. */
#define EXPECT_INT_EQ(actual, expected) do { \
    long _a = (long)(actual); \
    long _e = (long)(expected); \
    if (_a != _e) { \
        printf("  FAIL [%s]: %s == %ld, expected %ld (line %d)\n", \
               g_current, #actual, _a, _e, __LINE__); \
        g_current_failed = 1; \
    } \
} while (0)

/** Vypíše souhrn a vrátí z main() 0 při úspěchu, 1 při selhání. */
#define SUMMARY() do { \
    printf("\n========================================\n"); \
    printf("Vysledky: %d testu, %d PASS, %d FAIL\n", \
           g_tests_run, g_tests_passed, g_tests_failed); \
    printf("========================================\n"); \
    return g_tests_failed > 0 ? 1 : 0; \
} while (0)

/* ========== Read callback ========== */

/** Kontext read callbacku - čtení z bufferu od adresy 0. */
typedef struct {
    const u8 *bytes;  /**< Buffer instrukcí (čte se od adresy 0). */
    size_t len;       /**< Velikost bufferu. */
} read_ctx_t;

/**
 * @brief Read callback pro z80_dasm (mimo buffer vrací 0x00 = NOP).
 * @param addr     Adresa čteného bajtu.
 * @param userdata Ukazatel na read_ctx_t.
 * @return Bajt z bufferu, nebo 0x00 mimo něj.
 */
static u8 read_cb(u16 addr, void *userdata) {
    const read_ctx_t *ctx = (const read_ctx_t *)userdata;
    return (addr < ctx->len) ? ctx->bytes[addr] : 0x00;
}

/* ========== Helper ========== */

/**
 * @brief Dekóduje instrukci na adrese @p addr a vyformátuje ji velkými písmeny.
 * @param bytes   Buffer instrukcí od adresy 0.
 * @param len     Velikost bufferu.
 * @param addr    Adresa dekódované instrukce.
 * @param inst    Výstup dekodéru.
 * @param out     Výstupní buffer pro text instrukce (bez adresy a bajtů).
 * @param out_sz  Velikost výstupního bufferu.
 */
static void decode(const u8 *bytes, size_t len, u16 addr,
                   z80_dasm_inst_t *inst, char *out, int out_sz) {
    read_ctx_t ctx = { bytes, len };
    z80_dasm(inst, read_cb, &ctx, addr);
    z80_dasm_format_t fmt;
    z80_dasm_format_default(&fmt);
    fmt.show_addr = 0;
    fmt.show_bytes = 0;
    fmt.uppercase = 1;
    z80_dasm_to_str(out, out_sz, inst, &fmt);
}

/* ========== Test cases ========== */

/** @brief DD DD 21 34 12: DD* (1 B, 4 T), pak LD IX,1234h. */
static void test_dd_dd_chain(void) {
    static const u8 bytes[] = { 0xDD, 0xDD, 0x21, 0x34, 0x12 };
    z80_dasm_inst_t inst;
    char out[64];
    TEST_BEGIN("DD DD 21 34 12 -> DD*, then LD IX,nn");
    decode(bytes, sizeof bytes, 0, &inst, out, sizeof out);
    EXPECT_INT_EQ(inst.length, 1);
    EXPECT_INT_EQ(inst.t_states, 4);
    EXPECT_INT_EQ(inst.cls, Z80_CLASS_INVALID);
    EXPECT_STR_EQ(inst.mnemonic, "DD*");
    EXPECT_STR_EQ(out, "DD*");

    decode(bytes, sizeof bytes, 1, &inst, out, sizeof out);
    EXPECT_INT_EQ(inst.length, 4);
    EXPECT_STR_EQ(inst.mnemonic, "LD");
    TEST_END();
}

/** @brief Ostatní kombinace ignorovaného prefixu. */
static void test_other_ignored_prefixes(void) {
    static const struct { u8 b0, b1; const char *want; } cases[] = {
        { 0xFD, 0xDD, "FD*" }, { 0xFD, 0xFD, "FD*" }, { 0xDD, 0xFD, "DD*" },
        { 0xDD, 0xED, "DD*" }, { 0xFD, 0xED, "FD*" },
    };
    TEST_BEGIN("FD DD, FD FD, DD FD, DD ED, FD ED -> DD* / FD*");
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        const u8 bytes[] = { cases[i].b0, cases[i].b1, 0x00, 0x00 };
        z80_dasm_inst_t inst;
        char out[64];
        decode(bytes, sizeof bytes, 0, &inst, out, sizeof out);
        EXPECT_INT_EQ(inst.length, 1);
        EXPECT_STR_EQ(inst.mnemonic, cases[i].want);
        EXPECT_STR_EQ(out, cases[i].want);
    }
    TEST_END();
}

/** @brief Nedefinovaná ED 00 zůstává NOP* (2 B, 8 T). */
static void test_invalid_ed_stays_nop(void) {
    static const u8 bytes[] = { 0xED, 0x00 };
    z80_dasm_inst_t inst;
    char out[64];
    TEST_BEGIN("ED 00 stays NOP* (2 bytes, 8 T)");
    decode(bytes, sizeof bytes, 0, &inst, out, sizeof out);
    EXPECT_INT_EQ(inst.length, 2);
    EXPECT_INT_EQ(inst.t_states, 8);
    EXPECT_STR_EQ(inst.mnemonic, "NOP*");
    EXPECT_STR_EQ(out, "NOP*");
    TEST_END();
}

/* ========== Main ========== */

/**
 * @brief Spustí všechny testy ignorovaného prefixu a vypíše souhrn.
 * @return 0 = všechny testy prošly, 1 = některé selhaly.
 */
int main(void) {
    printf("\n=== dasm-z80 ignored DD/FD prefix regression tests ===\n");

    test_dd_dd_chain();
    test_other_ignored_prefixes();
    test_invalid_ed_stays_nop();

    SUMMARY();
}
