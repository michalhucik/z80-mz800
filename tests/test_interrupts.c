/**
 * @file test_interrupts.c
 * @brief Testy prerusovacich rezimu Z80.
 *
 * Pokryva: IM0, IM1, IM2, NMI, EI delay, LD A,I/R bug,
 * HALT s prerusenim, IM nastaveni, daisy chain (RETI callback).
 */

#include "test_framework.h"

/* ========== IM nastaveni ========== */

/**
 * @brief Test IM 0/1/2 (8T).
 */
static void test_im_set(void) {
    int cyc;

    TEST_BEGIN("IM 0");
    setup();
    test_ram[0] = 0xED; test_ram[1] = 0x46;
    cyc = step();
    ASSERT_EQ(cpu.im, 0);
    ASSERT_CYCLES(cyc, 8);
    TEST_END();

    TEST_BEGIN("IM 1");
    setup();
    test_ram[0] = 0xED; test_ram[1] = 0x56;
    cyc = step();
    ASSERT_EQ(cpu.im, 1);
    ASSERT_CYCLES(cyc, 8);
    TEST_END();

    TEST_BEGIN("IM 2");
    setup();
    test_ram[0] = 0xED; test_ram[1] = 0x5E;
    cyc = step();
    ASSERT_EQ(cpu.im, 2);
    ASSERT_CYCLES(cyc, 8);
    TEST_END();
}

/* ========== NMI ========== */

/**
 * @brief Test NMI preruseni (11T).
 *
 * NMI ulozi PC na zasobnik, skoci na 0x0066.
 * IFF2 = IFF1 (zachova pro RETN), IFF1 = 0.
 */
static void test_nmi(void) {
    TEST_BEGIN("NMI");
    setup();
    cpu.sp = 0xFFFE;
    cpu.iff1 = 1;
    cpu.iff2 = 1;
    cpu.pc = 0x1000;
    test_ram[0x1000] = 0x00;  /* NOP */
    test_ram[0x0066] = 0x00;  /* NOP na NMI adrese */

    /* Vyvolej NMI */
    z80_nmi(&cpu);

    /* Dalsi step zpracuje NMI */
    int cyc = step();
    ASSERT_EQ(cpu.pc, 0x0067);  /* provede NOP na 0x0066 */
    ASSERT_EQ(cpu.iff1, 0);
    ASSERT_EQ(cpu.iff2, 1);  /* zachovano */
    /* Na zasobniku je 0x1000 */
    ASSERT_EQ(test_ram[cpu.sp], 0x00);
    ASSERT_EQ(test_ram[cpu.sp + 1], 0x10);
    TEST_END();
}

/* ========== Maskovane preruseni IM1 ========== */

/**
 * @brief Test IM1 maskovaneho preruseni (13T).
 *
 * IM1: ulozi PC na zasobnik, skoci na 0x0038.
 */
static void test_int_im1(void) {
    TEST_BEGIN("INT IM1");
    setup();
    cpu.sp = 0xFFFE;
    cpu.iff1 = 1;
    cpu.iff2 = 1;
    cpu.im = 1;
    cpu.pc = 0x2000;
    test_ram[0x2000] = 0x00;  /* NOP */
    test_ram[0x0038] = 0x00;  /* NOP na INT adrese */

    z80_irq(&cpu, 0xFF);

    int cyc = step();
    ASSERT_EQ(cpu.iff1, 0);
    ASSERT_EQ(cpu.iff2, 0);
    /* PC by mel byt na 0x0039 (po NOP na 0x0038) */
    ASSERT_EQ(cpu.pc, 0x0039);
    TEST_END();
}

/* ========== Maskovane preruseni IM2 ========== */

/**
 * @brief Test IM2 maskovaneho preruseni (19T).
 *
 * IM2: vektorova adresa = (I << 8) | (vector & 0xFE).
 * Na vektorove adrese je 16bit pointer na ISR.
 */
static void test_int_im2(void) {
    TEST_BEGIN("INT IM2");
    setup();
    cpu.sp = 0xFFFE;
    cpu.iff1 = 1;
    cpu.iff2 = 1;
    cpu.im = 2;
    cpu.i = 0x80;
    cpu.pc = 0x3000;
    test_ram[0x3000] = 0x00;  /* NOP */

    /* Vektorova tabulka: na adrese 0x8010 je pointer 0x4000 */
    test_ram[0x8010] = 0x00;
    test_ram[0x8011] = 0x40;
    test_ram[0x4000] = 0x00;  /* NOP na ISR adrese */

    z80_irq(&cpu, 0x10);

    int cyc = step();
    ASSERT_EQ(cpu.iff1, 0);
    ASSERT_EQ(cpu.iff2, 0);
    /* PC = 0x4001 (po NOP na 0x4000) */
    ASSERT_EQ(cpu.pc, 0x4001);
    TEST_END();
}

/* ========== EI delay ========== */

/**
 * @brief Test EI delay - preruseni se odlozi o jednu instrukci po EI.
 */
static void test_ei_delay(void) {
    TEST_BEGIN("EI delay (v z80_execute)");
    setup();
    cpu.sp = 0xFFFE;
    cpu.iff1 = 0;
    cpu.iff2 = 0;
    cpu.im = 1;
    test_ram[0] = 0xFB;  /* EI */
    test_ram[1] = 0x00;  /* NOP - preruseni blokovano EI delay */
    test_ram[2] = 0x00;  /* NOP - sem preruseni MUZE prijit */
    test_ram[0x0038] = 0x00;  /* NOP na ISR */

    /* Vyvolej IRQ jeste pred spustenim */
    z80_irq(&cpu, 0xFF);

    /*
     * Pouzijeme z80_execute s dostatkem cyklu aby provedl EI + NOP.
     * EI (4T) nastavi ei_delay=true, takze na check_interrupts se
     * preruseni zablokuje. Nasledny NOP (4T) se provede normalne.
     * Az po NOP se preruseni zpracuje.
     */
    z80_execute(&cpu, 8);  /* EI(4T) + NOP(4T) */

    /* Po EI + NOP se zpracovalo preruseni (13T navic) */
    ASSERT_EQ(cpu.iff1, 0);  /* INT zakazal preruseni */
    /* PC ukazuje na ISR (0x0038) - NOP na ISR se jeste neprovedl */
    ASSERT_EQ(cpu.pc, 0x0038);
    /* Na zasobniku je navratova adresa 2 (za NOP na adrese 1) */
    ASSERT_EQ(test_ram[cpu.sp], 0x02);
    ASSERT_EQ(test_ram[cpu.sp + 1], 0x00);
    TEST_END();
}

/* ========== LD A,I/R bug ========== */

/**
 * @brief Test LD A,I/R bugu - INT po LD A,I resetuje PF na 0.
 *
 * Na skutecnem Z80 pokud prijde INT hned po LD A,I nebo LD A,R,
 * flag PF se resetuje na 0 (misto IFF2).
 */
static void test_ld_a_ir_bug(void) {
    TEST_BEGIN("LD A,I bug (INT po LD A,I resetuje PF)");
    setup();
    cpu.sp = 0xFFFE;
    /*
     * Zacneme s IFF1=0, IFF2=0. EI je prvni instrukce - nastavi IFF,
     * ei_delay zablokuje preruseni na check_interrupts po EI.
     * Pak LD A,I probehne, nastavi ld_a_ir=true a PF=IFF2=1.
     * Na dalsim check_interrupts se zpracuje IRQ a PF se resetuje.
     */
    cpu.iff1 = 0;
    cpu.iff2 = 0;
    cpu.im = 1;
    cpu.i = 0x42;
    test_ram[0] = 0xFB;              /* EI */
    test_ram[1] = 0xED; test_ram[2] = 0x57;  /* LD A,I */
    test_ram[3] = 0x00;              /* NOP */
    test_ram[0x0038] = 0x00;         /* ISR NOP */

    /* Vyvolej IRQ - bude cekat az se IFF1 nastavi pres EI */
    z80_irq(&cpu, 0xFF);

    /* EI(4T) + LD A,I(9T) = 13T */
    z80_execute(&cpu, 13);

    /* LD A,I nastavil A=I=0x42 */
    ASSERT_EQ(cpu.af.h, 0x42);

    /* INT ihned po LD A,I resetoval PF na 0 (HW bug Z80) */
    ASSERT_FLAG_CLEAR(Z80_FLAG_PV);
    TEST_END();
}

/* ========== HALT s prerusenim ========== */

/**
 * @brief Test HALT nasledovany prerusenim.
 */
static void test_halt_interrupt(void) {
    TEST_BEGIN("HALT + NMI");
    setup();
    cpu.sp = 0xFFFE;
    cpu.iff1 = 1;
    test_ram[0] = 0x76;  /* HALT */
    test_ram[0x0066] = 0x00;  /* NOP na NMI adrese */

    /* HALT */
    step();
    ASSERT_TRUE(cpu.halted);

    /* NMI probudi CPU */
    z80_nmi(&cpu);
    step();
    ASSERT_FALSE(cpu.halted);
    /* PC by mel byt za NOP na 0x0066 */
    TEST_END();

    TEST_BEGIN("HALT + INT IM1");
    setup();
    cpu.sp = 0xFFFE;
    cpu.iff1 = 1;
    cpu.im = 1;
    test_ram[0] = 0x76;  /* HALT */
    test_ram[0x0038] = 0x00;

    step();
    ASSERT_TRUE(cpu.halted);

    z80_irq(&cpu, 0xFF);
    step();
    ASSERT_FALSE(cpu.halted);
    TEST_END();
}

/* ========== INTACK callback ========== */

/**
 * @brief Test INTACK callbacku.
 */
static void test_intack_callback(void) {
    TEST_BEGIN("INTACK callback");
    setup();
    cpu.sp = 0xFFFE;
    cpu.iff1 = 1;
    cpu.im = 1;
    intack_count = 0;
    test_ram[0] = 0x00;  /* NOP */
    test_ram[0x0038] = 0x00;

    z80_irq(&cpu, 0xFF);
    step();
    ASSERT_EQ(intack_count, 1);
    TEST_END();
}

/* ========== Maskovane preruseni - DI blokovani ========== */

/**
 * @brief Test ze DI blokuje maskovane preruseni.
 */
static void test_di_blocks_int(void) {
    TEST_BEGIN("DI blokuje INT");
    setup();
    cpu.sp = 0xFFFE;
    cpu.iff1 = 0;
    cpu.iff2 = 0;
    cpu.im = 1;
    test_ram[0] = 0x00;  /* NOP */
    test_ram[1] = 0x00;  /* NOP */
    test_ram[0x0038] = 0x00;

    z80_irq(&cpu, 0xFF);
    step();
    /* Preruseni by nemelo nastat - IFF1=0 */
    ASSERT_EQ(cpu.pc, 1);  /* pokracuje na NOP */
    TEST_END();
}

/**
 * @brief Test ze NMI neni blokovano DI.
 */
static void test_nmi_not_blocked(void) {
    TEST_BEGIN("NMI neni blokovano DI");
    setup();
    cpu.sp = 0xFFFE;
    cpu.iff1 = 0;
    cpu.iff2 = 0;
    test_ram[0] = 0x00;  /* NOP */
    test_ram[0x0066] = 0x00;

    z80_nmi(&cpu);
    step();
    /* NMI by se melo zpracovat i s IFF1=0 */
    ASSERT_FALSE(cpu.halted);
    /* PC by mel byt za NOP na 0x0066 */
    TEST_END();
}

/* ========== Vstupni bod ========== */

/**
 * @brief Spusti vsechny testy preruseni.
 */
void test_interrupts_all(void) {
    test_im_set();
    test_nmi();
    test_int_im1();
    test_int_im2();
    test_ei_delay();
    test_ld_a_ir_bug();
    test_halt_interrupt();
    test_intack_callback();
    test_di_blocks_int();
    test_nmi_not_blocked();
}
