/*
 * Copyright (c) 2026 Michal Hucik
 * SPDX-License-Identifier: MIT
 * https://github.com/michalhucik/z80-mz800
 */
/**
 * @file test_motorola_ix.c
 * @brief Regresni testy ix_iy_style Motorola pro sdas/sdcc-asz80.
 *
 * Standalone test - linkuje jen libdasm_z80.a. Pokriva:
 *   - Z80_IX_MOTOROLA render "(d,IX)" / "(-d,IY)" pro kladny / zaporny /
 *     nulovy / extremni displacement (-128..+127).
 *   - Z80_IX_ZILOG (default) zachovava "(IX+d)" / "(IY-d)" - zero
 *     regression pro stavajici konzumenty.
 *
 * Inline mini-framework ve stylu tests/test_framework.h.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "z80_dasm.h"

/* ========== Inline test framework ========== */

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

#define EXPECT_STR_EQ(actual, expected) do { \
    const char *_a = (actual); \
    const char *_e = (expected); \
    if (_a == NULL || strcmp(_a, _e) != 0) { \
        printf("  FAIL [%s]: got \"%s\", expected \"%s\" (line %d)\n", \
               g_current, _a ? _a : "(null)", _e, __LINE__); \
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

#define SUMMARY() do { \
    printf("\n========================================\n"); \
    printf("Vysledky: %d testu, %d PASS, %d FAIL\n", \
           g_tests_run, g_tests_passed, g_tests_failed); \
    printf("========================================\n"); \
    return g_tests_failed > 0 ? 1 : 0; \
} while (0)

/* ========== Read callback ========== */

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

/* ========== Helper ========== */

static void dasm_one_style(u16 addr, const u8 *bytes, size_t len,
                           z80_ix_style_t ix_style, int uppercase,
                           char *out_buf, int out_size)
{
    read_ctx_t ctx = { addr, bytes, len };
    z80_dasm_inst_t inst;
    z80_dasm(&inst, read_cb, &ctx, addr);

    z80_dasm_format_t fmt;
    z80_dasm_format_default(&fmt);
    fmt.ix_iy_style = ix_style;
    fmt.uppercase = uppercase;

    z80_dasm_to_str(out_buf, out_size, &inst, &fmt);
}

/* ========== Test cases ========== */

/* LD A,(IX+5)  -> DD 7E 05 */
static void test_motorola_ix_positive(void)
{
    TEST_BEGIN("Motorola: (5,IX) positive displacement");
    u8 bytes[] = { 0xDD, 0x7E, 0x05 };
    char out[64];
    dasm_one_style(0x0000, bytes, sizeof(bytes), Z80_IX_MOTOROLA, 1,
                   out, sizeof(out));
    EXPECT_CONTAINS(out, "(5,IX)");
    TEST_END();
}

/* LD A,(IX-2)  -> DD 7E FE */
static void test_motorola_ix_negative(void)
{
    TEST_BEGIN("Motorola: (-2,IX) negative displacement");
    u8 bytes[] = { 0xDD, 0x7E, 0xFE };
    char out[64];
    dasm_one_style(0x0000, bytes, sizeof(bytes), Z80_IX_MOTOROLA, 1,
                   out, sizeof(out));
    EXPECT_CONTAINS(out, "(-2,IX)");
    TEST_END();
}

/* LD A,(IX+0)  -> DD 7E 00 */
static void test_motorola_ix_zero(void)
{
    TEST_BEGIN("Motorola: (0,IX) zero displacement");
    u8 bytes[] = { 0xDD, 0x7E, 0x00 };
    char out[64];
    dasm_one_style(0x0000, bytes, sizeof(bytes), Z80_IX_MOTOROLA, 1,
                   out, sizeof(out));
    EXPECT_CONTAINS(out, "(0,IX)");
    TEST_END();
}

/* LD A,(IY+10) -> FD 7E 0A */
static void test_motorola_iy_positive(void)
{
    TEST_BEGIN("Motorola: (10,IY) IY register");
    u8 bytes[] = { 0xFD, 0x7E, 0x0A };
    char out[64];
    dasm_one_style(0x0000, bytes, sizeof(bytes), Z80_IX_MOTOROLA, 1,
                   out, sizeof(out));
    EXPECT_CONTAINS(out, "(10,IY)");
    TEST_END();
}

/* Z80_IX_ZILOG (default) must still produce "(IX+5)" - regression check */
static void test_zilog_ix_default(void)
{
    TEST_BEGIN("Zilog default: (IX+5) unchanged");
    u8 bytes[] = { 0xDD, 0x7E, 0x05 };
    char out[64];
    dasm_one_style(0x0000, bytes, sizeof(bytes), Z80_IX_ZILOG, 1,
                   out, sizeof(out));
    EXPECT_CONTAINS(out, "(IX+#05)");
    TEST_END();
}

/* Extreme displacement -128 (0x80) -> "(-128,IX)" */
static void test_motorola_ix_extreme_negative(void)
{
    TEST_BEGIN("Motorola: (-128,IX) extreme negative");
    u8 bytes[] = { 0xDD, 0x7E, 0x80 };
    char out[64];
    dasm_one_style(0x0000, bytes, sizeof(bytes), Z80_IX_MOTOROLA, 1,
                   out, sizeof(out));
    EXPECT_CONTAINS(out, "(-128,IX)");
    TEST_END();
}

/* Extreme positive 127 (0x7F) -> "(127,IX)" */
static void test_motorola_ix_extreme_positive(void)
{
    TEST_BEGIN("Motorola: (127,IX) extreme positive");
    u8 bytes[] = { 0xDD, 0x7E, 0x7F };
    char out[64];
    dasm_one_style(0x0000, bytes, sizeof(bytes), Z80_IX_MOTOROLA, 1,
                   out, sizeof(out));
    EXPECT_CONTAINS(out, "(127,IX)");
    TEST_END();
}

/* ========== Main ========== */

int main(void)
{
    printf("\n=== dasm-z80 Motorola ix_iy_style regression tests ===\n");

    test_motorola_ix_positive();
    test_motorola_ix_negative();
    test_motorola_ix_zero();
    test_motorola_iy_positive();
    test_zilog_ix_default();
    test_motorola_ix_extreme_negative();
    test_motorola_ix_extreme_positive();

    SUMMARY();
}
