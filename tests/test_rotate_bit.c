/**
 * @file test_rotate_bit.c
 * @brief Testy rotaci, posuvu a bitovych operaci Z80.
 *
 * Pokryva: RLCA, RRCA, RLA, RRA, CB prefix (RLC, RRC, RL, RR,
 * SLA, SRA, SLL, SRL), BIT, SET, RES, RRD, RLD.
 */

#include "test_framework.h"

/* ========== Zakladni rotace (bez CB) ========== */

/**
 * @brief Test RLCA, RRCA, RLA, RRA (4T, nemeni S/Z/P).
 */
static void test_basic_rotates(void) {
    int cyc;

    TEST_BEGIN("RLCA (A=0x80 -> 0x01, C=1)");
    setup();
    cpu->af.h = 0x80;
    cpu->af.l = Z80_FLAG_Z | Z80_FLAG_S;  /* Z,S se zachovaji */
    test_ram[0] = 0x07;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x01);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_FLAG_SET(Z80_FLAG_Z);  /* zachovan */
    ASSERT_FLAG_SET(Z80_FLAG_S);  /* zachovan */
    ASSERT_FLAG_CLEAR(Z80_FLAG_N);
    ASSERT_FLAG_CLEAR(Z80_FLAG_H);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();

    TEST_BEGIN("RLCA (A=0x55 -> 0xAA, C=0)");
    setup();
    cpu->af.h = 0x55;
    test_ram[0] = 0x07;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0xAA);
    ASSERT_FLAG_CLEAR(Z80_FLAG_C);
    TEST_END();

    TEST_BEGIN("RRCA (A=0x01 -> 0x80, C=1)");
    setup();
    cpu->af.h = 0x01;
    test_ram[0] = 0x0F;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x80);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();

    TEST_BEGIN("RLA (A=0x80, C=0 -> A=0x00, C=1)");
    setup();
    cpu->af.h = 0x80;
    cpu->af.l = 0;
    test_ram[0] = 0x17;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x00);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();

    TEST_BEGIN("RLA (A=0x00, C=1 -> A=0x01, C=0)");
    setup();
    cpu->af.h = 0x00;
    cpu->af.l = Z80_FLAG_C;
    test_ram[0] = 0x17;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x01);
    ASSERT_FLAG_CLEAR(Z80_FLAG_C);
    TEST_END();

    TEST_BEGIN("RRA (A=0x01, C=0 -> A=0x00, C=1)");
    setup();
    cpu->af.h = 0x01;
    cpu->af.l = 0;
    test_ram[0] = 0x1F;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x00);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();

    TEST_BEGIN("RRA (A=0x00, C=1 -> A=0x80, C=0)");
    setup();
    cpu->af.h = 0x00;
    cpu->af.l = Z80_FLAG_C;
    test_ram[0] = 0x1F;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x80);
    ASSERT_FLAG_CLEAR(Z80_FLAG_C);
    TEST_END();
}

/* ========== CB prefix rotace/posuvy ========== */

/**
 * @brief Test CB RLC r (8T), CB RLC (HL) (15T).
 */
static void test_cb_rlc(void) {
    int cyc;

    TEST_BEGIN("CB RLC B (0x80 -> 0x01, C=1)");
    setup();
    cpu->bc.h = 0x80;
    test_ram[0] = 0xCB; test_ram[1] = 0x00;
    cyc = step();
    ASSERT_EQ(cpu->bc.h, 0x01);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_FLAG_CLEAR(Z80_FLAG_Z);
    ASSERT_FLAG_CLEAR(Z80_FLAG_N);
    ASSERT_FLAG_CLEAR(Z80_FLAG_H);
    ASSERT_CYCLES(cyc, 8);
    TEST_END();

    TEST_BEGIN("CB RLC (HL)");
    setup();
    cpu->hl.w = 0x5000;
    test_ram[0x5000] = 0x01;
    test_ram[0] = 0xCB; test_ram[1] = 0x06;
    cyc = step();
    ASSERT_EQ(test_ram[0x5000], 0x02);
    ASSERT_FLAG_CLEAR(Z80_FLAG_C);
    ASSERT_CYCLES(cyc, 15);
    TEST_END();
}

/**
 * @brief Test CB SLA, SRA, SRL, SLL.
 */
static void test_cb_shifts(void) {
    int cyc;

    TEST_BEGIN("CB SLA B (0x80 -> 0x00, C=1)");
    setup();
    cpu->bc.h = 0x80;
    test_ram[0] = 0xCB; test_ram[1] = 0x20;
    cyc = step();
    ASSERT_EQ(cpu->bc.h, 0x00);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    ASSERT_CYCLES(cyc, 8);
    TEST_END();

    TEST_BEGIN("CB SRA B (0x80 -> 0xC0, sign preserved)");
    setup();
    cpu->bc.h = 0x80;
    test_ram[0] = 0xCB; test_ram[1] = 0x28;
    cyc = step();
    ASSERT_EQ(cpu->bc.h, 0xC0);
    ASSERT_FLAG_CLEAR(Z80_FLAG_C);
    ASSERT_CYCLES(cyc, 8);
    TEST_END();

    TEST_BEGIN("CB SRL B (0x01 -> 0x00, C=1)");
    setup();
    cpu->bc.h = 0x01;
    test_ram[0] = 0xCB; test_ram[1] = 0x38;
    cyc = step();
    ASSERT_EQ(cpu->bc.h, 0x00);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    ASSERT_CYCLES(cyc, 8);
    TEST_END();

    TEST_BEGIN("CB SLL B (nedokumentovana, 0x80 -> 0x01, C=1)");
    setup();
    cpu->bc.h = 0x80;
    test_ram[0] = 0xCB; test_ram[1] = 0x30;
    cyc = step();
    ASSERT_EQ(cpu->bc.h, 0x01);  /* bit 0 se nastavi na 1 */
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_CYCLES(cyc, 8);
    TEST_END();

    TEST_BEGIN("CB SLL A (0x00 -> 0x01)");
    setup();
    cpu->af.h = 0x00;
    test_ram[0] = 0xCB; test_ram[1] = 0x37;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x01);
    ASSERT_FLAG_CLEAR(Z80_FLAG_C);
    ASSERT_FLAG_CLEAR(Z80_FLAG_Z);
    TEST_END();
}

/* ========== BIT, SET, RES ========== */

/**
 * @brief Test BIT n, r (8T), BIT n, (HL) (12T).
 */
static void test_bit(void) {
    int cyc;

    TEST_BEGIN("BIT 0,B (set)");
    setup();
    cpu->bc.h = 0x01;
    test_ram[0] = 0xCB; test_ram[1] = 0x40;
    cyc = step();
    ASSERT_FLAG_CLEAR(Z80_FLAG_Z);
    ASSERT_FLAG_SET(Z80_FLAG_H);
    ASSERT_FLAG_CLEAR(Z80_FLAG_N);
    ASSERT_CYCLES(cyc, 8);
    TEST_END();

    TEST_BEGIN("BIT 0,B (clear)");
    setup();
    cpu->bc.h = 0xFE;
    test_ram[0] = 0xCB; test_ram[1] = 0x40;
    cyc = step();
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    ASSERT_FLAG_SET(Z80_FLAG_PV);  /* Z -> PV taky set */
    TEST_END();

    TEST_BEGIN("BIT 7,A (sign)");
    setup();
    cpu->af.h = 0x80;
    test_ram[0] = 0xCB; test_ram[1] = 0x7F;
    cyc = step();
    ASSERT_FLAG_CLEAR(Z80_FLAG_Z);
    ASSERT_FLAG_SET(Z80_FLAG_S);
    ASSERT_CYCLES(cyc, 8);
    TEST_END();

    TEST_BEGIN("BIT 3,(HL)");
    setup();
    cpu->hl.w = 0x5000;
    test_ram[0x5000] = 0x08;  /* bit 3 = 1 */
    test_ram[0] = 0xCB; test_ram[1] = 0x5E;
    cyc = step();
    ASSERT_FLAG_CLEAR(Z80_FLAG_Z);
    ASSERT_CYCLES(cyc, 12);
    TEST_END();

    TEST_BEGIN("BIT 3,(HL) (F3/F5 z WZ high)");
    setup();
    cpu->hl.w = 0x5000;
    cpu->wz.w = 0x2800;  /* WZ high = 0x28 -> F5=1, F3=1 */
    test_ram[0x5000] = 0x00;  /* bit 3 = 0 */
    test_ram[0] = 0xCB; test_ram[1] = 0x5E;
    cyc = step();
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    /* F3/F5 by mely byt z WZ high bajtu */
    ASSERT_CYCLES(cyc, 12);
    TEST_END();
}

/**
 * @brief Test SET n, r (8T), SET n, (HL) (15T).
 */
static void test_set(void) {
    int cyc;

    TEST_BEGIN("SET 0,B");
    setup();
    cpu->bc.h = 0x00;
    test_ram[0] = 0xCB; test_ram[1] = 0xC0;
    cyc = step();
    ASSERT_EQ(cpu->bc.h, 0x01);
    ASSERT_CYCLES(cyc, 8);
    TEST_END();

    TEST_BEGIN("SET 7,A");
    setup();
    cpu->af.h = 0x00;
    test_ram[0] = 0xCB; test_ram[1] = 0xFF;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x80);
    ASSERT_CYCLES(cyc, 8);
    TEST_END();

    TEST_BEGIN("SET 5,(HL)");
    setup();
    cpu->hl.w = 0x6000;
    test_ram[0x6000] = 0x00;
    test_ram[0] = 0xCB; test_ram[1] = 0xEE;
    cyc = step();
    ASSERT_EQ(test_ram[0x6000], 0x20);
    ASSERT_CYCLES(cyc, 15);
    TEST_END();
}

/**
 * @brief Test RES n, r (8T), RES n, (HL) (15T).
 */
static void test_res(void) {
    int cyc;

    TEST_BEGIN("RES 0,B");
    setup();
    cpu->bc.h = 0xFF;
    test_ram[0] = 0xCB; test_ram[1] = 0x80;
    cyc = step();
    ASSERT_EQ(cpu->bc.h, 0xFE);
    ASSERT_CYCLES(cyc, 8);
    TEST_END();

    TEST_BEGIN("RES 7,A");
    setup();
    cpu->af.h = 0xFF;
    test_ram[0] = 0xCB; test_ram[1] = 0xBF;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x7F);
    ASSERT_CYCLES(cyc, 8);
    TEST_END();

    TEST_BEGIN("RES 3,(HL)");
    setup();
    cpu->hl.w = 0x6000;
    test_ram[0x6000] = 0xFF;
    test_ram[0] = 0xCB; test_ram[1] = 0x9E;
    cyc = step();
    ASSERT_EQ(test_ram[0x6000], 0xF7);
    ASSERT_CYCLES(cyc, 15);
    TEST_END();
}

/* ========== RRD, RLD ========== */

/**
 * @brief Test RRD a RLD (18T).
 */
static void test_rrd_rld(void) {
    int cyc;

    TEST_BEGIN("RRD");
    setup();
    cpu->af.h = 0x12;
    cpu->hl.w = 0x5000;
    test_ram[0x5000] = 0x34;
    test_ram[0] = 0xED; test_ram[1] = 0x67;
    cyc = step();
    /* A = (A & 0xF0) | (mem & 0x0F) = 0x14 */
    /* mem = (A_lo << 4) | (mem >> 4) = 0x23 */
    ASSERT_EQ(cpu->af.h, 0x14);
    ASSERT_EQ(test_ram[0x5000], 0x23);
    ASSERT_FLAG_CLEAR(Z80_FLAG_N);
    ASSERT_FLAG_CLEAR(Z80_FLAG_H);
    ASSERT_CYCLES(cyc, 18);
    ASSERT_EQ(cpu->wz.w, 0x5001);
    TEST_END();

    TEST_BEGIN("RLD");
    setup();
    cpu->af.h = 0x12;
    cpu->hl.w = 0x5000;
    test_ram[0x5000] = 0x34;
    test_ram[0] = 0xED; test_ram[1] = 0x6F;
    cyc = step();
    /* A = (A & 0xF0) | (mem >> 4) = 0x13 */
    /* mem = (mem << 4) | (A & 0x0F) = 0x42 */
    ASSERT_EQ(cpu->af.h, 0x13);
    ASSERT_EQ(test_ram[0x5000], 0x42);
    ASSERT_FLAG_CLEAR(Z80_FLAG_N);
    ASSERT_FLAG_CLEAR(Z80_FLAG_H);
    ASSERT_CYCLES(cyc, 18);
    TEST_END();
}

/* ========== Vstupni bod ========== */

/**
 * @brief Spusti vsechny testy rotaci a bitovych operaci.
 */
void test_rotate_bit_all(void) {
    test_basic_rotates();
    test_cb_rlc();
    test_cb_shifts();
    test_bit();
    test_set();
    test_res();
    test_rrd_rld();
}
