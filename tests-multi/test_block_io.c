/**
 * @file test_block_io.c
 * @brief Testy blokovych instrukci a I/O instrukci Z80.
 *
 * Pokryva: LDI, LDIR, LDD, LDDR, CPI, CPIR, CPD, CPDR,
 * INI, INIR, IND, INDR, OUTI, OTIR, OUTD, OTDR,
 * IN A,(n), OUT (n),A, IN r,(C), OUT (C),r.
 */

#include "test_framework.h"

/* ========== Zakladni I/O ========== */

/**
 * @brief Test IN A,(n) a OUT (n),A (11T).
 */
static void test_in_out_basic(void) {
    int cyc;

    TEST_BEGIN("OUT (0x10),A");
    setup();
    cpu->af.h = 0x42;
    test_ram[0] = 0xD3; test_ram[1] = 0x10;
    cyc = step();
    ASSERT_EQ(io_ports[0x10], 0x42);
    ASSERT_EQ(io_write_count, 1);
    /* Port adresa = A:n = 0x4210 */
    ASSERT_EQ(last_io_write_port, 0x4210);
    ASSERT_CYCLES(cyc, 11);
    TEST_END();

    TEST_BEGIN("IN A,(0x20)");
    setup();
    cpu->af.h = 0x55;
    io_read_data[0x20] = 0xAB;
    test_ram[0] = 0xDB; test_ram[1] = 0x20;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0xAB);
    ASSERT_EQ(io_read_count, 1);
    /* Port adresa = A:n = 0x5520 */
    ASSERT_EQ(last_io_read_port, 0x5520);
    ASSERT_CYCLES(cyc, 11);
    /* MEMPTR = port + 1 */
    ASSERT_EQ(cpu->wz.w, 0x5521);
    TEST_END();
}

/**
 * @brief Test IN r,(C) - ED prefix (12T).
 * Nastavuje flagy (S, Z, P, N=0, H=0), zachova C.
 */
static void test_in_r_c(void) {
    int cyc;

    TEST_BEGIN("IN B,(C)");
    setup();
    cpu->bc.w = 0x1234;
    io_read_data[0x34] = 0x80;
    test_ram[0] = 0xED; test_ram[1] = 0x40;
    cyc = step();
    ASSERT_EQ(cpu->bc.h, 0x80);
    ASSERT_FLAG_SET(Z80_FLAG_S);
    ASSERT_FLAG_CLEAR(Z80_FLAG_Z);
    ASSERT_FLAG_CLEAR(Z80_FLAG_N);
    ASSERT_FLAG_CLEAR(Z80_FLAG_H);
    ASSERT_CYCLES(cyc, 12);
    TEST_END();

    TEST_BEGIN("IN F,(C) (ED 70 - cteni bez ulozeni, jen flagy)");
    setup();
    cpu->bc.w = 0x0050;
    io_read_data[0x50] = 0x00;
    test_ram[0] = 0xED; test_ram[1] = 0x70;
    cyc = step();
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    ASSERT_FLAG_SET(Z80_FLAG_PV);  /* parita 0 = sudy */
    ASSERT_CYCLES(cyc, 12);
    TEST_END();
}

/**
 * @brief Test OUT (C), r - ED prefix (12T).
 */
static void test_out_c_r(void) {
    TEST_BEGIN("OUT (C),A");
    setup();
    cpu->af.h = 0x42;
    cpu->bc.w = 0x1234;
    test_ram[0] = 0xED; test_ram[1] = 0x79;
    int cyc = step();
    ASSERT_EQ(io_ports[0x34], 0x42);
    ASSERT_EQ(last_io_write_port, 0x1234);
    ASSERT_CYCLES(cyc, 12);
    /* MEMPTR = BC + 1 */
    ASSERT_EQ(cpu->wz.w, 0x1235);
    TEST_END();

    TEST_BEGIN("OUT (C),0 (ED 71)");
    setup();
    cpu->bc.w = 0x0060;
    test_ram[0] = 0xED; test_ram[1] = 0x71;
    int cyc2 = step();
    ASSERT_EQ(io_ports[0x60], 0x00);
    ASSERT_CYCLES(cyc2, 12);
    TEST_END();
}

/* ========== Blokove presuny ========== */

/**
 * @brief Test LDI (16T).
 */
static void test_ldi(void) {
    TEST_BEGIN("LDI");
    setup();
    cpu->hl.w = 0x1000; cpu->de.w = 0x2000; cpu->bc.w = 0x0003;
    test_ram[0x1000] = 0x42;
    test_ram[0] = 0xED; test_ram[1] = 0xA0;
    int cyc = step();
    ASSERT_EQ(test_ram[0x2000], 0x42);
    ASSERT_EQ(cpu->hl.w, 0x1001);
    ASSERT_EQ(cpu->de.w, 0x2001);
    ASSERT_EQ(cpu->bc.w, 0x0002);
    ASSERT_FLAG_SET(Z80_FLAG_PV);  /* BC != 0 */
    ASSERT_FLAG_CLEAR(Z80_FLAG_N);
    ASSERT_FLAG_CLEAR(Z80_FLAG_H);
    ASSERT_CYCLES(cyc, 16);
    TEST_END();

    TEST_BEGIN("LDI (BC=1 -> PV clear)");
    setup();
    cpu->hl.w = 0x1000; cpu->de.w = 0x2000; cpu->bc.w = 0x0001;
    test_ram[0x1000] = 0xFF;
    test_ram[0] = 0xED; test_ram[1] = 0xA0;
    cyc = step();
    ASSERT_EQ(cpu->bc.w, 0x0000);
    ASSERT_FLAG_CLEAR(Z80_FLAG_PV);
    TEST_END();
}

/**
 * @brief Test LDIR (21T repeating, 16T last).
 */
static void test_ldir(void) {
    TEST_BEGIN("LDIR (BC=3, kopiruje 3 bajty)");
    setup();
    cpu->hl.w = 0x1000; cpu->de.w = 0x2000; cpu->bc.w = 0x0003;
    test_ram[0x1000] = 0xAA;
    test_ram[0x1001] = 0xBB;
    test_ram[0x1002] = 0xCC;
    test_ram[0] = 0xED; test_ram[1] = 0xB0;

    /* Prvni iterace - BC=3->2, opakuje (21T) */
    int cyc = step();
    ASSERT_EQ(test_ram[0x2000], 0xAA);
    ASSERT_EQ(cpu->bc.w, 0x0002);
    ASSERT_EQ(cpu->pc, 0);  /* opakuje instrukci */
    ASSERT_CYCLES(cyc, 21);

    /* Druha iterace - BC=2->1 */
    cyc = step();
    ASSERT_EQ(test_ram[0x2001], 0xBB);
    ASSERT_EQ(cpu->bc.w, 0x0001);
    ASSERT_CYCLES(cyc, 21);

    /* Treti iterace - BC=1->0, posledni (16T) */
    cyc = step();
    ASSERT_EQ(test_ram[0x2002], 0xCC);
    ASSERT_EQ(cpu->bc.w, 0x0000);
    ASSERT_EQ(cpu->pc, 2);  /* pokracuje dal */
    ASSERT_FLAG_CLEAR(Z80_FLAG_PV);
    ASSERT_CYCLES(cyc, 16);
    TEST_END();
}

/**
 * @brief Test LDD (16T).
 */
static void test_ldd(void) {
    TEST_BEGIN("LDD");
    setup();
    cpu->hl.w = 0x1002; cpu->de.w = 0x2002; cpu->bc.w = 0x0002;
    test_ram[0x1002] = 0x55;
    test_ram[0] = 0xED; test_ram[1] = 0xA8;
    int cyc = step();
    ASSERT_EQ(test_ram[0x2002], 0x55);
    ASSERT_EQ(cpu->hl.w, 0x1001);
    ASSERT_EQ(cpu->de.w, 0x2001);
    ASSERT_EQ(cpu->bc.w, 0x0001);
    ASSERT_FLAG_SET(Z80_FLAG_PV);
    ASSERT_CYCLES(cyc, 16);
    TEST_END();
}

/**
 * @brief Test LDDR (21T repeating, 16T last).
 */
static void test_lddr(void) {
    TEST_BEGIN("LDDR (BC=2)");
    setup();
    cpu->hl.w = 0x1001; cpu->de.w = 0x2001; cpu->bc.w = 0x0002;
    test_ram[0x1001] = 0xBB;
    test_ram[0x1000] = 0xAA;
    test_ram[0] = 0xED; test_ram[1] = 0xB8;

    int cyc = step();
    ASSERT_EQ(test_ram[0x2001], 0xBB);
    ASSERT_EQ(cpu->bc.w, 0x0001);
    ASSERT_CYCLES(cyc, 21);

    cyc = step();
    ASSERT_EQ(test_ram[0x2000], 0xAA);
    ASSERT_EQ(cpu->bc.w, 0x0000);
    ASSERT_CYCLES(cyc, 16);
    TEST_END();
}

/* ========== Blokove porovnani ========== */

/**
 * @brief Test CPI (16T).
 */
static void test_cpi(void) {
    TEST_BEGIN("CPI (nalezeno)");
    setup();
    cpu->af.h = 0x42;
    cpu->hl.w = 0x5000; cpu->bc.w = 0x0005;
    test_ram[0x5000] = 0x42;
    test_ram[0] = 0xED; test_ram[1] = 0xA1;
    int cyc = step();
    ASSERT_EQ(cpu->hl.w, 0x5001);
    ASSERT_EQ(cpu->bc.w, 0x0004);
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    ASSERT_FLAG_SET(Z80_FLAG_N);
    ASSERT_FLAG_SET(Z80_FLAG_PV);  /* BC != 0 */
    ASSERT_CYCLES(cyc, 16);
    TEST_END();

    TEST_BEGIN("CPI (nenalezeno)");
    setup();
    cpu->af.h = 0x42;
    cpu->hl.w = 0x5000; cpu->bc.w = 0x0001;
    test_ram[0x5000] = 0x00;
    test_ram[0] = 0xED; test_ram[1] = 0xA1;
    cyc = step();
    ASSERT_FLAG_CLEAR(Z80_FLAG_Z);
    ASSERT_FLAG_CLEAR(Z80_FLAG_PV);  /* BC == 0 */
    TEST_END();
}

/**
 * @brief Test CPIR (21T repeating, 16T last).
 */
static void test_cpir(void) {
    TEST_BEGIN("CPIR (nalezeno na 2. pozici)");
    setup();
    cpu->af.h = 0xBB;
    cpu->hl.w = 0x5000; cpu->bc.w = 0x0005;
    test_ram[0x5000] = 0xAA;
    test_ram[0x5001] = 0xBB;
    test_ram[0] = 0xED; test_ram[1] = 0xB1;

    /* 1. iterace: 0xAA != 0xBB, opakuje (21T) */
    int cyc = step();
    ASSERT_FLAG_CLEAR(Z80_FLAG_Z);
    ASSERT_CYCLES(cyc, 21);

    /* 2. iterace: 0xBB == 0xBB, konci (16T) */
    cyc = step();
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    ASSERT_EQ(cpu->bc.w, 0x0003);
    ASSERT_CYCLES(cyc, 16);
    TEST_END();
}

/**
 * @brief Test CPD (16T).
 */
static void test_cpd(void) {
    TEST_BEGIN("CPD");
    setup();
    cpu->af.h = 0x42;
    cpu->hl.w = 0x5005; cpu->bc.w = 0x0003;
    test_ram[0x5005] = 0x42;
    test_ram[0] = 0xED; test_ram[1] = 0xA9;
    int cyc = step();
    ASSERT_EQ(cpu->hl.w, 0x5004);
    ASSERT_EQ(cpu->bc.w, 0x0002);
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    ASSERT_CYCLES(cyc, 16);
    TEST_END();
}

/* ========== Blokove I/O ========== */

/**
 * @brief Test INI (16T).
 */
static void test_ini(void) {
    TEST_BEGIN("INI");
    setup();
    cpu->bc.w = 0x0310;  /* B=3, C=0x10 */
    cpu->hl.w = 0x8000;
    io_read_data[0x10] = 0x42;
    test_ram[0] = 0xED; test_ram[1] = 0xA2;
    int cyc = step();
    ASSERT_EQ(test_ram[0x8000], 0x42);
    ASSERT_EQ(cpu->hl.w, 0x8001);
    ASSERT_EQ(cpu->bc.h, 0x02);  /* B-- */
    ASSERT_CYCLES(cyc, 16);
    TEST_END();
}

/**
 * @brief Test OUTI (16T).
 */
static void test_outi(void) {
    TEST_BEGIN("OUTI");
    setup();
    cpu->bc.w = 0x0220;  /* B=2, C=0x20 */
    cpu->hl.w = 0x8000;
    test_ram[0x8000] = 0xAB;
    test_ram[0] = 0xED; test_ram[1] = 0xA3;
    int cyc = step();
    /* B se dekrementuje pred I/O */
    ASSERT_EQ(cpu->bc.h, 0x01);
    /* Port = B_novy:C = 0x0120 */
    ASSERT_EQ(io_ports[0x20], 0xAB);
    ASSERT_EQ(cpu->hl.w, 0x8001);
    ASSERT_CYCLES(cyc, 16);
    TEST_END();
}

/**
 * @brief Test INIR (21T repeating, 16T last).
 */
static void test_inir(void) {
    TEST_BEGIN("INIR (B=2)");
    setup();
    cpu->bc.w = 0x0230;  /* B=2, C=0x30 */
    cpu->hl.w = 0x9000;
    io_read_data[0x30] = 0x55;
    test_ram[0] = 0xED; test_ram[1] = 0xB2;

    int cyc = step();
    ASSERT_EQ(cpu->bc.h, 0x01);
    ASSERT_CYCLES(cyc, 21);

    cyc = step();
    ASSERT_EQ(cpu->bc.h, 0x00);
    ASSERT_CYCLES(cyc, 16);
    TEST_END();
}

/**
 * @brief Test IND (16T).
 */
static void test_ind(void) {
    TEST_BEGIN("IND");
    setup();
    cpu->bc.w = 0x0340;  /* B=3, C=0x40 */
    cpu->hl.w = 0x8005;
    io_read_data[0x40] = 0x77;
    test_ram[0] = 0xED; test_ram[1] = 0xAA;
    int cyc = step();
    ASSERT_EQ(test_ram[0x8005], 0x77);
    ASSERT_EQ(cpu->hl.w, 0x8004);
    ASSERT_EQ(cpu->bc.h, 0x02);
    ASSERT_CYCLES(cyc, 16);
    TEST_END();
}

/**
 * @brief Test OUTD (16T).
 */
static void test_outd(void) {
    TEST_BEGIN("OUTD");
    setup();
    cpu->bc.w = 0x0250;  /* B=2, C=0x50 */
    cpu->hl.w = 0x8005;
    test_ram[0x8005] = 0xCC;
    test_ram[0] = 0xED; test_ram[1] = 0xAB;
    int cyc = step();
    ASSERT_EQ(cpu->bc.h, 0x01);
    ASSERT_EQ(io_ports[0x50], 0xCC);
    ASSERT_EQ(cpu->hl.w, 0x8004);
    ASSERT_CYCLES(cyc, 16);
    TEST_END();
}

/* ========== Vstupni bod ========== */

/**
 * @brief Spusti vsechny testy blokovych a I/O instrukci.
 */
void test_block_io_all(void) {
    test_in_out_basic();
    test_in_r_c();
    test_out_c_r();
    test_ldi();
    test_ldir();
    test_ldd();
    test_lddr();
    test_cpi();
    test_cpir();
    test_cpd();
    test_ini();
    test_outi();
    test_inir();
    test_ind();
    test_outd();
}
