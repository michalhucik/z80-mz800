/**
 * @file test_r_register.c
 * @brief Regresni testy inkrementu registru R behem HALT a pri prijeti INT/NMI.
 *
 * Do v0.3.1 jadro nezvysovalo R pri opakovanem fetchi HALT ani pri prijeti
 * maskovatelneho preruseni a NMI (z80ex oboji dela). Kod, ktery cte LD A,R
 * po HALT cekajicim na preruseni (napr. loader odvozujici desifrovaci klic
 * z R), tak videl jinou hodnotu nez na realnem Z80.
 *
 * Ocekavane chovani:
 *   - kazdy opakovany fetch HALT (po 4 T) je M1 cyklus -> R += 1
 *   - prijeti INT (IM 0/1/2) i NMI -> R += 1
 *   - inkrementuje se jen dolnich 7 bitu, bit 7 se zachova
 *   - neprijate INT (DI) R navic nezvysi
 *
 * Pokryta je per-step cesta (z80_step) i davkova cesta (z80_execute).
 * Pozn.: step() po prijeti preruseni vykona i instrukci na vektoru (NOP),
 * ktera prida dalsi +1.
 */

#include "test_framework.h"

/**
 * @brief Pripravi HALT na 0x0100, IM 1, EI stav (iff1 = iff2 = 1), R = r0.
 * @param r0 Pocatecni hodnota registru R.
 */
static void prepare_halt(u8 r0) {
    test_ram[0x0100] = 0x76;  /* HALT */
    cpu->pc = 0x0100;
    cpu->sp = 0x8000;
    cpu->im = 1;
    cpu->iff1 = 1;
    cpu->iff2 = 1;
    cpu->r = r0;
}

/**
 * @brief HALT: fetch HALT + kazde opakovani (4 T) zvysi R o 1 (per-step).
 */
static void test_halt_refetch_increments_r_step(void) {
    TEST_BEGIN("HALT re-fetch increments R (per-step)");
    setup();
    prepare_halt(0x10);
    (void)step();             /* fetch HALT: R = 0x11 */
    ASSERT_TRUE(cpu->halted);
    ASSERT_EQ(cpu->r, 0x11);
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(step(), 4);
    }
    ASSERT_TRUE(cpu->halted);
    ASSERT_EQ(cpu->r, 0x11 + 10);
    TEST_END();
}

/**
 * @brief HALT: davkova cesta z80_execute - R roste o 1 za kazde 4 T v HALT.
 */
static void test_halt_refetch_increments_r_batch(void) {
    TEST_BEGIN("HALT re-fetch increments R (batch)");
    setup();
    cpu->iff1 = 0;            /* bez preruseni, jen cekani v HALT */
    prepare_halt(0x20);
    int cycles = 0;
    while (cycles < 4 + 40) {
        cycles += z80_execute(cpu, 4);
    }
    ASSERT_TRUE(cpu->halted);
    ASSERT_EQ(cycles, 44);
    ASSERT_EQ(cpu->r, 0x20 + 1 + 10);
    TEST_END();
}

/**
 * @brief HALT: inkrement pretece jen v dolnich 7 bitech, bit 7 zustane.
 */
static void test_halt_refetch_keeps_bit7(void) {
    TEST_BEGIN("HALT re-fetch keeps R bit 7");
    setup();
    prepare_halt(0xFE);
    (void)step();             /* HALT: 0xFE -> 0xFF */
    (void)step();             /* 0xFF -> 0x80 (bit 7 drzi) */
    ASSERT_EQ(cpu->r, 0x80);
    TEST_END();
}

/**
 * @brief Prijeti INT (IM 1) z HALT zvysi R o 1 navic k opakovanym fetchum.
 */
static void test_int_ack_im1_from_halt_increments_r(void) {
    TEST_BEGIN("INT IM1 ack from HALT increments R");
    setup();
    prepare_halt(0x30);
    test_ram[0x0038] = 0x00;  /* NOP na vektoru */
    (void)step();             /* HALT: R = 0x31 */
    (void)step();             /* opakovani: R = 0x32 */
    z80_irq(cpu, 0xFF);
    (void)step();             /* INT ack (+1) + NOP na 0x0038 (+1) */
    ASSERT_FALSE(cpu->halted);
    ASSERT_EQ(cpu->pc, 0x0039);
    ASSERT_EQ(cpu->r, 0x34);
    TEST_END();
}

/**
 * @brief Prijeti INT mimo HALT (IM 2) zvysi R o 1.
 */
static void test_int_ack_im2_increments_r(void) {
    TEST_BEGIN("INT IM2 ack outside HALT increments R");
    setup();
    cpu->pc = 0x0200;
    cpu->sp = 0x8000;
    cpu->im = 2;
    cpu->i = 0x40;
    cpu->iff1 = 1;
    cpu->iff2 = 1;
    cpu->r = 0x45;
    test_ram[0x0200] = 0x00;  /* NOP */
    test_ram[0x40FE] = 0x00;  /* IM2 tabulka: handler na 0x5000 */
    test_ram[0x40FF] = 0x50;
    test_ram[0x5000] = 0x00;  /* NOP v handleru */
    (void)step();             /* NOP: R = 0x46 */
    z80_irq(cpu, 0xFE);
    (void)step();             /* INT ack (+1) + NOP na 0x5000 (+1) */
    ASSERT_EQ(cpu->pc, 0x5001);
    ASSERT_EQ(cpu->r, 0x48);
    TEST_END();
}

/**
 * @brief Prijeti NMI z HALT zvysi R o 1 (vcetne preteceni do bitu 0-6).
 */
static void test_nmi_ack_increments_r(void) {
    TEST_BEGIN("NMI ack from HALT increments R");
    setup();
    prepare_halt(0x7F);
    test_ram[0x0066] = 0x00;  /* NOP na NMI vektoru */
    (void)step();             /* HALT: 0x7F -> 0x00 */
    ASSERT_EQ(cpu->r, 0x00);
    z80_nmi(cpu);
    (void)step();             /* NMI ack (+1) + NOP na 0x0066 (+1) */
    ASSERT_EQ(cpu->pc, 0x0067);
    ASSERT_EQ(cpu->r, 0x02);
    TEST_END();
}

/**
 * @brief Neprijate INT (DI) R navic nezvysi - jen opakovany fetch HALT.
 */
static void test_int_not_accepted_no_extra_r(void) {
    TEST_BEGIN("INT not accepted under DI adds no extra R increment");
    setup();
    prepare_halt(0x50);
    cpu->iff1 = 0;
    cpu->iff2 = 0;
    (void)step();             /* HALT: R = 0x51 */
    z80_irq(cpu, 0xFF);
    (void)step();             /* jen opakovani HALT: R = 0x52 */
    ASSERT_TRUE(cpu->halted);
    ASSERT_EQ(cpu->r, 0x52);
    TEST_END();
}

/**
 * @brief Davkova cesta: prijeti INT v z80_execute zvysi R o 1
 *        (pak se vykona jeste NOP na 0x0038, ten prida dalsi 1).
 */
static void test_int_ack_batch_increments_r(void) {
    TEST_BEGIN("INT IM1 ack in batch core increments R");
    setup();
    cpu->pc = 0x0200;
    cpu->sp = 0x8000;
    cpu->im = 1;
    cpu->iff1 = 1;
    cpu->iff2 = 1;
    cpu->r = 0x10;
    test_ram[0x0200] = 0x00;  /* NOP */
    test_ram[0x0038] = 0x00;  /* NOP na vektoru */
    z80_irq(cpu, 0xFF);
    z80_execute(cpu, 1);      /* vstupni check prijme INT, pak NOP na 0x0038 */
    ASSERT_EQ(cpu->pc, 0x0039);
    ASSERT_EQ(cpu->r, 0x12);  /* +1 INT ack, +1 NOP */
    TEST_END();
}

/* ========== Hlavni runner ========== */

/**
 * @brief Spusti vsechny testy registru R.
 */
void test_r_register_all(void) {
    test_halt_refetch_increments_r_step();
    test_halt_refetch_increments_r_batch();
    test_halt_refetch_keeps_bit7();
    test_int_ack_im1_from_halt_increments_r();
    test_int_ack_im2_increments_r();
    test_nmi_ack_increments_r();
    test_int_not_accepted_no_extra_r();
    test_int_ack_batch_increments_r();
}
