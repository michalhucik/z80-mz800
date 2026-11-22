/**
 * @file test_read16_order.c
 * @brief Testy pořadí a T-stavu čtení 16bitové hodnoty z paměti.
 *
 * Zilog UM0080 (Z80 CPU User Manual): LD HL,(nn), LD dd,(nn), LD IX,(nn)
 * čtou nejdřív nižší bajt z nn, pak vyšší z nn+1; POP, RET, RETI, RETN
 * čtou nejdřív nižší bajt z SP, pak vyšší z SP+1; EX (SP),HL čte nižší
 * bajt z SP a vyšší z SP+1 a teprve pak zapisuje.
 *
 * Každé čtení je samostatný M-cyklus o 3 T: druhé čtení musí vidět
 * cpu->op_tstate o 3 vyšší než první. Na tom závisí hostitel, který podle
 * op_tstate synchronizuje periferie (čekací stavy, čas čtení mapovaných
 * portů). Do v0.3.6 jádro skládalo hodnotu výrazem RD(a) | (RD(a + 1) << 8),
 * který měnil op_tstate dvakrát bez určeného pořadí; GCC 16.2 (-O0 i -O2)
 * provedl obě přičtení před prvním čtením, takže obě čtení viděla stejný
 * op_tstate (pořadí adres bylo správné).
 *
 * Pokrývá (pořadí čtení a jejich op_tstate z callbacku paměti, výsledná
 * hodnota):
 *   LD HL,(nn), LD BC,(nn) (ED 4B), LD SP,(nn) (ED 7B), LD IX,(nn),
 *   POP BC, POP IX, RET, RET NZ (splněno), RETI, EX (SP),HL.
 */

#include "test_framework.h"

/** Začátek datové oblasti; čtení pod ní (kód) se nezaznamenává. */
#define R16_DATA_BASE 0x4000

/** Největší počet zaznamenaných čtení. */
#define R16_MAX_READS 16

/** Adresy datových čtení v pořadí, jak je CPU provedlo. */
static u16 r16_reads[R16_MAX_READS];
/** Hodnota cpu->op_tstate v okamžiku každého datového čtení. */
static int r16_read_tstate[R16_MAX_READS];
/** Počet datových čtení od posledního vynulování (může přesáhnout R16_MAX_READS). */
static int r16_nreads = 0;

/**
 * @brief Callback čtení paměti, který zaznamenává datová čtení a jejich op_tstate.
 * @param c         Ukazatel na CPU instanci.
 * @param addr      Čtená adresa.
 * @param m1_state  1 = M1 fetch, 0 = běžné čtení (nepoužito).
 * @param user_data Uživatelská data (nepoužita).
 * @return Bajt z test_ram.
 */
static u8 r16_mread(z80_t *c, u16 addr, int m1_state, void *user_data) {
    (void)m1_state; (void)user_data;
    if (addr >= R16_DATA_BASE) {
        if (r16_nreads < R16_MAX_READS) {
            r16_reads[r16_nreads] = addr;
            r16_read_tstate[r16_nreads] = c->op_tstate;
        }
        r16_nreads++;
    }
    return test_ram[addr];
}

/**
 * @brief Nová CPU instance se zaznamenáváním datových čtení.
 * @post Paměť vynulovaná, záznam čtení prázdný.
 */
static void r16_setup(void) {
    setup();
    z80_set_mread(cpu, r16_mread, NULL);
    r16_nreads = 0;
}

/**
 * @brief Provede jednu instrukci z adresy 0x0100 a ověří pořadí datových čtení.
 *
 * Na @p addr uloží hodnotu 0xBEEF (nižší bajt na addr), SP nastaví na
 * @p addr, instrukci zkopíruje na 0x0100 a krokuje.
 *
 * @param code Bajty instrukce.
 * @param len  Délka instrukce.
 * @param addr Adresa čtené 16bitové hodnoty (>= R16_DATA_BASE).
 * @post Proběhla právě dvě datová čtení: addr, pak addr + 1, druhé o 3 T
 *       později než první.
 */
static void step_and_check(const u8 *code, int len, u16 addr) {
    memcpy(&test_ram[0x0100], code, (size_t)len);
    test_ram[addr] = 0xEF;
    test_ram[(u16)(addr + 1)] = 0xBE;
    cpu->pc = 0x0100;
    cpu->sp = addr;
    r16_nreads = 0;
    (void)step();
    ASSERT_EQ(r16_nreads, 2);                          /* právě dvě datová čtení */
    ASSERT_EQ(r16_reads[0], addr);                     /* nejdřív nižší bajt */
    ASSERT_EQ(r16_reads[1], (u16)(addr + 1));          /* pak vyšší bajt */
    ASSERT_EQ(r16_read_tstate[1], r16_read_tstate[0] + 3);  /* každé čtení svůj T-stav */
}

/** @brief LD HL,(nn): nižší bajt z nn dřív. */
static void test_ld_hl_nn_low_byte_first(void) {
    static const u8 code[] = { 0x2A, 0x00, 0x50 };
    TEST_BEGIN("LD HL,(nn) reads the low byte first, 3 T apart");
    r16_setup();
    step_and_check(code, 3, 0x5000);
    ASSERT_EQ(cpu->hl.w, 0xBEEF);
    TEST_END();
}

/** @brief LD BC,(nn) (ED 4B): nižší bajt z nn dřív. */
static void test_ld_bc_nn_low_byte_first(void) {
    static const u8 code[] = { 0xED, 0x4B, 0x00, 0x50 };
    TEST_BEGIN("LD BC,(nn) reads the low byte first, 3 T apart");
    r16_setup();
    step_and_check(code, 4, 0x5000);
    ASSERT_EQ(cpu->bc.w, 0xBEEF);
    TEST_END();
}

/** @brief LD SP,(nn) (ED 7B): nižší bajt z nn dřív. */
static void test_ld_sp_nn_low_byte_first(void) {
    static const u8 code[] = { 0xED, 0x7B, 0x00, 0x50 };
    TEST_BEGIN("LD SP,(nn) reads the low byte first, 3 T apart");
    r16_setup();
    step_and_check(code, 4, 0x5000);
    ASSERT_EQ(cpu->sp, 0xBEEF);
    TEST_END();
}

/** @brief LD IX,(nn): nižší bajt z nn dřív. */
static void test_ld_ix_nn_low_byte_first(void) {
    static const u8 code[] = { 0xDD, 0x2A, 0x00, 0x50 };
    TEST_BEGIN("LD IX,(nn) reads the low byte first, 3 T apart");
    r16_setup();
    step_and_check(code, 4, 0x5000);
    ASSERT_EQ(cpu->ix.w, 0xBEEF);
    TEST_END();
}

/** @brief POP BC: nižší bajt z SP dřív. */
static void test_pop_bc_low_byte_first(void) {
    static const u8 code[] = { 0xC1 };
    TEST_BEGIN("POP BC reads the low byte first, 3 T apart");
    r16_setup();
    step_and_check(code, 1, 0x6000);
    ASSERT_EQ(cpu->bc.w, 0xBEEF);
    ASSERT_EQ(cpu->sp, 0x6002);
    TEST_END();
}

/** @brief POP IX: nižší bajt z SP dřív. */
static void test_pop_ix_low_byte_first(void) {
    static const u8 code[] = { 0xDD, 0xE1 };
    TEST_BEGIN("POP IX reads the low byte first, 3 T apart");
    r16_setup();
    step_and_check(code, 2, 0x6000);
    ASSERT_EQ(cpu->ix.w, 0xBEEF);
    TEST_END();
}

/** @brief RET: nižší bajt návratové adresy z SP dřív. */
static void test_ret_low_byte_first(void) {
    static const u8 code[] = { 0xC9 };
    TEST_BEGIN("RET reads the low byte first, 3 T apart");
    r16_setup();
    step_and_check(code, 1, 0x6000);
    ASSERT_EQ(cpu->pc, 0xBEEF);
    TEST_END();
}

/** @brief RET NZ (podmínka splněna): nižší bajt z SP dřív. */
static void test_ret_cc_low_byte_first(void) {
    static const u8 code[] = { 0xC0 };
    TEST_BEGIN("RET NZ (taken) reads the low byte first, 3 T apart");
    r16_setup();
    cpu->af.w = 0x0000;  /* Z = 0 */
    step_and_check(code, 1, 0x6000);
    ASSERT_EQ(cpu->pc, 0xBEEF);
    TEST_END();
}

/** @brief RETI: nižší bajt z SP dřív. */
static void test_reti_low_byte_first(void) {
    static const u8 code[] = { 0xED, 0x4D };
    TEST_BEGIN("RETI reads the low byte first, 3 T apart");
    r16_setup();
    step_and_check(code, 2, 0x6000);
    ASSERT_EQ(cpu->pc, 0xBEEF);
    TEST_END();
}

/** @brief EX (SP),HL: čte nižší bajt z SP, pak vyšší z SP+1. */
static void test_ex_sp_hl_low_byte_first(void) {
    static const u8 code[] = { 0xE3 };
    TEST_BEGIN("EX (SP),HL reads the low byte first, 3 T apart");
    r16_setup();
    cpu->hl.w = 0x1234;
    step_and_check(code, 1, 0x6000);
    ASSERT_EQ(cpu->hl.w, 0xBEEF);
    ASSERT_EQ(test_ram[0x6000], 0x34);
    ASSERT_EQ(test_ram[0x6001], 0x12);
    TEST_END();
}

/* ========== Hlavní runner ========== */

/**
 * @brief Spustí všechny testy pořadí a T-stavu čtení 16bitové hodnoty.
 */
void test_read16_order_all(void) {
    test_ld_hl_nn_low_byte_first();
    test_ld_bc_nn_low_byte_first();
    test_ld_sp_nn_low_byte_first();
    test_ld_ix_nn_low_byte_first();
    test_pop_bc_low_byte_first();
    test_pop_ix_low_byte_first();
    test_ret_low_byte_first();
    test_ret_cc_low_byte_first();
    test_reti_low_byte_first();
    test_ex_sp_hl_low_byte_first();
}
