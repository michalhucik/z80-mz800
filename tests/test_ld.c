/**
 * @file test_ld.c
 * @brief Testy load instrukci Z80 (LD 8bit, 16bit, indirect).
 *
 * Pokryva: LD r,r', LD r,n, LD r,(HL), LD (HL),r, LD rr,nn,
 * LD (nn),rr, LD rr,(nn), LD A,(BC), LD A,(DE), LD (BC),A,
 * LD (DE),A, LD A,(nn), LD (nn),A, LD SP,HL, EX instrukce.
 */

#include "test_framework.h"

/* ========== 8bit load ========== */

/**
 * @brief Test LD r, r' - registr do registru (4T).
 */
static void test_ld_r_r(void) {
    TEST_BEGIN("LD B,C");
    setup();
    cpu.bc.l = 0x42;  /* C = 0x42 */
    test_ram[0] = 0x41;  /* LD B, C */
    int cyc = step();
    ASSERT_EQ(cpu.bc.h, 0x42);
    ASSERT_CYCLES(cyc, 4);
    ASSERT_EQ(cpu.pc, 1);
    TEST_END();

    TEST_BEGIN("LD A,B");
    setup();
    cpu.bc.h = 0xAB;
    test_ram[0] = 0x78;  /* LD A, B */
    cyc = step();
    ASSERT_EQ(cpu.af.h, 0xAB);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();

    TEST_BEGIN("LD D,E");
    setup();
    cpu.de.l = 0x37;
    test_ram[0] = 0x53;  /* LD D, E */
    cyc = step();
    ASSERT_EQ(cpu.de.h, 0x37);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();

    TEST_BEGIN("LD H,L");
    setup();
    cpu.hl.l = 0xCD;
    test_ram[0] = 0x65;  /* LD H, L */
    cyc = step();
    ASSERT_EQ(cpu.hl.h, 0xCD);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();

    TEST_BEGIN("LD A,A (noop)");
    setup();
    cpu.af.h = 0x55;
    test_ram[0] = 0x7F;  /* LD A, A */
    cyc = step();
    ASSERT_EQ(cpu.af.h, 0x55);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();
}

/**
 * @brief Test LD r, n - immediate 8bit (7T).
 */
static void test_ld_r_n(void) {
    TEST_BEGIN("LD B,0x42");
    setup();
    test_ram[0] = 0x06; test_ram[1] = 0x42;
    int cyc = step();
    ASSERT_EQ(cpu.bc.h, 0x42);
    ASSERT_CYCLES(cyc, 7);
    ASSERT_EQ(cpu.pc, 2);
    TEST_END();

    TEST_BEGIN("LD C,0xFF");
    setup();
    test_ram[0] = 0x0E; test_ram[1] = 0xFF;
    cyc = step();
    ASSERT_EQ(cpu.bc.l, 0xFF);
    ASSERT_CYCLES(cyc, 7);
    TEST_END();

    TEST_BEGIN("LD A,0x00");
    setup();
    test_ram[0] = 0x3E; test_ram[1] = 0x00;
    cyc = step();
    ASSERT_EQ(cpu.af.h, 0x00);
    ASSERT_CYCLES(cyc, 7);
    TEST_END();

    TEST_BEGIN("LD D,0x80");
    setup();
    test_ram[0] = 0x16; test_ram[1] = 0x80;
    cyc = step();
    ASSERT_EQ(cpu.de.h, 0x80);
    ASSERT_CYCLES(cyc, 7);
    TEST_END();

    TEST_BEGIN("LD E,0x01");
    setup();
    test_ram[0] = 0x1E; test_ram[1] = 0x01;
    cyc = step();
    ASSERT_EQ(cpu.de.l, 0x01);
    ASSERT_CYCLES(cyc, 7);
    TEST_END();

    TEST_BEGIN("LD H,0xAB");
    setup();
    test_ram[0] = 0x26; test_ram[1] = 0xAB;
    cyc = step();
    ASSERT_EQ(cpu.hl.h, 0xAB);
    ASSERT_CYCLES(cyc, 7);
    TEST_END();

    TEST_BEGIN("LD L,0xCD");
    setup();
    test_ram[0] = 0x2E; test_ram[1] = 0xCD;
    cyc = step();
    ASSERT_EQ(cpu.hl.l, 0xCD);
    ASSERT_CYCLES(cyc, 7);
    TEST_END();
}

/**
 * @brief Test LD r, (HL) - cteni z pameti (7T).
 */
static void test_ld_r_hl(void) {
    TEST_BEGIN("LD A,(HL)");
    setup();
    cpu.hl.w = 0x8000;
    test_ram[0x8000] = 0xBE;
    test_ram[0] = 0x7E;  /* LD A, (HL) */
    int cyc = step();
    ASSERT_EQ(cpu.af.h, 0xBE);
    ASSERT_CYCLES(cyc, 7);
    TEST_END();

    TEST_BEGIN("LD B,(HL)");
    setup();
    cpu.hl.w = 0x4000;
    test_ram[0x4000] = 0x99;
    test_ram[0] = 0x46;
    int cyc2 = step();
    ASSERT_EQ(cpu.bc.h, 0x99);
    ASSERT_CYCLES(cyc2, 7);
    TEST_END();
}

/**
 * @brief Test LD (HL), r - zapis do pameti (7T).
 */
static void test_ld_hl_r(void) {
    TEST_BEGIN("LD (HL),A");
    setup();
    cpu.af.h = 0x42;
    cpu.hl.w = 0x9000;
    test_ram[0] = 0x77;  /* LD (HL), A */
    int cyc = step();
    ASSERT_EQ(test_ram[0x9000], 0x42);
    ASSERT_CYCLES(cyc, 7);
    TEST_END();

    TEST_BEGIN("LD (HL),B");
    setup();
    cpu.bc.h = 0xAA;
    cpu.hl.w = 0x5000;
    test_ram[0] = 0x70;
    cyc = step();
    ASSERT_EQ(test_ram[0x5000], 0xAA);
    ASSERT_CYCLES(cyc, 7);
    TEST_END();
}

/**
 * @brief Test LD (HL), n - immediate do pameti (10T).
 */
static void test_ld_hl_n(void) {
    TEST_BEGIN("LD (HL),0x55");
    setup();
    cpu.hl.w = 0x7000;
    test_ram[0] = 0x36; test_ram[1] = 0x55;
    int cyc = step();
    ASSERT_EQ(test_ram[0x7000], 0x55);
    ASSERT_CYCLES(cyc, 10);
    TEST_END();
}

/* ========== 16bit load ========== */

/**
 * @brief Test LD rr, nn - 16bit immediate (10T).
 */
static void test_ld_rr_nn(void) {
    TEST_BEGIN("LD BC,0x1234");
    setup();
    test_ram[0] = 0x01; test_ram[1] = 0x34; test_ram[2] = 0x12;
    int cyc = step();
    ASSERT_EQ(cpu.bc.w, 0x1234);
    ASSERT_CYCLES(cyc, 10);
    TEST_END();

    TEST_BEGIN("LD DE,0xABCD");
    setup();
    test_ram[0] = 0x11; test_ram[1] = 0xCD; test_ram[2] = 0xAB;
    cyc = step();
    ASSERT_EQ(cpu.de.w, 0xABCD);
    ASSERT_CYCLES(cyc, 10);
    TEST_END();

    TEST_BEGIN("LD HL,0x8000");
    setup();
    test_ram[0] = 0x21; test_ram[1] = 0x00; test_ram[2] = 0x80;
    cyc = step();
    ASSERT_EQ(cpu.hl.w, 0x8000);
    ASSERT_CYCLES(cyc, 10);
    TEST_END();

    TEST_BEGIN("LD SP,0xFFF0");
    setup();
    test_ram[0] = 0x31; test_ram[1] = 0xF0; test_ram[2] = 0xFF;
    cyc = step();
    ASSERT_EQ(cpu.sp, 0xFFF0);
    ASSERT_CYCLES(cyc, 10);
    TEST_END();
}

/**
 * @brief Test LD (nn), HL a LD HL, (nn) (16T).
 */
static void test_ld_nn_hl(void) {
    TEST_BEGIN("LD (0x8000),HL");
    setup();
    cpu.hl.w = 0xBEEF;
    test_ram[0] = 0x22; test_ram[1] = 0x00; test_ram[2] = 0x80;
    int cyc = step();
    ASSERT_EQ(test_ram[0x8000], 0xEF);
    ASSERT_EQ(test_ram[0x8001], 0xBE);
    ASSERT_CYCLES(cyc, 16);
    ASSERT_EQ(cpu.wz.w, 0x8001);
    TEST_END();

    TEST_BEGIN("LD HL,(0x9000)");
    setup();
    test_ram[0x9000] = 0x34;
    test_ram[0x9001] = 0x12;
    test_ram[0] = 0x2A; test_ram[1] = 0x00; test_ram[2] = 0x90;
    cyc = step();
    ASSERT_EQ(cpu.hl.w, 0x1234);
    ASSERT_CYCLES(cyc, 16);
    ASSERT_EQ(cpu.wz.w, 0x9001);
    TEST_END();
}

/**
 * @brief Test LD (nn), A a LD A, (nn) (13T).
 */
static void test_ld_nn_a(void) {
    TEST_BEGIN("LD (0x5000),A");
    setup();
    cpu.af.h = 0x42;
    test_ram[0] = 0x32; test_ram[1] = 0x00; test_ram[2] = 0x50;
    int cyc = step();
    ASSERT_EQ(test_ram[0x5000], 0x42);
    ASSERT_CYCLES(cyc, 13);
    /* MEMPTR: lo = (addr+1)&0xFF, hi = A */
    ASSERT_EQ(cpu.wz.l, 0x01);
    ASSERT_EQ(cpu.wz.h, 0x42);
    TEST_END();

    TEST_BEGIN("LD A,(0x6000)");
    setup();
    test_ram[0x6000] = 0xAB;
    test_ram[0] = 0x3A; test_ram[1] = 0x00; test_ram[2] = 0x60;
    cyc = step();
    ASSERT_EQ(cpu.af.h, 0xAB);
    ASSERT_CYCLES(cyc, 13);
    ASSERT_EQ(cpu.wz.w, 0x6001);
    TEST_END();
}

/* ========== Indirect load ========== */

/**
 * @brief Test LD A,(BC) a LD A,(DE) (7T).
 */
static void test_ld_a_indirect(void) {
    TEST_BEGIN("LD A,(BC)");
    setup();
    cpu.bc.w = 0x3000;
    test_ram[0x3000] = 0x77;
    test_ram[0] = 0x0A;
    int cyc = step();
    ASSERT_EQ(cpu.af.h, 0x77);
    ASSERT_CYCLES(cyc, 7);
    ASSERT_EQ(cpu.wz.w, 0x3001);
    TEST_END();

    TEST_BEGIN("LD A,(DE)");
    setup();
    cpu.de.w = 0x4000;
    test_ram[0x4000] = 0x88;
    test_ram[0] = 0x1A;
    cyc = step();
    ASSERT_EQ(cpu.af.h, 0x88);
    ASSERT_CYCLES(cyc, 7);
    ASSERT_EQ(cpu.wz.w, 0x4001);
    TEST_END();
}

/**
 * @brief Test LD (BC),A a LD (DE),A (7T).
 */
static void test_ld_indirect_a(void) {
    TEST_BEGIN("LD (BC),A");
    setup();
    cpu.af.h = 0xEE;
    cpu.bc.w = 0x2000;
    test_ram[0] = 0x02;
    int cyc = step();
    ASSERT_EQ(test_ram[0x2000], 0xEE);
    ASSERT_CYCLES(cyc, 7);
    /* MEMPTR: lo = (addr+1)&0xFF, hi = A */
    ASSERT_EQ(cpu.wz.l, 0x01);
    ASSERT_EQ(cpu.wz.h, 0xEE);
    TEST_END();

    TEST_BEGIN("LD (DE),A");
    setup();
    cpu.af.h = 0xDD;
    cpu.de.w = 0x3000;
    test_ram[0] = 0x12;
    cyc = step();
    ASSERT_EQ(test_ram[0x3000], 0xDD);
    ASSERT_CYCLES(cyc, 7);
    TEST_END();
}

/**
 * @brief Test LD SP, HL (6T).
 */
static void test_ld_sp_hl(void) {
    TEST_BEGIN("LD SP,HL");
    setup();
    cpu.hl.w = 0xABCD;
    test_ram[0] = 0xF9;
    int cyc = step();
    ASSERT_EQ(cpu.sp, 0xABCD);
    ASSERT_CYCLES(cyc, 6);
    TEST_END();
}

/* ========== ED prefix load ========== */

/**
 * @brief Test ED LD (nn),rr a LD rr,(nn) (20T).
 */
static void test_ld_ed_indirect(void) {
    TEST_BEGIN("ED LD (0x8000),BC");
    setup();
    cpu.bc.w = 0x1234;
    test_ram[0] = 0xED; test_ram[1] = 0x43;
    test_ram[2] = 0x00; test_ram[3] = 0x80;
    int cyc = step();
    ASSERT_EQ(test_ram[0x8000], 0x34);
    ASSERT_EQ(test_ram[0x8001], 0x12);
    ASSERT_CYCLES(cyc, 20);
    ASSERT_EQ(cpu.wz.w, 0x8001);
    TEST_END();

    TEST_BEGIN("ED LD DE,(0x9000)");
    setup();
    test_ram[0x9000] = 0xCD; test_ram[0x9001] = 0xAB;
    test_ram[0] = 0xED; test_ram[1] = 0x5B;
    test_ram[2] = 0x00; test_ram[3] = 0x90;
    cyc = step();
    ASSERT_EQ(cpu.de.w, 0xABCD);
    ASSERT_CYCLES(cyc, 20);
    TEST_END();

    TEST_BEGIN("ED LD (0x7000),SP");
    setup();
    cpu.sp = 0xFFF0;
    test_ram[0] = 0xED; test_ram[1] = 0x73;
    test_ram[2] = 0x00; test_ram[3] = 0x70;
    cyc = step();
    ASSERT_EQ(test_ram[0x7000], 0xF0);
    ASSERT_EQ(test_ram[0x7001], 0xFF);
    ASSERT_CYCLES(cyc, 20);
    TEST_END();

    TEST_BEGIN("ED LD SP,(0x6000)");
    setup();
    test_ram[0x6000] = 0xFE; test_ram[0x6001] = 0xFF;
    test_ram[0] = 0xED; test_ram[1] = 0x7B;
    test_ram[2] = 0x00; test_ram[3] = 0x60;
    cyc = step();
    ASSERT_EQ(cpu.sp, 0xFFFE);
    ASSERT_CYCLES(cyc, 20);
    TEST_END();
}

/**
 * @brief Test LD I,A a LD R,A a LD A,I a LD A,R (9T).
 */
static void test_ld_ir(void) {
    TEST_BEGIN("LD I,A");
    setup();
    cpu.af.h = 0x42;
    test_ram[0] = 0xED; test_ram[1] = 0x47;
    int cyc = step();
    ASSERT_EQ(cpu.i, 0x42);
    ASSERT_CYCLES(cyc, 9);
    TEST_END();

    TEST_BEGIN("LD R,A");
    setup();
    cpu.af.h = 0x80;
    test_ram[0] = 0xED; test_ram[1] = 0x4F;
    cyc = step();
    ASSERT_EQ(cpu.r, 0x80);
    ASSERT_CYCLES(cyc, 9);
    TEST_END();

    TEST_BEGIN("LD A,I (IFF2=1 -> PF set)");
    setup();
    cpu.i = 0x55;
    cpu.iff2 = 1;
    cpu.af.l = Z80_FLAG_C;  /* zachova carry */
    test_ram[0] = 0xED; test_ram[1] = 0x57;
    cyc = step();
    ASSERT_EQ(cpu.af.h, 0x55);
    ASSERT_FLAG_SET(Z80_FLAG_PV);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_FLAG_CLEAR(Z80_FLAG_Z);
    ASSERT_FLAG_CLEAR(Z80_FLAG_N);
    ASSERT_FLAG_CLEAR(Z80_FLAG_H);
    ASSERT_CYCLES(cyc, 9);
    TEST_END();

    TEST_BEGIN("LD A,I (IFF2=0 -> PF clear)");
    setup();
    cpu.i = 0x00;
    cpu.iff2 = 0;
    test_ram[0] = 0xED; test_ram[1] = 0x57;
    cyc = step();
    ASSERT_EQ(cpu.af.h, 0x00);
    ASSERT_FLAG_CLEAR(Z80_FLAG_PV);
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    ASSERT_CYCLES(cyc, 9);
    TEST_END();

    TEST_BEGIN("LD A,R");
    setup();
    cpu.r = 0x80;
    cpu.iff2 = 1;
    test_ram[0] = 0xED; test_ram[1] = 0x5F;
    cyc = step();
    /* R se inkrementuje pri fetch, takze hodnota bude jina nez 0x80 */
    ASSERT_FLAG_SET(Z80_FLAG_PV);  /* IFF2=1 */
    ASSERT_CYCLES(cyc, 9);
    TEST_END();
}

/* ========== Exchange instrukce ========== */

/**
 * @brief Test EX AF, AF' (4T).
 */
static void test_ex_af(void) {
    TEST_BEGIN("EX AF,AF'");
    setup();
    cpu.af.w = 0x1234;
    cpu.af2.w = 0x5678;
    test_ram[0] = 0x08;
    int cyc = step();
    ASSERT_EQ(cpu.af.w, 0x5678);
    ASSERT_EQ(cpu.af2.w, 0x1234);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();
}

/**
 * @brief Test EXX (4T).
 */
static void test_exx(void) {
    TEST_BEGIN("EXX");
    setup();
    cpu.bc.w = 0x1111; cpu.de.w = 0x2222; cpu.hl.w = 0x3333;
    cpu.bc2.w = 0xAAAA; cpu.de2.w = 0xBBBB; cpu.hl2.w = 0xCCCC;
    test_ram[0] = 0xD9;
    int cyc = step();
    ASSERT_EQ(cpu.bc.w, 0xAAAA);
    ASSERT_EQ(cpu.de.w, 0xBBBB);
    ASSERT_EQ(cpu.hl.w, 0xCCCC);
    ASSERT_EQ(cpu.bc2.w, 0x1111);
    ASSERT_EQ(cpu.de2.w, 0x2222);
    ASSERT_EQ(cpu.hl2.w, 0x3333);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();
}

/**
 * @brief Test EX DE, HL (4T).
 */
static void test_ex_de_hl(void) {
    TEST_BEGIN("EX DE,HL");
    setup();
    cpu.de.w = 0xAAAA;
    cpu.hl.w = 0xBBBB;
    test_ram[0] = 0xEB;
    int cyc = step();
    ASSERT_EQ(cpu.de.w, 0xBBBB);
    ASSERT_EQ(cpu.hl.w, 0xAAAA);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();
}

/**
 * @brief Test EX (SP), HL (19T).
 */
static void test_ex_sp_hl(void) {
    TEST_BEGIN("EX (SP),HL");
    setup();
    cpu.sp = 0xFFF0;
    cpu.hl.w = 0x1234;
    test_ram[0xFFF0] = 0xCD;
    test_ram[0xFFF1] = 0xAB;
    test_ram[0] = 0xE3;
    int cyc = step();
    ASSERT_EQ(cpu.hl.w, 0xABCD);
    ASSERT_EQ(test_ram[0xFFF0], 0x34);
    ASSERT_EQ(test_ram[0xFFF1], 0x12);
    ASSERT_EQ(cpu.wz.w, 0xABCD);
    ASSERT_CYCLES(cyc, 19);
    TEST_END();
}

/* ========== Vstupni bod ========== */

/**
 * @brief Spusti vsechny LD testy.
 */
void test_ld_all(void) {
    test_ld_r_r();
    test_ld_r_n();
    test_ld_r_hl();
    test_ld_hl_r();
    test_ld_hl_n();
    test_ld_rr_nn();
    test_ld_nn_hl();
    test_ld_nn_a();
    test_ld_a_indirect();
    test_ld_indirect_a();
    test_ld_sp_hl();
    test_ld_ed_indirect();
    test_ld_ir();
    test_ex_af();
    test_exx();
    test_ex_de_hl();
    test_ex_sp_hl();
}
