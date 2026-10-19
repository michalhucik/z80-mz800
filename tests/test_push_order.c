/**
 * @file test_push_order.c
 * @brief Testy pořadí zápisů 16bitové hodnoty na zásobník.
 *
 * Zilog UM0080 (Z80 CPU User Manual, UM008005-0205): PUSH qq, PUSH IX,
 * PUSH IY, CALL nn, CALL cc,nn, RST p i přijetí NMI nejdřív sníží SP
 * a zapíše VYŠŠÍ bajt, pak znovu sníží SP a zapíše nižší bajt. IM 1 je
 * "identické s NMI" (restart na 0x0038), IM 2 "provede CALL". Naopak
 * LD (nn),rr zapisuje nižší bajt na nn a vyšší na nn+1 v tomto pořadí.
 *
 * Pokrývá (pořadí zápisů z callbacku paměti, obsah paměti po instrukci):
 *   - PUSH BC, PUSH IX, CALL nn, CALL NZ,nn (splněno), RST 38h
 *   - přijetí NMI, INT v IM 0 (vektor RST 38h), IM 1, IM 2
 *   - LD (nn),HL a LD (nn),BC (ED 43) zůstávají nižší bajt dřív
 */

#include "test_framework.h"

/** Největší počet zaznamenaných zápisů. */
#define PO_MAX_WRITES 16

/** Jeden zápis do paměti v pořadí, jak ho CPU provedlo. */
typedef struct {
    u16 addr;  /**< Adresa zápisu. */
    u8  value; /**< Zapsaná hodnota. */
} po_write_t;

/** Zápisy od posledního po_clear_writes(). */
static po_write_t po_writes[PO_MAX_WRITES];
/** Počet zápisů od posledního po_clear_writes() (může přesáhnout PO_MAX_WRITES). */
static int po_nwrites = 0;
/** Vektor přerušení vracený callbackem intread (IM 0 / IM 2). */
static u8 po_int_vector = 0xFF;

/**
 * @brief Callback zápisu do paměti, který navíc zaznamenává pořadí zápisů.
 * @param cpu       Ukazatel na CPU instanci (nepoužit).
 * @param addr      Adresa zápisu.
 * @param value     Zapisovaný bajt.
 * @param user_data Uživatelská data (nepoužita).
 */
static void po_mwrite(z80_t *cpu, u16 addr, u8 value, void *user_data) {
    (void)cpu; (void)user_data;
    test_ram[addr] = value;
    if (po_nwrites < PO_MAX_WRITES) {
        po_writes[po_nwrites].addr = addr;
        po_writes[po_nwrites].value = value;
    }
    po_nwrites++;
}

/**
 * @brief Callback čtení vektoru přerušení - vrací po_int_vector.
 * @param cpu       Ukazatel na CPU instanci (nepoužit).
 * @param user_data Uživatelská data (nepoužita).
 * @return Vektor přerušení.
 */
static u8 po_intread(z80_t *cpu, void *user_data) {
    (void)cpu; (void)user_data;
    return po_int_vector;
}

/**
 * @brief Nová CPU instance se záznamem zápisů a callbackem vektoru.
 * @post Paměť vynulovaná, záznam zápisů prázdný, po_int_vector = 0xFF.
 */
static void po_setup(void) {
    setup();
    z80_set_mwrite(cpu, po_mwrite, NULL);
    z80_set_intread(cpu, po_intread, NULL);
    po_nwrites = 0;
    po_int_vector = 0xFF;
}

/** @brief Smaže záznam zápisů (před měřenou instrukcí). */
static void po_clear_writes(void) {
    po_nwrites = 0;
}

/**
 * @brief Ověří, že zápisy byly vyšší bajt na @p sp-1, pak nižší na @p sp-2.
 * @param sp  SP před instrukcí.
 * @param val Ukládaná 16bitová hodnota.
 */
static void assert_stack_push(u16 sp, u16 val) {
    ASSERT_EQ(po_nwrites, 2);
    ASSERT_EQ(po_writes[0].addr, (u16)(sp - 1));   /* první zápis na SP-1 */
    ASSERT_EQ(po_writes[0].value, (u8)(val >> 8));  /* ... a je to vyšší bajt */
    ASSERT_EQ(po_writes[1].addr, (u16)(sp - 2));   /* druhý zápis na SP-2 */
    ASSERT_EQ(po_writes[1].value, (u8)(val & 0xFF));
    /* Obsah paměti po instrukci je stejný jako dřív (little endian). */
    ASSERT_EQ(cpu->sp, (u16)(sp - 2));
    ASSERT_EQ(test_ram[(u16)(sp - 2)], (u8)(val & 0xFF));
    ASSERT_EQ(test_ram[(u16)(sp - 1)], (u8)(val >> 8));
}

/** @brief PUSH BC: vyšší bajt (B) na SP-1 dřív. */
static void test_push_bc_high_byte_first(void) {
    TEST_BEGIN("PUSH BC writes the high byte first");
    po_setup();
    test_ram[0x0100] = 0xC5;  /* PUSH BC */
    cpu->pc = 0x0100;
    cpu->sp = 0x8000;
    cpu->bc.w = 0x1234;
    po_clear_writes();
    (void)step();
    assert_stack_push(0x8000, 0x1234);
    TEST_END();
}

/** @brief PUSH IX: vyšší bajt na SP-1 dřív. */
static void test_push_ix_high_byte_first(void) {
    TEST_BEGIN("PUSH IX writes the high byte first");
    po_setup();
    test_ram[0x0100] = 0xDD;
    test_ram[0x0101] = 0xE5;  /* PUSH IX */
    cpu->pc = 0x0100;
    cpu->sp = 0x8000;
    cpu->ix.w = 0xA1B2;
    po_clear_writes();
    (void)step();
    assert_stack_push(0x8000, 0xA1B2);
    TEST_END();
}

/** @brief CALL nn: návratová adresa, vyšší bajt dřív. */
static void test_call_high_byte_first(void) {
    TEST_BEGIN("CALL nn writes the high byte first");
    po_setup();
    test_ram[0x1234] = 0xCD;  /* CALL 0x2000 */
    test_ram[0x1235] = 0x00;
    test_ram[0x1236] = 0x20;
    cpu->pc = 0x1234;
    cpu->sp = 0x8000;
    po_clear_writes();
    (void)step();
    assert_stack_push(0x8000, 0x1237);
    ASSERT_EQ(cpu->pc, 0x2000);
    TEST_END();
}

/** @brief CALL NZ,nn (podmínka splněna): vyšší bajt dřív. */
static void test_call_cc_high_byte_first(void) {
    TEST_BEGIN("CALL NZ,nn (taken) writes the high byte first");
    po_setup();
    test_ram[0x1234] = 0xC4;  /* CALL NZ,0x2000 */
    test_ram[0x1235] = 0x00;
    test_ram[0x1236] = 0x20;
    cpu->pc = 0x1234;
    cpu->sp = 0x8000;
    cpu->af.w = 0x0000;       /* Z = 0 */
    po_clear_writes();
    (void)step();
    assert_stack_push(0x8000, 0x1237);
    TEST_END();
}

/** @brief RST 38h: vyšší bajt dřív. */
static void test_rst_high_byte_first(void) {
    TEST_BEGIN("RST 38h writes the high byte first");
    po_setup();
    test_ram[0x1234] = 0xFF;  /* RST 38h */
    cpu->pc = 0x1234;
    cpu->sp = 0x8000;
    po_clear_writes();
    (void)step();
    assert_stack_push(0x8000, 0x1235);
    ASSERT_EQ(cpu->pc, 0x0038);
    TEST_END();
}

/**
 * @brief Připraví CPU na přijetí maskovaného přerušení v daném IM.
 *
 * Program: IM n, EI, NOP (konec odkladu EI); INT se pak přijme před
 * instrukcí na 0x0004. Handlery jsou NOP (zápisy jsou jen z přijetí).
 *
 * @param im_opcode Druhý bajt instrukce IM (0x46 = IM 0, 0x56 = IM 1, 0x5E = IM 2).
 */
static void prepare_int(u8 im_opcode) {
    test_ram[0x0000] = 0xED;
    test_ram[0x0001] = im_opcode;  /* IM 0 / 1 / 2 */
    test_ram[0x0002] = 0xFB;       /* EI */
    test_ram[0x0003] = 0x00;       /* NOP - konec odkladu EI */
    cpu->pc = 0x0000;
    cpu->sp = 0x8000;
    (void)step();
    (void)step();
    (void)step();
    ASSERT_EQ(cpu->pc, 0x0004);
}

/** @brief Vyvolá INT a provede krok, ve kterém se přijme (handler je NOP). */
static void accept_int(void) {
    po_clear_writes();
    z80_int(cpu);
    (void)step();
}

/** @brief Přijetí INT v IM 1: vyšší bajt dřív (jako NMI / RST 38h). */
static void test_int_im1_high_byte_first(void) {
    TEST_BEGIN("INT IM 1 acceptance writes the high byte first");
    po_setup();
    prepare_int(0x56);  /* IM 1 */
    accept_int();
    assert_stack_push(0x8000, 0x0004);
    TEST_END();
}

/** @brief Přijetí INT v IM 0 s vektorem RST 38h: vyšší bajt dřív. */
static void test_int_im0_high_byte_first(void) {
    TEST_BEGIN("INT IM 0 acceptance writes the high byte first");
    po_setup();
    po_int_vector = 0xFF;  /* RST 38h na sběrnici */
    prepare_int(0x46);     /* IM 0 */
    accept_int();
    assert_stack_push(0x8000, 0x0004);
    TEST_END();
}

/** @brief Přijetí INT v IM 2: vyšší bajt dřív (CALL přes tabulku). */
static void test_int_im2_high_byte_first(void) {
    TEST_BEGIN("INT IM 2 acceptance writes the high byte first");
    po_setup();
    po_int_vector = 0x40;
    test_ram[0x3040] = 0x00;  /* tabulka: 0x3000 + vektor -> rutina 0x5000 */
    test_ram[0x3041] = 0x50;
    prepare_int(0x5E);        /* IM 2 */
    cpu->i = 0x30;
    accept_int();
    assert_stack_push(0x8000, 0x0004);
    TEST_END();
}

/** @brief Přijetí NMI: vyšší bajt dřív. */
static void test_nmi_high_byte_first(void) {
    TEST_BEGIN("NMI acceptance writes the high byte first");
    po_setup();
    test_ram[0x1234] = 0x00;  /* NOP */
    cpu->pc = 0x1234;
    cpu->sp = 0x8000;
    po_clear_writes();
    z80_nmi(cpu);
    (void)step();
    assert_stack_push(0x8000, 0x1234);
    TEST_END();
}

/** @brief LD (nn),HL zůstává nižší bajt na nn dřív. */
static void test_ld_nn_hl_low_byte_first(void) {
    TEST_BEGIN("LD (nn),HL still writes the low byte first");
    po_setup();
    test_ram[0x0100] = 0x22;  /* LD (0x4000),HL */
    test_ram[0x0101] = 0x00;
    test_ram[0x0102] = 0x40;
    cpu->pc = 0x0100;
    cpu->hl.w = 0xBEEF;
    po_clear_writes();
    (void)step();
    ASSERT_EQ(po_nwrites, 2);
    ASSERT_EQ(po_writes[0].addr, 0x4000);
    ASSERT_EQ(po_writes[0].value, 0xEF);
    ASSERT_EQ(po_writes[1].addr, 0x4001);
    ASSERT_EQ(po_writes[1].value, 0xBE);
    TEST_END();
}

/** @brief LD (nn),BC (ED 43) zůstává nižší bajt na nn dřív. */
static void test_ld_nn_bc_low_byte_first(void) {
    TEST_BEGIN("LD (nn),BC still writes the low byte first");
    po_setup();
    test_ram[0x0100] = 0xED;
    test_ram[0x0101] = 0x43;  /* LD (0x4000),BC */
    test_ram[0x0102] = 0x00;
    test_ram[0x0103] = 0x40;
    cpu->pc = 0x0100;
    cpu->bc.w = 0x1234;
    po_clear_writes();
    (void)step();
    ASSERT_EQ(po_nwrites, 2);
    ASSERT_EQ(po_writes[0].addr, 0x4000);
    ASSERT_EQ(po_writes[0].value, 0x34);
    ASSERT_EQ(po_writes[1].addr, 0x4001);
    ASSERT_EQ(po_writes[1].value, 0x12);
    TEST_END();
}

/* ========== Hlavní runner ========== */

/**
 * @brief Spustí všechny testy pořadí zápisů na zásobník.
 */
void test_push_order_all(void) {
    test_push_bc_high_byte_first();
    test_push_ix_high_byte_first();
    test_call_high_byte_first();
    test_call_cc_high_byte_first();
    test_rst_high_byte_first();
    test_int_im0_high_byte_first();
    test_int_im1_high_byte_first();
    test_int_im2_high_byte_first();
    test_nmi_high_byte_first();
    test_ld_nn_hl_low_byte_first();
    test_ld_nn_bc_low_byte_first();
}
