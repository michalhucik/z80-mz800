/**
 * @file test_alu.c
 * @brief Testy aritmetickych a logickych instrukci Z80.
 *
 * Pokryva: ADD, ADC, SUB, SBC (8bit i 16bit), AND, OR, XOR, CP,
 * INC, DEC (8bit i 16bit), NEG, CPL, SCF, CCF, DAA.
 */

#include "test_framework.h"

/* ========== 8bit aritmetika ========== */

/**
 * @brief Test ADD A, r - 8bit scitani.
 */
static void test_add_a(void) {
    int cyc;

    TEST_BEGIN("ADD A,B (zakladni)");
    setup();
    cpu->af.h = 0x10; cpu->bc.h = 0x20;
    test_ram[0] = 0x80;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x30);
    ASSERT_FLAG_CLEAR(Z80_FLAG_C);
    ASSERT_FLAG_CLEAR(Z80_FLAG_N);
    ASSERT_FLAG_CLEAR(Z80_FLAG_Z);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();

    TEST_BEGIN("ADD A,B (carry)");
    setup();
    cpu->af.h = 0xFF; cpu->bc.h = 0x01;
    test_ram[0] = 0x80;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x00);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    ASSERT_FLAG_CLEAR(Z80_FLAG_N);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();

    TEST_BEGIN("ADD A,B (half carry)");
    setup();
    cpu->af.h = 0x0F; cpu->bc.h = 0x01;
    test_ram[0] = 0x80;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x10);
    ASSERT_FLAG_SET(Z80_FLAG_H);
    ASSERT_FLAG_CLEAR(Z80_FLAG_C);
    TEST_END();

    TEST_BEGIN("ADD A,B (overflow)");
    setup();
    cpu->af.h = 0x7F; cpu->bc.h = 0x01;
    test_ram[0] = 0x80;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x80);
    ASSERT_FLAG_SET(Z80_FLAG_PV);
    ASSERT_FLAG_SET(Z80_FLAG_S);
    TEST_END();

    TEST_BEGIN("ADD A,(HL)");
    setup();
    cpu->af.h = 0x10; cpu->hl.w = 0x5000;
    test_ram[0x5000] = 0x05;
    test_ram[0] = 0x86;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x15);
    ASSERT_CYCLES(cyc, 7);
    TEST_END();

    TEST_BEGIN("ADD A,n");
    setup();
    cpu->af.h = 0x10;
    test_ram[0] = 0xC6; test_ram[1] = 0x2A;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x3A);
    ASSERT_CYCLES(cyc, 7);
    TEST_END();
}

/**
 * @brief Test ADC A, r - 8bit scitani s carry.
 */
static void test_adc_a(void) {
    TEST_BEGIN("ADC A,B (carry set)");
    setup();
    cpu->af.h = 0x10; cpu->bc.h = 0x20;
    cpu->af.l = Z80_FLAG_C;
    test_ram[0] = 0x88;
    int cyc = step();
    ASSERT_EQ(cpu->af.h, 0x31);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();

    TEST_BEGIN("ADC A,B (carry clear)");
    setup();
    cpu->af.h = 0x10; cpu->bc.h = 0x20;
    cpu->af.l = 0;
    test_ram[0] = 0x88;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x30);
    TEST_END();

    TEST_BEGIN("ADC A,n");
    setup();
    cpu->af.h = 0xFF;
    cpu->af.l = Z80_FLAG_C;
    test_ram[0] = 0xCE; test_ram[1] = 0x00;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x00);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    ASSERT_CYCLES(cyc, 7);
    TEST_END();
}

/**
 * @brief Test SUB r - 8bit odcitani.
 */
static void test_sub(void) {
    TEST_BEGIN("SUB B (zakladni)");
    setup();
    cpu->af.h = 0x30; cpu->bc.h = 0x10;
    test_ram[0] = 0x90;
    int cyc = step();
    ASSERT_EQ(cpu->af.h, 0x20);
    ASSERT_FLAG_SET(Z80_FLAG_N);
    ASSERT_FLAG_CLEAR(Z80_FLAG_C);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();

    TEST_BEGIN("SUB B (borrow)");
    setup();
    cpu->af.h = 0x00; cpu->bc.h = 0x01;
    test_ram[0] = 0x90;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0xFF);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_FLAG_SET(Z80_FLAG_N);
    ASSERT_FLAG_SET(Z80_FLAG_S);
    TEST_END();

    TEST_BEGIN("SUB A (self = 0)");
    setup();
    cpu->af.h = 0x42;
    test_ram[0] = 0x97;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x00);
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    ASSERT_FLAG_SET(Z80_FLAG_N);
    ASSERT_FLAG_CLEAR(Z80_FLAG_C);
    TEST_END();
}

/**
 * @brief Test SBC A, r - 8bit odcitani s borrow.
 */
static void test_sbc_a(void) {
    TEST_BEGIN("SBC A,B (carry set)");
    setup();
    cpu->af.h = 0x30; cpu->bc.h = 0x10;
    cpu->af.l = Z80_FLAG_C;
    test_ram[0] = 0x98;
    int cyc = step();
    ASSERT_EQ(cpu->af.h, 0x1F);
    ASSERT_FLAG_SET(Z80_FLAG_N);
    ASSERT_FLAG_SET(Z80_FLAG_H);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();
}

/**
 * @brief Test AND, OR, XOR.
 */
static void test_logic(void) {
    int cyc;

    TEST_BEGIN("AND B");
    setup();
    cpu->af.h = 0xFF; cpu->bc.h = 0x0F;
    test_ram[0] = 0xA0;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x0F);
    ASSERT_FLAG_SET(Z80_FLAG_H);
    ASSERT_FLAG_CLEAR(Z80_FLAG_N);
    ASSERT_FLAG_CLEAR(Z80_FLAG_C);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();

    TEST_BEGIN("AND n (zero)");
    setup();
    cpu->af.h = 0xF0;
    test_ram[0] = 0xE6; test_ram[1] = 0x0F;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x00);
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    ASSERT_FLAG_SET(Z80_FLAG_PV);  /* parita 0 = sudy pocet bitu */
    ASSERT_CYCLES(cyc, 7);
    TEST_END();

    TEST_BEGIN("OR B");
    setup();
    cpu->af.h = 0xF0; cpu->bc.h = 0x0F;
    test_ram[0] = 0xB0;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0xFF);
    ASSERT_FLAG_CLEAR(Z80_FLAG_H);
    ASSERT_FLAG_CLEAR(Z80_FLAG_C);
    ASSERT_FLAG_SET(Z80_FLAG_PV);  /* 0xFF = 8 bitu = sudy */
    ASSERT_FLAG_SET(Z80_FLAG_S);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();

    TEST_BEGIN("XOR A (self = 0)");
    setup();
    cpu->af.h = 0x42;
    test_ram[0] = 0xAF;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x00);
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    ASSERT_FLAG_SET(Z80_FLAG_PV);
    ASSERT_FLAG_CLEAR(Z80_FLAG_C);
    ASSERT_FLAG_CLEAR(Z80_FLAG_H);
    ASSERT_FLAG_CLEAR(Z80_FLAG_N);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();

    TEST_BEGIN("XOR B");
    setup();
    cpu->af.h = 0xFF; cpu->bc.h = 0x0F;
    test_ram[0] = 0xA8;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0xF0);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();
}

/**
 * @brief Test CP r - porovnani.
 */
static void test_cp(void) {
    TEST_BEGIN("CP B (equal)");
    setup();
    cpu->af.h = 0x42; cpu->bc.h = 0x42;
    test_ram[0] = 0xB8;
    int cyc = step();
    ASSERT_EQ(cpu->af.h, 0x42);  /* A se nemeni */
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    ASSERT_FLAG_SET(Z80_FLAG_N);
    ASSERT_FLAG_CLEAR(Z80_FLAG_C);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();

    TEST_BEGIN("CP B (A < B)");
    setup();
    cpu->af.h = 0x10; cpu->bc.h = 0x20;
    test_ram[0] = 0xB8;
    cyc = step();
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_FLAG_SET(Z80_FLAG_N);
    ASSERT_FLAG_CLEAR(Z80_FLAG_Z);
    TEST_END();

    TEST_BEGIN("CP n (F3/F5 z operandu)");
    setup();
    cpu->af.h = 0x00;
    test_ram[0] = 0xFE; test_ram[1] = 0x28;  /* CP 0x28 */
    cyc = step();
    /* F3 a F5 maji byt z operandu (0x28), ne z vysledku */
    ASSERT_EQ(cpu->af.l & Z80_FLAG_3, 0x28 & Z80_FLAG_3);
    ASSERT_EQ(cpu->af.l & Z80_FLAG_5, 0x28 & Z80_FLAG_5);
    ASSERT_CYCLES(cyc, 7);
    TEST_END();
}

/* ========== INC/DEC 8bit ========== */

/**
 * @brief Test INC r a DEC r - 8bit inkrementace/dekrementace.
 */
static void test_inc_dec_8(void) {
    int cyc;

    TEST_BEGIN("INC B (zakladni)");
    setup();
    cpu->bc.h = 0x41;
    test_ram[0] = 0x04;
    cyc = step();
    ASSERT_EQ(cpu->bc.h, 0x42);
    ASSERT_FLAG_CLEAR(Z80_FLAG_N);
    ASSERT_FLAG_CLEAR(Z80_FLAG_Z);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();

    TEST_BEGIN("INC B (half carry)");
    setup();
    cpu->bc.h = 0x0F;
    test_ram[0] = 0x04;
    cyc = step();
    ASSERT_EQ(cpu->bc.h, 0x10);
    ASSERT_FLAG_SET(Z80_FLAG_H);
    TEST_END();

    TEST_BEGIN("INC B (overflow 0x7F->0x80)");
    setup();
    cpu->bc.h = 0x7F;
    test_ram[0] = 0x04;
    cyc = step();
    ASSERT_EQ(cpu->bc.h, 0x80);
    ASSERT_FLAG_SET(Z80_FLAG_PV);
    ASSERT_FLAG_SET(Z80_FLAG_S);
    TEST_END();

    TEST_BEGIN("INC B (wrap 0xFF->0x00)");
    setup();
    cpu->bc.h = 0xFF;
    cpu->af.l = Z80_FLAG_C;  /* carry se zachova */
    test_ram[0] = 0x04;
    cyc = step();
    ASSERT_EQ(cpu->bc.h, 0x00);
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    ASSERT_FLAG_SET(Z80_FLAG_C);  /* carry nezmeneno */
    ASSERT_FLAG_SET(Z80_FLAG_H);
    TEST_END();

    TEST_BEGIN("INC (HL)");
    setup();
    cpu->hl.w = 0x5000;
    test_ram[0x5000] = 0x41;
    test_ram[0] = 0x34;
    cyc = step();
    ASSERT_EQ(test_ram[0x5000], 0x42);
    ASSERT_CYCLES(cyc, 11);
    TEST_END();

    TEST_BEGIN("DEC B (zakladni)");
    setup();
    cpu->bc.h = 0x42;
    test_ram[0] = 0x05;
    cyc = step();
    ASSERT_EQ(cpu->bc.h, 0x41);
    ASSERT_FLAG_SET(Z80_FLAG_N);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();

    TEST_BEGIN("DEC B (half borrow)");
    setup();
    cpu->bc.h = 0x10;
    test_ram[0] = 0x05;
    cyc = step();
    ASSERT_EQ(cpu->bc.h, 0x0F);
    ASSERT_FLAG_SET(Z80_FLAG_H);
    TEST_END();

    TEST_BEGIN("DEC B (underflow 0x80->0x7F)");
    setup();
    cpu->bc.h = 0x80;
    test_ram[0] = 0x05;
    cyc = step();
    ASSERT_EQ(cpu->bc.h, 0x7F);
    ASSERT_FLAG_SET(Z80_FLAG_PV);
    TEST_END();

    TEST_BEGIN("DEC B (zero)");
    setup();
    cpu->bc.h = 0x01;
    test_ram[0] = 0x05;
    cyc = step();
    ASSERT_EQ(cpu->bc.h, 0x00);
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    TEST_END();

    TEST_BEGIN("DEC (HL)");
    setup();
    cpu->hl.w = 0x5000;
    test_ram[0x5000] = 0x01;
    test_ram[0] = 0x35;
    cyc = step();
    ASSERT_EQ(test_ram[0x5000], 0x00);
    ASSERT_CYCLES(cyc, 11);
    TEST_END();
}

/* ========== INC/DEC 16bit ========== */

/**
 * @brief Test INC rr a DEC rr - 16bit (6T, neovlivnuji flagy).
 */
static void test_inc_dec_16(void) {
    int cyc;

    TEST_BEGIN("INC BC");
    setup();
    cpu->bc.w = 0x00FF;
    test_ram[0] = 0x03;
    cyc = step();
    ASSERT_EQ(cpu->bc.w, 0x0100);
    ASSERT_CYCLES(cyc, 6);
    TEST_END();

    TEST_BEGIN("INC HL (wrap)");
    setup();
    cpu->hl.w = 0xFFFF;
    test_ram[0] = 0x23;
    cyc = step();
    ASSERT_EQ(cpu->hl.w, 0x0000);
    ASSERT_CYCLES(cyc, 6);
    TEST_END();

    TEST_BEGIN("DEC BC");
    setup();
    cpu->bc.w = 0x0100;
    test_ram[0] = 0x0B;
    cyc = step();
    ASSERT_EQ(cpu->bc.w, 0x00FF);
    ASSERT_CYCLES(cyc, 6);
    TEST_END();

    TEST_BEGIN("DEC SP");
    setup();
    cpu->sp = 0x0000;
    test_ram[0] = 0x3B;
    cyc = step();
    ASSERT_EQ(cpu->sp, 0xFFFF);
    ASSERT_CYCLES(cyc, 6);
    TEST_END();

    TEST_BEGIN("INC SP");
    setup();
    cpu->sp = 0xFFFE;
    test_ram[0] = 0x33;
    cyc = step();
    ASSERT_EQ(cpu->sp, 0xFFFF);
    ASSERT_CYCLES(cyc, 6);
    TEST_END();
}

/* ========== 16bit aritmetika ========== */

/**
 * @brief Test ADD HL, rr (11T).
 */
static void test_add_hl(void) {
    int cyc;

    TEST_BEGIN("ADD HL,BC");
    setup();
    cpu->hl.w = 0x1000; cpu->bc.w = 0x0234;
    test_ram[0] = 0x09;
    cyc = step();
    ASSERT_EQ(cpu->hl.w, 0x1234);
    ASSERT_FLAG_CLEAR(Z80_FLAG_C);
    ASSERT_FLAG_CLEAR(Z80_FLAG_N);
    ASSERT_CYCLES(cyc, 11);
    /* MEMPTR = HL_pred + 1 */
    ASSERT_EQ(cpu->wz.w, 0x1001);
    TEST_END();

    TEST_BEGIN("ADD HL,HL (carry)");
    setup();
    cpu->hl.w = 0x8000;
    test_ram[0] = 0x29;
    cyc = step();
    ASSERT_EQ(cpu->hl.w, 0x0000);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_CYCLES(cyc, 11);
    TEST_END();

    TEST_BEGIN("ADD HL,DE (half carry)");
    setup();
    cpu->hl.w = 0x0FFF; cpu->de.w = 0x0001;
    test_ram[0] = 0x19;
    cyc = step();
    ASSERT_EQ(cpu->hl.w, 0x1000);
    ASSERT_FLAG_SET(Z80_FLAG_H);
    TEST_END();
}

/**
 * @brief Test ADC HL, rr (15T).
 */
static void test_adc_hl(void) {
    TEST_BEGIN("ADC HL,BC (s carry)");
    setup();
    cpu->hl.w = 0x1000; cpu->bc.w = 0x0001;
    cpu->af.l = Z80_FLAG_C;
    test_ram[0] = 0xED; test_ram[1] = 0x4A;
    int cyc = step();
    ASSERT_EQ(cpu->hl.w, 0x1002);
    ASSERT_FLAG_CLEAR(Z80_FLAG_N);
    ASSERT_CYCLES(cyc, 15);
    TEST_END();

    TEST_BEGIN("ADC HL,HL (zero result)");
    setup();
    cpu->hl.w = 0x8000;
    cpu->af.l = 0;
    test_ram[0] = 0xED; test_ram[1] = 0x6A;
    int cyc2 = step();
    ASSERT_EQ(cpu->hl.w, 0x0000);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    ASSERT_CYCLES(cyc2, 15);
    TEST_END();
}

/**
 * @brief Test SBC HL, rr (15T).
 */
static void test_sbc_hl(void) {
    TEST_BEGIN("SBC HL,BC (bez borrow)");
    setup();
    cpu->hl.w = 0x1234; cpu->bc.w = 0x0234;
    cpu->af.l = 0;
    test_ram[0] = 0xED; test_ram[1] = 0x42;
    int cyc = step();
    ASSERT_EQ(cpu->hl.w, 0x1000);
    ASSERT_FLAG_SET(Z80_FLAG_N);
    ASSERT_FLAG_CLEAR(Z80_FLAG_C);
    ASSERT_CYCLES(cyc, 15);
    TEST_END();

    TEST_BEGIN("SBC HL,HL (self = 0 nebo -1)");
    setup();
    cpu->hl.w = 0x1234;
    cpu->af.l = 0;
    test_ram[0] = 0xED; test_ram[1] = 0x62;
    cyc = step();
    ASSERT_EQ(cpu->hl.w, 0x0000);
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    ASSERT_FLAG_SET(Z80_FLAG_N);
    TEST_END();

    TEST_BEGIN("SBC HL,HL (s borrow)");
    setup();
    cpu->hl.w = 0x1234;
    cpu->af.l = Z80_FLAG_C;
    test_ram[0] = 0xED; test_ram[1] = 0x62;
    cyc = step();
    ASSERT_EQ(cpu->hl.w, 0xFFFF);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_FLAG_SET(Z80_FLAG_S);
    TEST_END();
}

/* ========== NEG ========== */

/**
 * @brief Test NEG (ED 44) - negace A (8T).
 */
static void test_neg(void) {
    TEST_BEGIN("NEG (A=1 -> 0xFF)");
    setup();
    cpu->af.h = 0x01;
    test_ram[0] = 0xED; test_ram[1] = 0x44;
    int cyc = step();
    ASSERT_EQ(cpu->af.h, 0xFF);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_FLAG_SET(Z80_FLAG_N);
    ASSERT_FLAG_SET(Z80_FLAG_S);
    ASSERT_CYCLES(cyc, 8);
    TEST_END();

    TEST_BEGIN("NEG (A=0 -> 0)");
    setup();
    cpu->af.h = 0x00;
    test_ram[0] = 0xED; test_ram[1] = 0x44;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x00);
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    ASSERT_FLAG_CLEAR(Z80_FLAG_C);
    TEST_END();

    TEST_BEGIN("NEG (A=0x80 -> overflow)");
    setup();
    cpu->af.h = 0x80;
    test_ram[0] = 0xED; test_ram[1] = 0x44;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x80);
    ASSERT_FLAG_SET(Z80_FLAG_PV);
    TEST_END();
}

/* ========== CPL, SCF, CCF ========== */

/**
 * @brief Test CPL, SCF, CCF.
 */
static void test_cpl_scf_ccf(void) {
    int cyc;

    TEST_BEGIN("CPL");
    setup();
    cpu->af.h = 0x55;
    test_ram[0] = 0x2F;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0xAA);
    ASSERT_FLAG_SET(Z80_FLAG_H);
    ASSERT_FLAG_SET(Z80_FLAG_N);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();

    TEST_BEGIN("SCF");
    setup();
    cpu->af.l = 0;
    test_ram[0] = 0x37;
    cyc = step();
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_FLAG_CLEAR(Z80_FLAG_N);
    ASSERT_FLAG_CLEAR(Z80_FLAG_H);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();

    TEST_BEGIN("CCF (C=1 -> C=0, H=1)");
    setup();
    cpu->af.l = Z80_FLAG_C;
    test_ram[0] = 0x3F;
    cyc = step();
    ASSERT_FLAG_CLEAR(Z80_FLAG_C);
    ASSERT_FLAG_SET(Z80_FLAG_H);
    ASSERT_FLAG_CLEAR(Z80_FLAG_N);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();

    TEST_BEGIN("CCF (C=0 -> C=1, H=0)");
    setup();
    cpu->af.l = 0;
    test_ram[0] = 0x3F;
    cyc = step();
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_FLAG_CLEAR(Z80_FLAG_H);
    TEST_END();
}

/* ========== Vstupni bod ========== */

/**
 * @brief Spusti vsechny ALU testy.
 */
void test_alu_all(void) {
    test_add_a();
    test_adc_a();
    test_sub();
    test_sbc_a();
    test_logic();
    test_cp();
    test_inc_dec_8();
    test_inc_dec_16();
    test_add_hl();
    test_adc_hl();
    test_sbc_hl();
    test_neg();
    test_cpl_scf_ccf();
}
