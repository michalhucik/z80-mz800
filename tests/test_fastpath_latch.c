/**
 * @file test_fastpath_latch.c
 * @brief Testy data-bus latche v RAM fast-path (překlad s -DCPU_Z80_RAM_FASTPATH).
 *
 * Fast-path přistupuje do čisté RAM bez callbacku, a proto musí replikovat
 * jeho vedlejší efekt na data-bus latch hostitele: latch drží poslední bajt,
 * který šel po datové sběrnici, po čtení i po zápisu. Do v0.3.7 fast-path
 * nastavoval latch jen při čtení, po zápisu zůstal na předchozí hodnotě.
 *
 * Každý případ se provede dvakrát: přes callbacky (fast-path vypnutý)
 * a přes fast-path. Výsledný latch se musí shodovat a odpovídat bajtu,
 * který šel po sběrnici naposledy.
 *
 * Pokrývá:
 *   - LD (nn),A: latch = zapsaný bajt
 *   - LD A,(nn): latch = přečtený bajt
 *   - PUSH BC: latch = poslední zapsaný (nižší) bajt
 *
 * Samostatný program (vlastní main), překládá se s -DCPU_Z80_RAM_FASTPATH.
 */

#include "test_framework.h"

#ifndef CPU_Z80_RAM_FASTPATH
#error "test_fastpath_latch.c must be compiled with -DCPU_Z80_RAM_FASTPATH"
#endif

/* Definice globálních počítadel (deklarovaná jako extern v test_framework.h) */
int tests_run = 0;
int tests_passed = 0;
int tests_failed = 0;
const char *current_test_name = NULL;
int current_test_failed = 0;

/** Data-bus latch hostitele: poslední bajt na datové sběrnici. */
static u8 fp_latch = 0x00;

/**
 * @brief Read callback hostitele: přečte bajt a nastaví latch.
 * @param c         Ukazatel na CPU instanci (nepoužit).
 * @param addr      Čtená adresa.
 * @param m1_state  M1 příznak (nepoužit).
 * @param user_data Uživatelská data (nepoužita).
 * @return Bajt z test_ram.
 */
static u8 fp_mread(z80_t *c, u16 addr, int m1_state, void *user_data) {
    (void)c; (void)m1_state; (void)user_data;
    fp_latch = test_ram[addr];
    return fp_latch;
}

/**
 * @brief Write callback hostitele: zapíše bajt a nastaví latch.
 * @param c         Ukazatel na CPU instanci (nepoužit).
 * @param addr      Zapisovaná adresa.
 * @param value     Zapisovaný bajt.
 * @param user_data Uživatelská data (nepoužita).
 */
static void fp_mwrite(z80_t *c, u16 addr, u8 value, void *user_data) {
    (void)c; (void)user_data;
    test_ram[addr] = value;
    fp_latch = value;
}

/**
 * @brief Nová CPU instance s callbacky hostitele a fast-path přes celou RAM.
 * @param enabled true = fast-path aktivní, false = vše přes callbacky.
 * @post Všech 16 stránek ukazuje do test_ram, latch registrovaný, fp_latch = 0.
 */
static void fp_setup(bool enabled) {
    u8 *table[16];
    setup();
    z80_set_mread(cpu, fp_mread, NULL);
    z80_set_mwrite(cpu, fp_mwrite, NULL);
    for (int i = 0; i < 16; i++) table[i] = &test_ram[i * 0x1000];
    z80_set_ram_fastpath(cpu, table, table, &fp_latch, enabled);
    fp_latch = 0x00;
}

/**
 * @brief Provede program od 0x0100 v daném režimu a vrátí latch po něm.
 * @param code    Bajty programu.
 * @param len     Délka programu.
 * @param steps   Počet instrukcí k provedení.
 * @param enabled Režim fast-path.
 * @param init    Volitelná příprava registrů a paměti (může být NULL).
 * @return Hodnota latche po provedení.
 */
static u8 fp_run(const u8 *code, int len, int steps, bool enabled, void (*init)(void)) {
    fp_setup(enabled);
    memcpy(&test_ram[0x0100], code, (size_t)len);
    cpu->pc = 0x0100;
    cpu->sp = 0x8000;
    if (init) init();
    for (int i = 0; i < steps; i++) (void)step();
    return fp_latch;
}

/** @brief Příprava pro LD A,(nn): A = 0x00, na 0x5000 bajt 0xC3. */
static void init_ld_a_nn(void) { cpu->af.h = 0x00; test_ram[0x5000] = 0xC3; }

/** @brief Příprava pro LD (nn),A: A = 0x5A. */
static void init_ld_nn_a(void) { cpu->af.h = 0x5A; }

/** @brief Příprava pro PUSH BC: BC = 0x1234. */
static void init_push_bc(void) { cpu->bc.w = 0x1234; }

/** @brief LD (nn),A: latch = zapsaný bajt přes callback i přes fast-path. */
static void test_latch_after_write(void) {
    static const u8 code[] = { 0x32, 0x00, 0x50 };  /* LD (0x5000),A */
    TEST_BEGIN("fast path: latch holds the written byte");
    u8 cb = fp_run(code, 3, 1, false, init_ld_nn_a);
    u8 fp = fp_run(code, 3, 1, true, init_ld_nn_a);
    ASSERT_EQ(cb, 0x5A);
    ASSERT_EQ(fp, cb);
    ASSERT_EQ(test_ram[0x5000], 0x5A);
    TEST_END();
}

/** @brief LD A,(nn): latch = přečtený bajt přes callback i přes fast-path. */
static void test_latch_after_read(void) {
    static const u8 code[] = { 0x3A, 0x00, 0x50 };  /* LD A,(0x5000) */
    TEST_BEGIN("fast path: latch holds the read byte");
    u8 cb = fp_run(code, 3, 1, false, init_ld_a_nn);
    u8 fp = fp_run(code, 3, 1, true, init_ld_a_nn);
    ASSERT_EQ(cb, 0xC3);
    ASSERT_EQ(fp, cb);
    TEST_END();
}

/** @brief PUSH BC: latch = poslední zapsaný bajt (nižší, zapisuje se druhý). */
static void test_latch_after_push(void) {
    static const u8 code[] = { 0xC5 };  /* PUSH BC */
    TEST_BEGIN("fast path: latch holds the last byte pushed");
    u8 cb = fp_run(code, 1, 1, false, init_push_bc);
    u8 fp = fp_run(code, 1, 1, true, init_push_bc);
    ASSERT_EQ(cb, 0x34);
    ASSERT_EQ(fp, cb);
    TEST_END();
}

/**
 * @brief Spustí testy latche ve fast-path a vypíše souhrn.
 * @return 0 = všechny testy prošly, 1 = některé selhaly.
 */
int main(void) {
    printf("========================================\n");
    printf("  cpu-z80 " CPU_Z80_VERSION " - RAM fast-path data-bus latch\n");
    printf("========================================\n");
    test_latch_after_write();
    test_latch_after_read();
    test_latch_after_push();
    TEST_SUMMARY();
}
