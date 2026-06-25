/*
 * Copyright (c) 2026 Michal Hucik
 * SPDX-License-Identifier: MIT
 * https://github.com/michalhucik/z80-mz800
 */
/**
 * @file z80.c
 * @brief cpu-z80 - Presny a rychly multi-instance Z80A emulator.
 *
 * Callbacky jsou ulozeny v z80_t strukture - kazda instance ma vlastni.
 * V z80_execute() se callback pointery cachuji do lokalnich promennych
 * pro eliminaci opakovane dereference cpu-> (nulovy overhead).
 *
 * Optimalizace:
 * - Computed goto dispatch (GCC/Clang)
 * - Dve varianty jadra z jednoho zdroje (z80_execute.inc): batch
 *   (registrova cache, z80_execute) a per-step (registry primo v cpu->,
 *   z80_step) - per-step jadro nema prolog/epilog cache, ideal pro
 *   cycle-accurate hostitelske smycky vyhodnocujici boundary po kazde
 *   instrukci.
 * - Volitelny RAM-access fast-path (CPU_Z80_RAM_FASTPATH): inline page-table
 *   pristup do ciste RAM misto callbacku (opt-in pres z80_set_ram_fastpath).
 * - Lokalni cache callback pointeru
 * - GCC atributy: hot, likely/unlikely
 * - Eliminace null-checku callbacku (default handlery)
 * - DAA lookup tabulka (2048 zaznamu)
 * - Inline prefix handlery (CB/ED/DD/FD/DDCB/FDCB)
 *
 * @version v0.3.1
 */

#include "cpu/z80.h"
#include <string.h>
#include <stdlib.h>

/* Compiler hints pro agresivni optimalizaci */
#if defined(__GNUC__) || defined(__clang__)
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#define HOT         __attribute__((hot))
#define NOINLINE    __attribute__((noinline))
#define ALWAYS_INLINE __attribute__((always_inline)) static inline
#else
#define LIKELY(x)   (x)
#define UNLIKELY(x) (x)
#define HOT
#define NOINLINE
#define ALWAYS_INLINE static inline
#endif

/* ========== Default callbacky ========== */

/**
 * @brief Default callback pro cteni z pameti - vraci 0xFF (odpojeny bus).
 * @param cpu Ukazatel na CPU instanci (ignorovan).
 * @param addr Adresa (ignorovana).
 * @param m1_state M1 stav (ignorovan).
 * @param user_data User data (ignorovana).
 * @return Vzdy 0xFF.
 */
static u8 default_mread(z80_t *cpu, u16 addr, int m1_state, void *user_data) {
    (void)cpu; (void)addr; (void)m1_state; (void)user_data; return 0xFF;
}

/**
 * @brief Default callback pro zapis do pameti - no-op.
 * @param cpu Ukazatel na CPU instanci (ignorovan).
 * @param addr Adresa (ignorovana).
 * @param value Data (ignorovana).
 * @param user_data User data (ignorovana).
 */
static void default_mwrite(z80_t *cpu, u16 addr, u8 value, void *user_data) {
    (void)cpu; (void)addr; (void)value; (void)user_data;
}

/**
 * @brief Default callback pro cteni z I/O portu - vraci 0xFF.
 * @param cpu Ukazatel na CPU instanci (ignorovan).
 * @param port Adresa portu (ignorovana).
 * @param user_data User data (ignorovana).
 * @return Vzdy 0xFF.
 */
static u8 default_pread(z80_t *cpu, u16 port, void *user_data) {
    (void)cpu; (void)port; (void)user_data; return 0xFF;
}

/**
 * @brief Default callback pro zapis na I/O port - no-op.
 * @param cpu Ukazatel na CPU instanci (ignorovan).
 * @param port Adresa portu (ignorovana).
 * @param value Data (ignorovana).
 * @param user_data User data (ignorovana).
 */
static void default_pwrite(z80_t *cpu, u16 port, u8 value, void *user_data) {
    (void)cpu; (void)port; (void)value; (void)user_data;
}

/* ========== Lookup tabulky ========== */

/** Paritni tabulka: PF pokud sudy pocet jednickovych bitu */
static u8 parity_table[256];

/** SZ53P tabulka: flagy S, Z, F5, F3, P pro danou hodnotu */
static u8 sz53p_table[256];

/** SZ53 tabulka: flagy S, Z, F5, F3 (bez parity) */
static u8 sz53_table[256];

/**
 * @brief DAA lookup tabulka.
 *
 * 2048 zaznamu indexovanych: A | (carry << 8) | (half << 9) | (sub << 10).
 * Kazdy zaznam obsahuje novou hodnotu A (dolnich 8 bitu) a novy F (hornich 8 bitu).
 */
static u16 daa_table[2048];

/* Flagove konstanty - kratke aliasy */
#define CF  Z80_FLAG_C
#define NF  Z80_FLAG_N
#define PF  Z80_FLAG_PV
#define F3  Z80_FLAG_3
#define HF  Z80_FLAG_H
#define F5  Z80_FLAG_5
#define ZF  Z80_FLAG_Z
#define SF  Z80_FLAG_S

/**
 * @brief Inicializace vsech lookup tabulek.
 *
 * Generuje parity_table, sz53_table, sz53p_table a daa_table.
 * Volano jednou pri prvnim z80_init().
 */
static void init_tables(void) {
    /* Parita, SZ53, SZ53P */
    for (int i = 0; i < 256; i++) {
        int bits = 0;
        for (int b = 0; b < 8; b++) {
            if (i & (1 << b)) bits++;
        }
        parity_table[i] = (bits % 2 == 0) ? PF : 0;

        sz53_table[i] = (i & SF) | (i & F5) | (i & F3);
        if (i == 0) sz53_table[i] |= ZF;

        sz53p_table[i] = sz53_table[i] | parity_table[i];
    }

    /* DAA tabulka */
    for (int idx = 0; idx < 2048; idx++) {
        u8 a = (u8)(idx & 0xFF);
        int carry_in = (idx >> 8) & 1;
        int half_in = (idx >> 9) & 1;
        int sub = (idx >> 10) & 1;

        u8 correction = 0;
        u8 carry_out = 0;
        u8 hf_out = 0;

        if ((a & 0x0F) > 9 || half_in) correction = 0x06;
        if (a > 0x99 || carry_in) { correction |= 0x60; carry_out = CF; }

        u8 new_a;
        if (sub) {
            hf_out = ((correction & 0x0F) > (a & 0x0F)) ? HF : 0;
            new_a = a - correction;
        } else {
            hf_out = ((a & 0x0F) + (correction & 0x0F)) >= 0x10 ? HF : 0;
            new_a = a + correction;
        }

        u8 new_f = sz53p_table[new_a] | carry_out | hf_out | (sub ? NF : 0);
        daa_table[idx] = (u16)new_a | ((u16)new_f << 8);
    }
}

static bool tables_initialized = false;

/*
 * Inline pristup k pameti pouzivany MIMO z80_execute()
 * (handle_interrupts_internal). Uvnitr z80_execute() se pouzivaji
 * makra RD/WR/IO_RD/IO_WR s lokalni cache callback pointeru.
 */

/** Cteni bajtu z pameti (pro pouziti mimo execute loop). */
static inline u8 rd_slow(z80_t *cpu, u16 addr) {
    return cpu->mread_cb(cpu, addr, 0, cpu->mread_data);
}

/** Zapis bajtu do pameti (pro pouziti mimo execute loop). */
static inline void wr_slow(z80_t *cpu, u16 addr, u8 data) {
    cpu->mwrite_cb(cpu, addr, data, cpu->mwrite_data);
}

/** Cteni 16bitove hodnoty (pro pouziti mimo execute loop). */
static inline u16 rd16_slow(z80_t *cpu, u16 addr) {
    u8 lo = rd_slow(cpu, addr);
    u8 hi = rd_slow(cpu, addr + 1);
    return (u16)(lo | (hi << 8));
}

/** Zapis 16bitove hodnoty (pro pouziti mimo execute loop). */
static inline void wr16_slow(z80_t *cpu, u16 addr, u16 val) {
    wr_slow(cpu, addr, (u8)(val & 0xFF));
    wr_slow(cpu, addr + 1, (u8)(val >> 8));
}

/* Forward deklarace - pouzita v NEXT makru uvnitr z80_execute() */
static int handle_interrupts_internal(z80_t *cpu);

#ifdef CPU_Z80_RAM_FASTPATH
/*
 * RAM-access fast-path helpery. Definovany JEDNOU na
 * urovni TU (ne v z80_execute.inc, ktery se includuje 2x) - sdileny batch i
 * step variantou. Volane z RD/WR/FETCH/M1_FETCH maker; op_tstate inkrementuje
 * volajici makro (timing zachovan), tady jen samotny pristup + side efekty.
 */

/**
 * @brief Fast-path RAM read (bez op_tstate tiku - ten dela volajici makro).
 *
 * @param cpu CPU instance.
 * @param addr 16bit adresa.
 * @return Prectena hodnota, je-li stranka cista RAM; jinak fall-back na
 *         cpu->mread_cb (= bit-identicky s baseline, vcetne sync/latch/CDL).
 * @post Je-li cesta cista RAM a ram_fp_dbus_latch != NULL, latch = retval
 *       (replikace memory_read_cb side-efektu regDBUS_latch).
 * @note m1_state se ignoruje stejne jako v memory_read_cb (logging cb, ktery
 *       m1_state ctе, je v debug rezimu = ram_fp_enabled false -> sem nevejde).
 */
#ifdef CPU_Z80_RAM_FASTPATH_VERIFY
/*
 * Diff-verify mod (E1 dukaz presnosti): globalni citace + assert.
 * z80_fp_read/write spusti fast-path I kanonicky callback a porovna vysledky
 * (hodnota + regDBUS_latch). Pri neshode abort s diagnostikou. Pomale - jen
 * pro dukaz ekvivalence na realnem behu, ne pro perf.
 */
#include <stdio.h>
#include <stdlib.h>
unsigned long long g_z80_fp_verify_reads  = 0;  /**< Pocet overenych RAM cteni. */
unsigned long long g_z80_fp_verify_writes = 0;  /**< Pocet overenych RAM zapisu. */
unsigned long long g_z80_fp_verify_fastpath = 0; /**< Z toho kolik slo fast-path vetvi. */

#include <signal.h>

/**
 * @brief Report verify citacu (jen verify build).
 *
 * Vypise na stderr pocet overenych pristupu. Pokud doslo k neshode, proces
 * uz davno abortoval - tento report tedy bezi jen pri ukonceni bez neshody =
 * implicitni dukaz "0 neshod na N pristupech". Volan z atexit i ze SIGINT/
 * SIGTERM handleru (headless emu se ukoncuje signalem, atexit pak nebezi).
 */
void z80_fp_verify_report(void) {
    fprintf(stderr,
        "[RAM_FASTPATH_VERIFY] clean (no mismatch): reads=%llu writes=%llu "
        "fastpath_hits=%llu\n",
        g_z80_fp_verify_reads, g_z80_fp_verify_writes,
        g_z80_fp_verify_fastpath);
    fflush(stderr);
}

/** Signal handler: vypise verify report a ukonci proces (headless SIGINT/TERM). */
static void z80_fp_verify_sigreport(int sig) {
    z80_fp_verify_report();
    _exit(sig == SIGINT ? 130 : 143);
}
#endif

ALWAYS_INLINE u8 z80_fp_read(z80_t *cpu, u16 addr, int m1_state) {
#ifdef CPU_Z80_RAM_FASTPATH_VERIFY
    /*
     * Verify: spocti fast-path hodnotu (bez latch update), pak zavolej
     * kanonicky callback (= nastavi latch sam), porovnej. Latch po callbacku
     * MUSI byt roven fast-path hodnote (memory_read_cb nastavuje latch =
     * retval pro RAM), jinak fast-path latch replikace neni bit-identicka.
     */
    g_z80_fp_verify_reads++;
    /* Periodicky report (1. pristup + pak kazdych 10M) - dukaz "bezi + 0
     * mismatchu" i kdyz se proces ukonci signalem (headless) bez atexit. */
    if (g_z80_fp_verify_reads == 1
        || (g_z80_fp_verify_reads % 1000000ULL) == 0) z80_fp_verify_report();
    if (cpu->ram_fp_enabled) {
        u8 *page = cpu->ram_fp_read[addr >> 12];
        if (page) {
            u8 fp_v = page[addr & 0x0FFF];
            u8 cb_v = cpu->mread_cb(cpu, addr, m1_state, cpu->mread_data);
            u8 cb_latch = cpu->ram_fp_dbus_latch ? *cpu->ram_fp_dbus_latch : cb_v;
            g_z80_fp_verify_fastpath++;
            if (fp_v != cb_v || (cpu->ram_fp_dbus_latch && cb_latch != fp_v)) {
                fprintf(stderr,
                    "[RAM_FASTPATH_VERIFY] READ MISMATCH addr=%04X m1=%d "
                    "fp=%02X cb=%02X latch=%02X\n",
                    addr, m1_state, fp_v, cb_v, cb_latch);
                abort();
            }
            return cb_v; /* kanonicky vysledek (latch uz nastaven callbackem) */
        }
    }
    return cpu->mread_cb(cpu, addr, m1_state, cpu->mread_data);
#else
    if (cpu->ram_fp_enabled) {
        u8 *page = cpu->ram_fp_read[addr >> 12];
        if (page) {
            u8 v = page[addr & 0x0FFF];
            if (cpu->ram_fp_dbus_latch) *cpu->ram_fp_dbus_latch = v;
            return v;
        }
    }
    return cpu->mread_cb(cpu, addr, m1_state, cpu->mread_data);
#endif
}

/**
 * @brief Fast-path RAM write (bez op_tstate tiku - ten dela volajici makro).
 *
 * @param cpu CPU instance.
 * @param addr 16bit adresa.
 * @param val Zapisovana hodnota.
 * @post Je-li stranka cista RAM, zapise primo; jinak fall-back na
 *       cpu->mwrite_cb (bit-identicky s baseline). Cista RAM write nema
 *       zadny vedlejsi efekt (memory_write_cb pro RAM jen ulozi byte).
 */
ALWAYS_INLINE void z80_fp_write(z80_t *cpu, u16 addr, u8 val) {
#ifdef CPU_Z80_RAM_FASTPATH_VERIFY
    /*
     * Verify: proved kanonicky callback zapis, pak over ze fast-path cilova
     * bunka (ram_fp_write[page]+off) obsahuje zapsanou hodnotu = fast-path by
     * zapsal na TOTEZ misto. Chyti zamenu banku / stale tabulku po bankingu.
     * Pozn: tento test je validni jen kdyz callback zapsal do RAM (KIND_RAM
     * stranka); pro non-RAM (write potlaceny ROM/VRAM jinou cestou) fast-path
     * page == NULL, takze sem nevejde.
     */
    g_z80_fp_verify_writes++;
    if (cpu->ram_fp_enabled) {
        u8 *page = cpu->ram_fp_write[addr >> 12];
        if (page) {
            g_z80_fp_verify_fastpath++;
            cpu->mwrite_cb(cpu, addr, val, cpu->mwrite_data);
            u8 after = page[addr & 0x0FFF];
            if (after != val) {
                fprintf(stderr,
                    "[RAM_FASTPATH_VERIFY] WRITE MISMATCH addr=%04X "
                    "val=%02X fp_cell_after_cb=%02X\n",
                    addr, val, after);
                abort();
            }
            return;
        }
    }
    cpu->mwrite_cb(cpu, addr, val, cpu->mwrite_data);
#else
    if (cpu->ram_fp_enabled) {
        u8 *page = cpu->ram_fp_write[addr >> 12];
        if (page) {
            page[addr & 0x0FFF] = val;
            return;
        }
    }
    cpu->mwrite_cb(cpu, addr, val, cpu->mwrite_data);
#endif
}
#endif /* CPU_Z80_RAM_FASTPATH */

/*
 * Dve varianty jadra generovane z z80_execute.inc:
 *   z80_execute_batch - registrova cache (puvodni chovani, z80_execute API)
 *   z80_execute_step  - registry primo v cpu-> (per-step, volana z z80_step)
 */
HOT int z80_execute_batch(z80_t *cpu, int target_cycles);
HOT int z80_execute_step(z80_t *cpu, int target_cycles);

/* ========== Computed goto podpora ========== */

#if defined(__GNUC__) || defined(__clang__)
#define USE_COMPUTED_GOTO 1
#else
#define USE_COMPUTED_GOTO 0
#endif

/* ========== Hlavni emulacni smycka ========== */


/*
 * Telo emulacni smycky je vyclenene do z80_execute.inc a includovane DVAKRAT:
 * jednou jako batch jadro (registrova cache) a jednou
 * jako per-step jadro bez cache (registry primo v cpu->). Sdileny zdroj =
 * zadna textova duplikace handleru, zadna diverze pri budoucich opravach.
 * Sdilene tabulky (sz53_table aj.) a helpery (handle_interrupts_internal,
 * rd_slow) jsou static na urovni tohoto TU -> jedna kopie, zadny ODR problem.
 */

/* 1) Batch varianta (puvodni chovani) - volana z verejneho z80_execute(). */
#define Z80_EXEC_FN     z80_execute_batch
#define Z80_DIRECT_REGS 0
#include "cpu/z80_execute.inc"
#undef Z80_EXEC_FN
#undef Z80_DIRECT_REGS

/* 2) Per-step varianta bez registrove cache - volana z z80_step(). */
#define Z80_EXEC_FN     z80_execute_step
#define Z80_DIRECT_REGS 1
#include "cpu/z80_execute.inc"
#undef Z80_EXEC_FN
#undef Z80_DIRECT_REGS

/**
 * @brief Provedeni instrukci po dobu daneho poctu T-stavu (verejne API).
 *
 * Tenky wrapper na batch variantu jadra (z80_execute_batch). Zachovava
 * puvodni verejny kontrakt; volajici mimo per-1 hot loop (napr. ne-cycle-
 * accurate davkove provadeni) pouzivaji tuto entry.
 *
 * @param cpu Ukazatel na CPU instanci.
 * @param target_cycles Cilovy pocet T-stavu.
 * @return Skutecny pocet provedenych T-stavu.
 * @pre cpu != NULL, z80_init() bylo volano.
 */
HOT int z80_execute(z80_t *cpu, int target_cycles) {
    return z80_execute_batch(cpu, target_cycles);
}

/* ========== Zpracovani preruseni ========== */

/**
 * @brief Interni handler preruseni.
 *
 * Zpracovava NMI a maskovane preruseni (IM0/1/2).
 * Volan z execute smycky - pracuje primo se strukturou (ne s locals).
 *
 * @param cpu Ukazatel na CPU instanci.
 * @return Pocet T-stavu spotrebovanych obsluhou, nebo 0.
 */
static int handle_interrupts_internal(z80_t *cpu) {
    int int_cycles = 0;

#ifdef Z80_NO_EI_DELAY
    bool ei_blocked = false;
    cpu->ei_delay = false;
#else
    bool ei_blocked = cpu->ei_delay;
    cpu->ei_delay = false;
#endif

    /* NMI ma vyssi prioritu.
     *
     * NMI ack: IFF2 si zachova kopii puvodniho IFF1 (-> RETN ho obnovi),
     * IFF1 -> 0. Fire iff_change vzdy - reason NMI_ACK signalizuje
     * semantiku, ne nutne realnou zmenu IFF2.
     */
    if (cpu->nmi_pending) {
        bool was_halted = cpu->halted;
        cpu->nmi_pending = false;
        cpu->halted = false;
        cpu->iff2 = cpu->iff1;
        cpu->iff1 = 0;
        /* Behem HALT je cpu->pc = X (adresa instrukce HALT, viz op_76).
         * Pred ulozenim navratove adresy ho inkrementujeme na X+1 (= adresa
         * za HALT) - realny Z80 inkrementuje PC pred skokem do rutiny. Tim
         * push i HALT_EXIT event vidi X+1 (semantika beze zmeny). */
        if (was_halted) cpu->pc++;
        cpu->sp -= 2;
        wr16_slow(cpu, cpu->sp, cpu->pc);
        /*
         * Fire HALT_EXIT pred zmenou cpu->pc (= predame adresu za HALT,
         * kam by se CPU vratilo bez NMI). Az pak update cpu->pc na vektor.
         */
        if (was_halted && cpu->cpu_ctrl_event_cb) {
            cpu->cpu_ctrl_event_cb(cpu, (u8)Z80_CPU_CTRL_HALT_EXIT,
                                   cpu->pc, cpu->cpu_ctrl_event_data);
        }
        cpu->pc = 0x0066;
        cpu->wz.w = cpu->pc;
        int_cycles = 11;
        if (cpu->iff_change_cb) {
            cpu->iff_change_cb(cpu, cpu->iff1, cpu->iff2,
                               (u8)Z80_IFF_REASON_NMI_ACK,
                               cpu->iff_change_data);
        }
    }
    /* Maskovane preruseni - blokovano po EI.
     *
     * INT ack pokryva vsechny IM 0/1/2 - Z80 pri ack INT vzdy clearuje
     * IFF1+IFF2 nezavisle na IM mode (Zilog Z80 manual, Sean Young).
     * IM mode jen ovlivnuje co se vykona po clearu.
     */
    else if (cpu->iff1 && !ei_blocked) {
        if (!cpu->int_pending) {
            return 0;
        }
        bool was_halted = cpu->halted;
        cpu->int_pending = false;
        cpu->halted = false;
        cpu->iff1 = 0;
        cpu->iff2 = 0;

        /* Behem HALT je cpu->pc = X (adresa instrukce HALT, viz op_76).
         * Pred HALT_EXIT eventem i pushem v switch(im) nize ho inkrementujeme
         * na X+1 (= adresa za HALT, navratova adresa) - realny Z80
         * inkrementuje PC pred skokem do rutiny. */
        if (was_halted) cpu->pc++;

        /* HALT exit fire jen pri skutecnem prechodu 1->0 (= IRQ pred INT
         * mezi beznymi instrukcemi nesmi fire HALT_EXIT). PC je za HALT
         * instrukci, tedy adresa kam by se CPU vratilo. */
        if (was_halted && cpu->cpu_ctrl_event_cb) {
            cpu->cpu_ctrl_event_cb(cpu, (u8)Z80_CPU_CTRL_HALT_EXIT,
                                   cpu->pc, cpu->cpu_ctrl_event_data);
        }

        if (cpu->intack_cb) cpu->intack_cb(cpu, cpu->intack_data);
        if (cpu->iff_change_cb) {
            cpu->iff_change_cb(cpu, cpu->iff1, cpu->iff2,
                               (u8)Z80_IFF_REASON_INT_ACK,
                               cpu->iff_change_data);
        }

        /* Cteni vektoru pres intread callback (pokud existuje a neni nastaven primo) */
        if (cpu->intread_cb && cpu->int_vector == 0) {
            cpu->int_vector = cpu->intread_cb(cpu, cpu->intread_data);
        }

        switch (cpu->im) {
            case 0:
                cpu->sp -= 2;
                wr16_slow(cpu, cpu->sp, cpu->pc);
                cpu->pc = cpu->int_vector & 0x38;
                cpu->wz.w = cpu->pc;
                int_cycles = 13;
                break;
            case 1:
                cpu->sp -= 2;
                wr16_slow(cpu, cpu->sp, cpu->pc);
                cpu->pc = 0x0038;
                cpu->wz.w = cpu->pc;
                int_cycles = 13;
                break;
            case 2:
                cpu->sp -= 2;
                wr16_slow(cpu, cpu->sp, cpu->pc);
                {
                    u16 vec_addr = ((u16)cpu->i << 8) | (cpu->int_vector & 0xFE);
                    cpu->pc = rd16_slow(cpu, vec_addr);
                }
                cpu->wz.w = cpu->pc;
                int_cycles = 19;
                break;
        }
    }

    return int_cycles;
}

/* ========== Verejne API ========== */

/**
 * @brief Inicializace lookup tabulek (pri prvnim volani).
 *
 * Bezpecne pro volani z vice vlaken - tabulky jsou sdilene a read-only.
 */
static void ensure_tables(void) {
    if (!tables_initialized) {
        init_tables();
        tables_initialized = true;
    }
}

z80_t *z80_create(
    z80_mread_cb mread, void *mread_data,
    z80_mwrite_cb mwrite, void *mwrite_data,
    z80_pread_cb pread, void *pread_data,
    z80_pwrite_cb pwrite, void *pwrite_data,
    z80_intread_cb intread, void *intread_data
) {
    ensure_tables();

    z80_t *cpu = (z80_t *)malloc(sizeof(z80_t));
    if (!cpu) return NULL;
    memset(cpu, 0, sizeof(z80_t));

    /* Nastaveni callbacku */
    cpu->mread_cb   = mread  ? mread  : default_mread;
    cpu->mread_data  = mread_data;
    cpu->mwrite_cb  = mwrite ? mwrite : default_mwrite;
    cpu->mwrite_data = mwrite_data;
    cpu->pread_cb   = pread  ? pread  : default_pread;
    cpu->pread_data  = pread_data;
    cpu->pwrite_cb  = pwrite ? pwrite : default_pwrite;
    cpu->pwrite_data = pwrite_data;
    cpu->intread_cb  = intread;
    cpu->intread_data = intread_data;

    z80_reset(cpu);
    return cpu;
}

void z80_destroy(z80_t *cpu) {
    free(cpu);
}

void z80_reset(z80_t *cpu) {
    /* Zachovame callbacky */
    z80_mread_cb  save_mread  = cpu->mread_cb;
    void         *save_mrd    = cpu->mread_data;
    z80_mwrite_cb save_mwrite = cpu->mwrite_cb;
    void         *save_mwd    = cpu->mwrite_data;
    z80_pread_cb  save_pread  = cpu->pread_cb;
    void         *save_prd    = cpu->pread_data;
    z80_pwrite_cb save_pwrite = cpu->pwrite_cb;
    void         *save_pwd    = cpu->pwrite_data;
    z80_intread_cb save_intread = cpu->intread_cb;
    void          *save_ird    = cpu->intread_data;
    z80_intack_cb  save_intack = cpu->intack_cb;
    void          *save_iad    = cpu->intack_data;
    z80_reti_cb    save_reti   = cpu->reti_cb;
    void          *save_rtd    = cpu->reti_data;
    z80_ei_cb      save_ei     = cpu->ei_cb;
    void          *save_eid    = cpu->ei_data;
    z80_di_cb      save_di     = cpu->di_cb;
    void          *save_did    = cpu->di_data;
    z80_im_cb      save_im     = cpu->im_cb;
    void          *save_imd    = cpu->im_data;
    z80_halt_cb    save_halt   = cpu->halt_cb;
    void          *save_haltd  = cpu->halt_data;
    z80_nmi_cb     save_nmi    = cpu->nmi_cb;
    void          *save_nmid   = cpu->nmi_data;
    z80_iff_change_cb save_iff_change   = cpu->iff_change_cb;
    void          *save_iff_change_data = cpu->iff_change_data;
    z80_cpu_ctrl_event_cb save_cpu_ctrl = cpu->cpu_ctrl_event_cb;
    void          *save_cpu_ctrl_data   = cpu->cpu_ctrl_event_data;
    z80_call_cb    save_call   = cpu->call_cb;
    void          *save_calld  = cpu->call_data;
    z80_ret_cb     save_ret    = cpu->ret_cb;
    void          *save_retd   = cpu->ret_data;
    void (*save_ps)(z80_t *, void *) = cpu->post_step_cb;
    void *save_psd = cpu->post_step_data;

    cpu->af.w = 0xFFFF;
    cpu->bc.w = 0x0000;
    cpu->de.w = 0x0000;
    cpu->hl.w = 0x0000;
    cpu->af2.w = 0x0000;
    cpu->bc2.w = 0x0000;
    cpu->de2.w = 0x0000;
    cpu->hl2.w = 0x0000;
    cpu->ix.w = 0x0000;
    cpu->iy.w = 0x0000;
    cpu->wz.w = 0x0000;
    cpu->sp = 0xFFFF;
    cpu->pc = 0x0000;
    cpu->i = 0x00;
    cpu->r = 0x00;
    cpu->iff1 = 0;
    cpu->iff2 = 0;
    cpu->im = 0;
    cpu->halted = false;
    cpu->int_pending = false;
    cpu->nmi_pending = false;
    cpu->ei_delay = false;
    cpu->ld_a_ir = false;
    cpu->cycles = 0;
    cpu->total_cycles = 0;
    cpu->wait_cycles = 0;
    cpu->op_tstate = 0;
    cpu->q = 0;

    /* Obnovime callbacky */
    cpu->mread_cb   = save_mread;   cpu->mread_data  = save_mrd;
    cpu->mwrite_cb  = save_mwrite;  cpu->mwrite_data = save_mwd;
    cpu->pread_cb   = save_pread;   cpu->pread_data  = save_prd;
    cpu->pwrite_cb  = save_pwrite;  cpu->pwrite_data = save_pwd;
    cpu->intread_cb = save_intread; cpu->intread_data = save_ird;
    cpu->intack_cb  = save_intack;  cpu->intack_data = save_iad;
    cpu->reti_cb    = save_reti;    cpu->reti_data   = save_rtd;
    cpu->ei_cb      = save_ei;     cpu->ei_data     = save_eid;
    cpu->di_cb      = save_di;     cpu->di_data     = save_did;
    cpu->im_cb      = save_im;     cpu->im_data     = save_imd;
    cpu->halt_cb    = save_halt;   cpu->halt_data   = save_haltd;
    cpu->nmi_cb     = save_nmi;    cpu->nmi_data    = save_nmid;
    cpu->iff_change_cb   = save_iff_change;
    cpu->iff_change_data = save_iff_change_data;
    cpu->cpu_ctrl_event_cb   = save_cpu_ctrl;
    cpu->cpu_ctrl_event_data = save_cpu_ctrl_data;
    cpu->call_cb    = save_call;   cpu->call_data  = save_calld;
    cpu->ret_cb     = save_ret;    cpu->ret_data   = save_retd;
    cpu->post_step_cb = save_ps;    cpu->post_step_data = save_psd;

    /*
     * Fire iff_change reset event - konzument muze sledovat reset jako
     * konkretni IFF source. Volame az po obnoveni callbacku (= save/restore
     * zachoval iff_change_cb pres reset cyklus).
     */
    if (cpu->iff_change_cb) {
        cpu->iff_change_cb(cpu, cpu->iff1, cpu->iff2,
                           (u8)Z80_IFF_REASON_RESET,
                           cpu->iff_change_data);
    }
}

/* ========== Dynamicka zmena callbacku ========== */

void z80_set_mread(z80_t *cpu, z80_mread_cb fn, void *data) {
    cpu->mread_cb = fn ? fn : default_mread;
    cpu->mread_data = data;
}

void z80_set_mwrite(z80_t *cpu, z80_mwrite_cb fn, void *data) {
    cpu->mwrite_cb = fn ? fn : default_mwrite;
    cpu->mwrite_data = data;
}

void z80_set_pread(z80_t *cpu, z80_pread_cb fn, void *data) {
    cpu->pread_cb = fn ? fn : default_pread;
    cpu->pread_data = data;
}

void z80_set_pwrite(z80_t *cpu, z80_pwrite_cb fn, void *data) {
    cpu->pwrite_cb = fn ? fn : default_pwrite;
    cpu->pwrite_data = data;
}

void z80_set_intread(z80_t *cpu, z80_intread_cb fn, void *data) {
    cpu->intread_cb = fn;
    cpu->intread_data = data;
}

void z80_set_intack(z80_t *cpu, z80_intack_cb fn, void *data) {
    cpu->intack_cb = fn;
    cpu->intack_data = data;
}

void z80_set_reti(z80_t *cpu, z80_reti_cb fn, void *data) {
    cpu->reti_cb = fn;
    cpu->reti_data = data;
}

void z80_set_ei(z80_t *cpu, z80_ei_cb fn, void *data) {
    cpu->ei_cb = fn;
    cpu->ei_data = data;
}

void z80_set_di(z80_t *cpu, z80_di_cb fn, void *data) {
    cpu->di_cb = fn;
    cpu->di_data = data;
}

void z80_set_im_change(z80_t *cpu, z80_im_cb fn, void *data) {
    cpu->im_cb = fn;
    cpu->im_data = data;
}

void z80_set_halt(z80_t *cpu, z80_halt_cb fn, void *data) {
    cpu->halt_cb = fn;
    cpu->halt_data = data;
}

void z80_set_nmi_cb(z80_t *cpu, z80_nmi_cb fn, void *data) {
    cpu->nmi_cb = fn;
    cpu->nmi_data = data;
}

#ifdef CPU_Z80_RAM_FASTPATH
void z80_set_ram_fastpath(z80_t *cpu, u8 *const read_table[16],
                          u8 *const write_table[16],
                          u8 *dbus_latch, bool enabled) {
    for (int i = 0; i < 16; i++) {
        cpu->ram_fp_read[i]  = read_table[i];
        cpu->ram_fp_write[i] = write_table[i];
    }
    cpu->ram_fp_dbus_latch = dbus_latch;
    cpu->ram_fp_enabled    = enabled;
}
#endif

void z80_set_iff_change(z80_t *cpu, z80_iff_change_cb fn, void *data) {
    cpu->iff_change_cb = fn;
    cpu->iff_change_data = data;
}

void z80_set_cpu_ctrl_event(z80_t *cpu, z80_cpu_ctrl_event_cb fn, void *data) {
    cpu->cpu_ctrl_event_cb = fn;
    cpu->cpu_ctrl_event_data = data;
}

void z80_set_call(z80_t *cpu, z80_call_cb fn, void *data) {
    cpu->call_cb = fn;
    cpu->call_data = data;
}

void z80_set_ret(z80_t *cpu, z80_ret_cb fn, void *data) {
    cpu->ret_cb = fn;
    cpu->ret_data = data;
}

void z80_set_post_step(z80_t *cpu, void (*fn)(z80_t *cpu, void *data), void *data) {
    cpu->post_step_cb = fn;
    cpu->post_step_data = data;
}

void z80_add_wait_states(z80_t *cpu, int wait) {
    cpu->wait_cycles += wait;
    cpu->op_tstate += wait;
}

/**
 * @brief Provedeni jedne instrukce (per-step jadro).
 *
 * Vola z80_execute_step() (Z80_DIRECT_REGS=1) misto batch varianty. Per-step
 * jadro provede PRAVE jednu instrukci a vrati se (target_cycles=1); registry
 * ziji primo v cpu->, takze odpada prolog/epilog registrove cache (RELOAD/
 * WRITEBACK). KDY se vyhodnocuje event/interrupt boundary se NEMENI -
 * semantika je bit-identicka s z80_execute(cpu, 1).
 *
 * @param cpu Ukazatel na CPU instanci.
 * @return Pocet T-stavu spotrebovanych instrukci.
 * @pre cpu != NULL.
 */
int z80_step(z80_t *cpu) {
    int before = (int)cpu->total_cycles;
    z80_execute_step(cpu, 1);
    return (int)cpu->total_cycles - before;
}

void z80_int(z80_t *cpu) {
    cpu->int_pending = true;
    cpu->int_vector = 0;
}

void z80_irq(z80_t *cpu, u8 vector) {
    cpu->int_pending = true;
    cpu->int_vector = vector;
}

void z80_nmi(z80_t *cpu) {
    cpu->nmi_pending = true;
    if (cpu->nmi_cb) cpu->nmi_cb(cpu, cpu->nmi_data);
}

/* ========== Pristup k registrum ========== */

u16 z80_get_reg(z80_t *cpu, z80_reg_t reg) {
    switch (reg) {
        case Z80_REG_AF:  return cpu->af.w;
        case Z80_REG_BC:  return cpu->bc.w;
        case Z80_REG_DE:  return cpu->de.w;
        case Z80_REG_HL:  return cpu->hl.w;
        case Z80_REG_AF2: return cpu->af2.w;
        case Z80_REG_BC2: return cpu->bc2.w;
        case Z80_REG_DE2: return cpu->de2.w;
        case Z80_REG_HL2: return cpu->hl2.w;
        case Z80_REG_IX:  return cpu->ix.w;
        case Z80_REG_IY:  return cpu->iy.w;
        case Z80_REG_SP:  return cpu->sp;
        case Z80_REG_PC:  return cpu->pc;
        case Z80_REG_WZ:  return cpu->wz.w;
        case Z80_REG_IR:  return (u16)((cpu->i << 8) | cpu->r);
        default:          return 0;
    }
}

void z80_set_reg(z80_t *cpu, z80_reg_t reg, u16 value) {
    /*
     * Pokud probiha z80_execute() (cpu->_active_cache != NULL), modifikujeme
     * krome pole z80_t i lokalni cache, aby zmena byla videt v probihajici
     * instrukci. Bez toho by WRITEBACK na konci instrukce prepsal cpu->...w
     * zpet starou hodnotou ze stale neaktualizovane lokalni cache.
     */
    z80_local_cache_t *lc = cpu->_active_cache;
    switch (reg) {
        case Z80_REG_AF:
            cpu->af.w = value;
            if (lc) {
                *lc->A = (u8)(value >> 8);
                *lc->F = (u8)value;
                *lc->savedF = *lc->F;
            }
            break;
        case Z80_REG_BC:
            cpu->bc.w = value;
            if (lc) { *lc->B = (u8)(value >> 8); *lc->C = (u8)value; }
            break;
        case Z80_REG_DE:
            cpu->de.w = value;
            if (lc) { *lc->D = (u8)(value >> 8); *lc->E = (u8)value; }
            break;
        case Z80_REG_HL:
            cpu->hl.w = value;
            if (lc) { *lc->H = (u8)(value >> 8); *lc->L = (u8)value; }
            break;
        case Z80_REG_AF2: cpu->af2.w = value; break;
        case Z80_REG_BC2: cpu->bc2.w = value; break;
        case Z80_REG_DE2: cpu->de2.w = value; break;
        case Z80_REG_HL2: cpu->hl2.w = value; break;
        case Z80_REG_IX:  cpu->ix.w  = value; break;
        case Z80_REG_IY:  cpu->iy.w  = value; break;
        case Z80_REG_SP:
            cpu->sp = value;
            if (lc) *lc->SP = value;
            break;
        case Z80_REG_PC:
            cpu->pc = value;
            if (lc) *lc->PC = value;
            break;
        case Z80_REG_WZ:
            cpu->wz.w = value;
            if (lc) *lc->WZ = value;
            break;
        case Z80_REG_IR:
            cpu->i = (u8)(value >> 8);
            cpu->r = (u8)value;
            if (lc) *lc->R = (u8)value;
            break;
        default: break;
    }
}

bool z80_is_halted(z80_t *cpu) {
    return cpu->halted;
}
