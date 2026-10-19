/**
 * @file test_reset_state.c
 * @brief Testy stavu rozpracované instrukce po z80_reset().
 *
 * cpu->op_tstate drží T-stavy od začátku aktuální instrukce. DISPATCH ho
 * nuluje až před další instrukcí, takže mezi instrukcemi v něm zůstává
 * délka předchozí instrukce. Hostitel ho může číst i mimo instrukci
 * (např. synchronizace času periferií). z80_reset() proto musí op_tstate
 * i wait_cycles vynulovat, jinak by se po resetu propsala délka nebo
 * čekací stavy poslední instrukce před resetem.
 *
 * Pokrývá:
 *   - op_tstate po instrukci != 0, po z80_reset() == 0
 *   - čekací stavy přidané mimo instrukci (z80_add_wait_states) po resetu
 *     nezůstanou ve wait_cycles ani v op_tstate
 */

#include "test_framework.h"

/**
 * @brief Po instrukci zůstane op_tstate nenulové, z80_reset() ho vynuluje.
 *
 * LD BC,0x1234 (10 T) od 0x0000. Kontroluje se i předpoklad testu
 * (op_tstate po instrukci != 0), aby test neprošel jen náhodou.
 */
static void test_reset_clears_op_tstate_after_instruction(void) {
    TEST_BEGIN("z80_reset clears op_tstate left by the last instruction");
    setup();
    test_ram[0x0000] = 0x01;  /* LD BC,0x1234 */
    test_ram[0x0001] = 0x34;
    test_ram[0x0002] = 0x12;
    ASSERT_EQ(step(), 10);
    ASSERT_TRUE(cpu->op_tstate != 0);
    z80_reset(cpu);
    ASSERT_EQ(cpu->op_tstate, 0);
    TEST_END();
}

/**
 * @brief Čekací stavy přidané mimo instrukci po resetu nezůstanou.
 *
 * z80_add_wait_states() přičítá do wait_cycles i do op_tstate. Po resetu
 * musí být oba účty 0.
 */
static void test_reset_clears_wait_accounts(void) {
    TEST_BEGIN("z80_reset clears wait_cycles and op_tstate added outside an instruction");
    setup();
    z80_add_wait_states(cpu, 7);
    ASSERT_TRUE(cpu->op_tstate != 0);
    z80_reset(cpu);
    ASSERT_EQ(cpu->op_tstate, 0);
    ASSERT_EQ(cpu->wait_cycles, 0);
    TEST_END();
}

/* ========== Hlavní runner ========== */

/**
 * @brief Spustí všechny testy stavu po z80_reset().
 */
void test_reset_state_all(void) {
    test_reset_clears_op_tstate_after_instruction();
    test_reset_clears_wait_accounts();
}
