/**
 * @file test_halt_pc.c
 * @brief Testy hodnoty PC behem HALT a navratove adresy pri INT/NMI.
 *
 * Realny Z80 drzi behem HALT PC = X = adresa instrukce HALT (instrukce
 * se re-fetchuje cyklicky kvuli DRAM refreshi). PC se inkrementuje az
 * tesne pred skokem do INT/NMI rutiny, takze na zasobnik se ulozi X+1
 * (adresa za HALT).
 *
 * Pokriva:
 *   - po HALT je cpu->pc = adresa HALT (X), ne X+1
 *   - PC zustava na X i pres dalsi step (halt re-fetch smycka)
 *   - NMI z HALT: navratova adresa na stacku = X+1, PC = 0x0066
 *   - INT IM1 z HALT: navratova adresa = X+1, PC = 0x0038
 *   - INT IM2 z HALT: navratova adresa = X+1
 *   - INT mimo HALT: navratova adresa neni dotcena was_halted inkrementem
 */

#include "test_framework.h"

/**
 * @brief Po HALT musi cpu->pc ukazovat na adresu instrukce HALT (X).
 */
static void test_halt_pc_holds_halt_address(void) {
    TEST_BEGIN("PC after HALT holds the HALT address");
    setup();
    cpu->pc = 0x1000;
    test_ram[0x1000] = 0x76;  /* HALT */
    (void)step();
    ASSERT_TRUE(cpu->halted);
    ASSERT_EQ(cpu->pc, 0x1000);  /* X, ne X+1 */
    TEST_END();
}

/**
 * @brief PC zustava na X i pres dalsi kroky v HALT smycce.
 */
static void test_halt_pc_stays_during_loop(void) {
    TEST_BEGIN("PC stays at HALT address across halt loop steps");
    setup();
    cpu->pc = 0x2000;
    test_ram[0x2000] = 0x76;  /* HALT */
    (void)step();             /* vykona HALT */
    (void)step();             /* halt smycka - bez preruseni */
    (void)step();
    ASSERT_TRUE(cpu->halted);
    ASSERT_EQ(cpu->pc, 0x2000);
    TEST_END();
}

/**
 * @brief NMI z HALT ulozi X+1 a skoci na 0x0066.
 */
static void test_halt_nmi_return_addr(void) {
    TEST_BEGIN("NMI from HALT pushes X+1, jumps to 0x0066");
    setup();
    cpu->sp = 0xFFFE;
    cpu->pc = 0x1000;
    test_ram[0x1000] = 0x76;  /* HALT */
    test_ram[0x0066] = 0x00;  /* NMI vektor */
    (void)step();             /* HALT - PC = 0x1000 */
    ASSERT_EQ(cpu->pc, 0x1000);
    z80_nmi(cpu);
    (void)step();
    ASSERT_FALSE(cpu->halted);
    /* step() po akceptaci NMI vykona i NOP na vektoru 0x0066 -> PC = 0x0067 */
    ASSERT_EQ(cpu->pc, 0x0067);
    /* Navratova adresa na stacku = X+1 = 0x1001 (klicova asserce fixu) */
    ASSERT_EQ(test_ram[cpu->sp], 0x01);
    ASSERT_EQ(test_ram[cpu->sp + 1], 0x10);
    TEST_END();
}

/**
 * @brief INT IM1 z HALT ulozi X+1 a skoci na 0x0038.
 */
static void test_halt_int_im1_return_addr(void) {
    TEST_BEGIN("INT IM1 from HALT pushes X+1, jumps to 0x0038");
    setup();
    cpu->sp = 0xFFFE;
    cpu->iff1 = 1;
    cpu->im = 1;
    cpu->pc = 0x1000;
    test_ram[0x1000] = 0x76;  /* HALT */
    test_ram[0x0038] = 0x00;
    (void)step();
    ASSERT_EQ(cpu->pc, 0x1000);
    z80_irq(cpu, 0xFF);
    (void)step();
    ASSERT_FALSE(cpu->halted);
    /* step() po akceptaci INT vykona i NOP na vektoru 0x0038 -> PC = 0x0039 */
    ASSERT_EQ(cpu->pc, 0x0039);
    /* Navratova adresa na stacku = X+1 = 0x1001 (klicova asserce fixu) */
    ASSERT_EQ(test_ram[cpu->sp], 0x01);
    ASSERT_EQ(test_ram[cpu->sp + 1], 0x10);
    TEST_END();
}

/**
 * @brief INT IM2 z HALT ulozi X+1 (vektor pres I:vec).
 */
static void test_halt_int_im2_return_addr(void) {
    TEST_BEGIN("INT IM2 from HALT pushes X+1");
    setup();
    cpu->sp = 0xFFFE;
    cpu->iff1 = 1;
    cpu->im = 2;
    cpu->i = 0x20;            /* high byte vektorove tabulky */
    cpu->pc = 0x3000;
    test_ram[0x3000] = 0x76;  /* HALT */
    /* IM2 vektor: adresa (I<<8 | (vec & 0xFE)) = 0x2080, handler tam */
    test_ram[0x2080] = 0x00;
    test_ram[0x2081] = 0x40;  /* handler na 0x4000 */
    (void)step();
    ASSERT_EQ(cpu->pc, 0x3000);
    z80_irq(cpu, 0x80);
    (void)step();
    ASSERT_FALSE(cpu->halted);
    /* Navratova adresa na stacku = X+1 = 0x3001 */
    ASSERT_EQ(test_ram[cpu->sp], 0x01);
    ASSERT_EQ(test_ram[cpu->sp + 1], 0x30);
    TEST_END();
}

/**
 * @brief INT mimo HALT - navratova adresa NESMI byt posunuta was_halted
 *        inkrementem (regression guard).
 */
static void test_int_outside_halt_return_addr(void) {
    TEST_BEGIN("INT outside HALT pushes normal return address");
    setup();
    cpu->sp = 0xFFFE;
    cpu->iff1 = 1;
    cpu->im = 1;
    cpu->pc = 0x5000;
    test_ram[0x5000] = 0x00;  /* NOP */
    test_ram[0x0038] = 0x00;  /* NOP na vektoru */
    /*
     * INT je pending na zacatku step() - akceptuje se drive nez se vykona
     * instrukce na 0x5000, takze navratova adresa je aktualni PC = 0x5000
     * (BEZ was_halted +1, protoze CPU nebylo v HALT). Pak step() vykona
     * NOP na vektoru 0x0038 -> PC = 0x0039.
     */
    z80_irq(cpu, 0xFF);
    (void)step();
    ASSERT_FALSE(cpu->halted);
    ASSERT_EQ(cpu->pc, 0x0039);
    /* Navratova adresa = 0x5000 (bez was_halted posunu) - regression guard */
    ASSERT_EQ(test_ram[cpu->sp], 0x00);
    ASSERT_EQ(test_ram[cpu->sp + 1], 0x50);
    TEST_END();
}

/* ========== Hlavni runner ========== */

void test_halt_pc_all(void) {
    test_halt_pc_holds_halt_address();
    test_halt_pc_stays_during_loop();
    test_halt_nmi_return_addr();
    test_halt_int_im1_return_addr();
    test_halt_int_im2_return_addr();
    test_int_outside_halt_return_addr();
}
