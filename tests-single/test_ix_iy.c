/**
 * @file test_ix_iy.c
 * @brief Testy DD/FD prefix instrukci (IX/IY) a DD CB/FD CB instrukci.
 *
 * Pokryva: LD IX/IY,nn, LD r,(IX+d), LD (IX+d),r, ADD IX,rr,
 * INC/DEC IX, INC/DEC IXH/IXL (nedokumentovane),
 * aritmetika s IXH/IXL, DD CB prefix (indexed bit ops).
 */

#include "test_framework.h"

/* ========== Zakladni IX instrukce ========== */

/**
 * @brief Test LD IX, nn (14T).
 */
static void test_ld_ix_nn(void) {
    TEST_BEGIN("LD IX,0x1234");
    setup();
    test_ram[0] = 0xDD; test_ram[1] = 0x21;
    test_ram[2] = 0x34; test_ram[3] = 0x12;
    int cyc = step();
    ASSERT_EQ(cpu->ix.w, 0x1234);
    ASSERT_CYCLES(cyc, 14);
    TEST_END();

    TEST_BEGIN("LD IY,0xABCD");
    setup();
    test_ram[0] = 0xFD; test_ram[1] = 0x21;
    test_ram[2] = 0xCD; test_ram[3] = 0xAB;
    cyc = step();
    ASSERT_EQ(cpu->iy.w, 0xABCD);
    ASSERT_CYCLES(cyc, 14);
    TEST_END();
}

/**
 * @brief Test LD r, (IX+d) (19T).
 */
static void test_ld_r_ixd(void) {
    TEST_BEGIN("LD A,(IX+5)");
    setup();
    cpu->ix.w = 0x1000;
    test_ram[0x1005] = 0x42;
    test_ram[0] = 0xDD; test_ram[1] = 0x7E; test_ram[2] = 0x05;
    int cyc = step();
    ASSERT_EQ(cpu->af.h, 0x42);
    ASSERT_EQ(cpu->wz.w, 0x1005);
    ASSERT_CYCLES(cyc, 19);
    TEST_END();

    TEST_BEGIN("LD B,(IX-3)");
    setup();
    cpu->ix.w = 0x2003;
    test_ram[0x2000] = 0xAB;
    test_ram[0] = 0xDD; test_ram[1] = 0x46; test_ram[2] = 0xFD;  /* -3 */
    cyc = step();
    ASSERT_EQ(cpu->bc.h, 0xAB);
    ASSERT_CYCLES(cyc, 19);
    TEST_END();

    TEST_BEGIN("LD C,(IY+0)");
    setup();
    cpu->iy.w = 0x3000;
    test_ram[0x3000] = 0xEE;
    test_ram[0] = 0xFD; test_ram[1] = 0x4E; test_ram[2] = 0x00;
    cyc = step();
    ASSERT_EQ(cpu->bc.l, 0xEE);
    ASSERT_CYCLES(cyc, 19);
    TEST_END();
}

/**
 * @brief Test LD (IX+d), r (19T).
 */
static void test_ld_ixd_r(void) {
    TEST_BEGIN("LD (IX+2),A");
    setup();
    cpu->ix.w = 0x4000;
    cpu->af.h = 0x55;
    test_ram[0] = 0xDD; test_ram[1] = 0x77; test_ram[2] = 0x02;
    int cyc = step();
    ASSERT_EQ(test_ram[0x4002], 0x55);
    ASSERT_CYCLES(cyc, 19);
    TEST_END();
}

/**
 * @brief Test LD (IX+d), n (19T).
 */
static void test_ld_ixd_n(void) {
    TEST_BEGIN("LD (IX+1),0xAB");
    setup();
    cpu->ix.w = 0x5000;
    test_ram[0] = 0xDD; test_ram[1] = 0x36;
    test_ram[2] = 0x01; test_ram[3] = 0xAB;
    int cyc = step();
    ASSERT_EQ(test_ram[0x5001], 0xAB);
    ASSERT_CYCLES(cyc, 19);
    TEST_END();
}

/* ========== IX aritmetika ========== */

/**
 * @brief Test ADD IX, rr (15T).
 */
static void test_add_ix(void) {
    TEST_BEGIN("ADD IX,BC");
    setup();
    cpu->ix.w = 0x1000; cpu->bc.w = 0x0234;
    test_ram[0] = 0xDD; test_ram[1] = 0x09;
    int cyc = step();
    ASSERT_EQ(cpu->ix.w, 0x1234);
    ASSERT_FLAG_CLEAR(Z80_FLAG_N);
    ASSERT_CYCLES(cyc, 15);
    TEST_END();

    TEST_BEGIN("ADD IY,SP");
    setup();
    cpu->iy.w = 0x1000; cpu->sp = 0x0100;
    test_ram[0] = 0xFD; test_ram[1] = 0x39;
    cyc = step();
    ASSERT_EQ(cpu->iy.w, 0x1100);
    ASSERT_CYCLES(cyc, 15);
    TEST_END();

    TEST_BEGIN("ADD IX,IX");
    setup();
    cpu->ix.w = 0x4000;
    test_ram[0] = 0xDD; test_ram[1] = 0x29;
    cyc = step();
    ASSERT_EQ(cpu->ix.w, 0x8000);
    ASSERT_CYCLES(cyc, 15);
    TEST_END();
}

/**
 * @brief Test INC IX / DEC IX (10T).
 */
static void test_inc_dec_ix(void) {
    int cyc;

    TEST_BEGIN("INC IX");
    setup();
    cpu->ix.w = 0x00FF;
    test_ram[0] = 0xDD; test_ram[1] = 0x23;
    cyc = step();
    ASSERT_EQ(cpu->ix.w, 0x0100);
    ASSERT_CYCLES(cyc, 10);
    TEST_END();

    TEST_BEGIN("DEC IY");
    setup();
    cpu->iy.w = 0x0100;
    test_ram[0] = 0xFD; test_ram[1] = 0x2B;
    cyc = step();
    ASSERT_EQ(cpu->iy.w, 0x00FF);
    ASSERT_CYCLES(cyc, 10);
    TEST_END();
}

/**
 * @brief Test INC/DEC (IX+d) (23T).
 */
static void test_inc_dec_ixd(void) {
    int cyc;

    TEST_BEGIN("INC (IX+3)");
    setup();
    cpu->ix.w = 0x6000;
    test_ram[0x6003] = 0x41;
    test_ram[0] = 0xDD; test_ram[1] = 0x34; test_ram[2] = 0x03;
    cyc = step();
    ASSERT_EQ(test_ram[0x6003], 0x42);
    ASSERT_CYCLES(cyc, 23);
    TEST_END();

    TEST_BEGIN("DEC (IY-1)");
    setup();
    cpu->iy.w = 0x7001;
    test_ram[0x7000] = 0x01;
    test_ram[0] = 0xFD; test_ram[1] = 0x35; test_ram[2] = 0xFF;
    cyc = step();
    ASSERT_EQ(test_ram[0x7000], 0x00);
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    ASSERT_CYCLES(cyc, 23);
    TEST_END();
}

/* ========== Nedokumentovane IXH/IXL ========== */

/**
 * @brief Test nedokumentovanych instrukci IXH/IXL (8T).
 */
static void test_ixh_ixl(void) {
    int cyc;

    TEST_BEGIN("INC IXH");
    setup();
    cpu->ix.w = 0x1200;
    test_ram[0] = 0xDD; test_ram[1] = 0x24;
    cyc = step();
    ASSERT_EQ(cpu->ix.h, 0x13);
    ASSERT_CYCLES(cyc, 8);
    TEST_END();

    TEST_BEGIN("DEC IXL");
    setup();
    cpu->ix.w = 0x0010;
    test_ram[0] = 0xDD; test_ram[1] = 0x2D;
    cyc = step();
    ASSERT_EQ(cpu->ix.l, 0x0F);
    ASSERT_CYCLES(cyc, 8);
    TEST_END();

    TEST_BEGIN("LD IXH,0x42");
    setup();
    test_ram[0] = 0xDD; test_ram[1] = 0x26; test_ram[2] = 0x42;
    cyc = step();
    ASSERT_EQ(cpu->ix.h, 0x42);
    ASSERT_CYCLES(cyc, 11);
    TEST_END();

    TEST_BEGIN("LD A,IXH");
    setup();
    cpu->ix.w = 0xAB00;
    test_ram[0] = 0xDD; test_ram[1] = 0x7C;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0xAB);
    ASSERT_CYCLES(cyc, 8);
    TEST_END();

    TEST_BEGIN("LD A,IXL");
    setup();
    cpu->ix.w = 0x00CD;
    test_ram[0] = 0xDD; test_ram[1] = 0x7D;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0xCD);
    ASSERT_CYCLES(cyc, 8);
    TEST_END();

    TEST_BEGIN("LD IXH,B");
    setup();
    cpu->bc.h = 0x55;
    test_ram[0] = 0xDD; test_ram[1] = 0x60;
    cyc = step();
    ASSERT_EQ(cpu->ix.h, 0x55);
    ASSERT_CYCLES(cyc, 8);
    TEST_END();

    TEST_BEGIN("ADD A,IXH");
    setup();
    cpu->af.h = 0x10; cpu->ix.w = 0x2000;
    test_ram[0] = 0xDD; test_ram[1] = 0x84;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x30);
    ASSERT_CYCLES(cyc, 8);
    TEST_END();

    TEST_BEGIN("SUB IYL");
    setup();
    cpu->af.h = 0x50; cpu->iy.w = 0x0010;
    test_ram[0] = 0xFD; test_ram[1] = 0x95;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x40);
    ASSERT_CYCLES(cyc, 8);
    TEST_END();

    TEST_BEGIN("CP IXH");
    setup();
    cpu->af.h = 0x42; cpu->ix.w = 0x4200;
    test_ram[0] = 0xDD; test_ram[1] = 0xBC;
    cyc = step();
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    ASSERT_CYCLES(cyc, 8);
    TEST_END();
}

/**
 * @brief Test aritmetiky s (IX+d) (19T).
 */
static void test_alu_ixd(void) {
    int cyc;

    TEST_BEGIN("ADD A,(IX+2)");
    setup();
    cpu->af.h = 0x10; cpu->ix.w = 0x3000;
    test_ram[0x3002] = 0x05;
    test_ram[0] = 0xDD; test_ram[1] = 0x86; test_ram[2] = 0x02;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x15);
    ASSERT_CYCLES(cyc, 19);
    TEST_END();

    TEST_BEGIN("CP (IY-1)");
    setup();
    cpu->af.h = 0x42; cpu->iy.w = 0x4001;
    test_ram[0x4000] = 0x42;
    test_ram[0] = 0xFD; test_ram[1] = 0xBE; test_ram[2] = 0xFF;
    cyc = step();
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    ASSERT_CYCLES(cyc, 19);
    TEST_END();
}

/* ========== DD CB / FD CB prefix ========== */

/**
 * @brief Test DD CB d xx - indexed bit operations (23T shift/set/res, 20T bit).
 */
static void test_ddcb(void) {
    int cyc;

    TEST_BEGIN("DD CB: RLC (IX+2)");
    setup();
    cpu->ix.w = 0x5000;
    test_ram[0x5002] = 0x80;
    test_ram[0] = 0xDD; test_ram[1] = 0xCB;
    test_ram[2] = 0x02; test_ram[3] = 0x06;  /* RLC (IX+2) */
    cyc = step();
    ASSERT_EQ(test_ram[0x5002], 0x01);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_CYCLES(cyc, 23);
    TEST_END();

    TEST_BEGIN("DD CB: BIT 3,(IX+1)");
    setup();
    cpu->ix.w = 0x6000;
    test_ram[0x6001] = 0x08;  /* bit 3 = 1 */
    test_ram[0] = 0xDD; test_ram[1] = 0xCB;
    test_ram[2] = 0x01; test_ram[3] = 0x5E;  /* BIT 3,(IX+1) */
    cyc = step();
    ASSERT_FLAG_CLEAR(Z80_FLAG_Z);
    ASSERT_CYCLES(cyc, 20);
    TEST_END();

    TEST_BEGIN("DD CB: SET 5,(IX+0)");
    setup();
    cpu->ix.w = 0x7000;
    test_ram[0x7000] = 0x00;
    test_ram[0] = 0xDD; test_ram[1] = 0xCB;
    test_ram[2] = 0x00; test_ram[3] = 0xEE;  /* SET 5,(IX+0) */
    cyc = step();
    ASSERT_EQ(test_ram[0x7000], 0x20);
    ASSERT_CYCLES(cyc, 23);
    TEST_END();

    TEST_BEGIN("DD CB: RES 7,(IY-2)");
    setup();
    cpu->iy.w = 0x8002;
    test_ram[0x8000] = 0xFF;
    test_ram[0] = 0xFD; test_ram[1] = 0xCB;
    test_ram[2] = 0xFE; test_ram[3] = 0xBE;  /* RES 7,(IY-2) */
    cyc = step();
    ASSERT_EQ(test_ram[0x8000], 0x7F);
    ASSERT_CYCLES(cyc, 23);
    TEST_END();

    TEST_BEGIN("DD CB: SLA (IX+1)->B (undoc copy to register)");
    setup();
    cpu->ix.w = 0x5000;
    test_ram[0x5001] = 0x40;
    test_ram[0] = 0xDD; test_ram[1] = 0xCB;
    test_ram[2] = 0x01; test_ram[3] = 0x20;  /* SLA (IX+1)->B */
    cyc = step();
    ASSERT_EQ(test_ram[0x5001], 0x80);
    ASSERT_EQ(cpu->bc.h, 0x80);  /* kopie do B */
    ASSERT_CYCLES(cyc, 23);
    TEST_END();

    TEST_BEGIN("DD CB: SET 0,(IX+0)->A (undoc copy to register)");
    setup();
    cpu->ix.w = 0x5000;
    test_ram[0x5000] = 0x00;
    test_ram[0] = 0xDD; test_ram[1] = 0xCB;
    test_ram[2] = 0x00; test_ram[3] = 0xC7;  /* SET 0,(IX+0)->A */
    cyc = step();
    ASSERT_EQ(test_ram[0x5000], 0x01);
    ASSERT_EQ(cpu->af.h, 0x01);  /* kopie do A */
    ASSERT_CYCLES(cyc, 23);
    TEST_END();
}

/* ========== Ostatni DD/FD instrukce ========== */

/**
 * @brief Test PUSH IX/POP IX (15T/14T), EX (SP),IX (23T), JP (IX) (8T),
 * LD SP,IX (10T), LD (nn),IX/LD IX,(nn) (20T).
 */
static void test_ix_misc(void) {
    int cyc;

    TEST_BEGIN("PUSH IX / POP IY");
    setup();
    cpu->sp = 0xFFFE;
    cpu->ix.w = 0x1234;
    test_ram[0] = 0xDD; test_ram[1] = 0xE5;  /* PUSH IX */
    test_ram[2] = 0xFD; test_ram[3] = 0xE1;  /* POP IY */
    cyc = step();
    ASSERT_CYCLES(cyc, 15);
    cyc = step();
    ASSERT_EQ(cpu->iy.w, 0x1234);
    ASSERT_CYCLES(cyc, 14);
    TEST_END();

    TEST_BEGIN("JP (IX)");
    setup();
    cpu->ix.w = 0xABCD;
    test_ram[0] = 0xDD; test_ram[1] = 0xE9;
    cyc = step();
    ASSERT_EQ(cpu->pc, 0xABCD);
    ASSERT_CYCLES(cyc, 8);
    TEST_END();

    TEST_BEGIN("LD SP,IX");
    setup();
    cpu->ix.w = 0x1234;
    test_ram[0] = 0xDD; test_ram[1] = 0xF9;
    cyc = step();
    ASSERT_EQ(cpu->sp, 0x1234);
    ASSERT_CYCLES(cyc, 10);
    TEST_END();

    TEST_BEGIN("EX (SP),IX");
    setup();
    cpu->sp = 0xFFF0;
    cpu->ix.w = 0x1234;
    test_ram[0xFFF0] = 0xCD; test_ram[0xFFF1] = 0xAB;
    test_ram[0] = 0xDD; test_ram[1] = 0xE3;
    cyc = step();
    ASSERT_EQ(cpu->ix.w, 0xABCD);
    ASSERT_EQ(test_ram[0xFFF0], 0x34);
    ASSERT_EQ(test_ram[0xFFF1], 0x12);
    ASSERT_CYCLES(cyc, 23);
    TEST_END();

    TEST_BEGIN("LD (0x8000),IX");
    setup();
    cpu->ix.w = 0xBEEF;
    test_ram[0] = 0xDD; test_ram[1] = 0x22;
    test_ram[2] = 0x00; test_ram[3] = 0x80;
    cyc = step();
    ASSERT_EQ(test_ram[0x8000], 0xEF);
    ASSERT_EQ(test_ram[0x8001], 0xBE);
    ASSERT_CYCLES(cyc, 20);
    TEST_END();

    TEST_BEGIN("LD IY,(0x9000)");
    setup();
    test_ram[0x9000] = 0x34; test_ram[0x9001] = 0x12;
    test_ram[0] = 0xFD; test_ram[1] = 0x2A;
    test_ram[2] = 0x00; test_ram[3] = 0x90;
    cyc = step();
    ASSERT_EQ(cpu->iy.w, 0x1234);
    ASSERT_CYCLES(cyc, 20);
    TEST_END();
}

/* ========== Vstupni bod ========== */

/**
 * @brief Spusti vsechny IX/IY testy.
 */
void test_ix_iy_all(void) {
    test_ld_ix_nn();
    test_ld_r_ixd();
    test_ld_ixd_r();
    test_ld_ixd_n();
    test_add_ix();
    test_inc_dec_ix();
    test_inc_dec_ixd();
    test_ixh_ixl();
    test_alu_ixd();
    test_ddcb();
    test_ix_misc();
}
