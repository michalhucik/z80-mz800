/**
 * @file test_im2_vector.c
 * @brief Testy adresy záznamu tabulky vektorů v IM 2 (bit 0 vektoru).
 *
 * Z80 při přijetí přerušení v IM 2 čte cílovou adresu ze záznamu
 * (I << 8) | vektor. Zilog uvádí, že bit 0 je vždy 0, ale procesor ho
 * nemaskuje (Sean Young; změřeno i na Sharp LH0080A). Lichý vektor
 * přichází jako "duch" datové sběrnice, když vektor nedodá žádné
 * zařízení. Do v0.3.7 jádro bit 0 maskovalo; testy s lichým vektorem
 * na tom stavu selžou.
 *
 * Obsluha na cílové adrese je HALT, takže po kroku s přijetím přerušení
 * zůstane PC na cílové adrese; krok trvá 19 T přijetí + 4 T HALT.
 *
 * Pokrývá:
 *   - lichý vektor čte záznam z lichého (překrývajícího se) slova
 *   - sudý vektor beze změny
 *   - tabulka 257 bajtů vyplněná jedním bajtem vede pro libovolný vektor
 *     na tutéž obsluhu
 *   - návratová adresa se uloží na zásobník i při lichém vektoru
 */

#include "test_framework.h"

/**
 * @brief Připraví CPU v IM 2 s povolenými přerušeními a přijme jedno INT.
 *
 * @param i_reg  Hodnota registru I.
 * @param vector Bajt, který CPU přečte jako vektor.
 * @return Počet T-stavů kroku (přijetí přerušení + první instrukce obsluhy).
 * @pre Na cílové adrese obsluhy je HALT (0x76).
 * @post PC = slovo přečtené ze záznamu tabulky.
 */
static int accept_im2(u8 i_reg, u8 vector) {
    cpu->pc = 0x0100;
    cpu->sp = 0xC000;
    cpu->im = 2;
    cpu->i = i_reg;
    cpu->iff1 = 1;
    cpu->iff2 = 1;
    z80_irq(cpu, vector);
    return step();
}

/**
 * @brief Lichý vektor: záznam se čte z (I << 8) | vektor, ne z vektor & 0xFE.
 *
 * Záznamy 0x39FE a 0x39FF se překrývají; 0x39FE = 0x7C00, 0x39FF = 0xA57C.
 */
static void test_odd_vector_uses_odd_entry(void) {
    TEST_BEGIN("IM 2 odd vector reads the odd table entry");
    setup();
    test_ram[0x39FE] = 0x00;
    test_ram[0x39FF] = 0x7C;
    test_ram[0x3A00] = 0xA5;
    test_ram[0xA57C] = 0x76;  /* HALT v obsluze */
    test_ram[0x7C00] = 0x76;  /* HALT i na sudé adrese, kdyby se maskovalo */
    ASSERT_EQ(accept_im2(0x39, 0xFF), 19 + 4);
    ASSERT_EQ(cpu->pc, 0xA57C);
    TEST_END();
}

/** @brief Sudý vektor beze změny: záznam (I << 8) | vektor. */
static void test_even_vector_unchanged(void) {
    TEST_BEGIN("IM 2 even vector unchanged");
    setup();
    test_ram[0x4080] = 0x00;
    test_ram[0x4081] = 0x50;
    test_ram[0x5000] = 0x76;  /* HALT v obsluze */
    ASSERT_EQ(accept_im2(0x40, 0x80), 19 + 4);
    ASSERT_EQ(cpu->pc, 0x5000);
    TEST_END();
}

/**
 * @brief Tabulka 257 bajtů vyplněná jedním bajtem vede pro libovolný vektor
 *        (sudý i lichý) na tutéž obsluhu.
 */
static void test_filled_table_any_vector(void) {
    TEST_BEGIN("IM 2 table filled with one byte works for any vector");
    for (unsigned v = 0; v < 256; v++) {
        setup();
        memset(&test_ram[0x2000], 0x22, 257);
        test_ram[0x2222] = 0x76;  /* HALT v obsluze */
        ASSERT_EQ(accept_im2(0x20, (u8)v), 19 + 4);
        ASSERT_EQ(cpu->pc, 0x2222);
    }
    TEST_END();
}

/** @brief Návratová adresa se uloží na zásobník i při lichém vektoru. */
static void test_odd_vector_pushes_return_address(void) {
    TEST_BEGIN("IM 2 odd vector pushes the return address");
    setup();
    test_ram[0x3901] = 0x34;
    test_ram[0x3902] = 0x12;
    test_ram[0x1234] = 0x76;  /* HALT v obsluze */
    ASSERT_EQ(accept_im2(0x39, 0x01), 19 + 4);
    ASSERT_EQ(cpu->pc, 0x1234);
    ASSERT_EQ(cpu->sp, 0xBFFE);
    ASSERT_EQ(test_ram[0xBFFE], 0x00);
    ASSERT_EQ(test_ram[0xBFFF], 0x01);
    TEST_END();
}

/* ========== Hlavní runner ========== */

/**
 * @brief Spustí všechny testy adresy záznamu tabulky v IM 2.
 */
void test_im2_vector_all(void) {
    test_odd_vector_uses_odd_entry();
    test_even_vector_unchanged();
    test_filled_table_any_vector();
    test_odd_vector_pushes_return_address();
}
