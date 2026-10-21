/**
 * @file test_r_register.c
 * @brief Regresni testy inkrementu registru R behem HALT a pri prijeti INT/NMI.
 *
 * Do v0.3.1 jadro nezvysovalo R pri opakovanem fetchi HALT ani pri prijeti
 * maskovatelneho preruseni a NMI (z80ex oboji dela). Kod, ktery cte LD A,R
 * po HALT cekajicim na preruseni (napr. loader odvozujici desifrovaci klic
 * z R), tak videl jinou hodnotu nez na realnem Z80.
 *
 * Ocekavane chovani:
 *   - kazdy opakovany fetch HALT (po 4 T) je M1 cyklus -> R += 1
 *   - prijeti INT (IM 0/1/2) i NMI -> R += 1
 *   - inkrementuje se jen dolnich 7 bitu, bit 7 se zachova
 *   - neprijate INT (DI) R navic nezvysi
 *
 * Pokryta je per-step cesta (z80_step) i davkova cesta (z80_execute).
 * Pozn.: step() po prijeti preruseni vykona i instrukci na vektoru (NOP),
 * ktera prida dalsi +1.
 *
 * Osamocený prefix DD/FD (bajt za ním není instrukce s IX/IY) je jeden M1
 * cyklus: DD 00 = R + 2, DD DD 00 = R + 3. Jádro dřív bajt za prefixem
 * vrátilo a načetlo znovu (R + 3 u DD 00, dvojí M1 čtení téhož bajtu).
 * Prefix a instrukce jsou jeden krok CPU.
 */

#include <stdbool.h>

#include "test_framework.h"

/**
 * @brief Pripravi HALT na 0x0100, IM 1, EI stav (iff1 = iff2 = 1), R = r0.
 * @param r0 Pocatecni hodnota registru R.
 */
static void prepare_halt(u8 r0) {
    test_ram[0x0100] = 0x76;  /* HALT */
    cpu->pc = 0x0100;
    cpu->sp = 0x8000;
    cpu->im = 1;
    cpu->iff1 = 1;
    cpu->iff2 = 1;
    cpu->r = r0;
}

/**
 * @brief HALT: fetch HALT + kazde opakovani (4 T) zvysi R o 1 (per-step).
 */
static void test_halt_refetch_increments_r_step(void) {
    TEST_BEGIN("HALT re-fetch increments R (per-step)");
    setup();
    prepare_halt(0x10);
    (void)step();             /* fetch HALT: R = 0x11 */
    ASSERT_TRUE(cpu->halted);
    ASSERT_EQ(cpu->r, 0x11);
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(step(), 4);
    }
    ASSERT_TRUE(cpu->halted);
    ASSERT_EQ(cpu->r, 0x11 + 10);
    TEST_END();
}

/**
 * @brief HALT: davkova cesta z80_execute - R roste o 1 za kazde 4 T v HALT.
 */
static void test_halt_refetch_increments_r_batch(void) {
    TEST_BEGIN("HALT re-fetch increments R (batch)");
    setup();
    cpu->iff1 = 0;            /* bez preruseni, jen cekani v HALT */
    prepare_halt(0x20);
    int cycles = 0;
    while (cycles < 4 + 40) {
        cycles += z80_execute(cpu, 4);
    }
    ASSERT_TRUE(cpu->halted);
    ASSERT_EQ(cycles, 44);
    ASSERT_EQ(cpu->r, 0x20 + 1 + 10);
    TEST_END();
}

/**
 * @brief HALT: inkrement pretece jen v dolnich 7 bitech, bit 7 zustane.
 */
static void test_halt_refetch_keeps_bit7(void) {
    TEST_BEGIN("HALT re-fetch keeps R bit 7");
    setup();
    prepare_halt(0xFE);
    (void)step();             /* HALT: 0xFE -> 0xFF */
    (void)step();             /* 0xFF -> 0x80 (bit 7 drzi) */
    ASSERT_EQ(cpu->r, 0x80);
    TEST_END();
}

/**
 * @brief Prijeti INT (IM 1) z HALT zvysi R o 1 navic k opakovanym fetchum.
 */
static void test_int_ack_im1_from_halt_increments_r(void) {
    TEST_BEGIN("INT IM1 ack from HALT increments R");
    setup();
    prepare_halt(0x30);
    test_ram[0x0038] = 0x00;  /* NOP na vektoru */
    (void)step();             /* HALT: R = 0x31 */
    (void)step();             /* opakovani: R = 0x32 */
    z80_irq(cpu, 0xFF);
    (void)step();             /* INT ack (+1) + NOP na 0x0038 (+1) */
    ASSERT_FALSE(cpu->halted);
    ASSERT_EQ(cpu->pc, 0x0039);
    ASSERT_EQ(cpu->r, 0x34);
    TEST_END();
}

/**
 * @brief Prijeti INT mimo HALT (IM 2) zvysi R o 1.
 */
static void test_int_ack_im2_increments_r(void) {
    TEST_BEGIN("INT IM2 ack outside HALT increments R");
    setup();
    cpu->pc = 0x0200;
    cpu->sp = 0x8000;
    cpu->im = 2;
    cpu->i = 0x40;
    cpu->iff1 = 1;
    cpu->iff2 = 1;
    cpu->r = 0x45;
    test_ram[0x0200] = 0x00;  /* NOP */
    test_ram[0x40FE] = 0x00;  /* IM2 tabulka: handler na 0x5000 */
    test_ram[0x40FF] = 0x50;
    test_ram[0x5000] = 0x00;  /* NOP v handleru */
    (void)step();             /* NOP: R = 0x46 */
    z80_irq(cpu, 0xFE);
    (void)step();             /* INT ack (+1) + NOP na 0x5000 (+1) */
    ASSERT_EQ(cpu->pc, 0x5001);
    ASSERT_EQ(cpu->r, 0x48);
    TEST_END();
}

/**
 * @brief Prijeti NMI z HALT zvysi R o 1 (vcetne preteceni do bitu 0-6).
 */
static void test_nmi_ack_increments_r(void) {
    TEST_BEGIN("NMI ack from HALT increments R");
    setup();
    prepare_halt(0x7F);
    test_ram[0x0066] = 0x00;  /* NOP na NMI vektoru */
    (void)step();             /* HALT: 0x7F -> 0x00 */
    ASSERT_EQ(cpu->r, 0x00);
    z80_nmi(cpu);
    (void)step();             /* NMI ack (+1) + NOP na 0x0066 (+1) */
    ASSERT_EQ(cpu->pc, 0x0067);
    ASSERT_EQ(cpu->r, 0x02);
    TEST_END();
}

/**
 * @brief Neprijate INT (DI) R navic nezvysi - jen opakovany fetch HALT.
 */
static void test_int_not_accepted_no_extra_r(void) {
    TEST_BEGIN("INT not accepted under DI adds no extra R increment");
    setup();
    prepare_halt(0x50);
    cpu->iff1 = 0;
    cpu->iff2 = 0;
    (void)step();             /* HALT: R = 0x51 */
    z80_irq(cpu, 0xFF);
    (void)step();             /* jen opakovani HALT: R = 0x52 */
    ASSERT_TRUE(cpu->halted);
    ASSERT_EQ(cpu->r, 0x52);
    TEST_END();
}

/**
 * @brief Davkova cesta: prijeti INT v z80_execute zvysi R o 1
 *        (pak se vykona jeste NOP na 0x0038, ten prida dalsi 1).
 */
static void test_int_ack_batch_increments_r(void) {
    TEST_BEGIN("INT IM1 ack in batch core increments R");
    setup();
    cpu->pc = 0x0200;
    cpu->sp = 0x8000;
    cpu->im = 1;
    cpu->iff1 = 1;
    cpu->iff2 = 1;
    cpu->r = 0x10;
    test_ram[0x0200] = 0x00;  /* NOP */
    test_ram[0x0038] = 0x00;  /* NOP na vektoru */
    z80_irq(cpu, 0xFF);
    z80_execute(cpu, 1);      /* vstupni check prijme INT, pak NOP na 0x0038 */
    ASSERT_EQ(cpu->pc, 0x0039);
    ASSERT_EQ(cpu->r, 0x12);  /* +1 INT ack, +1 NOP */
    TEST_END();
}

/* ========== Osamocený prefix DD/FD ========== */

/** Počet M1 čtení podle adresy (plní rr_mread_m1). */
static u16 rr_m1_reads[0x10000];

/**
 * @brief Callback čtení z paměti, který navíc počítá M1 čtení podle adresy.
 * @param cpu       Ukazatel na CPU instanci (nepoužit).
 * @param addr      16bitová adresa.
 * @param m1_state  1 = M1 fetch, 0 = normální čtení.
 * @param user_data Uživatelská data (nepoužita).
 * @return Bajt na dané adrese v test_ram.
 * @post Při m1_state != 0 je rr_m1_reads[addr] zvýšené o 1.
 */
static u8 rr_mread_m1(z80_t *cpu, u16 addr, int m1_state, void *user_data) {
    (void)cpu; (void)user_data;
    if (m1_state) rr_m1_reads[addr]++;
    return test_ram[addr];
}

/**
 * @brief Nová CPU instance s čítačem M1 čtení.
 * @post Paměť vynulovaná (setup()), rr_m1_reads vynulované, čtení paměti
 *       přesměrované na rr_mread_m1.
 */
static void rr_prefix_setup(void) {
    setup();
    memset(rr_m1_reads, 0, sizeof(rr_m1_reads));
    z80_set_mread(cpu, rr_mread_m1, NULL);
}

/**
 * @brief Jeden krok per-step (z80_step) nebo dávkové varianty (z80_execute, cíl 1 T).
 * @param batch false = z80_step, true = z80_execute(cpu, 1).
 * @return Počet T-stavů provedeného kroku.
 */
static int rr_run_one(bool batch) {
    return batch ? z80_execute(cpu, 1) : z80_step(cpu);
}

/**
 * @brief Uloží bajty programu na 0x0100, nastaví PC, SP, R a vynuluje čítače M1.
 * @param bytes Bajty programu.
 * @param n     Počet bajtů.
 * @param r0    Počáteční hodnota registru R.
 */
static void rr_put_prog(const u8 *bytes, size_t n, u8 r0) {
    memcpy(&test_ram[0x0100], bytes, n);
    memset(rr_m1_reads, 0, sizeof(rr_m1_reads));
    cpu->pc = 0x0100;
    cpu->sp = 0x8000;
    cpu->r = r0;
}

/**
 * @brief DD 00: prefix + NOP v jednom kroku, 8 T, R + 2, bajt 00h načtený
 *        jedním M1 cyklem.
 */
static void test_stray_dd_nop(void) {
    static const u8 prog[] = { 0xDD, 0x00 };
    TEST_BEGIN("Stray DD prefix + NOP is one step, R + 2");
    rr_prefix_setup();
    for (int batch = 0; batch < 2; batch++) {
        rr_put_prog(prog, sizeof(prog), 0x10);
        ASSERT_CYCLES(rr_run_one(batch), 8);
        ASSERT_EQ(cpu->pc, 0x0102);
        ASSERT_EQ(cpu->r, 0x12);
        ASSERT_EQ(rr_m1_reads[0x0100], 1);
        ASSERT_EQ(rr_m1_reads[0x0101], 1);
    }
    TEST_END();
}

/**
 * @brief DD DD 00 a FD 3E 55 (LD A,n za ignorovaným prefixem): R podle počtu
 *        M1 cyklů, každý bajt načtený jednou, jeden krok.
 */
static void test_stray_prefix_chain(void) {
    static const u8 prog1[] = { 0xDD, 0xDD, 0x00 };
    static const u8 prog2[] = { 0xFD, 0x3E, 0x55 };
    TEST_BEGIN("Stray prefix chain: R per M1 cycle, single fetch per byte");
    rr_prefix_setup();
    for (int batch = 0; batch < 2; batch++) {
        rr_put_prog(prog1, sizeof(prog1), 0x7E);
        ASSERT_CYCLES(rr_run_one(batch), 12);
        ASSERT_EQ(cpu->pc, 0x0103);
        ASSERT_EQ(cpu->r, 0x01);                 /* 7Eh + 3, bit 7 drží 0 */
        for (u16 a = 0x0100; a < 0x0103; a++) {
            ASSERT_EQ(rr_m1_reads[a], 1);
        }

        rr_put_prog(prog2, sizeof(prog2), 0x80);
        ASSERT_CYCLES(rr_run_one(batch), 11);    /* 4 + LD A,n 7 */
        ASSERT_EQ(cpu->pc, 0x0103);
        ASSERT_EQ(cpu->af.h, 0x55);
        ASSERT_EQ(cpu->r, 0x82);                 /* bit 7 drží 1 */
        ASSERT_EQ(rr_m1_reads[0x0101], 1);
        ASSERT_EQ(rr_m1_reads[0x0102], 0);       /* operand, ne M1 */
    }
    TEST_END();
}

/**
 * @brief FD DD 21 34 12: platí poslední prefix, LD IX,1234h; IY beze změny,
 *        R + 3, 4 + 14 T.
 */
static void test_stray_prefix_last_wins(void) {
    static const u8 prog[] = { 0xFD, 0xDD, 0x21, 0x34, 0x12 };
    TEST_BEGIN("FD DD prefix chain: last prefix wins");
    rr_prefix_setup();
    for (int batch = 0; batch < 2; batch++) {
        rr_put_prog(prog, sizeof(prog), 0x20);
        cpu->ix.w = 0x0000;
        cpu->iy.w = 0xBEEF;
        ASSERT_CYCLES(rr_run_one(batch), 18);
        ASSERT_EQ(cpu->pc, 0x0105);
        ASSERT_EQ(cpu->ix.w, 0x1234);
        ASSERT_EQ(cpu->iy.w, 0xBEEF);
        ASSERT_EQ(cpu->r, 0x23);
        ASSERT_EQ(rr_m1_reads[0x0101], 1);
        ASSERT_EQ(rr_m1_reads[0x0102], 1);
    }
    TEST_END();
}

/**
 * @brief DD ED 44 (NEG): ED prefix DD ruší, R + 3, 4 + 8 T.
 */
static void test_stray_prefix_before_ed(void) {
    static const u8 prog[] = { 0xDD, 0xED, 0x44 };
    TEST_BEGIN("Stray DD prefix before ED NEG");
    rr_prefix_setup();
    for (int batch = 0; batch < 2; batch++) {
        rr_put_prog(prog, sizeof(prog), 0x00);
        cpu->af.h = 0x01;
        ASSERT_CYCLES(rr_run_one(batch), 12);
        ASSERT_EQ(cpu->pc, 0x0103);
        ASSERT_EQ(cpu->af.h, 0xFF);
        ASSERT_EQ(cpu->r, 0x03);
        ASSERT_EQ(rr_m1_reads[0x0101], 1);
    }
    TEST_END();
}

/**
 * @brief Přerušení se mezi osamoceným prefixem a instrukcí nepřijme.
 *
 * Krok DD 00 provede prefix i NOP (8 T). INT vyvolané po něm se přijme
 * v příštím kroku a návratová adresa na zásobníku je 0102h (za NOP), ne
 * 0101h. Na vektoru 0038h je HALT, aby PC po kroku zůstalo na 0038h.
 */
static void test_stray_prefix_no_int_between(void) {
    static const u8 prog[] = { 0xDD, 0x00, 0x00 };
    TEST_BEGIN("No INT acceptance between stray prefix and instruction");
    rr_prefix_setup();
    rr_put_prog(prog, sizeof(prog), 0x00);
    test_ram[0x0038] = 0x76;  /* HALT na vektoru IM 1 */
    cpu->im = 1;
    cpu->iff1 = 1; cpu->iff2 = 1;
    ASSERT_CYCLES(step(), 8);
    z80_int(cpu);
    (void)step();             /* přijetí INT + HALT na 0x0038 */
    ASSERT_EQ(cpu->iff1, 0);  /* INT přijato */
    ASSERT_EQ(cpu->pc, 0x0038);
    ASSERT_EQ(test_ram[0x7FFE], 0x02);
    ASSERT_EQ(test_ram[0x7FFF], 0x01);
    TEST_END();
}

/**
 * @brief Dlouhý řetěz: 100 x DD a NOP.
 *
 * Krok pojme nejvýš 64 prefixů, pak se rozdělí; R a T-stavy celkem
 * odpovídají 101 M1 cyklům (R se při rozdělení nezdvojí), řetěz skončí
 * za NOP.
 */
static void test_stray_prefix_long_chain_splits(void) {
    u8 prog[101];
    memset(prog, 0xDD, 100);
    prog[100] = 0x00;
    TEST_BEGIN("Long stray prefix chain splits without doubling R");
    rr_prefix_setup();
    for (int batch = 0; batch < 2; batch++) {
        rr_put_prog(prog, sizeof(prog), 0x00);
        int steps = 0, cycles = 0;
        while (cpu->pc != 0x0100 + 101 && steps < 10) {
            cycles += rr_run_one(batch);
            steps++;
        }
        ASSERT_EQ(cpu->pc, 0x0100 + 101);
        ASSERT_EQ(steps, 2);
        ASSERT_EQ(cycles, 101 * 4);
        ASSERT_EQ(cpu->r, 101 & 0x7F);
    }
    TEST_END();
}

/* ========== Hlavni runner ========== */

/**
 * @brief Spusti vsechny testy registru R.
 */
void test_r_register_all(void) {
    test_halt_refetch_increments_r_step();
    test_halt_refetch_increments_r_batch();
    test_halt_refetch_keeps_bit7();
    test_int_ack_im1_from_halt_increments_r();
    test_int_ack_im2_increments_r();
    test_nmi_ack_increments_r();
    test_int_not_accepted_no_extra_r();
    test_int_ack_batch_increments_r();
    test_stray_dd_nop();
    test_stray_prefix_chain();
    test_stray_prefix_last_wins();
    test_stray_prefix_before_ed();
    test_stray_prefix_no_int_between();
    test_stray_prefix_long_chain_splits();
}
