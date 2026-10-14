/**
 * @file test_step_cycles.c
 * @brief Testy návratové hodnoty z80_step() na hranicích čítače total_cycles.
 *
 * z80_step() vrací počet T-stavů instrukce jako rozdíl cpu->total_cycles
 * (u32, běží modulo 2^32). Do v0.3.2 se absolutní hodnoty převáděly na int
 * a teprve pak odečítaly - na hranici 2^31 to je přetečení se znaménkem
 * (nedefinované chování v C). Běžný build vracel správné číslo náhodou,
 * proto test hlídá kontrakt (délka instrukce i stav čítače po přetečení),
 * ne samotné UB. UB ukáže jen build s -ftrapv.
 *
 * Pokrývá:
 *   - NOP (4 T) těsně pod, na a těsně za hranicí 2^31
 *   - NOP (4 T) přes přetečení čítače na hranici 2^32
 *   - LD BC,nn (10 T) přes obě hranice
 */

#include "test_framework.h"

/**
 * @brief Provede jednu instrukci od PC 0x0100 se zadaným stavem čítače.
 *
 * @param start    Počáteční hodnota cpu->total_cycles.
 * @param expected Očekávaná délka instrukce v T-stavech.
 * @pre Instrukce je připravená v test_ram od adresy 0x0100.
 * @post cpu->total_cycles == start + expected (modulo 2^32).
 */
static void step_from(u32 start, int expected) {
    cpu->pc = 0x0100;
    cpu->total_cycles = start;
    ASSERT_EQ(step(), expected);
    ASSERT_EQ(cpu->total_cycles, (u32)(start + (u32)expected));
}

/**
 * @brief NOP (4 T) přes hranici 2^31.
 */
static void test_step_nop_across_2pow31(void) {
    TEST_BEGIN("z80_step length across 2^31 (NOP)");
    setup();
    test_ram[0x0100] = 0x00;  /* NOP */
    step_from(0x7FFFFFFDu, 4);
    step_from(0x7FFFFFFEu, 4);
    step_from(0x7FFFFFFFu, 4);
    step_from(0x80000000u, 4);
    TEST_END();
}

/**
 * @brief NOP (4 T) přes hranici 2^32 - čítač přeteče, délka zůstane 4.
 */
static void test_step_nop_across_2pow32(void) {
    TEST_BEGIN("z80_step length across 2^32 (NOP)");
    setup();
    test_ram[0x0100] = 0x00;  /* NOP */
    step_from(0xFFFFFFFDu, 4);
    step_from(0xFFFFFFFEu, 4);
    step_from(0xFFFFFFFFu, 4);
    step_from(0x00000000u, 4);
    TEST_END();
}

/**
 * @brief Delší instrukce LD BC,0x1234 (10 T) přes obě hranice.
 */
static void test_step_ld_bc_nn_across_boundaries(void) {
    TEST_BEGIN("z80_step length across 2^31 and 2^32 (LD BC,nn)");
    setup();
    test_ram[0x0100] = 0x01;  /* LD BC,0x1234 */
    test_ram[0x0101] = 0x34;
    test_ram[0x0102] = 0x12;
    step_from(0x7FFFFFF9u, 10);
    step_from(0xFFFFFFF9u, 10);
    TEST_END();
}

/* ========== Hlavní runner ========== */

/**
 * @brief Spustí všechny testy délky kroku z80_step().
 */
void test_step_cycles_all(void) {
    test_step_nop_across_2pow31();
    test_step_nop_across_2pow32();
    test_step_ld_bc_nn_across_boundaries();
}
