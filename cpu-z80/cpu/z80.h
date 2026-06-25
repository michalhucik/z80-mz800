/*
 * Copyright (c) 2026 Michal Hucik
 * SPDX-License-Identifier: MIT
 * https://github.com/michalhucik/z80-mz800
 */
/**
 * @file z80.h
 * @brief cpu-z80 - Presny a rychly multi-instance Z80A emulator.
 *
 * Multi-instance API: kazda CPU instance nese vlastni callbacky a user_data.
 * Umoznuje provozovat vice nezavislych CPU instanci soucasne.
 *
 * Kompletni instrukcni sada vcetne nedokumentovanych instrukci.
 * Presne pocitani T-stavu, spravne chovani vsech flagu (vcetne F3/F5),
 * MEMPTR/WZ registr, prerusovaci rezimy IM0/1/2, NMI.
 *
 * Optimalizace:
 * - Computed goto dispatch (GCC/Clang)
 * - Dve varianty jadra: batch (z80_execute, registrova cache) a per-step
 *   (z80_step, registry primo v cpu->) ze sdileneho zdroje z80_execute.inc
 * - Volitelny RAM-access fast-path (CPU_Z80_RAM_FASTPATH, opt-in)
 * - Lokalni cache callback pointeru (nulovy overhead)
 * - Eliminace null-checku callbacku (default handlery)
 * - DAA lookup tabulka (2048 zaznamu)
 * - Inline prefix handlery
 *
 * @version v0.3.1
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

/**
 * @brief Callback volany pri provedeni instrukce DI.
 * @param cpu Ukazatel na CPU instanci.
 * @param user_data Uzivatelska data predana pri registraci callbacku.
 */
typedef void (*z80_di_cb)(struct z80_s *cpu, void *user_data);

/**
 * @brief Callback volany pri zmene IM (Interrupt Mode).
 * @param cpu Ukazatel na CPU instanci.
 * @param new_im Nova hodnota IM (0, 1 nebo 2).
 * @param user_data Uzivatelska data predana pri registraci callbacku.
 */
typedef void (*z80_im_cb)(struct z80_s *cpu, u8 new_im, void *user_data);

/**
 * @brief Callback volany pri prechodu CPU do HALT stavu.
 * @param cpu Ukazatel na CPU instanci.
 * @param user_data Uzivatelska data predana pri registraci callbacku.
 */
typedef void (*z80_halt_cb)(struct z80_s *cpu, void *user_data);

/**
 * @brief Callback volany pri NMI assertion (z80_nmi).
 * @param cpu Ukazatel na CPU instanci.
 * @param user_data Uzivatelska data predana pri registraci callbacku.
 */
typedef void (*z80_nmi_cb)(struct z80_s *cpu, void *user_data);

/**
 * @brief Duvod zmeny IFF1 / IFF2.
 *
 * Predavano jako parametr do z80_iff_change_cb. Konzument muze podle
 * duvodu rozlisit dispatch-driven clear (INT_ACK / NMI_ACK) od
 * explicit instrukcni zmeny (EI / DI / RETI / RETN).
 *
 * @note INT_ACK pokryva vsechny IM 0/1/2 - Z80 pri ack maskovaneho
 *       preruseni vzdy clearuje IFF1+IFF2 nezavisle na IM mode (Zilog
 *       Z80 manual; Sean Young). IM mode jen urcuje co se vykona po
 *       clearu, ne zda se IFF clearuji.
 */
typedef enum {
    Z80_IFF_REASON_RESET    = 0,  /**< CPU reset - IFF1=0, IFF2=0 */
    Z80_IFF_REASON_EI       = 1,  /**< EI instrukce - IFF1=1, IFF2=1 */
    Z80_IFF_REASON_DI       = 2,  /**< DI instrukce - IFF1=0, IFF2=0 */
    Z80_IFF_REASON_INT_ACK  = 3,  /**< Maskovane INT prijato (IM 0/1/2) - IFF1=0, IFF2=0 */
    Z80_IFF_REASON_NMI_ACK  = 4,  /**< NMI prijato - IFF1=0, IFF2 zachovano */
    Z80_IFF_REASON_RETI     = 5,  /**< RETI - IFF1 <- IFF2 */
    Z80_IFF_REASON_RETN     = 6   /**< RETN - IFF1 <- IFF2 */
} z80_iff_change_reason_t;

/**
 * @brief Callback volany pri zmene IFF1 nebo IFF2.
 *
 * Fire vzdy kdyz CPU modifikuje IFF1 nebo IFF2 - tj. pri EI, DI, RETI,
 * RETN, INT ack (IM 0/1/2), NMI ack a reset. Doplnuje existujici
 * ei_cb / di_cb / nmi_cb / reti_cb (= ty zustavaji bez zmeny), poskytuje
 * sjednoceny event source pro debuggery a observery prerusovaciho systemu.
 *
 * Hot path: NULL ptr check v callsite (= nulovy overhead pokud nenastaven).
 *
 * @param cpu Ukazatel na CPU instanci.
 * @param new_iff1 Nova hodnota IFF1 (0 nebo 1) - jiz zapsana v cpu->iff1.
 * @param new_iff2 Nova hodnota IFF2 (0 nebo 1) - jiz zapsana v cpu->iff2.
 * @param reason Duvod zmeny (z80_iff_change_reason_t cast na u8).
 * @param user_data Uzivatelska data predana pri registraci callbacku.
 */
typedef void (*z80_iff_change_cb)(struct z80_s *cpu,
                                  u8 new_iff1,
                                  u8 new_iff2,
                                  u8 reason,
                                  void *user_data);

/**
 * @brief Typ CPU control eventu pro z80_cpu_ctrl_event_cb.
 *
 * HALT_ENTER a HALT_EXIT pokryvaji prechody cpu->halted 0->1 / 1->0
 * (vstup do HALT instrukci, vystup pri prijeti IRQ/NMI). RST_00..RST_38
 * jsou jednotlive RST opcody (C7/CF/D7/DF/E7/EF/F7/FF).
 */
typedef enum {
    Z80_CPU_CTRL_HALT_ENTER = 0,  /**< Instrukce HALT vykonana, cpu->halted 0->1 */
    Z80_CPU_CTRL_HALT_EXIT  = 1,  /**< IRQ/NMI probudilo z HALT, cpu->halted 1->0 */
    Z80_CPU_CTRL_RST_00     = 2,  /**< Opcode 0xC7 (RST 00h) dispatch */
    Z80_CPU_CTRL_RST_08     = 3,  /**< Opcode 0xCF (RST 08h) dispatch */
    Z80_CPU_CTRL_RST_10     = 4,  /**< Opcode 0xD7 (RST 10h) dispatch */
    Z80_CPU_CTRL_RST_18     = 5,  /**< Opcode 0xDF (RST 18h) dispatch */
    Z80_CPU_CTRL_RST_20     = 6,  /**< Opcode 0xE7 (RST 20h) dispatch */
    Z80_CPU_CTRL_RST_28     = 7,  /**< Opcode 0xEF (RST 28h) dispatch */
    Z80_CPU_CTRL_RST_30     = 8,  /**< Opcode 0xF7 (RST 30h) dispatch */
    Z80_CPU_CTRL_RST_38     = 9   /**< Opcode 0xFF (RST 38h) dispatch */
} z80_cpu_ctrl_event_t;

/**
 * @brief Callback pro CPU control events (HALT entry/exit, RST nn).
 *
 * Fire pri:
 *   - HALT opcode (0x76) - entry, cpu->halted 0->1; volano paralelne
 *     s existujicim halt_cb (= ten zustava beze zmeny, BC).
 *   - HALT exit pri NMI ack i pri maskovanem INT ack v
 *     handle_interrupts_internal, cpu->halted 1->0. Fire jen pokud
 *     CPU bylo skutecne v HALT pred ack (= ne pri preruseni mezi
 *     beznymi instrukcemi).
 *   - RST opcode 0xC7..0xFF dispatch, fire pred push PC do stacku.
 *
 * PC predavany do callbacku:
 *   - HALT_ENTER:   adresa za HALT instrukci (= rPC + 1 po fetch).
 *   - HALT_EXIT:    cpu->pc v okamziku exit (= adresa za HALT,
 *                   kam se vrati po obsluze IRQ/NMI).
 *   - RST_xx:       adresa za RST opcode (= rPC po fetch, pred push
 *                   do stacku; konzument si muze nacist adresu RST
 *                   instrukce jako pc - 1 pokud potrebuje).
 *
 * Hot path: NULL ptr check v callsite (= nulovy overhead pokud
 * nenastaven). Nezavisly na existujicim halt_cb.
 *
 * @param cpu Ukazatel na CPU instanci.
 * @param event Typ control eventu (z80_cpu_ctrl_event_t cast na u8).
 * @param pc PC v okamziku eventu (viz vyse mapping per event type).
 * @param user_data Uzivatelska data predana pri registraci.
 */
typedef void (*z80_cpu_ctrl_event_cb)(struct z80_s *cpu,
                                      u8 event,
                                      u16 pc,
                                      void *user_data);

/**
 * @brief Callback volany pri prijeti CALL instrukce (taken vetve).
 *
 * Fire bezprostredne pred `PUSH(rPC); rPC = target;` v opcode handleru
 * - tedy v okamziku, kdy uz probehl FETCH16 cile, ale stack jeste
 * nema ulozenou navratovou adresu a PC ukazuje za CALL instrukci
 * (= shodne s tim, co se vzapeti pushne).
 *
 * Pokriva vsechny CALL opkody:
 *   - unconditional CALL nn (0xCD)
 *   - CALL cc, nn (0xC4 / 0xCC / 0xD4 / 0xDC / 0xE4 / 0xEC / 0xF4 / 0xFC)
 *
 * U podminenych variant fire jen pokud je podminka splnena (= CALL
 * skutecne prijat, PC bude prepsan cilem). Pri condition=false se
 * callback nevola.
 *
 * Vyhrazeno pro callstack subsystemy (shadow stack push). Zero overhead
 * v hot path pri NULL ptr (NULL check v callsite). Nezavisle na
 * cpu_ctrl_event_cb - RST/CALL jsou rozdilne udalosti.
 *
 * @param cpu Ukazatel na CPU instanci.
 * @param call_site Adresa CALL opkodu v pameti
 *                  (= return_addr - 3 pro 3-bytove CALL).
 * @param target Cilova adresa (= operand za CALL opkodem).
 * @param return_addr Adresa za CALL instrukci (= co bude pushnuto na
 *                    stack a kam se RET vrati; v okamziku fire shodne
 *                    s cpu->pc).
 * @param user_data Uzivatelska data predana pri registraci callbacku.
 */
typedef void (*z80_call_cb)(struct z80_s *cpu,
                            u16 call_site,
                            u16 target,
                            u16 return_addr,
                            void *user_data);

/**
 * @brief Callback volany pri prijeti RET instrukce (taken vetve).
 *
 * Fire bezprostredne pred `rPC = POP();` v opcode handleru - tedy
 * v okamziku, kdy stack jeste obsahuje navratovou adresu na vrcholu
 * a SP ukazuje na ni. Konzument muze nacist pop_target pres PEEK16
 * (provedeno v callsite) a porovnat sp_before_pop se shadow stack
 * zaznamem (= match podle SP, ne podle adresy).
 *
 * Pokriva:
 *   - unconditional RET (0xC9)
 *   - RET cc (0xC0 / 0xC8 / 0xD0 / 0xD8 / 0xE0 / 0xE8 / 0xF0 / 0xF8)
 *
 * U podminenych variant fire jen pokud je podminka splnena. Pri
 * condition=false se POP nedela a callback se nevola.
 *
 * NEPOKRYVA RETI (ED 4D) ani RETN (ED 45). Ty maji vlastni reti_cb
 * resp. iff_change_cb s reason=RETI/RETN. Callstack si interrupt
 * navraty radi pres iff_change_cb.
 *
 * Vyhrazeno pro callstack subsystemy (shadow stack pop / unwind).
 * Zero overhead v hot path pri NULL ptr (NULL check v callsite).
 *
 * @param cpu Ukazatel na CPU instanci.
 * @param pop_target Adresa, na kterou se CPU vrati (= PEEK16(sp) v
 *                   okamziku fire, vzapeti nahraje do PC).
 * @param sp_before_pop Hodnota SP pred POP (= ukazatel na pop_target
 *                      na stacku).
 * @param user_data Uzivatelska data predana pri registraci callbacku.
 */
typedef void (*z80_ret_cb)(struct z80_s *cpu,
                           u16 pop_target,
                           u16 sp_before_pop,
                           void *user_data);

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
 * @brief Pointery na lokalni registrovou cache uvnitr z80_execute().
 *
 * Behem behu z80_execute() jsou hlavni registry drzeny v lokalnich
 * promennych pro rychlost (kompilator je drzi v CPU registrech).
 * Aby callbacky volane z prubehu instrukce mohly transparentne menit
 * registry pres z80_set_reg(), z80_execute() vyplni tuto strukturu
 * adresami svych lokalnich promennych a zaregistruje ji v z80_t._active_cache.
 *
 * z80_set_reg() pak pri zapisu nejen aktualizuje pole z80_t (cpu->af.w,
 * cpu->bc.w, ...), ale soucasne pres tyto pointery i lokalni cache,
 * aby se zmena projevila ihned v probihajici instrukci.
 *
 * @invariant Pokud je _active_cache != NULL, ukazuje na strukturu
 *            zijici po dobu jedineho behu z80_execute(). Mimo z80_execute()
 *            musi byt _active_cache == NULL.
 */
typedef struct z80_local_cache_s {
    u8  *A, *F;       /**< AF par */
    u8  *B, *C;       /**< BC par */
    u8  *D, *E;       /**< DE par */
    u8  *H, *L;       /**< HL par */
    u16 *PC, *SP, *WZ;/**< PC, SP, WZ */
    u8  *R, *Q;       /**< R refresh, Q interni */
    u8  *savedF;      /**< Snapshot F pro Q logiku - sync s F po setREG */
} z80_local_cache_t;

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
 * @invariant _active_cache je NULL mimo z80_execute().
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
    z80_di_cb      di_cb;       /**< Callback volany pri DI instrukci */
    void          *di_data;     /**< User data pro di_cb */
    z80_im_cb      im_cb;       /**< Callback volany pri IM 0/1/2 zmene */
    void          *im_data;     /**< User data pro im_cb */
    z80_halt_cb    halt_cb;     /**< Callback volany pri HALT instrukci */
    void          *halt_data;   /**< User data pro halt_cb */
    z80_nmi_cb     nmi_cb;      /**< Callback volany pri NMI assertion */
    void          *nmi_data;    /**< User data pro nmi_cb */
    z80_iff_change_cb iff_change_cb; /**< Callback volany pri zmene IFF1/IFF2 */
    void          *iff_change_data;  /**< User data pro iff_change_cb */
    z80_cpu_ctrl_event_cb cpu_ctrl_event_cb; /**< Callback pro CPU control eventy (HALT, RST) */
    void          *cpu_ctrl_event_data;      /**< User data pro cpu_ctrl_event_cb */
    z80_call_cb    call_cb;     /**< Callback pro CALL dispatch (callstack push) */
    void          *call_data;   /**< User data pro call_cb */
    z80_ret_cb     ret_cb;      /**< Callback pro RET dispatch (callstack pop) */
    void          *ret_data;    /**< User data pro ret_cb */
    /** Post-step callback - volano po kazde instrukci (MZ-700 per-line WAIT). */
    void (*post_step_cb)(struct z80_s *cpu, void *data);
    void *post_step_data;       /**< User data pro post_step_cb */

    /**
     * Pointery na lokalni registrovou cache aktivni instance z80_execute().
     * Nenulove pouze behem behu z80_execute(); umoznuje z80_set_reg()
     * propsat zmeny i do lokalnich promennych probihajici instrukce.
     * Mimo z80_execute() musi byt NULL.
     */
    z80_local_cache_t *_active_cache;

#ifdef CPU_Z80_RAM_FASTPATH
    /**
     * @name RAM-access fast-path (opt-in)
     *
     * Page-table 16 zaznamu (1 / 4 KiB stranka, index addr>>12). Kazda stranka
     * je BUD primy ukazatel na bazi 4 KiB ciste RAM (pristup ptr[addr&0xFFF]),
     * NEBO NULL = "NEEDS_CALLBACK" (mapped IO/ROM/contended) - tehdy RD/WR makro
     * spadne na puvodni mread_cb/mwrite_cb (presny side-effect/timing zachovan).
     *
     * Tabulky plni a invaliduje integrujici vrstva (volanim z80_set_ram_fastpath)
     * pri kazde zmene mapovani (banking). Jadro je arch-independent - jen
     * konzumuje pointery, nezna mapovaci pravidla.
     *
     * @invariant Stranka je v ram_fp_read/write non-NULL pouze pokud cisty RAM
     *            pristup na ni je BIT-IDENTICKY s pruchodem pres memory_*_cb
     *            (vcetne vsech vedlejsich efektu - viz ram_fp_dbus_latch).
     * @{
     */
    u8 *ram_fp_read[16];   /**< Read page-table: NULL = callback, jinak baze 4 KiB banku. */
    u8 *ram_fp_write[16];  /**< Write page-table: NULL = callback, jinak baze 4 KiB banku. */
    /**
     * Ukazatel na volitelny "data bus latch" integrujici vrstvy. Cista RAM cesta
     * v memory_read_cb muze tento latch nastavovat na prectenou hodnotu -
     * fast-path read ho proto musi replikovat, jinak neni bit-identicky.
     * NULL = vrstva latch nepouziva (fast-path read ho pak neaktualizuje).
     */
    u8 *ram_fp_dbus_latch;
    /**
     * Gating: fast-path je aktivni jen kdyz true. Integrujici vrstva ho nastavi
     * false kdykoliv je nainstalovan logging callback (debugger/trace) - tam
     * maji read callbacky vedlejsi efekty, ktere fast-path neumi replikovat,
     * takze tam zustava plny callback.
     */
    bool ram_fp_enabled;
    /** @} */
#endif
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
 * @brief Provedeni jedne instrukce (per-step jadro).
 *
 * Pouziva per-step variantu jadra (registry primo v cpu->, bez prologu/epilogu
 * registrove cache) - urceno pro cycle-accurate hostitelske smycky volajici
 * po jedne instrukci. Semantika je bit-identicka s z80_execute(cpu, 1).
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
void z80_set_di(z80_t *cpu, z80_di_cb fn, void *data);
void z80_set_im_change(z80_t *cpu, z80_im_cb fn, void *data);
void z80_set_halt(z80_t *cpu, z80_halt_cb fn, void *data);
void z80_set_nmi_cb(z80_t *cpu, z80_nmi_cb fn, void *data);

#ifdef CPU_Z80_RAM_FASTPATH
/**
 * @brief Nastavi RAM-access fast-path page-table a gating (opt-in).
 *
 * Naplni ram_fp_read[]/ram_fp_write[] (16 zaznamu / 4 KiB stranka), ukazatel
 * na data-bus latch a gating flag. Volat z integrujici vrstvy pri KAZDE zmene
 * mapovani (banking) a pri zmene read/write callbacku (logging on/off). Jadro
 * pak v RD/WR makrech preskakuje mread_cb/mwrite_cb pro stranky s non-NULL
 * ukazatelem. Dostupne jen pri prekladu s -DCPU_Z80_RAM_FASTPATH.
 *
 * @param cpu Ukazatel na CPU instanci.
 * @param read_table Pole 16 ukazatelu (NULL = callback, jinak baze 4 KiB banku). Kopiruje se.
 * @param write_table Pole 16 ukazatelu (NULL = callback, jinak baze 4 KiB banku). Kopiruje se.
 * @param dbus_latch Ukazatel na data-bus latch, nebo NULL. Drzi se (ne kopie).
 * @param enabled true = fast-path aktivni; false = vzdy callback (baseline).
 * @pre cpu != NULL, read_table != NULL, write_table != NULL.
 * @post Tabulky zkopirovany do cpu->; pri enabled=false se obsah ignoruje v hot path.
 */
void z80_set_ram_fastpath(z80_t *cpu, u8 *const read_table[16],
                          u8 *const write_table[16],
                          u8 *dbus_latch, bool enabled);
#endif

void z80_set_iff_change(z80_t *cpu, z80_iff_change_cb fn, void *data);
void z80_set_cpu_ctrl_event(z80_t *cpu, z80_cpu_ctrl_event_cb fn, void *data);
void z80_set_call(z80_t *cpu, z80_call_cb fn, void *data);
void z80_set_ret(z80_t *cpu, z80_ret_cb fn, void *data);
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
#define CPU_Z80_VERSION "v0.3.1"

#endif /* CPU_Z80_H */
