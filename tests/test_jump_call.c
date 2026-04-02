/**
 * @file test_jump_call.c
 * @brief Testy skoku, volani a navratu Z80.
 *
 * Pokryva: JP, JP cc, JR, JR cc, DJNZ, CALL, CALL cc, RET, RET cc,
 * RST, JP (HL), PUSH, POP, HALT, NOP.
 */

#include "test_framework.h"

/* ========== JP ========== */

/**
 * @brief Test JP nn (10T).
 */
static void test_jp(void) {
    TEST_BEGIN("JP 0x1234");
    setup();
    test_ram[0] = 0xC3; test_ram[1] = 0x34; test_ram[2] = 0x12;
    int cyc = step();
    ASSERT_EQ(cpu.pc, 0x1234);
    ASSERT_EQ(cpu.wz.w, 0x1234);
    ASSERT_CYCLES(cyc, 10);
    TEST_END();
}

/**
 * @brief Test JP cc, nn (10T vzdy - podmineny i nepodmineny).
 */
static void test_jp_cc(void) {
    int cyc;

    TEST_BEGIN("JP NZ (taken)");
    setup();
    cpu.af.l = 0;  /* Z=0 */
    test_ram[0] = 0xC2; test_ram[1] = 0x00; test_ram[2] = 0x50;
    cyc = step();
    ASSERT_EQ(cpu.pc, 0x5000);
    ASSERT_CYCLES(cyc, 10);
    TEST_END();

    TEST_BEGIN("JP NZ (not taken)");
    setup();
    cpu.af.l = Z80_FLAG_Z;
    test_ram[0] = 0xC2; test_ram[1] = 0x00; test_ram[2] = 0x50;
    cyc = step();
    ASSERT_EQ(cpu.pc, 3);  /* preskocit 3 bajty */
    ASSERT_CYCLES(cyc, 10);  /* JP cc vzdy 10T */
    TEST_END();

    TEST_BEGIN("JP Z (taken)");
    setup();
    cpu.af.l = Z80_FLAG_Z;
    test_ram[0] = 0xCA; test_ram[1] = 0x00; test_ram[2] = 0x60;
    cyc = step();
    ASSERT_EQ(cpu.pc, 0x6000);
    ASSERT_CYCLES(cyc, 10);
    TEST_END();

    TEST_BEGIN("JP C (taken)");
    setup();
    cpu.af.l = Z80_FLAG_C;
    test_ram[0] = 0xDA; test_ram[1] = 0x00; test_ram[2] = 0x70;
    cyc = step();
    ASSERT_EQ(cpu.pc, 0x7000);
    TEST_END();

    TEST_BEGIN("JP NC (not taken)");
    setup();
    cpu.af.l = Z80_FLAG_C;
    test_ram[0] = 0xD2; test_ram[1] = 0x00; test_ram[2] = 0x70;
    cyc = step();
    ASSERT_EQ(cpu.pc, 3);
    TEST_END();

    TEST_BEGIN("JP PE (taken, PV=1)");
    setup();
    cpu.af.l = Z80_FLAG_PV;
    test_ram[0] = 0xEA; test_ram[1] = 0x00; test_ram[2] = 0x80;
    cyc = step();
    ASSERT_EQ(cpu.pc, 0x8000);
    TEST_END();

    TEST_BEGIN("JP P (taken, S=0)");
    setup();
    cpu.af.l = 0;
    test_ram[0] = 0xF2; test_ram[1] = 0x00; test_ram[2] = 0x90;
    cyc = step();
    ASSERT_EQ(cpu.pc, 0x9000);
    TEST_END();

    TEST_BEGIN("JP M (taken, S=1)");
    setup();
    cpu.af.l = Z80_FLAG_S;
    test_ram[0] = 0xFA; test_ram[1] = 0x00; test_ram[2] = 0xA0;
    cyc = step();
    ASSERT_EQ(cpu.pc, 0xA000);
    TEST_END();
}

/**
 * @brief Test JP (HL) (4T).
 */
static void test_jp_hl(void) {
    TEST_BEGIN("JP (HL)");
    setup();
    cpu.hl.w = 0xABCD;
    test_ram[0] = 0xE9;
    int cyc = step();
    ASSERT_EQ(cpu.pc, 0xABCD);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();
}

/* ========== JR ========== */

/**
 * @brief Test JR d (12T), JR cc,d (12T taken, 7T not taken).
 */
static void test_jr(void) {
    int cyc;

    TEST_BEGIN("JR +5");
    setup();
    test_ram[0] = 0x18; test_ram[1] = 0x05;  /* PC = 2 + 5 = 7 */
    cyc = step();
    ASSERT_EQ(cpu.pc, 7);
    ASSERT_EQ(cpu.wz.w, 7);
    ASSERT_CYCLES(cyc, 12);
    TEST_END();

    TEST_BEGIN("JR -2 (nekonecna smycka)");
    setup();
    test_ram[0] = 0x18; test_ram[1] = 0xFE;  /* PC = 2 + (-2) = 0 */
    cyc = step();
    ASSERT_EQ(cpu.pc, 0);
    ASSERT_CYCLES(cyc, 12);
    TEST_END();

    TEST_BEGIN("JR NZ (taken)");
    setup();
    cpu.af.l = 0;
    test_ram[0] = 0x20; test_ram[1] = 0x10;
    cyc = step();
    ASSERT_EQ(cpu.pc, 0x12);
    ASSERT_CYCLES(cyc, 12);
    TEST_END();

    TEST_BEGIN("JR NZ (not taken)");
    setup();
    cpu.af.l = Z80_FLAG_Z;
    test_ram[0] = 0x20; test_ram[1] = 0x10;
    cyc = step();
    ASSERT_EQ(cpu.pc, 2);
    ASSERT_CYCLES(cyc, 7);
    TEST_END();

    TEST_BEGIN("JR Z (taken)");
    setup();
    cpu.af.l = Z80_FLAG_Z;
    test_ram[0] = 0x28; test_ram[1] = 0x05;
    cyc = step();
    ASSERT_EQ(cpu.pc, 7);
    ASSERT_CYCLES(cyc, 12);
    TEST_END();

    TEST_BEGIN("JR Z (not taken)");
    setup();
    cpu.af.l = 0;
    test_ram[0] = 0x28; test_ram[1] = 0x05;
    cyc = step();
    ASSERT_EQ(cpu.pc, 2);
    ASSERT_CYCLES(cyc, 7);
    TEST_END();

    TEST_BEGIN("JR C (taken)");
    setup();
    cpu.af.l = Z80_FLAG_C;
    test_ram[0] = 0x38; test_ram[1] = 0x03;
    cyc = step();
    ASSERT_EQ(cpu.pc, 5);
    ASSERT_CYCLES(cyc, 12);
    TEST_END();

    TEST_BEGIN("JR NC (not taken)");
    setup();
    cpu.af.l = Z80_FLAG_C;
    test_ram[0] = 0x30; test_ram[1] = 0x03;
    cyc = step();
    ASSERT_EQ(cpu.pc, 2);
    ASSERT_CYCLES(cyc, 7);
    TEST_END();
}

/* ========== DJNZ ========== */

/**
 * @brief Test DJNZ d (13T taken, 8T not taken).
 */
static void test_djnz(void) {
    int cyc;

    TEST_BEGIN("DJNZ (taken, B=2->1)");
    setup();
    cpu.bc.h = 2;
    test_ram[0] = 0x10; test_ram[1] = 0xFE;  /* skok na 0 */
    cyc = step();
    ASSERT_EQ(cpu.bc.h, 1);
    ASSERT_EQ(cpu.pc, 0);
    ASSERT_CYCLES(cyc, 13);
    TEST_END();

    TEST_BEGIN("DJNZ (not taken, B=1->0)");
    setup();
    cpu.bc.h = 1;
    test_ram[0] = 0x10; test_ram[1] = 0xFE;
    cyc = step();
    ASSERT_EQ(cpu.bc.h, 0);
    ASSERT_EQ(cpu.pc, 2);
    ASSERT_CYCLES(cyc, 8);
    TEST_END();

    TEST_BEGIN("DJNZ (B wraps 0->255)");
    setup();
    cpu.bc.h = 0;
    test_ram[0] = 0x10; test_ram[1] = 0x05;
    cyc = step();
    ASSERT_EQ(cpu.bc.h, 0xFF);
    ASSERT_EQ(cpu.pc, 7);
    ASSERT_CYCLES(cyc, 13);
    TEST_END();
}

/* ========== CALL a RET ========== */

/**
 * @brief Test CALL nn (17T).
 */
static void test_call(void) {
    TEST_BEGIN("CALL 0x1234");
    setup();
    cpu.sp = 0xFFFE;
    test_ram[0] = 0xCD; test_ram[1] = 0x34; test_ram[2] = 0x12;
    int cyc = step();
    ASSERT_EQ(cpu.pc, 0x1234);
    ASSERT_EQ(cpu.sp, 0xFFFC);
    /* Na zasobniku je navratova adresa (3) */
    ASSERT_EQ(test_ram[0xFFFC], 0x03);
    ASSERT_EQ(test_ram[0xFFFD], 0x00);
    ASSERT_EQ(cpu.wz.w, 0x1234);
    ASSERT_CYCLES(cyc, 17);
    TEST_END();
}

/**
 * @brief Test CALL cc (17T taken, 10T not taken).
 */
static void test_call_cc(void) {
    int cyc;

    TEST_BEGIN("CALL NZ (taken)");
    setup();
    cpu.sp = 0xFFFE;
    cpu.af.l = 0;
    test_ram[0] = 0xC4; test_ram[1] = 0x00; test_ram[2] = 0x50;
    cyc = step();
    ASSERT_EQ(cpu.pc, 0x5000);
    ASSERT_EQ(cpu.sp, 0xFFFC);
    ASSERT_CYCLES(cyc, 17);
    TEST_END();

    TEST_BEGIN("CALL NZ (not taken)");
    setup();
    cpu.sp = 0xFFFE;
    cpu.af.l = Z80_FLAG_Z;
    test_ram[0] = 0xC4; test_ram[1] = 0x00; test_ram[2] = 0x50;
    cyc = step();
    ASSERT_EQ(cpu.pc, 3);
    ASSERT_EQ(cpu.sp, 0xFFFE);  /* SP nezmeneno */
    ASSERT_CYCLES(cyc, 10);
    TEST_END();
}

/**
 * @brief Test RET (10T).
 */
static void test_ret(void) {
    TEST_BEGIN("RET");
    setup();
    cpu.sp = 0xFFF0;
    test_ram[0xFFF0] = 0x34;
    test_ram[0xFFF1] = 0x12;
    test_ram[0] = 0xC9;
    int cyc = step();
    ASSERT_EQ(cpu.pc, 0x1234);
    ASSERT_EQ(cpu.sp, 0xFFF2);
    ASSERT_EQ(cpu.wz.w, 0x1234);
    ASSERT_CYCLES(cyc, 10);
    TEST_END();
}

/**
 * @brief Test RET cc (11T taken, 5T not taken).
 */
static void test_ret_cc(void) {
    int cyc;

    TEST_BEGIN("RET NZ (taken)");
    setup();
    cpu.sp = 0xFFF0;
    cpu.af.l = 0;
    test_ram[0xFFF0] = 0x00; test_ram[0xFFF1] = 0x50;
    test_ram[0] = 0xC0;
    cyc = step();
    ASSERT_EQ(cpu.pc, 0x5000);
    ASSERT_CYCLES(cyc, 11);
    TEST_END();

    TEST_BEGIN("RET NZ (not taken)");
    setup();
    cpu.sp = 0xFFF0;
    cpu.af.l = Z80_FLAG_Z;
    test_ram[0] = 0xC0;
    cyc = step();
    ASSERT_EQ(cpu.pc, 1);
    ASSERT_CYCLES(cyc, 5);
    TEST_END();

    TEST_BEGIN("RET Z (taken)");
    setup();
    cpu.sp = 0xFFF0;
    cpu.af.l = Z80_FLAG_Z;
    test_ram[0xFFF0] = 0x00; test_ram[0xFFF1] = 0x60;
    test_ram[0] = 0xC8;
    cyc = step();
    ASSERT_EQ(cpu.pc, 0x6000);
    ASSERT_CYCLES(cyc, 11);
    TEST_END();
}

/* ========== RST ========== */

/**
 * @brief Test RST nn (11T).
 */
static void test_rst(void) {
    int cyc;

    TEST_BEGIN("RST 0x00");
    setup();
    cpu.sp = 0xFFFE;
    cpu.pc = 0x1000;
    test_ram[0x1000] = 0xC7;
    cyc = step();
    ASSERT_EQ(cpu.pc, 0x0000);
    ASSERT_EQ(cpu.sp, 0xFFFC);
    ASSERT_EQ(test_ram[0xFFFC], 0x01);
    ASSERT_EQ(test_ram[0xFFFD], 0x10);
    ASSERT_CYCLES(cyc, 11);
    TEST_END();

    TEST_BEGIN("RST 0x38");
    setup();
    cpu.sp = 0xFFFE;
    test_ram[0] = 0xFF;
    cyc = step();
    ASSERT_EQ(cpu.pc, 0x0038);
    ASSERT_CYCLES(cyc, 11);
    TEST_END();

    TEST_BEGIN("RST 0x08");
    setup();
    cpu.sp = 0xFFFE;
    test_ram[0] = 0xCF;
    cyc = step();
    ASSERT_EQ(cpu.pc, 0x0008);
    TEST_END();

    TEST_BEGIN("RST 0x10");
    setup();
    cpu.sp = 0xFFFE;
    test_ram[0] = 0xD7;
    cyc = step();
    ASSERT_EQ(cpu.pc, 0x0010);
    TEST_END();

    TEST_BEGIN("RST 0x28");
    setup();
    cpu.sp = 0xFFFE;
    test_ram[0] = 0xEF;
    cyc = step();
    ASSERT_EQ(cpu.pc, 0x0028);
    TEST_END();
}

/* ========== PUSH/POP ========== */

/**
 * @brief Test PUSH rr (11T) a POP rr (10T).
 */
static void test_push_pop(void) {
    int cyc;

    TEST_BEGIN("PUSH BC / POP DE");
    setup();
    cpu.sp = 0xFFFE;
    cpu.bc.w = 0x1234;
    test_ram[0] = 0xC5;  /* PUSH BC */
    test_ram[1] = 0xD1;  /* POP DE */
    cyc = step();
    ASSERT_EQ(cpu.sp, 0xFFFC);
    ASSERT_CYCLES(cyc, 11);
    cyc = step();
    ASSERT_EQ(cpu.de.w, 0x1234);
    ASSERT_EQ(cpu.sp, 0xFFFE);
    ASSERT_CYCLES(cyc, 10);
    TEST_END();

    TEST_BEGIN("PUSH AF / POP AF (flagy zachovany)");
    setup();
    cpu.sp = 0xFFFE;
    cpu.af.w = 0x42FF;
    test_ram[0] = 0xF5;  /* PUSH AF */
    test_ram[1] = 0x3E; test_ram[2] = 0x00;  /* LD A, 0 */
    test_ram[3] = 0xF1;  /* POP AF */
    step();  /* PUSH AF */
    step();  /* LD A, 0 */
    cyc = step();  /* POP AF */
    ASSERT_EQ(cpu.af.w, 0x42FF);
    ASSERT_CYCLES(cyc, 10);
    TEST_END();
}

/* ========== HALT, NOP ========== */

/**
 * @brief Test NOP (4T) a HALT (4T).
 */
static void test_nop_halt(void) {
    int cyc;

    TEST_BEGIN("NOP");
    setup();
    test_ram[0] = 0x00;
    cyc = step();
    ASSERT_EQ(cpu.pc, 1);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();

    TEST_BEGIN("HALT");
    setup();
    test_ram[0] = 0x76;
    cyc = step();
    ASSERT_TRUE(cpu.halted);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();
}

/* ========== RETN, RETI ========== */

/**
 * @brief Test RETN (14T) a RETI (14T).
 */
static void test_retn_reti(void) {
    int cyc;

    TEST_BEGIN("RETN (obnovi IFF1 z IFF2)");
    setup();
    cpu.sp = 0xFFF0;
    cpu.iff1 = 0;
    cpu.iff2 = 1;
    test_ram[0xFFF0] = 0x00; test_ram[0xFFF1] = 0x50;
    test_ram[0] = 0xED; test_ram[1] = 0x45;
    cyc = step();
    ASSERT_EQ(cpu.pc, 0x5000);
    ASSERT_EQ(cpu.iff1, 1);
    ASSERT_CYCLES(cyc, 14);
    TEST_END();

    TEST_BEGIN("RETI (obnovi IFF1, vyvola callback)");
    setup();
    cpu.sp = 0xFFF0;
    cpu.iff1 = 0;
    cpu.iff2 = 1;
    reti_count = 0;
    test_ram[0xFFF0] = 0x00; test_ram[0xFFF1] = 0x60;
    test_ram[0] = 0xED; test_ram[1] = 0x4D;
    cyc = step();
    ASSERT_EQ(cpu.pc, 0x6000);
    ASSERT_EQ(cpu.iff1, 1);
    ASSERT_EQ(reti_count, 1);
    ASSERT_CYCLES(cyc, 14);
    TEST_END();
}

/* ========== DI, EI ========== */

/**
 * @brief Test DI a EI (4T).
 */
static void test_di_ei(void) {
    int cyc;

    TEST_BEGIN("DI");
    setup();
    cpu.iff1 = 1; cpu.iff2 = 1;
    test_ram[0] = 0xF3;
    cyc = step();
    ASSERT_EQ(cpu.iff1, 0);
    ASSERT_EQ(cpu.iff2, 0);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();

    TEST_BEGIN("EI");
    setup();
    cpu.iff1 = 0; cpu.iff2 = 0;
    test_ram[0] = 0xFB;
    cyc = step();
    ASSERT_EQ(cpu.iff1, 1);
    ASSERT_EQ(cpu.iff2, 1);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();
}

/* ========== Vstupni bod ========== */

/**
 * @brief Spusti vsechny testy skoku, volani a navratu.
 */
void test_jump_call_all(void) {
    test_jp();
    test_jp_cc();
    test_jp_hl();
    test_jr();
    test_djnz();
    test_call();
    test_call_cc();
    test_ret();
    test_ret_cc();
    test_rst();
    test_push_pop();
    test_nop_halt();
    test_retn_reti();
    test_di_ei();
}
