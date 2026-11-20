/**
 * @file test_flags_daa.c
 * @brief Testy DAA instrukce a nedokumentovanych flagu (F3/F5, MEMPTR).
 *
 * Pokryva: DAA pro ruzne vstupy a rezimy (ADD/SUB),
 * nedokumentovane flagy F3/F5, MEMPTR/WZ chovani.
 */

#include "test_framework.h"

/* ========== DAA ========== */

/**
 * @brief Test DAA instrukce (4T).
 *
 * DAA koriguje BCD vysledek predchozi aritmeticke operace.
 */
static void test_daa(void) {
    int cyc;

    TEST_BEGIN("DAA po ADD (9+1=10 BCD)");
    setup();
    cpu->af.h = 0x09;
    cpu->bc.h = 0x01;
    test_ram[0] = 0x80;  /* ADD A,B */
    test_ram[1] = 0x27;  /* DAA */
    step();  /* ADD: A = 0x0A */
    cyc = step();  /* DAA */
    ASSERT_EQ(cpu->af.h, 0x10);
    ASSERT_FLAG_CLEAR(Z80_FLAG_C);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();

    TEST_BEGIN("DAA po ADD (99+1=00 BCD, carry)");
    setup();
    cpu->af.h = 0x99;
    cpu->bc.h = 0x01;
    test_ram[0] = 0x80;  /* ADD A,B */
    test_ram[1] = 0x27;  /* DAA */
    step();
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x00);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    TEST_END();

    TEST_BEGIN("DAA po ADD (15+1=16 BCD)");
    setup();
    cpu->af.h = 0x15;
    cpu->bc.h = 0x01;
    test_ram[0] = 0x80;  /* ADD A,B: 0x16 */
    test_ram[1] = 0x27;  /* DAA: 0x16 (uz je BCD) */
    step();
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x16);
    TEST_END();

    TEST_BEGIN("DAA po SUB (10-1=09 BCD)");
    setup();
    cpu->af.h = 0x10;
    cpu->bc.h = 0x01;
    test_ram[0] = 0x90;  /* SUB B: A = 0x0F */
    test_ram[1] = 0x27;  /* DAA */
    step();
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x09);
    ASSERT_FLAG_SET(Z80_FLAG_N);
    TEST_END();

    TEST_BEGIN("DAA po ADD (0x50+0x50=0xA0 -> 0x00 s carry)");
    setup();
    cpu->af.h = 0x50;
    cpu->bc.h = 0x50;
    test_ram[0] = 0x80;  /* ADD A,B: A = 0xA0 */
    test_ram[1] = 0x27;  /* DAA: korekce +0x60, A = 0x00 */
    step();
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x00);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    TEST_END();

    TEST_BEGIN("DAA (A=0x0A, no flags -> 0x10)");
    setup();
    cpu->af.h = 0x0A;
    cpu->af.l = 0;  /* N=0, C=0, H=0 */
    test_ram[0] = 0x27;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x10);
    ASSERT_FLAG_CLEAR(Z80_FLAG_C);
    ASSERT_FLAG_SET(Z80_FLAG_H);
    TEST_END();

    TEST_BEGIN("DAA (A=0x9A, N=0 -> 0x00 s carry)");
    setup();
    cpu->af.h = 0x9A;
    cpu->af.l = 0;
    test_ram[0] = 0x27;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x00);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    ASSERT_FLAG_SET(Z80_FLAG_Z);
    TEST_END();

    TEST_BEGIN("DAA (A=0xFF, H=1, C=1, N=0 -> 0x65)");
    setup();
    cpu->af.h = 0xFF;
    cpu->af.l = Z80_FLAG_C | Z80_FLAG_H;
    test_ram[0] = 0x27;
    cyc = step();
    ASSERT_EQ(cpu->af.h, 0x65);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    TEST_END();
}

/* ========== Nedokumentovane flagy F3/F5 ========== */

/**
 * @brief Test F3/F5 chovani u ruznych instrukci.
 *
 * F3 (bit 3) a F5 (bit 5) jsou kopie bitu z vysledku nebo operandu.
 */
static void test_undoc_flags(void) {
    TEST_BEGIN("ADD A,B: F3/F5 z vysledku");
    setup();
    cpu->af.h = 0x20; cpu->bc.h = 0x00;
    test_ram[0] = 0x80;
    step();
    /* Vysledek = 0x20, F5 = bit 5 = 1, F3 = bit 3 = 0 */
    ASSERT_FLAG_SET(Z80_FLAG_5);
    ASSERT_FLAG_CLEAR(Z80_FLAG_3);
    TEST_END();

    TEST_BEGIN("AND: F3/F5 z vysledku");
    setup();
    cpu->af.h = 0xFF;
    test_ram[0] = 0xE6; test_ram[1] = 0x28;  /* AND 0x28 */
    step();
    /* Vysledek = 0x28, F5 = 1, F3 = 1 */
    ASSERT_FLAG_SET(Z80_FLAG_5);
    ASSERT_FLAG_SET(Z80_FLAG_3);
    TEST_END();

    TEST_BEGIN("CP: F3/F5 z operandu (ne z vysledku)");
    setup();
    cpu->af.h = 0xFF;
    test_ram[0] = 0xFE; test_ram[1] = 0x28;  /* CP 0x28 */
    step();
    /* F3/F5 by mely byt z operandu (0x28) */
    ASSERT_EQ(cpu->af.l & Z80_FLAG_3, 0x28 & Z80_FLAG_3);
    ASSERT_EQ(cpu->af.l & Z80_FLAG_5, 0x28 & Z80_FLAG_5);
    TEST_END();

    TEST_BEGIN("INC: F3/F5 z vysledku");
    setup();
    cpu->bc.h = 0x27;
    test_ram[0] = 0x04;  /* INC B */
    step();
    /* Vysledek = 0x28, F5 = 1, F3 = 1 */
    ASSERT_FLAG_SET(Z80_FLAG_5);
    ASSERT_FLAG_SET(Z80_FLAG_3);
    TEST_END();

    TEST_BEGIN("SCF: F3/F5 z A|F");
    setup();
    cpu->af.h = 0x28;  /* A s bity 3 a 5 */
    cpu->af.l = 0;
    test_ram[0] = 0x37;  /* SCF */
    step();
    /* F3/F5 z (A | F) */
    ASSERT_FLAG_SET(Z80_FLAG_3);
    ASSERT_FLAG_SET(Z80_FLAG_5);
    TEST_END();

    TEST_BEGIN("CCF: F3/F5 z A|F");
    setup();
    cpu->af.h = 0x00;
    cpu->af.l = Z80_FLAG_C;
    test_ram[0] = 0x3F;  /* CCF */
    step();
    /* F3/F5 z (A | F) */
    ASSERT_FLAG_CLEAR(Z80_FLAG_3);
    ASSERT_FLAG_CLEAR(Z80_FLAG_5);
    TEST_END();

    TEST_BEGIN("CPL: F3/F5 z vysledku A");
    setup();
    cpu->af.h = 0x00;
    test_ram[0] = 0x2F;  /* CPL */
    step();
    /* A = 0xFF, F3 = 1, F5 = 1 */
    ASSERT_FLAG_SET(Z80_FLAG_3);
    ASSERT_FLAG_SET(Z80_FLAG_5);
    TEST_END();

    TEST_BEGIN("RLCA: F3/F5 z noveho A");
    setup();
    cpu->af.h = 0x28;
    test_ram[0] = 0x07;  /* RLCA */
    step();
    /* A = 0x50, F3 = 0, F5 = 0 (0x50 = 0101_0000) */
    ASSERT_FLAG_CLEAR(Z80_FLAG_3);
    ASSERT_FLAG_CLEAR(Z80_FLAG_5);
    TEST_END();
}

/* ========== MEMPTR/WZ testy ========== */

/**
 * @brief Test MEMPTR/WZ registru u ruznych instrukci.
 */
static void test_memptr(void) {
    TEST_BEGIN("JP nn: WZ = addr");
    setup();
    test_ram[0] = 0xC3; test_ram[1] = 0x34; test_ram[2] = 0x12;
    step();
    ASSERT_EQ(cpu->wz.w, 0x1234);
    TEST_END();

    TEST_BEGIN("CALL nn: WZ = addr");
    setup();
    cpu->sp = 0xFFFE;
    test_ram[0] = 0xCD; test_ram[1] = 0x00; test_ram[2] = 0x50;
    step();
    ASSERT_EQ(cpu->wz.w, 0x5000);
    TEST_END();

    TEST_BEGIN("RET: WZ = navratova adresa");
    setup();
    cpu->sp = 0xFFF0;
    test_ram[0xFFF0] = 0x34; test_ram[0xFFF1] = 0x12;
    test_ram[0] = 0xC9;
    step();
    ASSERT_EQ(cpu->wz.w, 0x1234);
    TEST_END();

    TEST_BEGIN("LD A,(nn): WZ = addr + 1");
    setup();
    test_ram[0] = 0x3A; test_ram[1] = 0xFF; test_ram[2] = 0x7F;
    step();
    ASSERT_EQ(cpu->wz.w, 0x8000);
    TEST_END();

    TEST_BEGIN("LD (nn),A: WZ_lo = (addr+1)&0xFF, WZ_hi = A");
    setup();
    cpu->af.h = 0x42;
    test_ram[0] = 0x32; test_ram[1] = 0xFF; test_ram[2] = 0x7F;
    step();
    ASSERT_EQ(cpu->wz.l, 0x00);
    ASSERT_EQ(cpu->wz.h, 0x42);
    TEST_END();

    TEST_BEGIN("ADD HL,rr: WZ = HL_pred + 1");
    setup();
    cpu->hl.w = 0x1234; cpu->bc.w = 0x0001;
    test_ram[0] = 0x09;
    step();
    ASSERT_EQ(cpu->wz.w, 0x1235);
    TEST_END();

    TEST_BEGIN("JR d: WZ = nova adresa");
    setup();
    test_ram[0] = 0x18; test_ram[1] = 0x10;
    step();
    ASSERT_EQ(cpu->wz.w, 0x12);
    TEST_END();

    TEST_BEGIN("EX (SP),HL: WZ = nova HL");
    setup();
    cpu->sp = 0xFFF0;
    cpu->hl.w = 0x1111;
    test_ram[0xFFF0] = 0xCD; test_ram[0xFFF1] = 0xAB;
    test_ram[0] = 0xE3;
    step();
    ASSERT_EQ(cpu->wz.w, 0xABCD);
    TEST_END();

    TEST_BEGIN("IN A,(n): WZ = port + 1");
    setup();
    cpu->af.h = 0x10;
    test_ram[0] = 0xDB; test_ram[1] = 0x20;
    step();
    /* Port = 0x1020, WZ = 0x1021 */
    ASSERT_EQ(cpu->wz.w, 0x1021);
    TEST_END();

    TEST_BEGIN("OUT (n),A: WZ_lo = (port+1)&0xFF, WZ_hi = A");
    setup();
    cpu->af.h = 0x42;
    test_ram[0] = 0xD3; test_ram[1] = 0xFF;
    step();
    ASSERT_EQ(cpu->wz.l, 0x00);
    ASSERT_EQ(cpu->wz.h, 0x42);
    TEST_END();

    TEST_BEGIN("IN r,(C): WZ = BC + 1");
    setup();
    cpu->bc.w = 0x1234;
    test_ram[0] = 0xED; test_ram[1] = 0x40;
    step();
    ASSERT_EQ(cpu->wz.w, 0x1235);
    TEST_END();

    TEST_BEGIN("OUT (C),r: WZ = BC + 1");
    setup();
    cpu->bc.w = 0x1234;
    test_ram[0] = 0xED; test_ram[1] = 0x41;
    step();
    ASSERT_EQ(cpu->wz.w, 0x1235);
    TEST_END();

    TEST_BEGIN("LD A,(BC): WZ = BC + 1");
    setup();
    cpu->bc.w = 0x5000;
    test_ram[0] = 0x0A;
    step();
    ASSERT_EQ(cpu->wz.w, 0x5001);
    TEST_END();

    TEST_BEGIN("LD (BC),A: WZ_lo = (BC+1)&0xFF, WZ_hi = A");
    setup();
    cpu->af.h = 0x99;
    cpu->bc.w = 0x5000;
    test_ram[0] = 0x02;
    step();
    ASSERT_EQ(cpu->wz.l, 0x01);
    ASSERT_EQ(cpu->wz.h, 0x99);
    TEST_END();

    TEST_BEGIN("RRD: WZ = HL + 1");
    setup();
    cpu->af.h = 0x12; cpu->hl.w = 0x5000;
    test_ram[0x5000] = 0x34;
    test_ram[0] = 0xED; test_ram[1] = 0x67;
    step();
    ASSERT_EQ(cpu->wz.w, 0x5001);
    TEST_END();
}

/* ========== Q registr (SCF/CCF F3/F5) ========== */

/** Maska nedokumentovaných příznaků YF (bit 5) a XF (bit 3). */
#define Q_YX (Z80_FLAG_5 | Z80_FLAG_3)

/**
 * @brief Jeden řádek tabulky testů registru Q.
 *
 * Sekvence začíná na adrese 0000h s A i F nastavenými instrukcemi samotnými
 * (XOR A nuluje A; CP 28h uloží do YF/XF bity operandu a A nechá nulové),
 * takže F má YF/XF = 1 a A má oba bity nulové. Jen tahle kombinace odliší
 * "YF/XF jen z A" (00h) od "YF/XF z A or F" (28h).
 */
typedef struct {
    const char *name;      /**< Název testu. */
    u8 code[8];            /**< Strojový kód, nevyužité bajty jsou NOP. */
    int instructions;      /**< Počet instrukcí sekvence (pro krokování). */
    int tstates;           /**< Součet T-stavů sekvence (pro batch). */
    u8 expected_yx;        /**< Očekávané F and 28h na Zilog Z80. */
} q_case_t;

/**
 * @brief Tabulka sekvencí pro registr Q.
 *
 * Očekávané hodnoty plynou ze vzorce YF/XF = ((Q xor F) or A) and 28h
 * (redcode/Z80, MAME); stejné čtyři vzory (s LD D,D / LD E,E místo NOP)
 * dávají na skutečném MZ-800 64 shod ze 64 (program syscheck).
 */
static const q_case_t q_cases[] = {
    { "Q: XOR A; CP 28h; SCF -> jen z A",         { 0xAF, 0xFE, 0x28, 0x37 },             3, 15, 0x00 },
    { "Q: XOR A; CP 28h; NOP; SCF -> z A or F",   { 0xAF, 0xFE, 0x28, 0x00, 0x37 },       4, 19, Q_YX },
    { "Q: XOR A; CP 28h; CCF -> jen z A",         { 0xAF, 0xFE, 0x28, 0x3F },             3, 15, 0x00 },
    { "Q: XOR A; CP 28h; NOP; CCF -> z A or F",   { 0xAF, 0xFE, 0x28, 0x00, 0x3F },       4, 19, Q_YX },
    /* Druhé CP zapíše do F stejnou hodnotu: Q se přesto musí nastavit. */
    { "Q: CP 28h; CP 28h; SCF -> jen z A",        { 0xAF, 0xFE, 0x28, 0xFE, 0x28, 0x37 }, 4, 22, 0x00 },
    /* První SCF vidí Q = 0 (po NOP) a nastaví YF/XF, druhé vidí Q = F. */
    { "Q: NOP; SCF; SCF -> druhe jen z A",        { 0xAF, 0xFE, 0x28, 0x00, 0x37, 0x37 }, 5, 23, 0x00 },
    { "Q: NOP; CCF; CCF -> druhe jen z A",        { 0xAF, 0xFE, 0x28, 0x00, 0x3F, 0x3F }, 5, 23, 0x00 },
    /* Osamocený prefix DD nuluje Q (převzato z dřívější implementace). */
    { "Q: CP 28h; DD SCF -> z A or F",            { 0xAF, 0xFE, 0x28, 0xDD, 0x37 },       3, 19, Q_YX },
};

/**
 * @brief Připraví CPU a nahraje sekvenci z tabulky na adresu 0000h.
 * @param test Řádek tabulky.
 * @post PC = 0000h, zbytek RAM je vynulovaný (NOP).
 */
static void q_setup(const q_case_t *test) {
    setup();
    load_bytes(0x0000, test->code, (int)sizeof(test->code));
}

/**
 * @brief Testy registru Q - vliv na F3/F5 u SCF a CCF.
 *
 * Každou sekvenci z tabulky ověřuje třikrát: krokováním (z80_step), jednou
 * dávkou (z80_execute) a s hranicí volání těsně před poslední instrukcí
 * (batch + step). Všechny tři běhy musí dát stejný výsledek. Dále pokrývá
 * instrukce a události, po kterých je Q = 0: POP AF, EX AF,AF', přijetí
 * přerušení a zápis AF přes z80_set_reg().
 */
static void test_q_register(void) {
    for (size_t i = 0; i < sizeof(q_cases) / sizeof(q_cases[0]); i++) {
        const q_case_t *test = &q_cases[i];

        TEST_BEGIN(test->name);

        /* Krokování: každá instrukce vlastním voláním z80_step(). */
        q_setup(test);
        for (int n = 0; n < test->instructions; n++) step();
        ASSERT_EQ(cpu->af.l & Q_YX, test->expected_yx);

        /* Batch: celá sekvence jedním voláním z80_execute(). */
        q_setup(test);
        z80_execute(cpu, test->tstates);
        ASSERT_EQ(cpu->af.l & Q_YX, test->expected_yx);

        /* Hranice volání před poslední instrukcí (4 T): Q musí přežít. */
        q_setup(test);
        z80_execute(cpu, test->tstates - 4);
        step();
        ASSERT_EQ(cpu->af.l & Q_YX, test->expected_yx);

        TEST_END();
    }

    TEST_BEGIN("Q: SCF bere YF/XF z A i po instrukci se zapisem F");
    setup();
    cpu->af.h = 0x28;
    test_ram[0] = 0xB7; test_ram[1] = 0x37;  /* OR A; SCF */
    step(); step();
    ASSERT_EQ(cpu->af.l & Q_YX, Q_YX);
    ASSERT_FLAG_SET(Z80_FLAG_C);
    TEST_END();

    TEST_BEGIN("Q: SCF s A = 0 a F = 0 necha YF/XF nulove");
    setup();
    cpu->af.w = 0x0000;
    test_ram[0] = 0x00; test_ram[1] = 0x37;  /* NOP; SCF */
    step(); step();
    ASSERT_EQ(cpu->af.l & Q_YX, 0x00);
    TEST_END();

    TEST_BEGIN("Q: SCF po POP AF -> Q = 0, z A or F");
    setup();
    cpu->sp = 0xFFF0;
    test_ram[0xFFF0] = 0x28; test_ram[0xFFF1] = 0x00;  /* F = 28h, A = 00h */
    test_ram[0] = 0xF1; test_ram[1] = 0x37;  /* POP AF; SCF */
    step(); step();
    ASSERT_EQ(cpu->af.h, 0x00);
    ASSERT_EQ(cpu->af.l & Q_YX, Q_YX);
    TEST_END();

    TEST_BEGIN("Q: SCF po EX AF,AF' -> Q = 0, z A or F");
    setup();
    cpu->af.w = 0x0000;
    cpu->af2.w = 0x0028;
    test_ram[0] = 0x08; test_ram[1] = 0x37;  /* EX AF,AF'; SCF */
    step(); step();
    ASSERT_EQ(cpu->af.h, 0x00);
    ASSERT_EQ(cpu->af.l & Q_YX, Q_YX);
    TEST_END();

    TEST_BEGIN("Q: prijeti INT nuluje Q");
    setup();
    cpu->sp = 0xFFFE;
    cpu->im = 1;
    test_ram[0] = 0xAF; test_ram[1] = 0xFE; test_ram[2] = 0x28;  /* XOR A; CP 28h */
    test_ram[0x0038] = 0x37;  /* SCF jako první instrukce obsluhy */
    step(); step();
    cpu->iff1 = 1; cpu->iff2 = 1;
    z80_irq(cpu, 0xFF);
    step();  /* přijetí INT + SCF na 0038h */
    ASSERT_EQ(cpu->pc, 0x0039);
    /* Bez nulování Q by SCF vidělo Q = F po CP a vzalo YF/XF jen z A. */
    ASSERT_EQ(cpu->af.l & Q_YX, Q_YX);
    TEST_END();

    TEST_BEGIN("Q: z80_set_reg(AF) nuluje Q");
    setup();
    test_ram[0] = 0xAF; test_ram[1] = 0xFE; test_ram[2] = 0x28;  /* XOR A; CP 28h */
    test_ram[3] = 0x37;  /* SCF */
    step(); step();
    z80_set_reg(cpu, Z80_REG_AF, 0x0028);
    step();
    ASSERT_EQ(cpu->af.l & Q_YX, Q_YX);
    TEST_END();
}

/* ========== Vstupni bod ========== */

/**
 * @brief Spusti vsechny testy DAA a flagu.
 */
void test_flags_daa_all(void) {
    test_daa();
    test_undoc_flags();
    test_memptr();
    test_q_register();
}
