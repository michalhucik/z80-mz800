/*
 * Copyright (c) 2026 Michal Hucik
 * SPDX-License-Identifier: MIT
 * https://github.com/michalhucik/z80-mz800
 */
/**
 * @file z80.c
 * @brief cpu-z80 v0.1 - Presny a rychly emulator procesoru Zilog Z-80A.
 *
 * Kompletni implementace instrukcni sady Z80 vcetne:
 * - Vsech oficialnich instrukci
 * - CB, ED, DD, FD prefixovych instrukci
 * - DD CB / FD CB dvojite prefixovanych instrukci
 * - Nedokumentovanych instrukci (IXH, IXL, IYH, IYL, SLL)
 * - Presneho pocitani T-stavu
 * - Spravneho chovani flagu vcetne nedokumentovanych bitu 3 a 5
 * - MEMPTR/WZ registru
 * - Prerusovacich rezimu IM0, IM1, IM2
 * - LD A,I/R HW bugu
 *
 * Optimalizace:
 * - Computed goto dispatch (GCC/Clang __label__)
 * - Lokalni registrova cache v z80_execute()
 * - Eliminace null-checku callbacku (default handlery)
 * - DAA lookup tabulka (2048 zaznamu)
 * - Inline prefix handlery (CB/ED/DD/FD/DDCB/FDCB)
 * - Block instruction fast paths (LDIR/LDDR)
 *
 * @version 0.1
 */

#include "cpu/z80.h"
#include <string.h>

/* ========== Default callbacky ========== */

/**
 * @brief Default callback pro cteni - vraci 0xFF (odpojeny bus).
 * @param addr Adresa (ignorovana).
 * @return Vzdy 0xFF.
 */
static u8 default_read(u16 addr) { (void)addr; return 0xFF; }

/**
 * @brief Default callback pro zapis - no-op.
 * @param addr Adresa (ignorovana).
 * @param data Data (ignorovana).
 */
static void default_write(u16 addr, u8 data) { (void)addr; (void)data; }

/* Callbacky - inicializovane na default (zadne null-checky v hot path) */
static z80_read_fn  mem_read_fn  = default_read;
static z80_write_fn mem_write_fn = default_write;
static z80_read_fn  mem_fetch_fn = default_read;
static z80_read_fn  io_read_fn   = default_read;
static z80_write_fn io_write_fn  = default_write;
static z80_intack_fn intack_fn   = NULL;
static z80_reti_fn reti_fn       = NULL;
static z80_int_line_fn int_line_fn = NULL;
static z80_ei_fn ei_fn = NULL;

/* Post-step callback - per-line WAIT v MZ-700 rezimu */
static void (*post_step_fn)(void) = NULL;

/* Pointer na aktualni CPU instanci - umoznuje I/O callbackum nastavit wait_cycles */
static z80_t *current_cpu = NULL;

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

/* ========== Inline pristup k pameti (bez null-checku) ========== */

/** Cteni bajtu z pameti. */
static inline u8 rd(u16 addr) { return mem_read_fn(addr); }

/** Zapis bajtu do pameti. */
static inline void wr(u16 addr, u8 data) { mem_write_fn(addr, data); }

/** Cteni bajtu z I/O portu. */
static inline u8 io_rd(u16 port) { return io_read_fn(port); }

/** Zapis bajtu na I/O port. */
static inline void io_wr(u16 port, u8 data) { io_write_fn(port, data); }

/** Cteni 16bitove hodnoty z adresy (little-endian). */
static inline u16 rd16(u16 addr) {
    u8 lo = rd(addr);
    u8 hi = rd(addr + 1);
    return (u16)(lo | (hi << 8));
}

/** Zapis 16bitove hodnoty na adresu (little-endian). */
static inline void wr16(u16 addr, u16 val) {
    wr(addr, (u8)(val & 0xFF));
    wr(addr + 1, (u8)(val >> 8));
}

/* Forward deklarace - pouzita v NEXT makru uvnitr z80_execute() */
static int handle_interrupts_internal(z80_t *cpu);

/* ========== Computed goto podpora ========== */

#if defined(__GNUC__) || defined(__clang__)
#define USE_COMPUTED_GOTO 1
#else
#define USE_COMPUTED_GOTO 0
#endif

/* ========== Hlavni emulacni smycka ========== */

/**
 * @brief Provedeni instrukci po dobu daneho poctu T-stavu.
 *
 * Pouziva lokalni registrovou cache pro hlavni registry (A, F, B, C, D, E,
 * H, L, PC, SP, WZ, R). IX, IY a alternativni sada zustavaji ve strukture.
 * Writeback do struktury pred: interrupt handling, post_step callback, navrat.
 *
 * @param cpu Ukazatel na CPU instanci.
 * @param target_cycles Cilovy pocet T-stavu.
 * @return Skutecny pocet provedenych T-stavu.
 * @pre cpu != NULL, z80_init() bylo volano.
 */
int z80_execute(z80_t *cpu, int target_cycles) {
    current_cpu = cpu;

    /* Lokalni registrova cache */
    u8  rA  = cpu->af.h;
    u8  rF  = cpu->af.l;
    u8  rB  = cpu->bc.h;
    u8  rC  = cpu->bc.l;
    u8  rD  = cpu->de.h;
    u8  rE  = cpu->de.l;
    u8  rH  = cpu->hl.h;
    u8  rL  = cpu->hl.l;
    u16 rPC = cpu->pc;
    u16 rSP = cpu->sp;
    u16 rWZ = cpu->wz.w;
    u8  rR  = cpu->r;

    int cycles_executed = 0;
    int last_sync = 0;  /* Posledni bod synchronizace cpu->cycles */

    /* Makra pro pristup k 16bit parum z lokalnich promennych */
#define AF_VAL  ((u16)((rA << 8) | rF))
#define BC_VAL  ((u16)((rB << 8) | rC))
#define DE_VAL  ((u16)((rD << 8) | rE))
#define HL_VAL  ((u16)((rH << 8) | rL))

#define SET_AF(v) do { rA = (u8)((v) >> 8); rF = (u8)(v); } while(0)
#define SET_BC(v) do { rB = (u8)((v) >> 8); rC = (u8)(v); } while(0)
#define SET_DE(v) do { rD = (u8)((v) >> 8); rE = (u8)(v); } while(0)
#define SET_HL(v) do { rH = (u8)((v) >> 8); rL = (u8)(v); } while(0)

    /* WZ pristup */
#define WZH_VAL ((u8)(rWZ >> 8))
#define WZL_VAL ((u8)(rWZ & 0xFF))
#define SET_WZH(v) do { rWZ = (rWZ & 0x00FF) | ((u16)(v) << 8); } while(0)
#define SET_WZL(v) do { rWZ = (rWZ & 0xFF00) | (u8)(v); } while(0)

    /* Fetch z PC */
#define FETCH() (mem_fetch_fn(rPC++))
#define FETCH16() (tmp_lo = FETCH(), tmp_hi = FETCH(), (u16)(tmp_lo | (tmp_hi << 8)))

    /* Inkrementace R (dolnich 7 bitu) */
#define INC_R() do { rR = (rR & 0x80) | ((rR + 1) & 0x7F); } while(0)

    /* PUSH/POP pres lokalni SP */
#define PUSH(val) do { rSP -= 2; wr16(rSP, val); } while(0)
#define POP() (tmp16 = rd16(rSP), rSP += 2, tmp16)

    /* Writeback lokalnich registru do struktury */
#define WRITEBACK() do { \
    cpu->af.h = rA; cpu->af.l = rF; \
    cpu->bc.h = rB; cpu->bc.l = rC; \
    cpu->de.h = rD; cpu->de.l = rE; \
    cpu->hl.h = rH; cpu->hl.l = rL; \
    cpu->pc = rPC; cpu->sp = rSP; \
    cpu->wz.w = rWZ; cpu->r = rR; \
} while(0)

    /* Reload lokalnich registru ze struktury */
#define RELOAD() do { \
    rA = cpu->af.h; rF = cpu->af.l; \
    rB = cpu->bc.h; rC = cpu->bc.l; \
    rD = cpu->de.h; rE = cpu->de.l; \
    rH = cpu->hl.h; rL = cpu->hl.l; \
    rPC = cpu->pc; rSP = cpu->sp; \
    rWZ = cpu->wz.w; rR = cpu->r; \
} while(0)

    /* Aritmeticke operace pouzivajici lokalni cache */

    /* 8bit ADD - vraci vysledek, nastavuje rF */
#define ADD8(a, b) ({ \
    u16 _r16 = (a) + (b); \
    u8 _r8 = (u8)_r16; \
    rF = sz53_table[_r8] \
       | ((_r16 >> 8) & CF) \
       | (((a) ^ (b) ^ _r8) & HF) \
       | ((((a) ^ (b) ^ 0x80) & ((a) ^ _r8) & 0x80) >> 5); \
    _r8; })

    /* 8bit ADC */
#define ADC8(a, b) ({ \
    u16 _r16 = (a) + (b) + (rF & CF); \
    u8 _r8 = (u8)_r16; \
    rF = sz53_table[_r8] \
       | ((_r16 >> 8) & CF) \
       | (((a) ^ (b) ^ _r8) & HF) \
       | ((((a) ^ (b) ^ 0x80) & ((a) ^ _r8) & 0x80) >> 5); \
    _r8; })

    /* 8bit SUB */
#define SUB8(a, b) ({ \
    u16 _r16 = (a) - (b); \
    u8 _r8 = (u8)_r16; \
    rF = sz53_table[_r8] \
       | ((_r16 >> 8) & CF) \
       | NF \
       | (((a) ^ (b) ^ _r8) & HF) \
       | ((((a) ^ (b)) & ((a) ^ _r8) & 0x80) >> 5); \
    _r8; })

    /* 8bit SBC */
#define SBC8(a, b) ({ \
    u16 _r16 = (a) - (b) - (rF & CF); \
    u8 _r8 = (u8)_r16; \
    rF = sz53_table[_r8] \
       | ((_r16 >> 8) & CF) \
       | NF \
       | (((a) ^ (b) ^ _r8) & HF) \
       | ((((a) ^ (b)) & ((a) ^ _r8) & 0x80) >> 5); \
    _r8; })

    /* AND */
#define AND8(val) do { rA &= (val); rF = sz53p_table[rA] | HF; } while(0)

    /* XOR */
#define XOR8(val) do { rA ^= (val); rF = sz53p_table[rA]; } while(0)

    /* OR */
#define OR8(val) do { rA |= (val); rF = sz53p_table[rA]; } while(0)

    /* CP - porovnani (jako SUB, ale vysledek se neuklada, F3/F5 z operandu) */
#define CP8(val) do { \
    u8 _v = (val); \
    u16 _r16 = rA - _v; \
    u8 _r8 = (u8)_r16; \
    rF = (sz53_table[_r8] & ~(F3 | F5)) \
       | (_v & (F3 | F5)) \
       | ((_r16 >> 8) & CF) \
       | NF \
       | ((rA ^ _v ^ _r8) & HF) \
       | (((rA ^ _v) & (rA ^ _r8) & 0x80) >> 5); \
} while(0)

    /* INC 8bit */
#define INC8(val) ({ \
    u8 _r = (val) + 1; \
    rF = (rF & CF) \
       | sz53_table[_r] \
       | ((_r & 0x0F) == 0 ? HF : 0) \
       | ((val) == 0x7F ? PF : 0); \
    _r; })

    /* DEC 8bit */
#define DEC8(val) ({ \
    u8 _r = (val) - 1; \
    rF = (rF & CF) \
       | sz53_table[_r] \
       | NF \
       | ((_r & 0x0F) == 0x0F ? HF : 0) \
       | ((val) == 0x80 ? PF : 0); \
    _r; })

    /* ADD 16bit (HL += val) - dest je pointer na registr, val je u16 */
#define ADD16(dest_h, dest_l, val) do { \
    u16 _dest = ((u16)(dest_h) << 8) | (dest_l); \
    u32 _r32 = _dest + (val); \
    u8 _rh = (u8)(_r32 >> 8); \
    rF = (rF & (SF | ZF | PF)) \
       | (_rh & (F3 | F5)) \
       | ((_r32 >> 16) & CF) \
       | (((_dest ^ (val) ^ _r32) >> 8) & HF); \
    (dest_h) = _rh; \
    (dest_l) = (u8)_r32; \
} while(0)

    /* ADC HL, val */
#define ADC16(val) do { \
    u16 _hl = HL_VAL; \
    u32 _r32 = _hl + (val) + (rF & CF); \
    u8 _rh = (u8)(_r32 >> 8); \
    rF = ((_r32 >> 8) & SF) \
       | ((_r32 & 0xFFFF) == 0 ? ZF : 0) \
       | (_rh & (F3 | F5)) \
       | ((_r32 >> 16) & CF) \
       | (((_hl ^ (val) ^ 0x8000) & (_hl ^ _r32) & 0x8000) >> 13) \
       | (((_hl ^ (val) ^ _r32) >> 8) & HF); \
    rH = _rh; rL = (u8)_r32; \
} while(0)

    /* SBC HL, val */
#define SBC16(val) do { \
    u16 _hl = HL_VAL; \
    u32 _r32 = _hl - (val) - (rF & CF); \
    u8 _rh = (u8)(_r32 >> 8); \
    rF = ((_r32 >> 8) & SF) \
       | ((_r32 & 0xFFFF) == 0 ? ZF : 0) \
       | (_rh & (F3 | F5)) \
       | ((_r32 >> 16) & CF) \
       | NF \
       | (((_hl ^ (val)) & (_hl ^ _r32) & 0x8000) >> 13) \
       | (((_hl ^ (val) ^ _r32) >> 8) & HF); \
    rH = _rh; rL = (u8)_r32; \
} while(0)

    /* Rotace/posuvy (CB prefix) - vraceni vysledek, nastavuji rF */
#define RLC(val) ({ \
    u8 _c = ((val) >> 7) & 1; \
    u8 _r = ((val) << 1) | _c; \
    rF = sz53p_table[_r] | _c; _r; })

#define RRC(val) ({ \
    u8 _c = (val) & 1; \
    u8 _r = ((val) >> 1) | (_c << 7); \
    rF = sz53p_table[_r] | _c; _r; })

#define RL(val) ({ \
    u8 _c = ((val) >> 7) & 1; \
    u8 _r = ((val) << 1) | (rF & CF); \
    rF = sz53p_table[_r] | _c; _r; })

#define RR(val) ({ \
    u8 _c = (val) & 1; \
    u8 _r = ((val) >> 1) | ((rF & CF) << 7); \
    rF = sz53p_table[_r] | _c; _r; })

#define SLA(val) ({ \
    u8 _c = ((val) >> 7) & 1; \
    u8 _r = (val) << 1; \
    rF = sz53p_table[_r] | _c; _r; })

#define SRA(val) ({ \
    u8 _c = (val) & 1; \
    u8 _r = ((val) >> 1) | ((val) & 0x80); \
    rF = sz53p_table[_r] | _c; _r; })

#define SLL(val) ({ \
    u8 _c = ((val) >> 7) & 1; \
    u8 _r = ((val) << 1) | 1; \
    rF = sz53p_table[_r] | _c; _r; })

#define SRL(val) ({ \
    u8 _c = (val) & 1; \
    u8 _r = (val) >> 1; \
    rF = sz53p_table[_r] | _c; _r; })

    /* BIT n, val */
#define BIT_OP(n, val) do { \
    u8 _r = (val) & (1 << (n)); \
    rF = (rF & CF) | HF | (_r & SF); \
    if (_r == 0) rF |= ZF | PF; \
    rF |= ((val) & (F3 | F5)); \
} while(0)

    /* BIT n, (HL)/(IX+d)/(IY+d) - F3/F5 z horniho bajtu adresy */
#define BIT_MEM(n, val, addr_hi) do { \
    u8 _r = (val) & (1 << (n)); \
    rF = (rF & CF) | HF | (_r & SF); \
    if (_r == 0) rF |= ZF | PF; \
    rF |= ((addr_hi) & (F3 | F5)); \
} while(0)

    /* Docasne promenne pro FETCH16/POP */
    u8 tmp_lo, tmp_hi;
    u16 tmp16;
    int cycles;

    /* ========== Dispatch tabulky (computed goto) ========== */

#if USE_COMPUTED_GOTO

    /* Base opcode dispatch tabulka (256 zaznamu) */
    static const void* const base_dispatch[256] = {
        &&op_00, &&op_01, &&op_02, &&op_03, &&op_04, &&op_05, &&op_06, &&op_07,
        &&op_08, &&op_09, &&op_0A, &&op_0B, &&op_0C, &&op_0D, &&op_0E, &&op_0F,
        &&op_10, &&op_11, &&op_12, &&op_13, &&op_14, &&op_15, &&op_16, &&op_17,
        &&op_18, &&op_19, &&op_1A, &&op_1B, &&op_1C, &&op_1D, &&op_1E, &&op_1F,
        &&op_20, &&op_21, &&op_22, &&op_23, &&op_24, &&op_25, &&op_26, &&op_27,
        &&op_28, &&op_29, &&op_2A, &&op_2B, &&op_2C, &&op_2D, &&op_2E, &&op_2F,
        &&op_30, &&op_31, &&op_32, &&op_33, &&op_34, &&op_35, &&op_36, &&op_37,
        &&op_38, &&op_39, &&op_3A, &&op_3B, &&op_3C, &&op_3D, &&op_3E, &&op_3F,
        &&op_40, &&op_41, &&op_42, &&op_43, &&op_44, &&op_45, &&op_46, &&op_47,
        &&op_48, &&op_49, &&op_4A, &&op_4B, &&op_4C, &&op_4D, &&op_4E, &&op_4F,
        &&op_50, &&op_51, &&op_52, &&op_53, &&op_54, &&op_55, &&op_56, &&op_57,
        &&op_58, &&op_59, &&op_5A, &&op_5B, &&op_5C, &&op_5D, &&op_5E, &&op_5F,
        &&op_60, &&op_61, &&op_62, &&op_63, &&op_64, &&op_65, &&op_66, &&op_67,
        &&op_68, &&op_69, &&op_6A, &&op_6B, &&op_6C, &&op_6D, &&op_6E, &&op_6F,
        &&op_70, &&op_71, &&op_72, &&op_73, &&op_74, &&op_75, &&op_76, &&op_77,
        &&op_78, &&op_79, &&op_7A, &&op_7B, &&op_7C, &&op_7D, &&op_7E, &&op_7F,
        &&op_80, &&op_81, &&op_82, &&op_83, &&op_84, &&op_85, &&op_86, &&op_87,
        &&op_88, &&op_89, &&op_8A, &&op_8B, &&op_8C, &&op_8D, &&op_8E, &&op_8F,
        &&op_90, &&op_91, &&op_92, &&op_93, &&op_94, &&op_95, &&op_96, &&op_97,
        &&op_98, &&op_99, &&op_9A, &&op_9B, &&op_9C, &&op_9D, &&op_9E, &&op_9F,
        &&op_A0, &&op_A1, &&op_A2, &&op_A3, &&op_A4, &&op_A5, &&op_A6, &&op_A7,
        &&op_A8, &&op_A9, &&op_AA, &&op_AB, &&op_AC, &&op_AD, &&op_AE, &&op_AF,
        &&op_B0, &&op_B1, &&op_B2, &&op_B3, &&op_B4, &&op_B5, &&op_B6, &&op_B7,
        &&op_B8, &&op_B9, &&op_BA, &&op_BB, &&op_BC, &&op_BD, &&op_BE, &&op_BF,
        &&op_C0, &&op_C1, &&op_C2, &&op_C3, &&op_C4, &&op_C5, &&op_C6, &&op_C7,
        &&op_C8, &&op_C9, &&op_CA, &&op_CB, &&op_CC, &&op_CD, &&op_CE, &&op_CF,
        &&op_D0, &&op_D1, &&op_D2, &&op_D3, &&op_D4, &&op_D5, &&op_D6, &&op_D7,
        &&op_D8, &&op_D9, &&op_DA, &&op_DB, &&op_DC, &&op_DD, &&op_DE, &&op_DF,
        &&op_E0, &&op_E1, &&op_E2, &&op_E3, &&op_E4, &&op_E5, &&op_E6, &&op_E7,
        &&op_E8, &&op_E9, &&op_EA, &&op_EB, &&op_EC, &&op_ED, &&op_EE, &&op_EF,
        &&op_F0, &&op_F1, &&op_F2, &&op_F3, &&op_F4, &&op_F5, &&op_F6, &&op_F7,
        &&op_F8, &&op_F9, &&op_FA, &&op_FB, &&op_FC, &&op_FD, &&op_FE, &&op_FF,
    };

    /* Dispatch makra pro computed goto */
#define DISPATCH() do { \
    u8 _op = FETCH(); \
    INC_R(); \
    goto *base_dispatch[_op]; \
} while(0)

    /*
     * NEXT: lightweight - jen pricte cykly a dispatch dalsi instrukce.
     * Interrupt handling a post_step jsou ve slow path (check_interrupts label).
     */
#define NEXT(c) do { \
    cycles_executed += (c); \
    if (__builtin_expect(cycles_executed >= target_cycles, 0)) { \
        cpu->cycles += cycles_executed - last_sync; \
        cpu->total_cycles += cycles_executed - last_sync; \
        last_sync = cycles_executed; \
        goto check_interrupts; \
    } \
    DISPATCH(); \
} while(0)

    /* NEXT_SLOW: pro instrukce co meni interrupt stav (EI, DI, HALT, RETI, RETN) */
#define NEXT_SLOW(c) do { \
    cycles_executed += (c); \
    cpu->cycles += cycles_executed - last_sync; \
    cpu->total_cycles += cycles_executed - last_sync; \
    last_sync = cycles_executed; \
    goto check_interrupts; \
} while(0)

#else /* !USE_COMPUTED_GOTO - fallback na switch */

#define DISPATCH() goto dispatch_switch
#define NEXT(c) do { \
    cycles_executed += (c); \
    if (cycles_executed >= target_cycles) { \
        cpu->cycles += cycles_executed - last_sync; \
        cpu->total_cycles += cycles_executed - last_sync; \
        last_sync = cycles_executed; \
        goto check_interrupts; \
    } \
    DISPATCH(); \
} while(0)
#define NEXT_SLOW(c) NEXT(c)

#endif /* USE_COMPUTED_GOTO */

    /* Vstupni bod: pocatecni interrupt check a start dispatch */
    {
        bool ld_a_ir_was = cpu->ld_a_ir;
        cpu->ld_a_ir = false;
        if (cpu->nmi_pending || (cpu->iff1 && !cpu->ei_delay)) {
            WRITEBACK();
            int int_cyc = handle_interrupts_internal(cpu);
            if (int_cyc > 0) {
                if (ld_a_ir_was) cpu->af.l &= ~PF;
                cpu->cycles += int_cyc;
                cpu->total_cycles += int_cyc;
                cycles_executed += int_cyc;
                last_sync = cycles_executed;
                RELOAD();
            } else {
                RELOAD();
            }
        }
        cpu->ei_delay = false;
    }

    if (cpu->halted) {
        cycles = 4;
        cpu->cycles += cycles;
        cpu->total_cycles += cycles;
        cycles_executed += cycles;
        last_sync = cycles_executed;
        goto done;
    }

    DISPATCH();

    /* Slow path: zpracovani preruseni, post_step, kontrola halted */
check_interrupts:
    {
        /* Post-step callback (MZ-700 per-line WAIT) */
        if (post_step_fn) {
            WRITEBACK();
            cpu->wait_cycles = 0;
            post_step_fn();
            if (cpu->wait_cycles > 0) {
                cpu->cycles += cpu->wait_cycles;
                cpu->total_cycles += cpu->wait_cycles;
                cycles_executed += cpu->wait_cycles;
                last_sync = cycles_executed;
            }
            RELOAD();
        }

        /* Zpracovani preruseni */
        bool ld_a_ir_was = cpu->ld_a_ir;
        cpu->ld_a_ir = false;

        if (cpu->nmi_pending || (cpu->iff1 && !cpu->ei_delay)) {
            WRITEBACK();
            int int_cyc = handle_interrupts_internal(cpu);
            if (int_cyc > 0) {
                if (ld_a_ir_was) cpu->af.l &= ~PF;
                cpu->cycles += int_cyc;
                cpu->total_cycles += int_cyc;
                cycles_executed += int_cyc;
                last_sync = cycles_executed;
                RELOAD();
            } else {
                RELOAD();
            }
        }
        cpu->ei_delay = false;

        /* HALT - cykly uz pricteny v NEXT_SLOW(4) */
        if (cpu->halted) goto done;

        if (cycles_executed >= target_cycles) goto done;

        DISPATCH();
    }

    /* ========== Base opkody (0x00-0xFF) ========== */

    /* NOP */
    op_00: NEXT(4);

    /* LD BC, nn */
    op_01: rC = FETCH(); rB = FETCH(); NEXT(10);

    /* LD (BC), A */
    op_02: wr(BC_VAL, rA); SET_WZL((BC_VAL + 1) & 0xFF); SET_WZH(rA); NEXT(7);

    /* INC BC */
    op_03: { u16 bc = BC_VAL + 1; SET_BC(bc); NEXT(6); }

    /* INC B */
    op_04: rB = INC8(rB); NEXT(4);

    /* DEC B */
    op_05: rB = DEC8(rB); NEXT(4);

    /* LD B, n */
    op_06: rB = FETCH(); NEXT(7);

    /* RLCA */
    op_07: { u8 c = rA >> 7; rA = (rA << 1) | c; rF = (rF & (SF|ZF|PF)) | (rA & (F3|F5)) | c; NEXT(4); }

    /* EX AF, AF' */
    op_08: { u16 tmp = AF_VAL; u16 af2 = cpu->af2.w; SET_AF(af2); cpu->af2.w = tmp; NEXT(4); }

    /* ADD HL, BC */
    op_09: rWZ = HL_VAL + 1; ADD16(rH, rL, BC_VAL); NEXT(11);

    /* LD A, (BC) */
    op_0A: rA = rd(BC_VAL); rWZ = BC_VAL + 1; NEXT(7);

    /* DEC BC */
    op_0B: { u16 bc = BC_VAL - 1; SET_BC(bc); NEXT(6); }

    /* INC C */
    op_0C: rC = INC8(rC); NEXT(4);

    /* DEC C */
    op_0D: rC = DEC8(rC); NEXT(4);

    /* LD C, n */
    op_0E: rC = FETCH(); NEXT(7);

    /* RRCA */
    op_0F: { u8 c = rA & 1; rA = (rA >> 1) | (c << 7); rF = (rF & (SF|ZF|PF)) | (rA & (F3|F5)) | c; NEXT(4); }

    /* DJNZ */
    op_10: { s8 d = (s8)FETCH(); rB--; if (rB != 0) { rPC += d; rWZ = rPC; NEXT(13); } else { NEXT(8); } }

    /* LD DE, nn */
    op_11: rE = FETCH(); rD = FETCH(); NEXT(10);

    /* LD (DE), A */
    op_12: wr(DE_VAL, rA); SET_WZL((DE_VAL + 1) & 0xFF); SET_WZH(rA); NEXT(7);

    /* INC DE */
    op_13: { u16 de = DE_VAL + 1; SET_DE(de); NEXT(6); }

    /* INC D */
    op_14: rD = INC8(rD); NEXT(4);

    /* DEC D */
    op_15: rD = DEC8(rD); NEXT(4);

    /* LD D, n */
    op_16: rD = FETCH(); NEXT(7);

    /* RLA */
    op_17: { u8 c = rA >> 7; rA = (rA << 1) | (rF & CF); rF = (rF & (SF|ZF|PF)) | (rA & (F3|F5)) | c; NEXT(4); }

    /* JR d */
    op_18: { s8 d = (s8)FETCH(); rPC += d; rWZ = rPC; NEXT(12); }

    /* ADD HL, DE */
    op_19: rWZ = HL_VAL + 1; ADD16(rH, rL, DE_VAL); NEXT(11);

    /* LD A, (DE) */
    op_1A: rA = rd(DE_VAL); rWZ = DE_VAL + 1; NEXT(7);

    /* DEC DE */
    op_1B: { u16 de = DE_VAL - 1; SET_DE(de); NEXT(6); }

    /* INC E */
    op_1C: rE = INC8(rE); NEXT(4);

    /* DEC E */
    op_1D: rE = DEC8(rE); NEXT(4);

    /* LD E, n */
    op_1E: rE = FETCH(); NEXT(7);

    /* RRA */
    op_1F: { u8 c = rA & 1; rA = (rA >> 1) | ((rF & CF) << 7); rF = (rF & (SF|ZF|PF)) | (rA & (F3|F5)) | c; NEXT(4); }

    /* JR NZ, d */
    op_20: { s8 d = (s8)FETCH(); if (!(rF & ZF)) { rPC += d; rWZ = rPC; NEXT(12); } else { NEXT(7); } }

    /* LD HL, nn */
    op_21: rL = FETCH(); rH = FETCH(); NEXT(10);

    /* LD (nn), HL */
    op_22: { u16 addr = FETCH16(); wr16(addr, HL_VAL); rWZ = addr + 1; NEXT(16); }

    /* INC HL */
    op_23: { u16 hl = HL_VAL + 1; SET_HL(hl); NEXT(6); }

    /* INC H */
    op_24: rH = INC8(rH); NEXT(4);

    /* DEC H */
    op_25: rH = DEC8(rH); NEXT(4);

    /* LD H, n */
    op_26: rH = FETCH(); NEXT(7);

    /* DAA */
    op_27: {
        u16 daa_idx = rA | ((rF & CF) ? 0x100 : 0) | ((rF & HF) ? 0x200 : 0) | ((rF & NF) ? 0x400 : 0);
        u16 daa_val = daa_table[daa_idx];
        rA = (u8)daa_val;
        rF = (u8)(daa_val >> 8);
        NEXT(4);
    }

    /* JR Z, d */
    op_28: { s8 d = (s8)FETCH(); if (rF & ZF) { rPC += d; rWZ = rPC; NEXT(12); } else { NEXT(7); } }

    /* ADD HL, HL */
    op_29: rWZ = HL_VAL + 1; ADD16(rH, rL, HL_VAL); NEXT(11);

    /* LD HL, (nn) */
    op_2A: { u16 addr = FETCH16(); u16 v = rd16(addr); SET_HL(v); rWZ = addr + 1; NEXT(16); }

    /* DEC HL */
    op_2B: { u16 hl = HL_VAL - 1; SET_HL(hl); NEXT(6); }

    /* INC L */
    op_2C: rL = INC8(rL); NEXT(4);

    /* DEC L */
    op_2D: rL = DEC8(rL); NEXT(4);

    /* LD L, n */
    op_2E: rL = FETCH(); NEXT(7);

    /* CPL */
    op_2F: rA = ~rA; rF = (rF & (SF|ZF|PF|CF)) | (rA & (F3|F5)) | NF | HF; NEXT(4);

    /* JR NC, d */
    op_30: { s8 d = (s8)FETCH(); if (!(rF & CF)) { rPC += d; rWZ = rPC; NEXT(12); } else { NEXT(7); } }

    /* LD SP, nn */
    op_31: rSP = FETCH16(); NEXT(10);

    /* LD (nn), A */
    op_32: { u16 addr = FETCH16(); wr(addr, rA); SET_WZL((addr + 1) & 0xFF); SET_WZH(rA); NEXT(13); }

    /* INC SP */
    op_33: rSP++; NEXT(6);

    /* INC (HL) */
    op_34: { u8 v = rd(HL_VAL); wr(HL_VAL, INC8(v)); NEXT(11); }

    /* DEC (HL) */
    op_35: { u8 v = rd(HL_VAL); wr(HL_VAL, DEC8(v)); NEXT(11); }

    /* LD (HL), n */
    op_36: wr(HL_VAL, FETCH()); NEXT(10);

    /* SCF */
    op_37: rF = (rF & (SF|ZF|PF)) | ((rA | rF) & (F3|F5)) | CF; NEXT(4);

    /* JR C, d */
    op_38: { s8 d = (s8)FETCH(); if (rF & CF) { rPC += d; rWZ = rPC; NEXT(12); } else { NEXT(7); } }

    /* ADD HL, SP */
    op_39: rWZ = HL_VAL + 1; ADD16(rH, rL, rSP); NEXT(11);

    /* LD A, (nn) */
    op_3A: { u16 addr = FETCH16(); rA = rd(addr); rWZ = addr + 1; NEXT(13); }

    /* DEC SP */
    op_3B: rSP--; NEXT(6);

    /* INC A */
    op_3C: rA = INC8(rA); NEXT(4);

    /* DEC A */
    op_3D: rA = DEC8(rA); NEXT(4);

    /* LD A, n */
    op_3E: rA = FETCH(); NEXT(7);

    /* CCF */
    op_3F: rF = (rF & (SF|ZF|PF)) | ((rA | rF) & (F3|F5)) | ((rF & CF) ? HF : 0) | ((rF & CF) ^ CF); NEXT(4);

    /* LD r, r' */
    op_40: NEXT(4);           /* LD B, B */
    op_41: rB = rC; NEXT(4);
    op_42: rB = rD; NEXT(4);
    op_43: rB = rE; NEXT(4);
    op_44: rB = rH; NEXT(4);
    op_45: rB = rL; NEXT(4);
    op_46: rB = rd(HL_VAL); NEXT(7);
    op_47: rB = rA; NEXT(4);
    op_48: rC = rB; NEXT(4);
    op_49: NEXT(4);           /* LD C, C */
    op_4A: rC = rD; NEXT(4);
    op_4B: rC = rE; NEXT(4);
    op_4C: rC = rH; NEXT(4);
    op_4D: rC = rL; NEXT(4);
    op_4E: rC = rd(HL_VAL); NEXT(7);
    op_4F: rC = rA; NEXT(4);
    op_50: rD = rB; NEXT(4);
    op_51: rD = rC; NEXT(4);
    op_52: NEXT(4);           /* LD D, D */
    op_53: rD = rE; NEXT(4);
    op_54: rD = rH; NEXT(4);
    op_55: rD = rL; NEXT(4);
    op_56: rD = rd(HL_VAL); NEXT(7);
    op_57: rD = rA; NEXT(4);
    op_58: rE = rB; NEXT(4);
    op_59: rE = rC; NEXT(4);
    op_5A: rE = rD; NEXT(4);
    op_5B: NEXT(4);           /* LD E, E */
    op_5C: rE = rH; NEXT(4);
    op_5D: rE = rL; NEXT(4);
    op_5E: rE = rd(HL_VAL); NEXT(7);
    op_5F: rE = rA; NEXT(4);
    op_60: rH = rB; NEXT(4);
    op_61: rH = rC; NEXT(4);
    op_62: rH = rD; NEXT(4);
    op_63: rH = rE; NEXT(4);
    op_64: NEXT(4);           /* LD H, H */
    op_65: rH = rL; NEXT(4);
    op_66: rH = rd(HL_VAL); NEXT(7);
    op_67: rH = rA; NEXT(4);
    op_68: rL = rB; NEXT(4);
    op_69: rL = rC; NEXT(4);
    op_6A: rL = rD; NEXT(4);
    op_6B: rL = rE; NEXT(4);
    op_6C: rL = rH; NEXT(4);
    op_6D: NEXT(4);           /* LD L, L */
    op_6E: rL = rd(HL_VAL); NEXT(7);
    op_6F: rL = rA; NEXT(4);
    op_70: wr(HL_VAL, rB); NEXT(7);
    op_71: wr(HL_VAL, rC); NEXT(7);
    op_72: wr(HL_VAL, rD); NEXT(7);
    op_73: wr(HL_VAL, rE); NEXT(7);
    op_74: wr(HL_VAL, rH); NEXT(7);
    op_75: wr(HL_VAL, rL); NEXT(7);

    /* HALT */
    op_76: cpu->halted = true; NEXT_SLOW(4);

    op_77: wr(HL_VAL, rA); NEXT(7);
    op_78: rA = rB; NEXT(4);
    op_79: rA = rC; NEXT(4);
    op_7A: rA = rD; NEXT(4);
    op_7B: rA = rE; NEXT(4);
    op_7C: rA = rH; NEXT(4);
    op_7D: rA = rL; NEXT(4);
    op_7E: rA = rd(HL_VAL); NEXT(7);
    op_7F: NEXT(4);           /* LD A, A */

    /* ADD A, r */
    op_80: rA = ADD8(rA, rB); NEXT(4);
    op_81: rA = ADD8(rA, rC); NEXT(4);
    op_82: rA = ADD8(rA, rD); NEXT(4);
    op_83: rA = ADD8(rA, rE); NEXT(4);
    op_84: rA = ADD8(rA, rH); NEXT(4);
    op_85: rA = ADD8(rA, rL); NEXT(4);
    op_86: rA = ADD8(rA, rd(HL_VAL)); NEXT(7);
    op_87: rA = ADD8(rA, rA); NEXT(4);

    /* ADC A, r */
    op_88: rA = ADC8(rA, rB); NEXT(4);
    op_89: rA = ADC8(rA, rC); NEXT(4);
    op_8A: rA = ADC8(rA, rD); NEXT(4);
    op_8B: rA = ADC8(rA, rE); NEXT(4);
    op_8C: rA = ADC8(rA, rH); NEXT(4);
    op_8D: rA = ADC8(rA, rL); NEXT(4);
    op_8E: rA = ADC8(rA, rd(HL_VAL)); NEXT(7);
    op_8F: rA = ADC8(rA, rA); NEXT(4);

    /* SUB r */
    op_90: rA = SUB8(rA, rB); NEXT(4);
    op_91: rA = SUB8(rA, rC); NEXT(4);
    op_92: rA = SUB8(rA, rD); NEXT(4);
    op_93: rA = SUB8(rA, rE); NEXT(4);
    op_94: rA = SUB8(rA, rH); NEXT(4);
    op_95: rA = SUB8(rA, rL); NEXT(4);
    op_96: rA = SUB8(rA, rd(HL_VAL)); NEXT(7);
    op_97: rA = SUB8(rA, rA); NEXT(4);

    /* SBC A, r */
    op_98: rA = SBC8(rA, rB); NEXT(4);
    op_99: rA = SBC8(rA, rC); NEXT(4);
    op_9A: rA = SBC8(rA, rD); NEXT(4);
    op_9B: rA = SBC8(rA, rE); NEXT(4);
    op_9C: rA = SBC8(rA, rH); NEXT(4);
    op_9D: rA = SBC8(rA, rL); NEXT(4);
    op_9E: rA = SBC8(rA, rd(HL_VAL)); NEXT(7);
    op_9F: rA = SBC8(rA, rA); NEXT(4);

    /* AND r */
    op_A0: AND8(rB); NEXT(4);
    op_A1: AND8(rC); NEXT(4);
    op_A2: AND8(rD); NEXT(4);
    op_A3: AND8(rE); NEXT(4);
    op_A4: AND8(rH); NEXT(4);
    op_A5: AND8(rL); NEXT(4);
    op_A6: AND8(rd(HL_VAL)); NEXT(7);
    op_A7: AND8(rA); NEXT(4);

    /* XOR r */
    op_A8: XOR8(rB); NEXT(4);
    op_A9: XOR8(rC); NEXT(4);
    op_AA: XOR8(rD); NEXT(4);
    op_AB: XOR8(rE); NEXT(4);
    op_AC: XOR8(rH); NEXT(4);
    op_AD: XOR8(rL); NEXT(4);
    op_AE: XOR8(rd(HL_VAL)); NEXT(7);
    op_AF: XOR8(rA); NEXT(4);

    /* OR r */
    op_B0: OR8(rB); NEXT(4);
    op_B1: OR8(rC); NEXT(4);
    op_B2: OR8(rD); NEXT(4);
    op_B3: OR8(rE); NEXT(4);
    op_B4: OR8(rH); NEXT(4);
    op_B5: OR8(rL); NEXT(4);
    op_B6: OR8(rd(HL_VAL)); NEXT(7);
    op_B7: OR8(rA); NEXT(4);

    /* CP r */
    op_B8: CP8(rB); NEXT(4);
    op_B9: CP8(rC); NEXT(4);
    op_BA: CP8(rD); NEXT(4);
    op_BB: CP8(rE); NEXT(4);
    op_BC: CP8(rH); NEXT(4);
    op_BD: CP8(rL); NEXT(4);
    op_BE: CP8(rd(HL_VAL)); NEXT(7);
    op_BF: CP8(rA); NEXT(4);

    /* RET NZ/Z/NC/C/PO/PE/P/M */
    op_C0: if (!(rF & ZF)) { rPC = POP(); rWZ = rPC; NEXT(11); } else { NEXT(5); }
    op_C8: if (rF & ZF)    { rPC = POP(); rWZ = rPC; NEXT(11); } else { NEXT(5); }
    op_D0: if (!(rF & CF)) { rPC = POP(); rWZ = rPC; NEXT(11); } else { NEXT(5); }
    op_D8: if (rF & CF)    { rPC = POP(); rWZ = rPC; NEXT(11); } else { NEXT(5); }
    op_E0: if (!(rF & PF)) { rPC = POP(); rWZ = rPC; NEXT(11); } else { NEXT(5); }
    op_E8: if (rF & PF)    { rPC = POP(); rWZ = rPC; NEXT(11); } else { NEXT(5); }
    op_F0: if (!(rF & SF)) { rPC = POP(); rWZ = rPC; NEXT(11); } else { NEXT(5); }
    op_F8: if (rF & SF)    { rPC = POP(); rWZ = rPC; NEXT(11); } else { NEXT(5); }

    /* RET */
    op_C9: rPC = POP(); rWZ = rPC; NEXT(10);

    /* POP rr */
    op_C1: { u16 v = POP(); SET_BC(v); NEXT(10); }
    op_D1: { u16 v = POP(); SET_DE(v); NEXT(10); }
    op_E1: { u16 v = POP(); SET_HL(v); NEXT(10); }
    op_F1: { u16 v = POP(); SET_AF(v); NEXT(10); }

    /* PUSH rr */
    op_C5: PUSH(BC_VAL); NEXT(11);
    op_D5: PUSH(DE_VAL); NEXT(11);
    op_E5: PUSH(HL_VAL); NEXT(11);
    op_F5: PUSH(AF_VAL); NEXT(11);

    /* JP cc, nn */
    op_C2: { u16 addr = FETCH16(); rWZ = addr; if (!(rF & ZF)) rPC = addr; NEXT(10); }
    op_CA: { u16 addr = FETCH16(); rWZ = addr; if (rF & ZF)    rPC = addr; NEXT(10); }
    op_D2: { u16 addr = FETCH16(); rWZ = addr; if (!(rF & CF)) rPC = addr; NEXT(10); }
    op_DA: { u16 addr = FETCH16(); rWZ = addr; if (rF & CF)    rPC = addr; NEXT(10); }
    op_E2: { u16 addr = FETCH16(); rWZ = addr; if (!(rF & PF)) rPC = addr; NEXT(10); }
    op_EA: { u16 addr = FETCH16(); rWZ = addr; if (rF & PF)    rPC = addr; NEXT(10); }
    op_F2: { u16 addr = FETCH16(); rWZ = addr; if (!(rF & SF)) rPC = addr; NEXT(10); }
    op_FA: { u16 addr = FETCH16(); rWZ = addr; if (rF & SF)    rPC = addr; NEXT(10); }

    /* JP nn */
    op_C3: { u16 addr = FETCH16(); rWZ = addr; rPC = addr; NEXT(10); }

    /* CALL cc, nn */
    op_C4: { u16 addr = FETCH16(); rWZ = addr; if (!(rF & ZF)) { PUSH(rPC); rPC = addr; NEXT(17); } else { NEXT(10); } }
    op_CC: { u16 addr = FETCH16(); rWZ = addr; if (rF & ZF)    { PUSH(rPC); rPC = addr; NEXT(17); } else { NEXT(10); } }
    op_D4: { u16 addr = FETCH16(); rWZ = addr; if (!(rF & CF)) { PUSH(rPC); rPC = addr; NEXT(17); } else { NEXT(10); } }
    op_DC: { u16 addr = FETCH16(); rWZ = addr; if (rF & CF)    { PUSH(rPC); rPC = addr; NEXT(17); } else { NEXT(10); } }
    op_E4: { u16 addr = FETCH16(); rWZ = addr; if (!(rF & PF)) { PUSH(rPC); rPC = addr; NEXT(17); } else { NEXT(10); } }
    op_EC: { u16 addr = FETCH16(); rWZ = addr; if (rF & PF)    { PUSH(rPC); rPC = addr; NEXT(17); } else { NEXT(10); } }
    op_F4: { u16 addr = FETCH16(); rWZ = addr; if (!(rF & SF)) { PUSH(rPC); rPC = addr; NEXT(17); } else { NEXT(10); } }
    op_FC: { u16 addr = FETCH16(); rWZ = addr; if (rF & SF)    { PUSH(rPC); rPC = addr; NEXT(17); } else { NEXT(10); } }

    /* CALL nn */
    op_CD: { u16 addr = FETCH16(); rWZ = addr; PUSH(rPC); rPC = addr; NEXT(17); }

    /* Aritmetika s okamzitym operandem */
    op_C6: { u8 v = FETCH(); rA = ADD8(rA, v); NEXT(7); }
    op_CE: { u8 v = FETCH(); rA = ADC8(rA, v); NEXT(7); }
    op_D6: { u8 v = FETCH(); rA = SUB8(rA, v); NEXT(7); }
    op_DE: { u8 v = FETCH(); rA = SBC8(rA, v); NEXT(7); }
    op_E6: { u8 v = FETCH(); AND8(v); NEXT(7); }
    op_EE: { u8 v = FETCH(); XOR8(v); NEXT(7); }
    op_F6: { u8 v = FETCH(); OR8(v); NEXT(7); }
    op_FE: { u8 v = FETCH(); CP8(v); NEXT(7); }

    /* RST nn */
    op_C7: PUSH(rPC); rPC = 0x00; rWZ = rPC; NEXT(11);
    op_CF: PUSH(rPC); rPC = 0x08; rWZ = rPC; NEXT(11);
    op_D7: PUSH(rPC); rPC = 0x10; rWZ = rPC; NEXT(11);
    op_DF: PUSH(rPC); rPC = 0x18; rWZ = rPC; NEXT(11);
    op_E7: PUSH(rPC); rPC = 0x20; rWZ = rPC; NEXT(11);
    op_EF: PUSH(rPC); rPC = 0x28; rWZ = rPC; NEXT(11);
    op_F7: PUSH(rPC); rPC = 0x30; rWZ = rPC; NEXT(11);
    op_FF: PUSH(rPC); rPC = 0x38; rWZ = rPC; NEXT(11);

    /* OUT (n), A */
    op_D3: { u8 port = FETCH(); u16 pa = (u16)((rA << 8) | port); io_wr(pa, rA); SET_WZL((port+1)&0xFF); SET_WZH(rA); NEXT(11); }

    /* IN A, (n) */
    op_DB: { u8 port = FETCH(); u16 pa = (u16)((rA << 8) | port); rWZ = pa + 1; rA = io_rd(pa); NEXT(11); }

    /* EX DE, HL */
    op_EB: { u8 t; t = rD; rD = rH; rH = t; t = rE; rE = rL; rL = t; NEXT(4); }

    /* EX (SP), HL */
    op_E3: { u16 v = rd16(rSP); wr16(rSP, HL_VAL); SET_HL(v); rWZ = v; NEXT(19); }

    /* JP (HL) */
    op_E9: rPC = HL_VAL; NEXT(4);

    /* LD SP, HL */
    op_F9: rSP = HL_VAL; NEXT(6);

    /* DI */
    op_F3: cpu->iff1 = 0; cpu->iff2 = 0; NEXT_SLOW(4);

    /* EI */
    op_FB: cpu->iff1 = 1; cpu->iff2 = 1; cpu->ei_delay = true;
           if (ei_fn) { WRITEBACK(); ei_fn(); RELOAD(); }
           NEXT_SLOW(4);

    /* EXX */
    op_D9: {
        u8 t;
        t = rB; rB = cpu->bc2.h; cpu->bc2.h = t;
        t = rC; rC = cpu->bc2.l; cpu->bc2.l = t;
        t = rD; rD = cpu->de2.h; cpu->de2.h = t;
        t = rE; rE = cpu->de2.l; cpu->de2.l = t;
        t = rH; rH = cpu->hl2.h; cpu->hl2.h = t;
        t = rL; rL = cpu->hl2.l; cpu->hl2.l = t;
        NEXT(4);
    }

    /* ========== CB prefix ========== */
    op_CB: {
        INC_R();
        u8 cbop = FETCH();
        int reg = cbop & 0x07;
        int bit_n = (cbop >> 3) & 0x07;
        int group = (cbop >> 6) & 0x03;

        u8 val;
        switch (reg) {
            case 0: val = rB; break;
            case 1: val = rC; break;
            case 2: val = rD; break;
            case 3: val = rE; break;
            case 4: val = rH; break;
            case 5: val = rL; break;
            case 6: val = rd(HL_VAL); break;
            case 7: val = rA; break;
            default: val = 0; break;
        }

        u8 result = val;
        int cb_cycles = 8;

        switch (group) {
            case 0: /* Rotace/posuvy */
                switch (bit_n) {
                    case 0: result = RLC(val); break;
                    case 1: result = RRC(val); break;
                    case 2: result = RL(val); break;
                    case 3: result = RR(val); break;
                    case 4: result = SLA(val); break;
                    case 5: result = SRA(val); break;
                    case 6: result = SLL(val); break;
                    case 7: result = SRL(val); break;
                }
                if (reg == 6) cb_cycles = 15;
                break;
            case 1: /* BIT */
                if (reg == 6) {
                    BIT_MEM(bit_n, val, WZH_VAL);
                    NEXT(12);
                } else {
                    BIT_OP(bit_n, val);
                    NEXT(8);
                }
            case 2: /* RES */
                result = val & ~(1 << bit_n);
                if (reg == 6) cb_cycles = 15;
                break;
            case 3: /* SET */
                result = val | (1 << bit_n);
                if (reg == 6) cb_cycles = 15;
                break;
        }

        switch (reg) {
            case 0: rB = result; break;
            case 1: rC = result; break;
            case 2: rD = result; break;
            case 3: rE = result; break;
            case 4: rH = result; break;
            case 5: rL = result; break;
            case 6: wr(HL_VAL, result); break;
            case 7: rA = result; break;
        }
        NEXT(cb_cycles);
    }

    /* ========== ED prefix ========== */
    op_ED: {
        INC_R();
        u8 edop = FETCH();

        switch (edop) {
            /* IN r, (C) */
            case 0x40: rWZ = BC_VAL + 1; rB = io_rd(BC_VAL); rF = (rF & CF) | sz53p_table[rB]; NEXT(12);
            case 0x48: rWZ = BC_VAL + 1; rC = io_rd(BC_VAL); rF = (rF & CF) | sz53p_table[rC]; NEXT(12);
            case 0x50: rWZ = BC_VAL + 1; rD = io_rd(BC_VAL); rF = (rF & CF) | sz53p_table[rD]; NEXT(12);
            case 0x58: rWZ = BC_VAL + 1; rE = io_rd(BC_VAL); rF = (rF & CF) | sz53p_table[rE]; NEXT(12);
            case 0x60: rWZ = BC_VAL + 1; rH = io_rd(BC_VAL); rF = (rF & CF) | sz53p_table[rH]; NEXT(12);
            case 0x68: rWZ = BC_VAL + 1; rL = io_rd(BC_VAL); rF = (rF & CF) | sz53p_table[rL]; NEXT(12);
            case 0x70: { rWZ = BC_VAL + 1; u8 t = io_rd(BC_VAL); rF = (rF & CF) | sz53p_table[t]; NEXT(12); }
            case 0x78: rWZ = BC_VAL + 1; rA = io_rd(BC_VAL); rF = (rF & CF) | sz53p_table[rA]; NEXT(12);

            /* OUT (C), r */
            case 0x41: io_wr(BC_VAL, rB); rWZ = BC_VAL + 1; NEXT(12);
            case 0x49: io_wr(BC_VAL, rC); rWZ = BC_VAL + 1; NEXT(12);
            case 0x51: io_wr(BC_VAL, rD); rWZ = BC_VAL + 1; NEXT(12);
            case 0x59: io_wr(BC_VAL, rE); rWZ = BC_VAL + 1; NEXT(12);
            case 0x61: io_wr(BC_VAL, rH); rWZ = BC_VAL + 1; NEXT(12);
            case 0x69: io_wr(BC_VAL, rL); rWZ = BC_VAL + 1; NEXT(12);
            case 0x71: io_wr(BC_VAL, 0);  rWZ = BC_VAL + 1; NEXT(12);
            case 0x79: io_wr(BC_VAL, rA); rWZ = BC_VAL + 1; NEXT(12);

            /* SBC HL, rr */
            case 0x42: rWZ = HL_VAL + 1; SBC16(BC_VAL); NEXT(15);
            case 0x52: rWZ = HL_VAL + 1; SBC16(DE_VAL); NEXT(15);
            case 0x62: rWZ = HL_VAL + 1; SBC16(HL_VAL); NEXT(15);
            case 0x72: rWZ = HL_VAL + 1; SBC16(rSP); NEXT(15);

            /* ADC HL, rr */
            case 0x4A: rWZ = HL_VAL + 1; ADC16(BC_VAL); NEXT(15);
            case 0x5A: rWZ = HL_VAL + 1; ADC16(DE_VAL); NEXT(15);
            case 0x6A: rWZ = HL_VAL + 1; ADC16(HL_VAL); NEXT(15);
            case 0x7A: rWZ = HL_VAL + 1; ADC16(rSP); NEXT(15);

            /* LD (nn), rr */
            case 0x43: { u16 addr = FETCH16(); wr16(addr, BC_VAL); rWZ = addr + 1; NEXT(20); }
            case 0x53: { u16 addr = FETCH16(); wr16(addr, DE_VAL); rWZ = addr + 1; NEXT(20); }
            case 0x63: { u16 addr = FETCH16(); wr16(addr, HL_VAL); rWZ = addr + 1; NEXT(20); }
            case 0x73: { u16 addr = FETCH16(); wr16(addr, rSP); rWZ = addr + 1; NEXT(20); }

            /* LD rr, (nn) */
            case 0x4B: { u16 addr = FETCH16(); u16 v = rd16(addr); SET_BC(v); rWZ = addr + 1; NEXT(20); }
            case 0x5B: { u16 addr = FETCH16(); u16 v = rd16(addr); SET_DE(v); rWZ = addr + 1; NEXT(20); }
            case 0x6B: { u16 addr = FETCH16(); u16 v = rd16(addr); SET_HL(v); rWZ = addr + 1; NEXT(20); }
            case 0x7B: { u16 addr = FETCH16(); rSP = rd16(addr); rWZ = addr + 1; NEXT(20); }

            /* NEG */
            case 0x44: case 0x4C: case 0x54: case 0x5C:
            case 0x64: case 0x6C: case 0x74: case 0x7C: {
                u8 t = rA; rA = 0; rA = SUB8(rA, t); NEXT(8);
            }

            /* RETN */
            case 0x45: case 0x55: case 0x65: case 0x75:
                cpu->iff1 = cpu->iff2; rPC = POP(); rWZ = rPC; NEXT_SLOW(14);

            /* RETI */
            case 0x4D: case 0x5D: case 0x6D: case 0x7D:
                cpu->iff1 = cpu->iff2; rPC = POP(); rWZ = rPC;
                if (reti_fn) reti_fn();
                NEXT_SLOW(14);

            /* IM */
            case 0x46: case 0x66: cpu->im = 0; NEXT(8);
            case 0x56: case 0x76: cpu->im = 1; NEXT(8);
            case 0x5E: case 0x7E: cpu->im = 2; NEXT(8);
            case 0x4E: case 0x6E: cpu->im = 0; NEXT(8);

            /* LD I, A / LD R, A */
            case 0x47: cpu->i = rA; NEXT(9);
            case 0x4F: rR = rA; NEXT(9);

            /* LD A, I */
            case 0x57:
                rA = cpu->i;
                rF = (rF & CF) | sz53_table[rA] | (cpu->iff2 ? PF : 0);
                cpu->ld_a_ir = true;
                NEXT_SLOW(9);

            /* LD A, R */
            case 0x5F:
                rA = rR;
                rF = (rF & CF) | sz53_table[rA] | (cpu->iff2 ? PF : 0);
                cpu->ld_a_ir = true;
                NEXT_SLOW(9);

            /* RRD */
            case 0x67: {
                u8 val = rd(HL_VAL);
                u8 nv = (rA << 4) | (val >> 4);
                rA = (rA & 0xF0) | (val & 0x0F);
                wr(HL_VAL, nv);
                rF = (rF & CF) | sz53p_table[rA];
                rWZ = HL_VAL + 1;
                NEXT(18);
            }

            /* RLD */
            case 0x6F: {
                u8 val = rd(HL_VAL);
                u8 nv = (val << 4) | (rA & 0x0F);
                rA = (rA & 0xF0) | (val >> 4);
                wr(HL_VAL, nv);
                rF = (rF & CF) | sz53p_table[rA];
                rWZ = HL_VAL + 1;
                NEXT(18);
            }

            /* LDI */
            case 0xA0: {
                u8 val = rd(HL_VAL);
                u16 de = DE_VAL; wr(de, val);
                de++; SET_DE(de);
                u16 hl = HL_VAL + 1; SET_HL(hl);
                u16 bc = BC_VAL - 1; SET_BC(bc);
                u8 n = val + rA;
                rF = (rF & (SF|ZF|CF)) | (bc != 0 ? PF : 0) | (n & F3) | ((n & 0x02) ? F5 : 0);
                NEXT(16);
            }

            /* CPI */
            case 0xA1: {
                u8 val = rd(HL_VAL);
                u8 res = rA - val;
                u16 hl = HL_VAL + 1; SET_HL(hl);
                u16 bc = BC_VAL - 1; SET_BC(bc);
                u8 nhf = (rA ^ val ^ res) & HF;
                u8 n = res - (nhf ? 1 : 0);
                rF = (rF & CF) | NF | (bc != 0 ? PF : 0) | sz53_table[res & 0xFF] | nhf;
                rF = (rF & ~(F3|F5)) | (n & F3) | ((n & 0x02) ? F5 : 0);
                rWZ++;
                NEXT(16);
            }

            /* INI */
            case 0xA2: {
                rWZ = BC_VAL + 1;
                u8 val = io_rd(BC_VAL);
                wr(HL_VAL, val);
                u16 hl = HL_VAL + 1; SET_HL(hl);
                rB--;
                u16 k = (u16)val + (u16)((rC + 1) & 0xFF);
                rF = sz53_table[rB]
                   | ((val & 0x80) ? NF : 0)
                   | (k > 255 ? (HF|CF) : 0)
                   | (parity_table[(u8)((k & 7) ^ rB)] ? PF : 0);
                NEXT(16);
            }

            /* OUTI */
            case 0xA3: {
                u8 val = rd(HL_VAL);
                rB--;
                io_wr(BC_VAL, val);
                u16 hl = HL_VAL + 1; SET_HL(hl);
                rWZ = BC_VAL + 1;
                u16 k = (u16)val + (u16)rL;
                rF = sz53_table[rB]
                   | ((val & 0x80) ? NF : 0)
                   | (k > 255 ? (HF|CF) : 0)
                   | (parity_table[(u8)((k & 7) ^ rB)] ? PF : 0);
                NEXT(16);
            }

            /* LDD */
            case 0xA8: {
                u8 val = rd(HL_VAL);
                u16 de = DE_VAL; wr(de, val);
                de--; SET_DE(de);
                u16 hl = HL_VAL - 1; SET_HL(hl);
                u16 bc = BC_VAL - 1; SET_BC(bc);
                u8 n = val + rA;
                rF = (rF & (SF|ZF|CF)) | (bc != 0 ? PF : 0) | (n & F3) | ((n & 0x02) ? F5 : 0);
                NEXT(16);
            }

            /* CPD */
            case 0xA9: {
                u8 val = rd(HL_VAL);
                u8 res = rA - val;
                u16 hl = HL_VAL - 1; SET_HL(hl);
                u16 bc = BC_VAL - 1; SET_BC(bc);
                u8 nhf = (rA ^ val ^ res) & HF;
                u8 n = res - (nhf ? 1 : 0);
                rF = (rF & CF) | NF | (bc != 0 ? PF : 0) | sz53_table[res & 0xFF] | nhf;
                rF = (rF & ~(F3|F5)) | (n & F3) | ((n & 0x02) ? F5 : 0);
                rWZ--;
                NEXT(16);
            }

            /* IND */
            case 0xAA: {
                rWZ = BC_VAL - 1;
                u8 val = io_rd(BC_VAL);
                wr(HL_VAL, val);
                u16 hl = HL_VAL - 1; SET_HL(hl);
                rB--;
                u16 k = (u16)val + (u16)((rC - 1) & 0xFF);
                rF = sz53_table[rB]
                   | ((val & 0x80) ? NF : 0)
                   | (k > 255 ? (HF|CF) : 0)
                   | (parity_table[(u8)((k & 7) ^ rB)] ? PF : 0);
                NEXT(16);
            }

            /* OUTD */
            case 0xAB: {
                u8 val = rd(HL_VAL);
                rB--;
                io_wr(BC_VAL, val);
                u16 hl = HL_VAL - 1; SET_HL(hl);
                rWZ = BC_VAL - 1;
                u16 k = (u16)val + (u16)rL;
                rF = sz53_table[rB]
                   | ((val & 0x80) ? NF : 0)
                   | (k > 255 ? (HF|CF) : 0)
                   | (parity_table[(u8)((k & 7) ^ rB)] ? PF : 0);
                NEXT(16);
            }

            /* LDIR */
            case 0xB0: {
                u8 val = rd(HL_VAL);
                u16 de = DE_VAL; wr(de, val);
                de++; SET_DE(de);
                u16 hl = HL_VAL + 1; SET_HL(hl);
                u16 bc = BC_VAL - 1; SET_BC(bc);
                u8 n = val + rA;
                rF = (rF & (SF|ZF|CF)) | (bc != 0 ? PF : 0) | (n & F3) | ((n & 0x02) ? F5 : 0);
                if (bc != 0) { rPC -= 2; rWZ = rPC + 1; NEXT(21); }
                NEXT(16);
            }

            /* CPIR */
            case 0xB1: {
                u8 val = rd(HL_VAL);
                u8 res = rA - val;
                u16 hl = HL_VAL + 1; SET_HL(hl);
                u16 bc = BC_VAL - 1; SET_BC(bc);
                u8 nhf = (rA ^ val ^ res) & HF;
                u8 n = res - (nhf ? 1 : 0);
                rF = (rF & CF) | NF | (bc != 0 ? PF : 0) | sz53_table[res & 0xFF] | nhf;
                rF = (rF & ~(F3|F5)) | (n & F3) | ((n & 0x02) ? F5 : 0);
                if (bc != 0 && !(rF & ZF)) { rPC -= 2; rWZ = rPC + 1; NEXT(21); }
                rWZ++;
                NEXT(16);
            }

            /* INIR */
            case 0xB2: {
                rWZ = BC_VAL + 1;
                u8 val = io_rd(BC_VAL);
                wr(HL_VAL, val);
                u16 hl = HL_VAL + 1; SET_HL(hl);
                rB--;
                u16 k = (u16)val + (u16)((rC + 1) & 0xFF);
                rF = sz53_table[rB]
                   | ((val & 0x80) ? NF : 0)
                   | (k > 255 ? (HF|CF) : 0)
                   | (parity_table[(u8)((k & 7) ^ rB)] ? PF : 0);
                if (rB != 0) { rPC -= 2; NEXT(21); }
                NEXT(16);
            }

            /* OTIR */
            case 0xB3: {
                u8 val = rd(HL_VAL);
                rB--;
                io_wr(BC_VAL, val);
                u16 hl = HL_VAL + 1; SET_HL(hl);
                rWZ = BC_VAL + 1;
                u16 k = (u16)val + (u16)rL;
                rF = sz53_table[rB]
                   | ((val & 0x80) ? NF : 0)
                   | (k > 255 ? (HF|CF) : 0)
                   | (parity_table[(u8)((k & 7) ^ rB)] ? PF : 0);
                if (rB != 0) { rPC -= 2; NEXT(21); }
                NEXT(16);
            }

            /* LDDR */
            case 0xB8: {
                u8 val = rd(HL_VAL);
                u16 de = DE_VAL; wr(de, val);
                de--; SET_DE(de);
                u16 hl = HL_VAL - 1; SET_HL(hl);
                u16 bc = BC_VAL - 1; SET_BC(bc);
                u8 n = val + rA;
                rF = (rF & (SF|ZF|CF)) | (bc != 0 ? PF : 0) | (n & F3) | ((n & 0x02) ? F5 : 0);
                if (bc != 0) { rPC -= 2; rWZ = rPC + 1; NEXT(21); }
                NEXT(16);
            }

            /* CPDR */
            case 0xB9: {
                u8 val = rd(HL_VAL);
                u8 res = rA - val;
                u16 hl = HL_VAL - 1; SET_HL(hl);
                u16 bc = BC_VAL - 1; SET_BC(bc);
                u8 nhf = (rA ^ val ^ res) & HF;
                u8 n = res - (nhf ? 1 : 0);
                rF = (rF & CF) | NF | (bc != 0 ? PF : 0) | sz53_table[res & 0xFF] | nhf;
                rF = (rF & ~(F3|F5)) | (n & F3) | ((n & 0x02) ? F5 : 0);
                if (bc != 0 && !(rF & ZF)) { rPC -= 2; rWZ = rPC + 1; NEXT(21); }
                rWZ--;
                NEXT(16);
            }

            /* INDR */
            case 0xBA: {
                rWZ = BC_VAL - 1;
                u8 val = io_rd(BC_VAL);
                wr(HL_VAL, val);
                u16 hl = HL_VAL - 1; SET_HL(hl);
                rB--;
                u16 k = (u16)val + (u16)((rC - 1) & 0xFF);
                rF = sz53_table[rB]
                   | ((val & 0x80) ? NF : 0)
                   | (k > 255 ? (HF|CF) : 0)
                   | (parity_table[(u8)((k & 7) ^ rB)] ? PF : 0);
                if (rB != 0) { rPC -= 2; NEXT(21); }
                NEXT(16);
            }

            /* OTDR */
            case 0xBB: {
                u8 val = rd(HL_VAL);
                rB--;
                io_wr(BC_VAL, val);
                u16 hl = HL_VAL - 1; SET_HL(hl);
                rWZ = BC_VAL - 1;
                u16 k = (u16)val + (u16)rL;
                rF = sz53_table[rB]
                   | ((val & 0x80) ? NF : 0)
                   | (k > 255 ? (HF|CF) : 0)
                   | (parity_table[(u8)((k & 7) ^ rB)] ? PF : 0);
                if (rB != 0) { rPC -= 2; NEXT(21); }
                NEXT(16);
            }

            /* Nedokumentovane/neplatne ED instrukce - NOP (8T) */
            default: NEXT(8);
        }
    }

    /* ========== DD/FD prefix makro ========== */
    /* Generujeme DD a FD handlery pomoci makra - IX vs IY */

#define DDFD_PREFIX(IDX_W, IDX_H, IDX_L) do { \
    INC_R(); \
    u8 ddop = FETCH(); \
    switch (ddop) { \
        /* ADD IX/IY, rr */ \
        case 0x09: rWZ = (IDX_W) + 1; { \
            u32 _r32 = (IDX_W) + BC_VAL; \
            u8 _rh = (u8)(_r32 >> 8); \
            rF = (rF & (SF|ZF|PF)) | (_rh & (F3|F5)) | ((_r32 >> 16) & CF) \
               | ((((IDX_W) ^ BC_VAL ^ _r32) >> 8) & HF); \
            (IDX_W) = (u16)_r32; } NEXT(15); \
        case 0x19: rWZ = (IDX_W) + 1; { \
            u32 _r32 = (IDX_W) + DE_VAL; \
            u8 _rh = (u8)(_r32 >> 8); \
            rF = (rF & (SF|ZF|PF)) | (_rh & (F3|F5)) | ((_r32 >> 16) & CF) \
               | ((((IDX_W) ^ DE_VAL ^ _r32) >> 8) & HF); \
            (IDX_W) = (u16)_r32; } NEXT(15); \
        case 0x29: rWZ = (IDX_W) + 1; { \
            u32 _r32 = (IDX_W) + (IDX_W); \
            u8 _rh = (u8)(_r32 >> 8); \
            rF = (rF & (SF|ZF|PF)) | (_rh & (F3|F5)) | ((_r32 >> 16) & CF) \
               | ((((IDX_W) ^ (IDX_W) ^ _r32) >> 8) & HF); \
            (IDX_W) = (u16)_r32; } NEXT(15); \
        case 0x39: rWZ = (IDX_W) + 1; { \
            u32 _r32 = (IDX_W) + rSP; \
            u8 _rh = (u8)(_r32 >> 8); \
            rF = (rF & (SF|ZF|PF)) | (_rh & (F3|F5)) | ((_r32 >> 16) & CF) \
               | ((((IDX_W) ^ rSP ^ _r32) >> 8) & HF); \
            (IDX_W) = (u16)_r32; } NEXT(15); \
        /* LD IX/IY, nn */ \
        case 0x21: { u8 _lo = FETCH(); u8 _hi = FETCH(); (IDX_W) = (u16)(_lo | (_hi << 8)); NEXT(14); } \
        /* LD (nn), IX/IY */ \
        case 0x22: { u16 _a = FETCH16(); wr16(_a, (IDX_W)); rWZ = _a + 1; NEXT(20); } \
        /* INC IX/IY */ \
        case 0x23: (IDX_W)++; NEXT(10); \
        /* INC IXH/IYH */ \
        case 0x24: (IDX_H) = INC8((IDX_H)); NEXT(8); \
        /* DEC IXH/IYH */ \
        case 0x25: (IDX_H) = DEC8((IDX_H)); NEXT(8); \
        /* LD IXH/IYH, n */ \
        case 0x26: (IDX_H) = FETCH(); NEXT(11); \
        /* LD IX/IY, (nn) */ \
        case 0x2A: { u16 _a = FETCH16(); (IDX_W) = rd16(_a); rWZ = _a + 1; NEXT(20); } \
        /* DEC IX/IY */ \
        case 0x2B: (IDX_W)--; NEXT(10); \
        /* INC IXL/IYL */ \
        case 0x2C: (IDX_L) = INC8((IDX_L)); NEXT(8); \
        /* DEC IXL/IYL */ \
        case 0x2D: (IDX_L) = DEC8((IDX_L)); NEXT(8); \
        /* LD IXL/IYL, n */ \
        case 0x2E: (IDX_L) = FETCH(); NEXT(11); \
        /* INC (IX/IY+d) */ \
        case 0x34: { s8 _d = (s8)FETCH(); u16 _a = (IDX_W) + _d; rWZ = _a; u8 _v = rd(_a); wr(_a, INC8(_v)); NEXT(23); } \
        /* DEC (IX/IY+d) */ \
        case 0x35: { s8 _d = (s8)FETCH(); u16 _a = (IDX_W) + _d; rWZ = _a; u8 _v = rd(_a); wr(_a, DEC8(_v)); NEXT(23); } \
        /* LD (IX/IY+d), n */ \
        case 0x36: { s8 _d = (s8)FETCH(); u8 _n = FETCH(); rWZ = (IDX_W) + _d; wr(rWZ, _n); NEXT(19); } \
        /* LD r, IXH/IYH */ \
        case 0x44: rB = (IDX_H); NEXT(8); \
        case 0x4C: rC = (IDX_H); NEXT(8); \
        case 0x54: rD = (IDX_H); NEXT(8); \
        case 0x5C: rE = (IDX_H); NEXT(8); \
        case 0x7C: rA = (IDX_H); NEXT(8); \
        /* LD r, IXL/IYL */ \
        case 0x45: rB = (IDX_L); NEXT(8); \
        case 0x4D: rC = (IDX_L); NEXT(8); \
        case 0x55: rD = (IDX_L); NEXT(8); \
        case 0x5D: rE = (IDX_L); NEXT(8); \
        case 0x7D: rA = (IDX_L); NEXT(8); \
        /* LD IXH/IYH, r */ \
        case 0x60: (IDX_H) = rB; NEXT(8); \
        case 0x61: (IDX_H) = rC; NEXT(8); \
        case 0x62: (IDX_H) = rD; NEXT(8); \
        case 0x63: (IDX_H) = rE; NEXT(8); \
        case 0x64: NEXT(8); /* LD IXH, IXH */ \
        case 0x65: (IDX_H) = (IDX_L); NEXT(8); \
        case 0x67: (IDX_H) = rA; NEXT(8); \
        /* LD IXL/IYL, r */ \
        case 0x68: (IDX_L) = rB; NEXT(8); \
        case 0x69: (IDX_L) = rC; NEXT(8); \
        case 0x6A: (IDX_L) = rD; NEXT(8); \
        case 0x6B: (IDX_L) = rE; NEXT(8); \
        case 0x6C: (IDX_L) = (IDX_H); NEXT(8); \
        case 0x6D: NEXT(8); /* LD IXL, IXL */ \
        case 0x6F: (IDX_L) = rA; NEXT(8); \
        /* LD r, (IX/IY+d) */ \
        case 0x46: { s8 _d=(s8)FETCH(); rWZ=(IDX_W)+_d; rB=rd(rWZ); NEXT(19); } \
        case 0x4E: { s8 _d=(s8)FETCH(); rWZ=(IDX_W)+_d; rC=rd(rWZ); NEXT(19); } \
        case 0x56: { s8 _d=(s8)FETCH(); rWZ=(IDX_W)+_d; rD=rd(rWZ); NEXT(19); } \
        case 0x5E: { s8 _d=(s8)FETCH(); rWZ=(IDX_W)+_d; rE=rd(rWZ); NEXT(19); } \
        case 0x66: { s8 _d=(s8)FETCH(); rWZ=(IDX_W)+_d; rH=rd(rWZ); NEXT(19); } \
        case 0x6E: { s8 _d=(s8)FETCH(); rWZ=(IDX_W)+_d; rL=rd(rWZ); NEXT(19); } \
        case 0x7E: { s8 _d=(s8)FETCH(); rWZ=(IDX_W)+_d; rA=rd(rWZ); NEXT(19); } \
        /* LD (IX/IY+d), r */ \
        case 0x70: { s8 _d=(s8)FETCH(); rWZ=(IDX_W)+_d; wr(rWZ, rB); NEXT(19); } \
        case 0x71: { s8 _d=(s8)FETCH(); rWZ=(IDX_W)+_d; wr(rWZ, rC); NEXT(19); } \
        case 0x72: { s8 _d=(s8)FETCH(); rWZ=(IDX_W)+_d; wr(rWZ, rD); NEXT(19); } \
        case 0x73: { s8 _d=(s8)FETCH(); rWZ=(IDX_W)+_d; wr(rWZ, rE); NEXT(19); } \
        case 0x74: { s8 _d=(s8)FETCH(); rWZ=(IDX_W)+_d; wr(rWZ, rH); NEXT(19); } \
        case 0x75: { s8 _d=(s8)FETCH(); rWZ=(IDX_W)+_d; wr(rWZ, rL); NEXT(19); } \
        case 0x77: { s8 _d=(s8)FETCH(); rWZ=(IDX_W)+_d; wr(rWZ, rA); NEXT(19); } \
        /* Aritmetika s IXH/IYH, IXL/IYL */ \
        case 0x84: rA = ADD8(rA, (IDX_H)); NEXT(8); \
        case 0x85: rA = ADD8(rA, (IDX_L)); NEXT(8); \
        case 0x8C: rA = ADC8(rA, (IDX_H)); NEXT(8); \
        case 0x8D: rA = ADC8(rA, (IDX_L)); NEXT(8); \
        case 0x94: rA = SUB8(rA, (IDX_H)); NEXT(8); \
        case 0x95: rA = SUB8(rA, (IDX_L)); NEXT(8); \
        case 0x9C: rA = SBC8(rA, (IDX_H)); NEXT(8); \
        case 0x9D: rA = SBC8(rA, (IDX_L)); NEXT(8); \
        case 0xA4: AND8((IDX_H)); NEXT(8); \
        case 0xA5: AND8((IDX_L)); NEXT(8); \
        case 0xAC: XOR8((IDX_H)); NEXT(8); \
        case 0xAD: XOR8((IDX_L)); NEXT(8); \
        case 0xB4: OR8((IDX_H)); NEXT(8); \
        case 0xB5: OR8((IDX_L)); NEXT(8); \
        case 0xBC: CP8((IDX_H)); NEXT(8); \
        case 0xBD: CP8((IDX_L)); NEXT(8); \
        /* Aritmetika s (IX/IY+d) */ \
        case 0x86: { s8 _d=(s8)FETCH(); rWZ=(IDX_W)+_d; rA=ADD8(rA,rd(rWZ)); NEXT(19); } \
        case 0x8E: { s8 _d=(s8)FETCH(); rWZ=(IDX_W)+_d; rA=ADC8(rA,rd(rWZ)); NEXT(19); } \
        case 0x96: { s8 _d=(s8)FETCH(); rWZ=(IDX_W)+_d; rA=SUB8(rA,rd(rWZ)); NEXT(19); } \
        case 0x9E: { s8 _d=(s8)FETCH(); rWZ=(IDX_W)+_d; rA=SBC8(rA,rd(rWZ)); NEXT(19); } \
        case 0xA6: { s8 _d=(s8)FETCH(); rWZ=(IDX_W)+_d; AND8(rd(rWZ)); NEXT(19); } \
        case 0xAE: { s8 _d=(s8)FETCH(); rWZ=(IDX_W)+_d; XOR8(rd(rWZ)); NEXT(19); } \
        case 0xB6: { s8 _d=(s8)FETCH(); rWZ=(IDX_W)+_d; OR8(rd(rWZ)); NEXT(19); } \
        case 0xBE: { s8 _d=(s8)FETCH(); rWZ=(IDX_W)+_d; CP8(rd(rWZ)); NEXT(19); } \
        /* DD CB / FD CB prefix */ \
        case 0xCB: { \
            s8 _d = (s8)FETCH(); \
            u8 _cbop = FETCH(); \
            u16 _addr = (IDX_W) + _d; \
            rWZ = _addr; \
            u8 _val = rd(_addr); \
            int _reg = _cbop & 0x07; \
            int _bit_n = (_cbop >> 3) & 0x07; \
            int _group = (_cbop >> 6) & 0x03; \
            u8 _result = _val; \
            switch (_group) { \
                case 0: \
                    switch (_bit_n) { \
                        case 0: _result = RLC(_val); break; \
                        case 1: _result = RRC(_val); break; \
                        case 2: _result = RL(_val); break; \
                        case 3: _result = RR(_val); break; \
                        case 4: _result = SLA(_val); break; \
                        case 5: _result = SRA(_val); break; \
                        case 6: _result = SLL(_val); break; \
                        case 7: _result = SRL(_val); break; \
                    } \
                    break; \
                case 1: \
                    BIT_MEM(_bit_n, _val, (u8)(_addr >> 8)); \
                    NEXT(20); \
                case 2: _result = _val & ~(1 << _bit_n); break; \
                case 3: _result = _val | (1 << _bit_n); break; \
            } \
            wr(_addr, _result); \
            if (_reg != 6) { \
                switch (_reg) { \
                    case 0: rB = _result; break; \
                    case 1: rC = _result; break; \
                    case 2: rD = _result; break; \
                    case 3: rE = _result; break; \
                    case 4: rH = _result; break; \
                    case 5: rL = _result; break; \
                    case 7: rA = _result; break; \
                } \
            } \
            NEXT(23); \
        } \
        /* POP IX/IY */ \
        case 0xE1: (IDX_W) = POP(); NEXT(14); \
        /* EX (SP), IX/IY */ \
        case 0xE3: { u16 _t = rd16(rSP); wr16(rSP, (IDX_W)); (IDX_W) = _t; rWZ = _t; NEXT(23); } \
        /* PUSH IX/IY */ \
        case 0xE5: PUSH((IDX_W)); NEXT(15); \
        /* JP (IX/IY) */ \
        case 0xE9: rPC = (IDX_W); NEXT(8); \
        /* LD SP, IX/IY */ \
        case 0xF9: rSP = (IDX_W); NEXT(10); \
        /* Nerozpoznany opcode - provest jako normalni (prefix se ignoruje) */ \
        default: rPC--; NEXT(4); \
    } \
} while(0)

    /* DD prefix (IX) */
    op_DD: DDFD_PREFIX(cpu->ix.w, cpu->ix.h, cpu->ix.l);

    /* FD prefix (IY) */
    op_FD: DDFD_PREFIX(cpu->iy.w, cpu->iy.h, cpu->iy.l);

#if !USE_COMPUTED_GOTO
dispatch_switch:
    {
        INC_R();
        u8 op = FETCH();
        /* Fallback switch - same labels ale pres switch */
        /* (Pro non-GCC kompilatory - neni implementovano v teto verzi) */
        (void)op;
        goto done; /* Placeholder */
    }
#endif

done:
    /* Final sync cyklu co se nahromadily v fast path */
    if (cycles_executed > last_sync) {
        cpu->cycles += cycles_executed - last_sync;
        cpu->total_cycles += cycles_executed - last_sync;
    }
    /* Writeback lokalnich registru */
    WRITEBACK();

    /* Uklid maker */
#undef AF_VAL
#undef BC_VAL
#undef DE_VAL
#undef HL_VAL
#undef SET_AF
#undef SET_BC
#undef SET_DE
#undef SET_HL
#undef WZH_VAL
#undef WZL_VAL
#undef SET_WZH
#undef SET_WZL
#undef FETCH
#undef FETCH16
#undef INC_R
#undef PUSH
#undef POP
#undef WRITEBACK
#undef RELOAD
#undef ADD8
#undef ADC8
#undef SUB8
#undef SBC8
#undef AND8
#undef XOR8
#undef OR8
#undef CP8
#undef INC8
#undef DEC8
#undef ADD16
#undef ADC16
#undef SBC16
#undef RLC
#undef RRC
#undef RL
#undef RR
#undef SLA
#undef SRA
#undef SLL
#undef SRL
#undef BIT_OP
#undef BIT_MEM
#undef DISPATCH
#undef NEXT
#undef NEXT_SLOW
#undef DDFD_PREFIX

    return cycles_executed;
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

    /* NMI ma vyssi prioritu */
    if (cpu->nmi_pending) {
        cpu->nmi_pending = false;
        cpu->halted = false;
        cpu->iff2 = cpu->iff1;
        cpu->iff1 = 0;
        cpu->sp -= 2;
        wr16(cpu->sp, cpu->pc);
        cpu->pc = 0x0066;
        cpu->wz.w = cpu->pc;
        int_cycles = 11;
    }
    /* Maskovane preruseni - blokovano po EI */
    else if (cpu->iff1 && !ei_blocked) {
        bool int_active = cpu->int_pending;
        if (!int_active && int_line_fn) {
            int_active = int_line_fn();
        }
        if (!int_active) {
            cpu->int_pending = false;
            return 0;
        }
        if (int_line_fn && !int_line_fn()) {
            cpu->int_pending = false;
            return 0;
        }
        cpu->int_pending = false;
        cpu->halted = false;
        cpu->iff1 = 0;
        cpu->iff2 = 0;

        if (intack_fn) intack_fn();

        switch (cpu->im) {
            case 0:
                cpu->sp -= 2;
                wr16(cpu->sp, cpu->pc);
                cpu->pc = cpu->int_vector & 0x38;
                cpu->wz.w = cpu->pc;
                int_cycles = 13;
                break;
            case 1:
                cpu->sp -= 2;
                wr16(cpu->sp, cpu->pc);
                cpu->pc = 0x0038;
                cpu->wz.w = cpu->pc;
                int_cycles = 13;
                break;
            case 2:
                cpu->sp -= 2;
                wr16(cpu->sp, cpu->pc);
                {
                    u16 vec_addr = ((u16)cpu->i << 8) | (cpu->int_vector & 0xFE);
                    cpu->pc = rd16(vec_addr);
                }
                cpu->wz.w = cpu->pc;
                int_cycles = 19;
                break;
        }
    }

    return int_cycles;
}

/* ========== Verejne API ========== */

void z80_init(z80_t *cpu) {
    if (!tables_initialized) {
        init_tables();
        tables_initialized = true;
    }
    memset(cpu, 0, sizeof(z80_t));
    z80_reset(cpu);
}

void z80_reset(z80_t *cpu) {
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
}

void z80_set_mem_read(z80_read_fn fn)   { mem_read_fn = fn ? fn : default_read; }
void z80_set_mem_write(z80_write_fn fn)  { mem_write_fn = fn ? fn : default_write; }
void z80_set_mem_fetch(z80_read_fn fn)   { mem_fetch_fn = fn ? fn : default_read; }
void z80_set_io_read(z80_read_fn fn)     { io_read_fn = fn ? fn : default_read; }
void z80_set_io_write(z80_write_fn fn)   { io_write_fn = fn ? fn : default_write; }
void z80_set_intack(z80_intack_fn fn)    { intack_fn = fn; }
void z80_set_reti_fn(z80_reti_fn fn)     { reti_fn = fn; }
void z80_set_int_line(z80_int_line_fn fn) { int_line_fn = fn; }
void z80_set_ei_fn(z80_ei_fn fn) { ei_fn = fn; }

void z80_add_wait_states(int wait) {
    if (current_cpu) current_cpu->wait_cycles += wait;
}

void z80_set_post_step_fn(void (*fn)(void)) {
    post_step_fn = fn;
}

/**
 * @brief Provedeni jedne instrukce (wrapper nad z80_execute).
 *
 * @param cpu Ukazatel na CPU instanci.
 * @return Pocet T-stavu spotrebovanych instrukci.
 */
int z80_step(z80_t *cpu) {
    int before = (int)cpu->total_cycles;
    z80_execute(cpu, 1);
    return (int)cpu->total_cycles - before;
}

void z80_irq(z80_t *cpu, u8 vector) {
    cpu->int_pending = true;
    cpu->int_vector = vector;
}

void z80_nmi(z80_t *cpu) {
    cpu->nmi_pending = true;
}
