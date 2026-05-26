/*
 * Copyright (c) 2026 Michal Hucik
 * SPDX-License-Identifier: MIT
 * https://github.com/michalhucik/z80-mz800
 */
/**
 * @file test_dasm_sym.c
 * @brief Regresni testy symbolickeho resolveru disassembleru dasm-z80.
 *
 * Standalone test - linkuje jen libdasm_z80.a. Pokriva symbol substituci
 * pro ridici instrukce (CALL / JP / JR / JR cc / DJNZ / RST + cond
 * varianty). Po pruchodu z80_dasm_to_str_sym() s naplnenou symtab se
 * cilova adresa musi nahradit jmenem symbolu pokud na ni symbol existuje;
 * pro adresy bez symbolu a pro nepřímé skoky (JP (HL)/(IX)/(IY)) musi
 * vystup zustat hex.
 *
 * RST_GROUP test je regression coverage pro fix RST symbol resolve:
 * RST p ma 8-bit operand (Z80_OP_RST_VEC), z80_dasm_to_str ho formatuje
 * jako "#%02x". Symtab substituce drive sestavovala hledaci string
 * vzdy jako "#%04x" a strstr() nenasel shodu.
 *
 * Inline mini-framework ve stylu tests/test_framework.h - bez externich
 * zavislosti.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "z80_dasm.h"

/* =========================================================================
 * Inline test framework
 * ========================================================================= */

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;
static const char *g_current = "";
static int g_current_failed = 0;

#define TEST_BEGIN(name) do { \
    g_current = name; \
    g_current_failed = 0; \
    g_tests_run++; \
} while (0)

#define TEST_END() do { \
    if (g_current_failed) g_tests_failed++; \
    else                  g_tests_passed++; \
} while (0)

#define EXPECT_TRUE(cond) do { \
    if (!(cond)) { \
        printf("  FAIL [%s]: %s is not true (line %d)\n", \
               g_current, #cond, __LINE__); \
        g_current_failed = 1; \
    } \
} while (0)

#define EXPECT_CONTAINS(haystack, needle) do { \
    const char *_h = (haystack); \
    const char *_n = (needle); \
    if (_h == NULL || strstr(_h, _n) == NULL) { \
        printf("  FAIL [%s]: \"%s\" does not contain \"%s\" (line %d)\n", \
               g_current, _h ? _h : "(null)", _n, __LINE__); \
        g_current_failed = 1; \
    } \
} while (0)

#define EXPECT_NOT_CONTAINS(haystack, needle) do { \
    const char *_h = (haystack); \
    const char *_n = (needle); \
    if (_h != NULL && strstr(_h, _n) != NULL) { \
        printf("  FAIL [%s]: \"%s\" must not contain \"%s\" (line %d)\n", \
               g_current, _h, _n, __LINE__); \
        g_current_failed = 1; \
    } \
} while (0)

#define SUMMARY() do { \
    printf("\n========================================\n"); \
    printf("Vysledky: %d testu, %d PASS, %d FAIL\n", \
           g_tests_run, g_tests_passed, g_tests_failed); \
    printf("========================================\n"); \
    return g_tests_failed > 0 ? 1 : 0; \
} while (0)

/* =========================================================================
 * Read callback - mapuje base_addr na opcode buffer
 * ========================================================================= */

typedef struct {
    u16 base_addr;
    const u8 *bytes;
    size_t len;
} read_ctx_t;

static u8 read_cb(u16 addr, void *userdata)
{
    read_ctx_t *ctx = (read_ctx_t *)userdata;
    u16 off = (u16)(addr - ctx->base_addr);
    if (off < ctx->len) return ctx->bytes[off];
    return 0x00;
}

/* =========================================================================
 * Helper: disassembluj jednu instrukci a vrat formatovany retezec
 * ========================================================================= */

static void dasm_one(u16 addr, const u8 *bytes, size_t len,
                     const z80_symtab_t *symtab,
                     char *out_buf, int out_size)
{
    read_ctx_t ctx = { addr, bytes, len };
    z80_dasm_inst_t inst;
    z80_dasm(&inst, read_cb, &ctx, addr);

    z80_dasm_format_t fmt;
    z80_dasm_format_default(&fmt);
    /*
     * For predictable substring matches in expectations we use the
     * lowercase / Z80_HEX_HASH style throughout. Default is HASH +
     * uppercase=1; we flip uppercase off for the search strings to
     * stay consistent.
     */
    fmt.uppercase = 0;

    z80_dasm_to_str_sym(out_buf, out_size, &inst, &fmt, symtab);
}

/* =========================================================================
 * Test cases
 * ========================================================================= */

/* --- CALL / JP / JR / JR cc / DJNZ resolve --- */

static void test_call_resolve(z80_symtab_t *tab)
{
    TEST_BEGIN("CALL nn resolves symbol");
    z80_symtab_clear(tab);
    z80_symtab_add(tab, 0x1234, "PRINT");
    u8 bytes[] = { 0xCD, 0x34, 0x12 };  /* CALL 0x1234 */
    char out[64];
    dasm_one(0x0000, bytes, sizeof(bytes), tab, out, sizeof(out));
    EXPECT_CONTAINS(out, "PRINT");
    EXPECT_NOT_CONTAINS(out, "#1234");
    TEST_END();
}

static void test_jp_resolve(z80_symtab_t *tab)
{
    TEST_BEGIN("JP nn resolves symbol");
    z80_symtab_clear(tab);
    z80_symtab_add(tab, 0x8000, "MAIN");
    u8 bytes[] = { 0xC3, 0x00, 0x80 };  /* JP 0x8000 */
    char out[64];
    dasm_one(0x0100, bytes, sizeof(bytes), tab, out, sizeof(out));
    EXPECT_CONTAINS(out, "MAIN");
    EXPECT_NOT_CONTAINS(out, "#8000");
    TEST_END();
}

static void test_jp_cc_resolve(z80_symtab_t *tab)
{
    TEST_BEGIN("JP cc,nn resolves symbol");
    z80_symtab_clear(tab);
    z80_symtab_add(tab, 0x8000, "RETRY");
    u8 bytes[] = { 0xC2, 0x00, 0x80 };  /* JP NZ, 0x8000 */
    char out[64];
    dasm_one(0x0100, bytes, sizeof(bytes), tab, out, sizeof(out));
    EXPECT_CONTAINS(out, "RETRY");
    TEST_END();
}

static void test_jr_resolve(z80_symtab_t *tab)
{
    TEST_BEGIN("JR e resolves symbol (absolute target)");
    z80_symtab_clear(tab);
    /* JR +0x05 at 0x0100 -> target 0x0107 (PC + 2 + 5) */
    z80_symtab_add(tab, 0x0107, "LOOP");
    u8 bytes[] = { 0x18, 0x05 };
    char out[64];
    dasm_one(0x0100, bytes, sizeof(bytes), tab, out, sizeof(out));
    EXPECT_CONTAINS(out, "LOOP");
    TEST_END();
}

static void test_jr_cc_resolve(z80_symtab_t *tab)
{
    TEST_BEGIN("JR cc,e resolves symbol");
    z80_symtab_clear(tab);
    z80_symtab_add(tab, 0x0107, "DONE");
    u8 bytes[] = { 0x28, 0x05 };  /* JR Z, +0x05 at 0x0100 -> 0x0107 */
    char out[64];
    dasm_one(0x0100, bytes, sizeof(bytes), tab, out, sizeof(out));
    EXPECT_CONTAINS(out, "DONE");
    TEST_END();
}

static void test_djnz_resolve(z80_symtab_t *tab)
{
    TEST_BEGIN("DJNZ e resolves symbol");
    z80_symtab_clear(tab);
    z80_symtab_add(tab, 0x0107, "AGAIN");
    u8 bytes[] = { 0x10, 0x05 };  /* DJNZ +0x05 at 0x0100 -> 0x0107 */
    char out[64];
    dasm_one(0x0100, bytes, sizeof(bytes), tab, out, sizeof(out));
    EXPECT_CONTAINS(out, "AGAIN");
    TEST_END();
}

/* --- RST resolve (regression for the bug Port 6 fixes) --- */

static void test_rst_resolve(z80_symtab_t *tab)
{
    TEST_BEGIN("RST 0x20 resolves symbol (regression)");
    z80_symtab_clear(tab);
    z80_symtab_add(tab, 0x0020, "PCG_PRINT");
    u8 bytes[] = { 0xE7 };  /* RST 0x20 */
    char out[64];
    dasm_one(0x0000, bytes, sizeof(bytes), tab, out, sizeof(out));
    /*
     * Before the fix, symtab built the lookup key as "#0020" while the
     * formatted text contained "#20", so strstr() did not match and the
     * symbol substitution was silently skipped.
     */
    EXPECT_CONTAINS(out, "PCG_PRINT");
    TEST_END();
}

static void test_rst_all_vectors_resolve(z80_symtab_t *tab)
{
    TEST_BEGIN("RST 0x00, 0x08, ..., 0x38 all resolve");
    /* RST 0..7 opcodes */
    static const u8 opcodes[8] = {
        0xC7, 0xCF, 0xD7, 0xDF, 0xE7, 0xEF, 0xF7, 0xFF
    };
    static const u16 targets[8] = {
        0x00, 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38
    };
    static const char *names[8] = {
        "RST00", "RST08", "RST10", "RST18",
        "RST20", "RST28", "RST30", "RST38"
    };
    z80_symtab_clear(tab);
    for (int i = 0; i < 8; i++) {
        z80_symtab_add(tab, targets[i], names[i]);
    }
    char out[64];
    for (int i = 0; i < 8; i++) {
        u8 b = opcodes[i];
        dasm_one(0x0100, &b, 1, tab, out, sizeof(out));
        EXPECT_CONTAINS(out, names[i]);
    }
    TEST_END();
}

/* --- Address without matching symbol must stay hex --- */

static void test_no_symbol_stays_hex(z80_symtab_t *tab)
{
    TEST_BEGIN("CALL to unknown address stays hex");
    z80_symtab_clear(tab);
    z80_symtab_add(tab, 0x9999, "UNRELATED");
    u8 bytes[] = { 0xCD, 0x34, 0x12 };  /* CALL 0x1234 */
    char out[64];
    dasm_one(0x0000, bytes, sizeof(bytes), tab, out, sizeof(out));
    EXPECT_NOT_CONTAINS(out, "UNRELATED");
    EXPECT_CONTAINS(out, "1234");
    TEST_END();
}

/* --- Indirect jumps must not get a symbol substituted --- */

static void test_jp_hl_not_substituted(z80_symtab_t *tab)
{
    TEST_BEGIN("JP (HL) does not get symbol substituted");
    z80_symtab_clear(tab);
    z80_symtab_add(tab, 0x0000, "ZERO");
    u8 bytes[] = { 0xE9 };  /* JP (HL) */
    char out[64];
    dasm_one(0x0000, bytes, sizeof(bytes), tab, out, sizeof(out));
    EXPECT_NOT_CONTAINS(out, "ZERO");
    TEST_END();
}

static void test_jp_ix_not_substituted(z80_symtab_t *tab)
{
    TEST_BEGIN("JP (IX) does not get symbol substituted");
    z80_symtab_clear(tab);
    z80_symtab_add(tab, 0x0000, "ZERO");
    u8 bytes[] = { 0xDD, 0xE9 };  /* JP (IX) */
    char out[64];
    dasm_one(0x0000, bytes, sizeof(bytes), tab, out, sizeof(out));
    EXPECT_NOT_CONTAINS(out, "ZERO");
    TEST_END();
}

/* --- HEX_H_SUFFIX leading-zero (v0.1.4 regression) ---
 *
 * H_SUFFIX render emituje "0c000h" pro adresy >= 0xA000 (leading '0'
 * aby asm parser nezamenil za identifikator). Symtab substituce drive
 * hledala jen "c000h", nasla substring uvnitr a zanechala orphan '0'
 * pred symbolem ("JP 0Lc000" misto "JP Lc000"). */

static void dasm_one_h_suffix(u16 addr, const u8 *bytes, size_t len,
                              const z80_symtab_t *symtab,
                              char *out_buf, int out_size)
{
    read_ctx_t ctx = { addr, bytes, len };
    z80_dasm_inst_t inst;
    z80_dasm(&inst, read_cb, &ctx, addr);

    z80_dasm_format_t fmt;
    z80_dasm_format_default(&fmt);
    fmt.hex_style = Z80_HEX_H_SUFFIX;
    fmt.uppercase = 0;

    z80_dasm_to_str_sym(out_buf, out_size, &inst, &fmt, symtab);
}

static void test_h_suffix_target_sym_above_a000(z80_symtab_t *tab)
{
    TEST_BEGIN("H_SUFFIX: JP 0xC000 -> symbol (no orphan '0')");
    z80_symtab_clear(tab);
    z80_symtab_add(tab, 0xC000, "Lc000");
    u8 bytes[] = { 0xC3, 0x00, 0xC0 };  /* JP 0xC000 */
    char out[64];
    dasm_one_h_suffix(0x0100, bytes, sizeof(bytes), tab, out, sizeof(out));
    EXPECT_CONTAINS(out, "Lc000");
    EXPECT_NOT_CONTAINS(out, "0Lc000");  /* orphan '0' regression */
    EXPECT_NOT_CONTAINS(out, "c000h");
    TEST_END();
}

static void test_h_suffix_target_sym_below_a000(z80_symtab_t *tab)
{
    TEST_BEGIN("H_SUFFIX: JP 0x1234 -> symbol (no leading-zero path)");
    z80_symtab_clear(tab);
    z80_symtab_add(tab, 0x1234, "ROUT");
    u8 bytes[] = { 0xC3, 0x34, 0x12 };  /* JP 0x1234 */
    char out[64];
    dasm_one_h_suffix(0x0100, bytes, sizeof(bytes), tab, out, sizeof(out));
    EXPECT_CONTAINS(out, "ROUT");
    EXPECT_NOT_CONTAINS(out, "1234h");
    TEST_END();
}

static void test_h_suffix_mem_sym_above_a000(z80_symtab_t *tab)
{
    TEST_BEGIN("H_SUFFIX: LD A,(0xE000) -> (symbol) (no orphan '0')");
    z80_symtab_clear(tab);
    z80_symtab_add(tab, 0xE000, "VRAM");
    u8 bytes[] = { 0x3A, 0x00, 0xE0 };  /* LD A,(0xE000) */
    char out[64];
    dasm_one_h_suffix(0x0100, bytes, sizeof(bytes), tab, out, sizeof(out));
    /*
     * mem_sym is only enabled when the user opts in via z80_symtab API;
     * here we just verify that if it IS substituted, no orphan '0'
     * appears. If render is "LD A,(VRAM)" - pass. If "LD A,(0e000h)"
     * (no substitution) - also acceptable on its own, but then orphan
     * cannot occur. The fail case is "LD A,(0VRAM)".
     */
    EXPECT_NOT_CONTAINS(out, "(0VRAM");
    EXPECT_NOT_CONTAINS(out, "0Le000");
    TEST_END();
}

static void test_h_suffix_rst_unaffected(z80_symtab_t *tab)
{
    TEST_BEGIN("H_SUFFIX: RST 0x20 still substitutes (RST is 8-bit)");
    z80_symtab_clear(tab);
    z80_symtab_add(tab, 0x0020, "PCG_PRINT");
    u8 bytes[] = { 0xE7 };  /* RST 0x20 */
    char out[64];
    dasm_one_h_suffix(0x0000, bytes, sizeof(bytes), tab, out, sizeof(out));
    EXPECT_CONTAINS(out, "PCG_PRINT");
    TEST_END();
}

/* --- NULL symtab fallback --- */

static void test_null_symtab(void)
{
    TEST_BEGIN("NULL symtab leaves output as hex");
    u8 bytes[] = { 0xCD, 0x34, 0x12 };  /* CALL 0x1234 */
    char out[64];
    dasm_one(0x0000, bytes, sizeof(bytes), NULL, out, sizeof(out));
    EXPECT_CONTAINS(out, "1234");
    TEST_END();
}

/* =========================================================================
 * Main
 * ========================================================================= */

int main(void)
{
    printf("\n=== dasm-z80 symbol resolver regression tests ===\n");

    z80_symtab_t *tab = z80_symtab_create();
    if (!tab) {
        fprintf(stderr, "FATAL: z80_symtab_create() returned NULL\n");
        return 2;
    }

    test_call_resolve(tab);
    test_jp_resolve(tab);
    test_jp_cc_resolve(tab);
    test_jr_resolve(tab);
    test_jr_cc_resolve(tab);
    test_djnz_resolve(tab);

    test_rst_resolve(tab);
    test_rst_all_vectors_resolve(tab);

    test_no_symbol_stays_hex(tab);
    test_jp_hl_not_substituted(tab);
    test_jp_ix_not_substituted(tab);

    test_h_suffix_target_sym_above_a000(tab);
    test_h_suffix_target_sym_below_a000(tab);
    test_h_suffix_mem_sym_above_a000(tab);
    test_h_suffix_rst_unaffected(tab);

    test_null_symtab();

    z80_symtab_destroy(tab);
    SUMMARY();
}
