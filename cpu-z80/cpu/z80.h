/*
 * Copyright (c) 2026 Michal Hucik
 * SPDX-License-Identifier: MIT
 * https://github.com/michalhucik/z80-mz800
 */
/**
 * @file z80.h
 * @brief cpu-z80 v0.2 - Presny a rychly multi-instance Z80A emulator.
 *
 * Multi-instance API: kazda CPU instance nese vlastni callbacky a user_data.
 * Umoznuje provozovat vice nezavislych CPU instanci soucasne.
 *
 * Kompletni instrukcni sada vcetne nedokumentovanych instrukci.
 * Presne pocitani T-stavu, spravne chovani vsech flagu (vcetne F3/F5),
 * MEMPTR/WZ registr, prerusovaci rezimy IM0/1/2, NMI.
 *
 * Optimalizace:
 * - Lokalni cache callback pointeru v z80_execute() (nulovy overhead)
 * - Computed goto dispatch (GCC/Clang)
 * - Lokalni registrova cache v z80_execute()
 * - Eliminace null-checku callbacku (default handlery)
 * - DAA lookup tabulka (2048 zaznamu)
 * - Inline prefix handlery
 *
 * @version v0.2.1
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

/* Forward deklarace pro callback typy */
struct z80_s;

/** @name Typy callbacku
 *
 * Vsechny callbacky dostavaji ukazatel na CPU instanci a user_data.
 * @{ */

/**
 * @brief Callback pro cteni bajtu z pameti.
 * @param cpu Ukazatel na CPU instanci.
 * @param addr 16bitova adresa.
 * @param m1_state 1 = M1 fetch (cteni instrukce), 0 = normalni cteni.
 * @param user_data Uzivatelska data predana pri registraci callbacku.
 * @return Precteny bajt.
 */
typedef u8 (*z80_mread_cb)(struct z80_s *cpu, u16 addr, int m1_state, void *user_data);

/**
 * @brief Callback pro zapis bajtu do pameti.
 * @param cpu Ukazatel na CPU instanci.
 * @param addr 16bitova adresa.
 * @param value Zapisovany bajt.
 * @param user_data Uzivatelska data predana pri registraci callbacku.
 */
typedef void (*z80_mwrite_cb)(struct z80_s *cpu, u16 addr, u8 value, void *user_data);

/**
 * @brief Callback pro cteni z I/O portu.
 * @param cpu Ukazatel na CPU instanci.
 * @param port 16bitova adresa portu.
 * @param user_data Uzivatelska data predana pri registraci callbacku.
 * @return Precteny bajt.
 */
typedef u8 (*z80_pread_cb)(struct z80_s *cpu, u16 port, void *user_data);

/**
 * @brief Callback pro zapis na I/O port.
 * @param cpu Ukazatel na CPU instanci.
 * @param port 16bitova adresa portu.
 * @param value Zapisovany bajt.
 * @param user_data Uzivatelska data predana pri registraci callbacku.
 */
typedef void (*z80_pwrite_cb)(struct z80_s *cpu, u16 port, u8 value, void *user_data);

/**
 * @brief Callback pro cteni vektoru preruseni.
 * @param cpu Ukazatel na CPU instanci.
 * @param user_data Uzivatelska data predana pri registraci callbacku.
 * @return Vektor preruseni.
 */
typedef u8 (*z80_intread_cb)(struct z80_s *cpu, void *user_data);

/**
 * @brief Callback pro potvrzeni preruseni (INTACK signal).
 * @param cpu Ukazatel na CPU instanci.
 * @param user_data Uzivatelska data predana pri registraci callbacku.
 */
typedef void (*z80_intack_cb)(struct z80_s *cpu, void *user_data);

/**
 * @brief Callback pro RETI instrukci - notifikace periferii (daisy chain).
 * @param cpu Ukazatel na CPU instanci.
 * @param user_data Uzivatelska data predana pri registraci callbacku.
 */
typedef void (*z80_reti_cb)(struct z80_s *cpu, void *user_data);

/**
 * @brief Callback volany pri provedeni instrukce EI.
 * @param cpu Ukazatel na CPU instanci.
 * @param user_data Uzivatelska data predana pri registraci callbacku.
 */
typedef void (*z80_ei_cb)(struct z80_s *cpu, void *user_data);

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
 * @brief Stav procesoru Z80 s multi-instance callbacky.
 *
 * Obsahuje vsechny registry, prerusovaci system, citace cyklu
 * a callbacky s user_data pro kazdy typ operace.
 * IX, IY a alternativni sada zustavaji ve strukture (mene caste pristupy).
 * Hlavni registry (AF, BC, DE, HL, PC, SP, WZ, R) jsou v z80_execute()
 * cachovany do lokalnich promennych pro rychlejsi pristup.
 *
 * @invariant im je vzdy 0, 1 nebo 2.
 * @invariant mread_cb a mwrite_cb nesmejou byt NULL (nastavi se default).
 */
typedef struct z80_s {
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
    u8   q;            /**< Interni Q registr: F z posledni ALU operace (pro SCF/CCF F3/F5) */
    /* Pocitadlo cyklu */
    u32 cycles;        /**< Aktualni T-stavy ve frame */
    u32 total_cycles;  /**< Celkovy pocet T-stavu */
    int wait_cycles;   /**< Extra wait states vlozene I/O zarizenim */
    int op_tstate;     /**< T-stavy od zacatku aktualni instrukce (inkrementovan pri FETCH/RD/WR/IO) */

    /* Callbacky s user_data - pro multi-instance */
    z80_mread_cb  mread_cb;     /**< Callback pro cteni z pameti */
    void         *mread_data;   /**< User data pro mread_cb */
    z80_mwrite_cb mwrite_cb;    /**< Callback pro zapis do pameti */
    void         *mwrite_data;  /**< User data pro mwrite_cb */
    z80_pread_cb  pread_cb;     /**< Callback pro cteni z I/O portu */
    void         *pread_data;   /**< User data pro pread_cb */
    z80_pwrite_cb pwrite_cb;    /**< Callback pro zapis na I/O port */
    void         *pwrite_data;  /**< User data pro pwrite_cb */
    z80_intread_cb intread_cb;  /**< Callback pro cteni vektoru preruseni */
    void          *intread_data;/**< User data pro intread_cb */
    z80_intack_cb  intack_cb;   /**< Callback pro INTACK signal */
    void          *intack_data; /**< User data pro intack_cb */
    z80_reti_cb    reti_cb;     /**< Callback pro RETI notifikaci */
    void          *reti_data;   /**< User data pro reti_cb */
    z80_ei_cb      ei_cb;       /**< Callback volany pri EI instrukci */
    void          *ei_data;     /**< User data pro ei_cb */
    /** Post-step callback - volano po kazde instrukci (MZ-700 per-line WAIT). */
    void (*post_step_cb)(struct z80_s *cpu, void *data);
    void *post_step_data;       /**< User data pro post_step_cb */
} z80_t;

/**
 * @brief Enum pro pristup k registrum pres z80_get_reg/z80_set_reg.
 */
typedef enum {
    Z80_REG_AF,   /**< Par AF */
    Z80_REG_BC,   /**< Par BC */
    Z80_REG_DE,   /**< Par DE */
    Z80_REG_HL,   /**< Par HL */
    Z80_REG_AF2,  /**< Alternativni AF' */
    Z80_REG_BC2,  /**< Alternativni BC' */
    Z80_REG_DE2,  /**< Alternativni DE' */
    Z80_REG_HL2,  /**< Alternativni HL' */
    Z80_REG_IX,   /**< Indexovy registr IX */
    Z80_REG_IY,   /**< Indexovy registr IY */
    Z80_REG_SP,   /**< Stack Pointer */
    Z80_REG_PC,   /**< Program Counter */
    Z80_REG_WZ,   /**< Interni registr MEMPTR/WZ */
    Z80_REG_IR    /**< I (vysoky bajt), R (nizky bajt) */
} z80_reg_t;

/* ========== Zivotni cyklus ========== */

/**
 * @brief Vytvori novou CPU instanci s callbacky.
 *
 * Alokuje pamet pro z80_t, inicializuje lookup tabulky (pri prvnim volani),
 * nastavi callbacky a provede reset.
 *
 * @param mread Callback pro cteni z pameti (nesmi byt NULL).
 * @param mread_data User data pro mread.
 * @param mwrite Callback pro zapis do pameti (nesmi byt NULL).
 * @param mwrite_data User data pro mwrite.
 * @param pread Callback pro cteni z I/O portu (nesmi byt NULL).
 * @param pread_data User data pro pread.
 * @param pwrite Callback pro zapis na I/O port (nesmi byt NULL).
 * @param pwrite_data User data pro pwrite.
 * @param intread Callback pro cteni vektoru preruseni (muze byt NULL).
 * @param intread_data User data pro intread.
 * @return Ukazatel na novou CPU instanci, nebo NULL pri chybe alokace.
 * @post Vsechny registry jsou v defaultnim stavu (z80_reset).
 */
z80_t *z80_create(
    z80_mread_cb mread, void *mread_data,
    z80_mwrite_cb mwrite, void *mwrite_data,
    z80_pread_cb pread, void *pread_data,
    z80_pwrite_cb pwrite, void *pwrite_data,
    z80_intread_cb intread, void *intread_data
);

/**
 * @brief Znici CPU instanci a uvolni pamet.
 *
 * @param cpu Ukazatel na CPU instanci. Muze byt NULL (no-op).
 */
void z80_destroy(z80_t *cpu);

/**
 * @brief Reset CPU do vychoziho stavu.
 *
 * Zachovava callbacky, resetuje registry a prerusovaci system.
 *
 * @param cpu Ukazatel na CPU instanci.
 * @pre cpu != NULL.
 * @post Vsechny registry jsou v defaultnim stavu, callbacky zachovany.
 */
void z80_reset(z80_t *cpu);

/* ========== Emulace ========== */

/**
 * @brief Provedeni jedne instrukce.
 *
 * @param cpu Ukazatel na CPU instanci.
 * @return Pocet T-stavu spotrebovanych instrukci.
 * @pre cpu != NULL.
 */
int z80_step(z80_t *cpu);

/**
 * @brief Provedeni instrukci po dobu daneho poctu T-stavu.
 *
 * Hlavni emulacni smycka s optimalizovanym dispatch.
 *
 * @param cpu Ukazatel na CPU instanci.
 * @param target_cycles Cilovy pocet T-stavu k provedeni.
 * @return Skutecny pocet provedenych T-stavu (>= target_cycles).
 * @pre cpu != NULL.
 */
int z80_execute(z80_t *cpu, int target_cycles);

/* ========== Preruseni ========== */

/**
 * @brief Vyvolani maskovaneho preruseni s callbackem pro vektor.
 *
 * Vektor se cte pres intread_cb callback pri zpracovani.
 *
 * @param cpu Ukazatel na CPU instanci.
 * @pre cpu != NULL.
 */
void z80_int(z80_t *cpu);

/**
 * @brief Vyvolani maskovaneho preruseni s explicitnim vektorem.
 *
 * Pro zpetnou kompatibilitu - vektor je ulozen primo.
 *
 * @param cpu Ukazatel na CPU instanci.
 * @param vector Vektor preruseni (pouzito v IM0 a IM2).
 * @pre cpu != NULL.
 */
void z80_irq(z80_t *cpu, u8 vector);

/**
 * @brief Vyvolani nemaskovaneho preruseni (NMI).
 *
 * @param cpu Ukazatel na CPU instanci.
 * @pre cpu != NULL.
 */
void z80_nmi(z80_t *cpu);

/* ========== Pristup k registrum ========== */

/**
 * @brief Cteni 16bitove hodnoty registru.
 *
 * @param cpu Ukazatel na CPU instanci.
 * @param reg Registr k precteni.
 * @return 16bitova hodnota registru.
 * @pre cpu != NULL.
 */
u16 z80_get_reg(z80_t *cpu, z80_reg_t reg);

/**
 * @brief Zapis 16bitove hodnoty do registru.
 *
 * @param cpu Ukazatel na CPU instanci.
 * @param reg Registr k nastaveni.
 * @param value 16bitova hodnota.
 * @pre cpu != NULL.
 */
void z80_set_reg(z80_t *cpu, z80_reg_t reg, u16 value);

/**
 * @brief Zjisteni zda je CPU v HALT stavu.
 *
 * @param cpu Ukazatel na CPU instanci.
 * @return true pokud je CPU zastavena instrukci HALT.
 * @pre cpu != NULL.
 */
bool z80_is_halted(z80_t *cpu);

/* ========== Dynamicka zmena callbacku ========== */

/** @name Settery pro callbacky
 * Umoznuji dynamickou zmenu callbacku za behu.
 * @{ */
void z80_set_mread(z80_t *cpu, z80_mread_cb fn, void *data);
void z80_set_mwrite(z80_t *cpu, z80_mwrite_cb fn, void *data);
void z80_set_pread(z80_t *cpu, z80_pread_cb fn, void *data);
void z80_set_pwrite(z80_t *cpu, z80_pwrite_cb fn, void *data);
void z80_set_intread(z80_t *cpu, z80_intread_cb fn, void *data);
void z80_set_intack(z80_t *cpu, z80_intack_cb fn, void *data);
void z80_set_reti(z80_t *cpu, z80_reti_cb fn, void *data);
void z80_set_ei(z80_t *cpu, z80_ei_cb fn, void *data);
void z80_set_post_step(z80_t *cpu, void (*fn)(z80_t *cpu, void *data), void *data);
/** @} */

/**
 * @brief Pridani wait states z callbacku.
 *
 * Volano z memory/IO callbacku pro pridani extra cekacich stavu
 * (napr. PSG READY signal). Pricita i do op_tstate.
 *
 * @param cpu Ukazatel na CPU instanci.
 * @param wait Pocet extra T-stavu.
 * @pre cpu != NULL.
 */
void z80_add_wait_states(z80_t *cpu, int wait);

/** Retezec verze knihovny. */
#define CPU_Z80_VERSION "v0.2.1"

#endif /* CPU_Z80_H */
