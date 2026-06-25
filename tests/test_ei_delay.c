/**
 * @file test_ei_delay.c
 * @brief Regresni testy pro EI-delay v per-step variante jadra.
 *
 * Po instrukci EI se maskovatelne preruseni smi prijmout az po NASLEDUJICI
 * instrukci (EI-delay = 1 instrukce). Per-step jadro (z80_step) drzelo
 * ei_delay spravne pres jeden krok jen kdyz epilog check_interrupts ei_delay
 * NEvycisti - jinak po EI step zustane ei_delay false a vstupni check
 * dalsiho stepu prijme preruseni JESTE PRED instrukci nasledujici po EI.
 *
 * Realny dopad: u sekvence "EI; LD SP,nn" by se INT prijal pred prepnutim
 * zasobniku a push navratove adresy by sel na STARY (neplatny) zasobnik ->
 * poskozeni pameti.
 *
 * Pokriva:
 *   - ei_delay drzi po EI, smaze se po nasledujici instrukci (lifetime)
 *   - retezeni EI;EI - delay drzi pres obe, smaze az po instrukci po nich
 *   - pending INT po EI je deferovan o jednu instrukci (per-step interni)
 *   - EI;LD SP regrese: INT se prijme az po LD SP, push na NOVY zasobnik
 *   - batch (z80_execute) si epilog clear ei_delay PONECHAVA (beze zmeny)
 *
 * ZEXALL nacasovani prijeti preruseni netestuje, proto je tento test nutny.
 */

#include "test_framework.h"

/**
 * @brief Po EI drzi ei_delay; po nasledujici instrukci je smazan.
 */
static void test_ei_delay_lifetime(void) {
    TEST_BEGIN("EI-delay lifetime: set after EI, cleared after next insn");
    setup();
    cpu->pc = 0x0000;
    test_ram[0x0000] = 0xFB;  /* EI */
    test_ram[0x0001] = 0x00;  /* NOP */
    (void)step();             /* EI */
    ASSERT_TRUE(cpu->ei_delay);
    (void)step();             /* NOP */
    ASSERT_FALSE(cpu->ei_delay);
    TEST_END();
}

/**
 * @brief EI;EI - delay drzi pres obe EI, smaze se az po instrukci po nich.
 */
static void test_ei_ei_chain(void) {
    TEST_BEGIN("EI-delay survives EI;EI, cleared after the following insn");
    setup();
    cpu->pc = 0x0000;
    test_ram[0x0000] = 0xFB;  /* EI */
    test_ram[0x0001] = 0xFB;  /* EI */
    test_ram[0x0002] = 0x00;  /* NOP */
    (void)step();             /* EI */
    ASSERT_TRUE(cpu->ei_delay);
    (void)step();             /* EI */
    ASSERT_TRUE(cpu->ei_delay);
    (void)step();             /* NOP */
    ASSERT_FALSE(cpu->ei_delay);
    TEST_END();
}

/**
 * @brief Pending INT po EI je deferovan o jednu instrukci (per-step interni).
 *
 * Po ES step se INT NEsmi prijmout (pc zustane na nasledujici instrukci, zadny
 * push); az po nasledujici instrukci (NOP) se prijme (IM1 -> 0x0038).
 */
static void test_ei_int_deferred_one_insn(void) {
    TEST_BEGIN("pending INT after EI is deferred by exactly one instruction");
    setup();
    cpu->pc = 0x0000;
    cpu->sp = 0xFF00;
    cpu->im = 1;
    test_ram[0x0000] = 0xFB;  /* EI */
    test_ram[0x0001] = 0x00;  /* NOP */
    test_ram[0x0038] = 0x00;  /* IM1 handler */
    z80_int(cpu);             /* maskovatelne preruseni pending */

    (void)step();             /* EI - INT NESMI byt prijat */
    ASSERT_TRUE(cpu->ei_delay);
    ASSERT_EQ(cpu->pc, 0x0001);   /* na NOP, ne 0x0038 */
    ASSERT_EQ(cpu->sp, 0xFF00);   /* zadny push */

    (void)step();             /* NOP - teprve ted se INT prijme */
    ASSERT_EQ(cpu->pc, 0x0038);
    ASSERT_EQ(cpu->sp, 0xFEFE);   /* push probehl */
    ASSERT_EQ(test_ram[0xFEFE], 0x02);  /* navratova adresa = za NOP */
    ASSERT_EQ(test_ram[0xFEFF], 0x00);
    TEST_END();
}

/**
 * @brief Regrese (gun-runner): "EI; LD SP,nn" s pending IM1.
 *
 * INT se SMI prijmout az po LD SP, takze push jde na NOVY zasobnik (0x9000),
 * ne na stary (0xC000). Pred fixem: po EI je ei_delay v per-step uz false,
 * INT se prijme pred LD SP a push jde na stary zasobnik 0xC000.
 */
static void test_ei_ldsp_int_to_new_stack(void) {
    TEST_BEGIN("EI; LD SP,nn - deferred INT pushes to the NEW stack");
    setup();
    cpu->pc = 0x0000;
    cpu->sp = 0xC000;             /* stary zasobnik */
    cpu->im = 1;
    test_ram[0x0000] = 0xFB;                       /* EI */
    test_ram[0x0001] = 0x31; test_ram[0x0002] = 0x00; test_ram[0x0003] = 0x90; /* LD SP,0x9000 */
    test_ram[0x0038] = 0x00;                       /* IM1 handler */
    z80_int(cpu);

    (void)step();                 /* EI */
    ASSERT_TRUE(cpu->ei_delay);
    ASSERT_EQ(cpu->sp, 0xC000);   /* zadny push pred LD SP */

    (void)step();                 /* LD SP,0x9000 -> INT az v epilogu */
    ASSERT_EQ(cpu->pc, 0x0038);   /* skok do ISR */
    ASSERT_EQ(cpu->sp, 0x8FFE);   /* push na NOVY zasobnik (0x9000-2), NE 0xBFFE */
    ASSERT_EQ(test_ram[0x8FFE], 0x04);  /* navratova adresa = za LD SP,nn */
    ASSERT_EQ(test_ram[0x8FFF], 0x00);
    TEST_END();
}

/**
 * @brief Batch (z80_execute) si epilog clear ei_delay PONECHAVA.
 *
 * Dokumentuje, ze fix je ciste per-step a batch varianta je beze zmeny:
 * vstupni clear bezi 1x na davku, proto epilog musi ei_delay v davce smazat.
 */
static void test_batch_epilog_clears_ei_delay(void) {
    TEST_BEGIN("batch core still clears ei_delay in the epilog");
    setup();
    cpu->pc = 0x0000;
    test_ram[0x0000] = 0xFB;  /* EI */
    test_ram[0x0001] = 0x00;  /* NOP */
    z80_execute(cpu, 4);      /* batch: vykona jen EI (4 T) */
    ASSERT_FALSE(cpu->ei_delay);
    TEST_END();
}

/**
 * @brief Spusti vsechny EI-delay testy.
 */
void test_ei_delay_all(void) {
    test_ei_delay_lifetime();
    test_ei_ei_chain();
    test_ei_int_deferred_one_insn();
    test_ei_ldsp_int_to_new_stack();
    test_batch_epilog_clears_ei_delay();
}
