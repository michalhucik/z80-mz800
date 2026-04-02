/*
 * Copyright (c) 2026 Michal Hucik
 * SPDX-License-Identifier: MIT
 * https://github.com/michalhucik/z80-mz800
 */
/**
 * @file z80.h
 * @brief cpu-z80 v0.1 - Presny a rychly emulator procesoru Zilog Z-80A.
 *
 * Kompletni instrukcni sada vcetne nedokumentovanych instrukci.
 * Presne pocitani T-stavu, spravne chovani vsech flagu (vcetne F3/F5),
 * MEMPTR/WZ registr, prerusovaci rezimy IM0/1/2, NMI.
 *
 * Optimalizace:
 * - Computed goto dispatch (GCC/Clang)
 * - Lokalni registrova cache v z80_execute()
 * - Eliminace null-checku callbacku (default handlery)
 * - DAA lookup tabulka (2048 zaznamu)
 * - Inline prefix handlery
 *
 * @version 0.1
 */

#ifndef CPU_Z80_H
#define CPU_Z80_H

#include "utils/types.h"

/** @name Flagy Z80
 * @{ */
#define Z80_FLAG_C  0x01  /**< Carry */
#define Z80_FLAG_N  0x02  /**< Subtract */
#define Z80_FLAG_PV 0x04  /**< Parity/Overflow */
#define Z80_FLAG_3  0x08  /**< Nedokumentovany bit 3 */
#define Z80_FLAG_H  0x10  /**< Half Carry */
#define Z80_FLAG_5  0x20  /**< Nedokumentovany bit 5 */
#define Z80_FLAG_Z  0x40  /**< Zero */
#define Z80_FLAG_S  0x80  /**< Sign */
/** @} */

/**
 * @brief Par registru s 16bit/8bit pristupem (little-endian).
 *
 * Umoznuje pristup k registrovemu paru jako k 16bitove hodnote (w)
 * nebo ke dvema 8bitovym polovinam (h, l).
 *
 * @invariant Na little-endian platforme: l je na nizsi adrese, h na vyssi.
 */
typedef union {
    u16 w;         /**< 16bitovy pristup */
    struct {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        u8 l, h;   /**< 8bitovy pristup: nizky a vysoky bajt */
#else
        u8 h, l;
#endif
    };
} z80_pair_t;

/**
 * @brief Stav procesoru Z80.
 *
 * Obsahuje vsechny registry, prerusovaci system a citace cyklu.
 * IX, IY a alternativni sada zustavaji ve strukture (mene caste pristupy).
 * Hlavni registry (AF, BC, DE, HL, PC, SP, WZ, R) jsou v z80_execute()
 * cachovany do lokalnich promennych pro rychlejsi pristup.
 *
 * @invariant im je vzdy 0, 1 nebo 2.
 */
typedef struct {
    /* Hlavni registrova sada */
    z80_pair_t af, bc, de, hl;
    /* Alternativni registrova sada */
    z80_pair_t af2, bc2, de2, hl2;
    /* Indexove registry */
    z80_pair_t ix, iy;
    /* Interni registr MEMPTR/WZ - ovlivnuje F3/F5 u BIT n,(HL) aj. */
    z80_pair_t wz;
    /* Specialni registry */
    u16 sp;          /**< Stack Pointer */
    u16 pc;          /**< Program Counter */
    u8  i;           /**< Interrupt Vector */
    u8  r;           /**< Memory Refresh */
    /* Prerusovaci system */
    u8  iff1, iff2;  /**< Interrupt Flip-Flops */
    u8  im;          /**< Interrupt Mode (0, 1, 2) */
    /* Stavy */
    bool halted;      /**< CPU je v HALT stavu */
    bool int_pending;  /**< Cekajici preruseni */
    bool nmi_pending;  /**< Cekajici NMI */
    bool ei_delay;     /**< EI delay: po EI se preruseni odlozi o 1 instrukci */
    bool ld_a_ir;      /**< HW bug: INT po LD A,I/R resetuje PF na 0 */
    u8   int_vector;   /**< Vektor preruseni (pro IM2) */
    /* Pocitadlo cyklu */
    u32 cycles;        /**< Aktualni T-stavy ve frame */
    u32 total_cycles;  /**< Celkovy pocet T-stavu */
    int wait_cycles;   /**< Extra wait states vlozene I/O zarizenim */
} z80_t;

/** Callback pro cteni bajtu z adresy. */
typedef u8 (*z80_read_fn)(u16 addr);
/** Callback pro zapis bajtu na adresu. */
typedef void (*z80_write_fn)(u16 addr, u8 data);
/** Callback pro potvrzeni preruseni (INTACK). */
typedef void (*z80_intack_fn)(void);
/** Callback pro RETI instrukci - notifikace periferii (daisy chain). */
typedef void (*z80_reti_fn)(void);
/** Callback pro level-sensitive INT linku - vraci true pokud je INT aktivni. */
typedef bool (*z80_int_line_fn)(void);
/** Callback volany pri provedeni instrukce EI. */
typedef void (*z80_ei_fn)(void);

/**
 * @brief Inicializace CPU (RESET).
 *
 * Inicializuje lookup tabulky (pri prvnim volani) a resetuje CPU.
 *
 * @param cpu Ukazatel na CPU instanci.
 * @pre cpu != NULL.
 * @post Vsechny registry jsou v defaultnim stavu, callbacky nastaveny na default.
 */
void z80_init(z80_t *cpu);

/**
 * @brief Reset CPU do vychoziho stavu.
 *
 * @param cpu Ukazatel na CPU instanci.
 * @pre cpu != NULL, z80_init() bylo volano alespon jednou.
 */
void z80_reset(z80_t *cpu);

/** @name Nastaveni callbacku pro pamet a I/O
 * @{ */
void z80_set_mem_read(z80_read_fn fn);
void z80_set_mem_write(z80_write_fn fn);
void z80_set_mem_fetch(z80_read_fn fn);
void z80_set_io_read(z80_read_fn fn);
void z80_set_io_write(z80_write_fn fn);
void z80_set_intack(z80_intack_fn fn);
void z80_set_reti_fn(z80_reti_fn fn);
void z80_set_int_line(z80_int_line_fn fn);
void z80_set_ei_fn(z80_ei_fn fn);
/** @} */

/**
 * @brief Provedeni jedne instrukce.
 *
 * @param cpu Ukazatel na CPU instanci.
 * @return Pocet T-stavu spotrebovanych instrukci.
 */
int z80_step(z80_t *cpu);

/**
 * @brief Pridani wait states z I/O callbacku.
 *
 * Volano z memory/IO callbacku pro pridani extra cekacich stavu
 * (napr. PSG READY signal).
 *
 * @param wait Pocet extra T-stavu.
 */
void z80_add_wait_states(int wait);

/**
 * @brief Post-step callback - volano po kazde instrukci.
 *
 * Slouzi pro nepodmineny per-line WAIT v MZ-700 rezimu.
 *
 * @param fn Callback funkce, nebo NULL pro deaktivaci.
 */
void z80_set_post_step_fn(void (*fn)(void));

/**
 * @brief Provedeni instrukci po dobu daneho poctu T-stavu.
 *
 * Hlavni emulacni smycka s optimalizovanym dispatch.
 *
 * @param cpu Ukazatel na CPU instanci.
 * @param target_cycles Cilovy pocet T-stavu k provedeni.
 * @return Skutecny pocet provedenych T-stavu (>= target_cycles).
 */
int z80_execute(z80_t *cpu, int target_cycles);

/**
 * @brief Vyvolani maskovaneho preruseni (IRQ).
 *
 * @param cpu Ukazatel na CPU instanci.
 * @param vector Vektor preruseni (pouzito v IM0 a IM2).
 */
void z80_irq(z80_t *cpu, u8 vector);

/**
 * @brief Vyvolani nemaskovaneho preruseni (NMI).
 *
 * @param cpu Ukazatel na CPU instanci.
 */
void z80_nmi(z80_t *cpu);

/** Retezec verze cpu-z80 knihovny. */
#define CPU_Z80_VERSION "0.1"

#endif /* CPU_Z80_H */
