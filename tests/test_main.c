/**
 * @file test_main.c
 * @brief Hlavni testovaci runner pro Z80 emulator.
 *
 * Spousti vsechny testovaci suity a vypisuje souhrn vysledku.
 */

#include "test_framework.h"

/* Definice globalnich pocitadel (deklarovana jako extern v test_framework.h) */
int tests_run = 0;
int tests_passed = 0;
int tests_failed = 0;
const char *current_test_name = NULL;
int current_test_failed = 0;

/* Deklarace testovacich suit z jednotlivych souboru */
extern void test_ld_all(void);
extern void test_alu_all(void);
extern void test_rotate_bit_all(void);
extern void test_jump_call_all(void);
extern void test_block_io_all(void);
extern void test_ix_iy_all(void);
extern void test_interrupts_all(void);
extern void test_flags_daa_all(void);
extern void test_timing_all(void);

/**
 * @brief Hlavni vstupni bod testovaci sady.
 *
 * Spusti vsechny testy, vypise souhrn a vrati 0 pri uspechu, 1 pri selhani.
 *
 * @return 0 = vsechny testy prosly, 1 = nektere selhaly.
 */
int main(void) {
    printf("========================================\n");
    printf("  Z80 Emulator - Kompletni testovaci sada\n");
    printf("  (cpu-z80 v" CPU_Z80_VERSION ")\n");
    printf("========================================\n");

    RUN_SUITE(test_ld_all);
    RUN_SUITE(test_alu_all);
    RUN_SUITE(test_rotate_bit_all);
    RUN_SUITE(test_jump_call_all);
    RUN_SUITE(test_block_io_all);
    RUN_SUITE(test_ix_iy_all);
    RUN_SUITE(test_interrupts_all);
    RUN_SUITE(test_flags_daa_all);
    RUN_SUITE(test_timing_all);

    TEST_SUMMARY();
}
