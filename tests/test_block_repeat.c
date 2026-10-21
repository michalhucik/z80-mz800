/**
 * @file test_block_repeat.c
 * @brief Testy příznaků v opakující iteraci blokových instrukcí Z80.
 *
 * Když se LDIR, LDDR, CPIR, CPDR, INIR, INDR, OTIR nebo OTDR opakuje, 5T
 * cyklus, který vrací PC o 2 zpět, přepíše část F (měření na reálných
 * procesorech Z80, ověřeno programem BLKFLAGS na MZ-800):
 *   - všech 8 instrukcí: YF (bit 5) = PC.13, XF (bit 3) = PC.11, kde PC je
 *     adresa prefixu EDh,
 *   - INxR a OTxR navíc mění PF a HF podle CF, bitu 7 přeneseného bajtu
 *     a dolních bitů už dekrementovaného B.
 * Program to vidí, když se instrukce zastaví mezi iteracemi (přijetí INT
 * nebo NMI, krok v debuggeru, instrukce přepíše svůj vlastní opkód).
 * Poslední (neopakující) iterace a jednorázové LDI ... OUTD se nemění.
 *
 * Očekávané hodnoty jsou měření, ne výpočet z modelu jádra:
 *   - tabulky "MZ-800": výsledek BLKFLAGS na skutečném MZ-800 (obsluha IM 1
 *     z CTC2 a instrukce, která přepíše svůj druhý bajt), B a L u OTxR jsou
 *     hodnoty zachycené obsluhou přerušení,
 *   - tabulka "Ped7g": Z80 Block Flags Test v5.0, hodnoty z reálných čipů Z80
 *     (A = 0A1h, F = 00h, BC = 16, data 0DEh).
 *
 * Každý případ běží v per-step variantě jádra (z80_step) i v dávkové
 * variantě (z80_execute); obě vznikají ze z80_execute.inc.
 */

#include <stdbool.h>

#include "test_framework.h"

/* ========== Port a pomocné funkce ========== */

/** Hodnota, kterou vrací čtení libovolného portu (br_pread). */
static u8 br_port_in = 0xFF;

/**
 * @brief Callback čtení z I/O portu - vrací br_port_in pro každý port.
 * @param cpu       Ukazatel na CPU instanci (nepoužit).
 * @param port      16bitová adresa portu (nepoužita).
 * @param user_data Uživatelská data (nepoužita).
 * @return Hodnota br_port_in.
 */
static u8 br_pread(z80_t *cpu, u16 port, void *user_data) {
    (void)cpu; (void)port; (void)user_data;
    return br_port_in;
}

/**
 * @brief Nová CPU instance pro testy blokových instrukcí.
 * @post Paměť vynulovaná (setup()), čtení portu přesměrované na br_pread,
 *       br_port_in = 0FFh, SP = 0F000h. Přerušení nejsou povolená, takže
 *       z80_execute(cpu, 1) provede právě jednu iteraci.
 */
static void br_setup(void) {
    setup();
    br_port_in = 0xFF;
    z80_set_pread(cpu, br_pread, NULL);
    cpu->sp = 0xF000;
}

/**
 * @brief Provede jednu instrukci (jednu iteraci blokové instrukce).
 * @param batch false = per-step varianta (z80_step), true = dávková
 *              varianta (z80_execute s cílem 1 T, skončí po první instrukci).
 * @return Počet T-stavů provedené instrukce.
 */
static int br_run_one(bool batch) {
    return batch ? z80_execute(cpu, 1) : z80_step(cpu);
}

/**
 * @brief Uloží blokovou instrukci ED op na adresu pc a nastaví PC.
 * @param pc Adresa prefixu EDh.
 * @param op Druhý bajt opkódu.
 */
static void br_put_block_insn(u16 pc, u8 op) {
    test_ram[pc] = 0xED;
    test_ram[(u16)(pc + 1)] = op;
    cpu->pc = pc;
}

/**
 * @brief Začátek jednoho případu tabulky uvnitř testu.
 *
 * Uloží dosavadní příznak selhání testu a vynuluje ho, aby br_case_end()
 * poznal selhání právě tohoto případu.
 *
 * @return Příznak selhání testu před případem (předat br_case_end()).
 */
static int br_case_begin(void) {
    int failed_before = current_test_failed;
    current_test_failed = 0;
    return failed_before;
}

/**
 * @brief Konec jednoho případu tabulky - při selhání vypíše jeho popis.
 * @param failed_before Návratová hodnota br_case_begin().
 * @param msg           Popis případu (index, opkód, PC, varianta jádra).
 * @post current_test_failed je nenulový, pokud selhal tento nebo některý
 *       dřívější případ testu.
 */
static void br_case_end(int failed_before, const char *msg) {
    if (current_test_failed) {
        printf("    -> %s\n", msg);
    }
    current_test_failed |= failed_before;
}

/* ========== LDxR a CPxR ========== */

/** Případ LDxR/CPxR: instrukce na pc, BC = 16, (HL) = data, F = 00h. */
typedef struct {
    u8  op;     /**< Druhý bajt opkódu (B0h LDIR, B1h CPIR, B8h LDDR, B9h CPDR). */
    u16 pc;     /**< Adresa prefixu EDh. */
    u8  a;      /**< Registr A. */
    u8  data;   /**< Bajt na (HL). */
    u8  exp_f;  /**< F po opakující iteraci (měřeno). */
} br_ldcp_case_t;

/** Případy LDxR/CPxR s naměřeným F. */
static const br_ldcp_case_t br_ldcp_cases[] = {
    /* Ped7g, reálné čipy Z80 */
    { 0xB0, 0x8002, 0xA1, 0xDE, 0x04 }, { 0xB0, 0x8802, 0xA1, 0xDE, 0x0C },
    { 0xB0, 0xA002, 0xA1, 0xDE, 0x24 }, { 0xB0, 0xA802, 0xA1, 0xDE, 0x2C },
    { 0xB8, 0x8002, 0xA1, 0xDE, 0x04 }, { 0xB8, 0x8802, 0xA1, 0xDE, 0x0C },
    { 0xB8, 0xA002, 0xA1, 0xDE, 0x24 }, { 0xB8, 0xA802, 0xA1, 0xDE, 0x2C },
    { 0xB1, 0x8002, 0xA1, 0xDE, 0x96 }, { 0xB1, 0x8802, 0xA1, 0xDE, 0x9E },
    { 0xB1, 0xA002, 0xA1, 0xDE, 0xB6 }, { 0xB1, 0xA802, 0xA1, 0xDE, 0xBE },
    { 0xB9, 0x8002, 0xA1, 0xDE, 0x96 }, { 0xB9, 0x8802, 0xA1, 0xDE, 0x9E },
    { 0xB9, 0xA002, 0xA1, 0xDE, 0xB6 }, { 0xB9, 0xA802, 0xA1, 0xDE, 0xBE },
    /* MZ-800, BLKFLAGS část B (přerušení IM 1) */
    { 0xB1, 0x402F, 0xA1, 0xDE, 0x96 }, { 0xB1, 0x482F, 0xA1, 0xDE, 0x9E },
    { 0xB1, 0x602F, 0xA1, 0xDE, 0xB6 }, { 0xB1, 0x682F, 0xA1, 0xDE, 0xBE },
    { 0xB9, 0x402F, 0xA1, 0xDE, 0x96 }, { 0xB9, 0x482F, 0xA1, 0xDE, 0x9E },
    { 0xB9, 0x602F, 0xA1, 0xDE, 0xB6 }, { 0xB9, 0x682F, 0xA1, 0xDE, 0xBE },
    { 0xB0, 0x402F, 0x0A, 0x00, 0x04 }, { 0xB0, 0x482F, 0x0A, 0x00, 0x0C },
    { 0xB0, 0x602F, 0x0A, 0x00, 0x24 }, { 0xB0, 0x682F, 0x0A, 0x00, 0x2C },
};

/**
 * @brief Projde tabulku LDxR/CPxR jednou variantou jádra.
 * @param batch false = z80_step, true = z80_execute(cpu, 1).
 */
static void br_check_ldcp_cases(bool batch) {
    for (size_t i = 0; i < sizeof(br_ldcp_cases) / sizeof(br_ldcp_cases[0]); i++) {
        const br_ldcp_case_t *c = &br_ldcp_cases[i];
        char msg[64];
        snprintf(msg, sizeof(msg), "case %u op %02X pc %04X batch %d",
                 (unsigned)i, c->op, c->pc, (int)batch);
        int failed_before = br_case_begin();
        memset(test_ram, 0, sizeof(test_ram));
        br_put_block_insn(c->pc, c->op);
        cpu->af.h = c->a;
        cpu->af.l = 0x00;
        cpu->bc.w = 16;
        cpu->hl.w = 0x3100;
        cpu->de.w = 0x3200;
        test_ram[0x3100] = c->data;

        ASSERT_CYCLES(br_run_one(batch), 21);
        ASSERT_EQ(cpu->pc, c->pc);
        ASSERT_EQ(cpu->bc.w, 15);
        ASSERT_FLAGS(c->exp_f);
        br_case_end(failed_before, msg);
    }
}

/** @brief LDxR a CPxR, opakující iterace, per-step jádro. */
static void test_ldcp_repeat_flags_step(void) {
    TEST_BEGIN("LDxR/CPxR repeat iteration flags (per-step)");
    br_setup();
    br_check_ldcp_cases(false);
    TEST_END();
}

/** @brief LDxR a CPxR, opakující iterace, dávkové jádro. */
static void test_ldcp_repeat_flags_batch(void) {
    TEST_BEGIN("LDxR/CPxR repeat iteration flags (batch)");
    br_setup();
    br_check_ldcp_cases(true);
    TEST_END();
}

/* ========== INxR a OTxR ========== */

/**
 * @brief Případ INxR/OTxR (hodnoty z BLKFLAGS na MZ-800).
 *
 * INxR čte port C (test vrací data), OTxR posílá (HL) = data. F před
 * instrukcí je 00h; INI/OUTI F nečtou, takže na něm nezáleží.
 */
typedef struct {
    u8  op;     /**< B2h INIR, B3h OTIR, BAh INDR, BBh OTDR. */
    u16 pc;     /**< Adresa prefixu EDh. */
    u8  f;      /**< F před instrukcí. */
    u8  b;      /**< B před instrukcí (iterace ho sníží o 1). */
    u8  c;      /**< C (port). */
    u16 hl;     /**< HL před instrukcí. */
    u8  data;   /**< Přenesený bajt. */
    u8  exp_f;  /**< F po opakující iteraci (měřeno na MZ-800). */
} br_io_case_t;

/** Případy INxR/OTxR s naměřeným F. */
static const br_io_case_t br_io_cases[] = {
    /* Část A: instrukce přepíše svůj druhý bajt, port 0D1h vrací 0FFh. */
    { 0xB2, 0x4010, 0x00, 0x02, 0xD1, 0x4030, 0xFF, 0x07 },
    { 0xB2, 0x4810, 0x00, 0x02, 0xD1, 0x4830, 0xFF, 0x0F },
    { 0xB2, 0x6010, 0x00, 0x02, 0xD1, 0x6030, 0xFF, 0x27 },
    { 0xB2, 0x6810, 0x00, 0x02, 0xD1, 0x6830, 0xFF, 0x2F },
    { 0xB2, 0x4010, 0x00, 0x11, 0xD1, 0x4030, 0xFF, 0x13 },
    { 0xB2, 0x4810, 0x00, 0x11, 0xD1, 0x4830, 0xFF, 0x1B },
    { 0xB2, 0x6010, 0x00, 0x11, 0xD1, 0x6030, 0xFF, 0x33 },
    { 0xB2, 0x6810, 0x00, 0x11, 0xD1, 0x6830, 0xFF, 0x3B },
    { 0xBA, 0x4010, 0x00, 0x02, 0xD1, 0x4030, 0xFF, 0x07 },
    { 0xBA, 0x4810, 0x00, 0x02, 0xD1, 0x4830, 0xFF, 0x0F },
    { 0xBA, 0x6010, 0x00, 0x02, 0xD1, 0x6030, 0xFF, 0x27 },
    { 0xBA, 0x6810, 0x00, 0x02, 0xD1, 0x6830, 0xFF, 0x2F },
    { 0xBA, 0x4010, 0x00, 0x11, 0xD1, 0x4030, 0xFF, 0x13 },
    { 0xBA, 0x4810, 0x00, 0x11, 0xD1, 0x4830, 0xFF, 0x1B },
    { 0xBA, 0x6010, 0x00, 0x11, 0xD1, 0x6030, 0xFF, 0x33 },
    { 0xBA, 0x6810, 0x00, 0x11, 0xD1, 0x6830, 0xFF, 0x3B },
    /* Část B: přerušení IM 1, B a L zachycené obsluhou. */
    { 0xB3, 0x402F, 0x00, 0x9B, 0xF1, 0x9065, 0x00, 0x80 },
    { 0xB3, 0x482F, 0x00, 0x9B, 0xF1, 0x9065, 0x00, 0x88 },
    { 0xB3, 0x602F, 0x00, 0x9B, 0xF1, 0x9065, 0x00, 0xA0 },
    { 0xB3, 0x682F, 0x00, 0x9A, 0xF1, 0x9066, 0x00, 0xAC },
    { 0xB3, 0x402F, 0x00, 0xA1, 0xF1, 0x906F, 0xFF, 0x97 },
    { 0xB3, 0x482F, 0x00, 0xA2, 0xF1, 0x906E, 0xFF, 0x8B },
    { 0xB3, 0x602F, 0x00, 0xA1, 0xF1, 0x906F, 0xFF, 0xB7 },
    { 0xB3, 0x682F, 0x00, 0xA0, 0xF1, 0x9070, 0xFF, 0xAF },
    { 0xB3, 0x402F, 0x00, 0x25, 0xF1, 0x90DB, 0x47, 0x05 },
    { 0xB3, 0x482F, 0x00, 0x24, 0xF1, 0x90DC, 0x47, 0x09 },
    { 0xB3, 0x602F, 0x00, 0x23, 0xF1, 0x90DD, 0x47, 0x25 },
    { 0xB3, 0x682F, 0x00, 0x22, 0xF1, 0x90DE, 0x47, 0x29 },
    { 0xBB, 0x402F, 0x00, 0xA0, 0xF1, 0x90A0, 0x00, 0x84 },
    { 0xBB, 0x482F, 0x00, 0xA0, 0xF1, 0x90A0, 0x00, 0x8C },
    { 0xBB, 0x602F, 0x00, 0x9F, 0xF1, 0x909F, 0x00, 0xA0 },
    { 0xBB, 0x682F, 0x00, 0x9E, 0xF1, 0x909E, 0x00, 0xA8 },
    { 0xBB, 0x402F, 0x00, 0xA2, 0xF1, 0x90A2, 0xFF, 0x83 },
    { 0xBB, 0x482F, 0x00, 0xA2, 0xF1, 0x90A2, 0xFF, 0x8B },
    { 0xBB, 0x602F, 0x00, 0xA2, 0xF1, 0x90A2, 0xFF, 0xA3 },
    { 0xBB, 0x682F, 0x00, 0xA1, 0xF1, 0x90A1, 0xFF, 0xBF },
    { 0xBB, 0x402F, 0x00, 0x23, 0xF1, 0x90E2, 0x47, 0x05 },
    { 0xBB, 0x482F, 0x00, 0x22, 0xF1, 0x90E1, 0x47, 0x0D },
    { 0xBB, 0x602F, 0x00, 0x20, 0xF1, 0x90DF, 0x47, 0x31 },
    { 0xBB, 0x682F, 0x00, 0x2A, 0xF1, 0x90E9, 0x47, 0x29 },
};

/**
 * @brief Připraví případ INxR/OTxR do paměti a registrů.
 * @param c Případ z tabulky br_io_cases.
 * @post Paměť vynulovaná kromě instrukce a (HL) = data, port vrací data.
 */
static void br_setup_io_case(const br_io_case_t *c) {
    memset(test_ram, 0, sizeof(test_ram));
    br_put_block_insn(c->pc, c->op);
    cpu->af.l = c->f;
    cpu->bc.h = c->b;
    cpu->bc.l = c->c;
    cpu->hl.w = c->hl;
    test_ram[c->hl] = c->data;
    br_port_in = c->data;
}

/**
 * @brief Projde tabulku INxR/OTxR jednou variantou jádra.
 * @param batch false = z80_step, true = z80_execute(cpu, 1).
 */
static void br_check_io_cases(bool batch) {
    for (size_t i = 0; i < sizeof(br_io_cases) / sizeof(br_io_cases[0]); i++) {
        const br_io_case_t *c = &br_io_cases[i];
        char msg[64];
        snprintf(msg, sizeof(msg), "case %u op %02X pc %04X batch %d",
                 (unsigned)i, c->op, c->pc, (int)batch);
        int failed_before = br_case_begin();
        br_setup_io_case(c);

        ASSERT_CYCLES(br_run_one(batch), 21);
        ASSERT_EQ(cpu->pc, c->pc);
        ASSERT_EQ(cpu->bc.h, (u8)(c->b - 1));
        ASSERT_FLAGS(c->exp_f);
        br_case_end(failed_before, msg);
    }
}

/** @brief INxR a OTxR, opakující iterace, per-step jádro. */
static void test_io_repeat_flags_step(void) {
    TEST_BEGIN("INxR/OTxR repeat iteration flags (per-step)");
    br_setup();
    br_check_io_cases(false);
    TEST_END();
}

/** @brief INxR a OTxR, opakující iterace, dávkové jádro. */
static void test_io_repeat_flags_batch(void) {
    TEST_BEGIN("INxR/OTxR repeat iteration flags (batch)");
    br_setup();
    br_check_io_cases(true);
    TEST_END();
}

/* ========== Instrukce přepíše svůj druhý bajt (metoda z80test, BLKFLAGS část A) ========== */

/**
 * @brief LDIR s BC = 2 zkopíruje 00h na svůj druhý bajt.
 *
 * Opakující iterace nastaví F, CPU pak načte ED 00h (NOP', 8 T), který F
 * nemění. Hodnoty F z MZ-800 (A = 0Ah).
 */
static void test_ldir_self_overwrite(void) {
    static const u16 pcs[] = { 0x4010, 0x4810, 0x6010, 0x6810 };
    static const u8  exp_f[] = { 0x04, 0x0C, 0x24, 0x2C };

    TEST_BEGIN("LDIR overwriting its own opcode keeps repeat flags");
    br_setup();
    for (int batch = 0; batch < 2; batch++) {
        for (size_t i = 0; i < 4; i++) {
            char msg[48];
            snprintf(msg, sizeof(msg), "pc %04X batch %d", pcs[i], batch);
            int failed_before = br_case_begin();
            memset(test_ram, 0, sizeof(test_ram));
            br_put_block_insn(pcs[i], 0xB0);
            cpu->af.h = 0x0A;
            cpu->af.l = 0x00;
            cpu->bc.w = 2;
            cpu->hl.w = 0x3100;                    /* zdroj: 00h */
            cpu->de.w = (u16)(pcs[i] + 1);         /* cíl: druhý bajt LDIR */

            ASSERT_CYCLES(br_run_one(batch), 21);
            ASSERT_EQ(test_ram[pcs[i] + 1], 0x00);
            ASSERT_CYCLES(br_run_one(batch), 8);   /* ED 00h = NOP' */
            ASSERT_EQ(cpu->pc, (u16)(pcs[i] + 2));
            ASSERT_FLAGS(exp_f[i]);
            br_case_end(failed_before, msg);
        }
    }
    TEST_END();
}

/* ========== Poslední iterace a ukončení beze změny ========== */

/**
 * @brief Poslední iterace (BC = 1 resp. B = 1) a CPIR se shodou.
 *
 * F jako u LDI/CPI/INI/OUTI (S. Young, The Undocumented Z80 Documented
 * v0.91), PC za instrukcí, 16 T. Jádro je před opravou počítalo stejně;
 * YF/XF tu nesmí záviset na adrese instrukce.
 */
static void test_last_iteration_unchanged(void) {
    static const u8 in_ops[] = { 0xB2, 0xBA };

    TEST_BEGIN("Block instruction last iteration flags unchanged");
    br_setup();
    for (int batch = 0; batch < 2; batch++) {
        /* LDIR, BC = 1, A = 0Ah, data 00h: n = 0Ah -> YF (bit 1), XF (bit 3) */
        memset(test_ram, 0, sizeof(test_ram));
        br_put_block_insn(0x8002, 0xB0);
        cpu->af.h = 0x0A; cpu->af.l = 0x00;
        cpu->bc.w = 1; cpu->hl.w = 0x3100; cpu->de.w = 0x3200;
        ASSERT_CYCLES(br_run_one(batch), 16);
        ASSERT_EQ(cpu->pc, 0x8004);
        ASSERT_FLAGS(0x28);

        /* CPIR, BC = 1, A = 0A1h, data 0DEh */
        memset(test_ram, 0, sizeof(test_ram));
        br_put_block_insn(0xA802, 0xB1);
        cpu->af.h = 0xA1; cpu->af.l = 0x00;
        cpu->bc.w = 1; cpu->hl.w = 0x3100;
        test_ram[0x3100] = 0xDE;
        ASSERT_CYCLES(br_run_one(batch), 16);
        ASSERT_EQ(cpu->pc, 0xA804);
        ASSERT_FLAGS(0xB2);

        /* CPIR se shodou A = (HL) při BC > 1: konec bez opakování */
        memset(test_ram, 0, sizeof(test_ram));
        br_put_block_insn(0xA802, 0xB1);
        cpu->af.h = 0xDE; cpu->af.l = 0x00;
        cpu->bc.w = 16; cpu->hl.w = 0x3100;
        test_ram[0x3100] = 0xDE;
        ASSERT_CYCLES(br_run_one(batch), 16);
        ASSERT_EQ(cpu->pc, 0xA804);
        ASSERT_FLAGS(0x46);

        /* OTIR, B = 1, data 0FFh, L po inkrementu 11h */
        memset(test_ram, 0, sizeof(test_ram));
        br_put_block_insn(0xA802, 0xB3);
        cpu->af.l = 0x00;
        cpu->bc.h = 0x01; cpu->bc.l = 0xF1; cpu->hl.w = 0x3110;
        test_ram[0x3110] = 0xFF;
        ASSERT_CYCLES(br_run_one(batch), 16);
        ASSERT_EQ(cpu->pc, 0xA804);
        ASSERT_FLAGS(0x57);

        /* INIR a INDR, B = 1, C = 0D1h, data 0FFh */
        for (int k = 0; k < 2; k++) {
            memset(test_ram, 0, sizeof(test_ram));
            br_put_block_insn(0xA802, in_ops[k]);
            cpu->af.l = 0x00;
            cpu->bc.h = 0x01; cpu->bc.l = 0xD1; cpu->hl.w = 0x3100;
            br_port_in = 0xFF;
            ASSERT_CYCLES(br_run_one(batch), 16);
            ASSERT_EQ(cpu->pc, 0xA804);
            ASSERT_FLAGS(0x53);
        }
    }
    TEST_END();
}

/* ========== MEMPTR (WZ) ========== */

/**
 * @brief MEMPTR v opakující iteraci INIR/INDR/OTIR/OTDR.
 *
 * MEMPTR v opakující iteraci je adresa instrukce + 1, stejně jako
 * u LDxR/CPxR (měřeno na MZ-800, BLKFLAGS část A, a na procesorech Zilog
 * a NEC). Poslední iterace nechává MEMPTR jako INI/IND/OUTI/OUTD: BC +- 1
 * (INxR B před dekrementem, OTxR po něm).
 */
static void test_io_repeat_memptr(void) {
    static const u8 ops[] = { 0xB2, 0xBA, 0xB3, 0xBB };

    TEST_BEGIN("INxR/OTxR MEMPTR in repeat and last iteration");
    br_setup();
    for (int batch = 0; batch < 2; batch++) {
        for (size_t i = 0; i < 4; i++) {
            const u8 op = ops[i];
            const bool is_in = (op == 0xB2 || op == 0xBA);
            const bool inc = (op == 0xB2 || op == 0xB3);
            char msg[48];
            snprintf(msg, sizeof(msg), "op %02X batch %d", op, batch);
            int failed_before = br_case_begin();

            /* Opakující iterace: B 10h -> 0Fh */
            memset(test_ram, 0, sizeof(test_ram));
            br_put_block_insn(0x6810, op);
            cpu->bc.h = 0x10; cpu->bc.l = 0xD1; cpu->hl.w = 0x3100;
            cpu->wz.w = 0x0000;
            ASSERT_CYCLES(br_run_one(batch), 21);
            ASSERT_EQ(cpu->pc, 0x6810);
            ASSERT_EQ(cpu->wz.w, 0x6811);

            /* Poslední iterace: B 01h -> 00h */
            memset(test_ram, 0, sizeof(test_ram));
            br_put_block_insn(0x6810, op);
            cpu->bc.h = 0x01; cpu->bc.l = 0xD1; cpu->hl.w = 0x3100;
            cpu->wz.w = 0x0000;
            ASSERT_CYCLES(br_run_one(batch), 16);
            ASSERT_EQ(cpu->pc, 0x6812);
            const u16 bc = is_in ? 0x01D1 : 0x00D1;
            ASSERT_EQ(cpu->wz.w, (u16)(inc ? bc + 1 : bc - 1));
            br_case_end(failed_before, msg);
        }
    }
    TEST_END();
}

/* ========== Hlavní runner ========== */

/**
 * @brief Spustí všechny testy příznaků a MEMPTR v opakující iteraci blokových instrukcí.
 */
void test_block_repeat_all(void) {
    test_ldcp_repeat_flags_step();
    test_ldcp_repeat_flags_batch();
    test_io_repeat_flags_step();
    test_io_repeat_flags_batch();
    test_ldir_self_overwrite();
    test_last_iteration_unchanged();
    test_io_repeat_memptr();
}
