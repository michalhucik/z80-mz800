/**
 * @file test_flags_daa.c
 * @brief Testy DAA instrukce a nedokumentovanych flagu (F3/F5, MEMPTR).
 *
 * Pokryva: DAA pro ruzne vstupy a rezimy (ADD/SUB),
 * nedokumentovane flagy F3/F5, MEMPTR/WZ chovani.
 */

#include "test_framework.h"

/* ========== DAA ========== */

/**
 * @brief Test DAA instrukce (4T).
 *
 * DAA koriguje BCD vysledek predchozi aritmeticke operace.
 */
static void test_daa(void) {
    int cyc;

    TEST_BEGIN("DAA po ADD (9+1=10 BCD)");
    setup();
    cpu->af.h = 0x09;
    cpu->bc.h = 0x01;
    test_ram[0] = 0x80;  /* ADD A,B */
    test_ram[1] = 0x27;  /* DAA */
    step();  /* ADD: A = 0x0A */
    cyc = step();  /* DAA */
    ASSERT_EQ(cpu->af.h, 0x10);
    ASSERT_FLAG_CLEAR(Z80_FLAG_C);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();

    TEST_BEGIN("DAA po ADD (99+1=00 BCD, carry)");
    setup();
    cpu->af.h = 0x99;
    cpu->bc.h = 0x01;
    test_ram[0] = 0x80;  /* ADD A,B */
    test_ram[1] = 0x27;  /* DAA */
    step();
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x00);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    TEST_END();

    TEST_BEGIN("DAA po ADD (15+1=16 BCD)");
    setup();
    cpu->af.h = 0x15;
    cpu->bc.h = 0x01;
    test_ram[0] = 0x80;  /* ADD A,B: 0x16 */
    test_ram[1] = 0x27;  /* DAA: 0x16 (uz je BCD) */
    step();
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x16);
    TEST_END();

    TEST_BEGIN("DAA po SUB (10-1=09 BCD)");
    setup();
    cpu->af.h = 0x10;
    cpu->bc.h = 0x01;
    test_ram[0] = 0x90;  /* SUB B: A = 0x0F */
    test_ram[1] = 0x27;  /* DAA */
    step();
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x09);
    ASSERT_FLAG_SET(Z80_FLAG_N);
    TEST_END();

    TEST_BEGIN("DAA po ADD (0x50+0x50=0xA0 -> 0x00 s carry)");
    setup();
    cpu->af.h = 0x50;
    cpu->bc.h = 0x50;
    test_ram[0] = 0x80;  /* ADD A,B: A = 0xA0 */
    test_ram[1] = 0x27;  /* DAA: korekce +0x60, A = 0x00 */
    step();
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x00);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    TEST_END();

    TEST_BEGIN("DAA (A=0x0A, no flags -> 0x10)");
    setup();
    cpu->af.h = 0x0A;
    cpu->af.l = 0;  /* N=0, C=0, H=0 */
    test_ram[0] = 0x27;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x10);
    ASSERT_FLAG_CLEAR(Z80_FLAG_C);
    ASSERT_FLAG_SET(Z80_FLAG_H);
    TEST_END();

    TEST_BEGIN("DAA (A=0x9A, N=0 -> 0x00 s carry)");
    setup();
    cpu->af.h = 0x9A;
    cpu->af.l = 0;
    test_ram[0] = 0x27;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x00);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    TEST_END();

    TEST_BEGIN("DAA (A=0xFF, H=1, C=1, N=0 -> 0x65)");
    setup();
    cpu->af.h = 0xFF;
    cpu->af.l = Z80_FLAG_C | Z80_FLAG_H;
    test_ram[0] = 0x27;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x65);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    TEST_END();
}

/* ========== Nedokumentovane flagy F3/F5 ========== */

/**
 * @brief Test F3/F5 chovani u ruznych instrukci.
 *
 * F3 (bit 3) a F5 (bit 5) jsou kopie bitu z vysledku nebo operandu.
 */
static void test_undoc_flags(void) {
    TEST_BEGIN("ADD A,B: F3/F5 z vysledku");
    setup();
    cpu->af.h = 0x20; cpu->bc.h = 0x00;
    test_ram[0] = 0x80;
    step();
    /* Vysledek = 0x20, F5 = bit 5 = 1, F3 = bit 3 = 0 */
    ASSERT_FLAG_SET(Z80_FLAG_5);
    ASSERT_FLAG_CLEAR(Z80_FLAG_3);
    TEST_END();

    TEST_BEGIN("AND: F3/F5 z vysledku");
    setup();
    cpu->af.h = 0xFF;
    test_ram[0] = 0xE6; test_ram[1] = 0x28;  /* AND 0x28 */
    step();
    /* Vysledek = 0x28, F5 = 1, F3 = 1 */
    ASSERT_FLAG_SET(Z80_FLAG_5);
    ASSERT_FLAG_SET(Z80_FLAG_3);
    TEST_END();

    TEST_BEGIN("CP: F3/F5 z operandu (ne z vysledku)");
    setup();
    cpu->af.h = 0xFF;
    test_ram[0] = 0xFE; test_ram[1] = 0x28;  /* CP 0x28 */
    step();
    /* F3/F5 by mely byt z operandu (0x28) */
    ASSERT_EQ(cpu->af.l & Z80_FLAG_3, 0x28 & Z80_FLAG_3);
    ASSERT_EQ(cpu->af.l & Z80_FLAG_5, 0x28 & Z80_FLAG_5);
    TEST_END();

    TEST_BEGIN("INC: F3/F5 z vysledku");
    setup();
    cpu->bc.h = 0x27;
    test_ram[0] = 0x04;  /* INC B */
    step();
    /* Vysledek = 0x28, F5 = 1, F3 = 1 */
    ASSERT_FLAG_SET(Z80_FLAG_5);
    ASSERT_FLAG_SET(Z80_FLAG_3);
    TEST_END();

    TEST_BEGIN("SCF: F3/F5 z A|F");
    setup();
    cpu->af.h = 0x28;  /* A s bity 3 a 5 */
    cpu->af.l = 0;
    test_ram[0] = 0x37;  /* SCF */
    step();
    /* F3/F5 z (A | F) */
    ASSERT_FLAG_SET(Z80_FLAG_3);
    ASSERT_FLAG_SET(Z80_FLAG_5);
    TEST_END();

    TEST_BEGIN("CCF: F3/F5 z A|F");
    setup();
    cpu->af.h = 0x00;
    cpu->af.l = Z80_FLAG_C;
    test_ram[0] = 0x3F;  /* CCF */
    step();
    /* F3/F5 z (A | F) */
    ASSERT_FLAG_CLEAR(Z80_FLAG_3);
    ASSERT_FLAG_CLEAR(Z80_FLAG_5);
    TEST_END();

    TEST_BEGIN("CPL: F3/F5 z vysledku A");
    setup();
    cpu->af.h = 0x00;
    test_ram[0] = 0x2F;  /* CPL */
    step();
    /* A = 0xFF, F3 = 1, F5 = 1 */
    ASSERT_FLAG_SET(Z80_FLAG_3);
    ASSERT_FLAG_SET(Z80_FLAG_5);
    TEST_END();

    TEST_BEGIN("RLCA: F3/F5 z noveho A");
    setup();
    cpu->af.h = 0x28;
    test_ram[0] = 0x07;  /* RLCA */
    step();
    /* A = 0x50, F3 = 0, F5 = 0 (0x50 = 0101_0000) */
    ASSERT_FLAG_CLEAR(Z80_FLAG_3);
    ASSERT_FLAG_CLEAR(Z80_FLAG_5);
    TEST_END();
}

/* ========== MEMPTR/WZ testy ========== */

/**
 * @brief Test MEMPTR/WZ registru u ruznych instrukci.
 */
static void test_memptr(void) {
    TEST_BEGIN("JP nn: WZ = addr");
    setup();
    test_ram[0] = 0xC3; test_ram[1] = 0x34; test_ram[2] = 0x12;
    step();
    ASSERT_EQ(cpu->wz.w, 0x1234);
    TEST_END();

    TEST_BEGIN("CALL nn: WZ = addr");
    setup();
    cpu->sp = 0xFFFE;
    test_ram[0] = 0xCD; test_ram[1] = 0x00; test_ram[2] = 0x50;
    step();
    ASSERT_EQ(cpu->wz.w, 0x5000);
    TEST_END();

    TEST_BEGIN("RET: WZ = navratova adresa");
    setup();
    cpu->sp = 0xFFF0;
    test_ram[0xFFF0] = 0x34; test_ram[0xFFF1] = 0x12;
    test_ram[0] = 0xC9;
    step();
    ASSERT_EQ(cpu->wz.w, 0x1234);
    TEST_END();

    TEST_BEGIN("LD A,(nn): WZ = addr + 1");
    setup();
    test_ram[0] = 0x3A; test_ram[1] = 0xFF; test_ram[2] = 0x7F;
    step();
    ASSERT_EQ(cpu->wz.w, 0x8000);
    TEST_END();

    TEST_BEGIN("LD (nn),A: WZ_lo = (addr+1)&0xFF, WZ_hi = A");
    setup();
    cpu->af.h = 0x42;
    test_ram[0] = 0x32; test_ram[1] = 0xFF; test_ram[2] = 0x7F;
    step();
    ASSERT_EQ(cpu->wz.l, 0x00);
    ASSERT_EQ(cpu->wz.h, 0x42);
    TEST_END();

    TEST_BEGIN("ADD HL,rr: WZ = HL_pred + 1");
    setup();
    cpu->hl.w = 0x1234; cpu->bc.w = 0x0001;
    test_ram[0] = 0x09;
    step();
    ASSERT_EQ(cpu->wz.w, 0x1235);
    TEST_END();

    TEST_BEGIN("JR d: WZ = nova adresa");
    setup();
    test_ram[0] = 0x18; test_ram[1] = 0x10;
    step();
    ASSERT_EQ(cpu->wz.w, 0x12);
    TEST_END();

    TEST_BEGIN("EX (SP),HL: WZ = nova HL");
    setup();
    cpu->sp = 0xFFF0;
    cpu->hl.w = 0x1111;
    test_ram[0xFFF0] = 0xCD; test_ram[0xFFF1] = 0xAB;
    test_ram[0] = 0xE3;
    step();
    ASSERT_EQ(cpu->wz.w, 0xABCD);
    TEST_END();

    TEST_BEGIN("IN A,(n): WZ = port + 1");
    setup();
    cpu->af.h = 0x10;
    test_ram[0] = 0xDB; test_ram[1] = 0x20;
    step();
    /* Port = 0x1020, WZ = 0x1021 */
    ASSERT_EQ(cpu->wz.w, 0x1021);
    TEST_END();

    TEST_BEGIN("OUT (n),A: WZ_lo = (port+1)&0xFF, WZ_hi = A");
    setup();
    cpu->af.h = 0x42;
    test_ram[0] = 0xD3; test_ram[1] = 0xFF;
    step();
    ASSERT_EQ(cpu->wz.l, 0x00);
    ASSERT_EQ(cpu->wz.h, 0x42);
    TEST_END();

    TEST_BEGIN("IN r,(C): WZ = BC + 1");
    setup();
    cpu->bc.w = 0x1234;
    test_ram[0] = 0xED; test_ram[1] = 0x40;
    step();
    ASSERT_EQ(cpu->wz.w, 0x1235);
    TEST_END();

    TEST_BEGIN("OUT (C),r: WZ = BC + 1");
    setup();
    cpu->bc.w = 0x1234;
    test_ram[0] = 0xED; test_ram[1] = 0x41;
    step();
    ASSERT_EQ(cpu->wz.w, 0x1235);
    TEST_END();

    TEST_BEGIN("LD A,(BC): WZ = BC + 1");
    setup();
    cpu->bc.w = 0x5000;
    test_ram[0] = 0x0A;
    step();
    ASSERT_EQ(cpu->wz.w, 0x5001);
    TEST_END();

    TEST_BEGIN("LD (BC),A: WZ_lo = (BC+1)&0xFF, WZ_hi = A");
    setup();
    cpu->af.h = 0x99;
    cpu->bc.w = 0x5000;
    test_ram[0] = 0x02;
    step();
    ASSERT_EQ(cpu->wz.l, 0x01);
    ASSERT_EQ(cpu->wz.h, 0x99);
    TEST_END();

    TEST_BEGIN("RRD: WZ = HL + 1");
    setup();
    cpu->af.h = 0x12; cpu->hl.w = 0x5000;
    test_ram[0x5000] = 0x34;
    test_ram[0] = 0xED; test_ram[1] = 0x67;
    step();
    ASSERT_EQ(cpu->wz.w, 0x5001);
    TEST_END();
}

/* ========== Q registr (SCF/CCF F3/F5) ========== */

/**
 * @brief Testy Q registru - vliv na F3/F5 u SCF a CCF.
 */
static void test_q_register(void) {
    int cyc;

    TEST_BEGIN("SCF po OR A: F3/F5 z (A | F)");
    setup();
    cpu->af.h = 0x28;
    test_ram[0] = 0xB7; test_ram[1] = 0x37;
    step(); step();
    ASSERT_FLAG_SET(Z80_FLAG_3);
    ASSERT_FLAG_SET(Z80_FLAG_5);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    TEST_END();

    TEST_BEGIN("SCF po LD A,n: F3/F5 jen z A");
    setup();
    test_ram[0] = 0xB7; test_ram[1] = 0x3E; test_ram[2] = 0x00; test_ram[3] = 0x37;
    step(); step(); step();
    ASSERT_FLAG_CLEAR(Z80_FLAG_3);
    ASSERT_FLAG_CLEAR(Z80_FLAG_5);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    TEST_END();

    TEST_BEGIN("CCF po OR A: F3/F5 z (A | F)");
    setup();
    cpu->af.h = 0x28;
    test_ram[0] = 0xB7; test_ram[1] = 0x3F;
    step(); step();
    ASSERT_FLAG_SET(Z80_FLAG_3);
    ASSERT_FLAG_SET(Z80_FLAG_5);
    TEST_END();

    TEST_BEGIN("CCF po LD: F3/F5 jen z A");
    setup();
    cpu->af.h = 0x00;
    test_ram[0] = 0xB7; test_ram[1] = 0x3E; test_ram[2] = 0x00; test_ram[3] = 0x3F;
    step(); step(); step();
    ASSERT_FLAG_CLEAR(Z80_FLAG_3);
    ASSERT_FLAG_CLEAR(Z80_FLAG_5);
    TEST_END();

    TEST_BEGIN("SCF po SCF: Q = F (SCF modifikuje flagy)");
    setup();
    cpu->af.h = 0x28;
    test_ram[0] = 0x37; test_ram[1] = 0x37;
    step(); step();
    ASSERT_FLAG_SET(Z80_FLAG_3);
    ASSERT_FLAG_SET(Z80_FLAG_5);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    TEST_END();

    TEST_BEGIN("SCF po NOP: Q = 0");
    setup();
    cpu->af.h = 0x00; cpu->af.l = 0xFF;
    test_ram[0] = 0x00; test_ram[1] = 0x37;
    step(); step();
    ASSERT_FLAG_CLEAR(Z80_FLAG_3);
    ASSERT_FLAG_CLEAR(Z80_FLAG_5);
    TEST_END();

    TEST_BEGIN("SCF po POP AF: Q = 0");
    setup();
    cpu->sp = 0xFFF0;
    test_ram[0xFFF0] = 0xFF; test_ram[0xFFF1] = 0x00;
    cpu->af.h = 0x00;
    test_ram[0] = 0xF1; test_ram[1] = 0x37;
    step(); step();
    ASSERT_FLAG_CLEAR(Z80_FLAG_3);
    ASSERT_FLAG_CLEAR(Z80_FLAG_5);
    TEST_END();

    TEST_BEGIN("SCF po EX AF,AF': Q = 0");
    setup();
    cpu->af.h = 0x00; cpu->af.l = 0x00;
    cpu->af2.w = 0x00FF;
    test_ram[0] = 0x08; test_ram[1] = 0x37;
    step(); step();
    ASSERT_FLAG_CLEAR(Z80_FLAG_3);
    ASSERT_FLAG_CLEAR(Z80_FLAG_5);
    TEST_END();
}

/* ========== Vstupni bod ========== */

/**
 * @brief Spusti vsechny testy DAA a flagu.
 */
void test_flags_daa_all(void) {
    test_daa();
    test_undoc_flags();
    test_memptr();
    test_q_register();
}
