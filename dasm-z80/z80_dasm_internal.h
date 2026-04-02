/*
 * Copyright (c) 2026 Michal Hucik
 * SPDX-License-Identifier: MIT
 * https://github.com/michalhucik/z80-mz800
 */
/**
 * @file z80_dasm_internal.h
 * @brief Interni deklarace sdilene mezi moduly disassembleru dasm-z80.
 * @version 0.1
 *
 * Tento soubor neni soucasti verejneho API. Obsahuje definici
 * opcode tabulek a pomocne makra pouzivane v implementacnich
 * souborech.
 */

#ifndef Z80_DASM_INTERNAL_H
#define Z80_DASM_INTERNAL_H

#include "z80_dasm.h"

/* ======================================================================
 * Opcode tabulka - zaznam
 * ====================================================================== */

/**
 * @brief Zaznam v opcode tabulce disassembleru.
 *
 * Kazda ze 7 tabulek (base, cb, ed, dd, fd, ddcb, fdcb) obsahuje
 * 256 zaznamu tohoto typu indexovanych hodnotou opcode bajtu.
 *
 * Pole format obsahuje mnemoniku s operandy a zastupnymi znaky:
 * - '@' = 16bitove slovo (2 bajty, little-endian)
 * - '#' = 8bitovy bajt
 * - '$' = znamenkovy displacement (IX/IY+d)
 * - '%%' = relativni offset (JR/DJNZ -> absolutni adresa)
 *
 * Pokud je format NULL, instrukce je neplatna. Pro DD/FD tabulky
 * NULL znamena fallback na zakladni (base) tabulku.
 */
typedef struct {
    const char *format;     /**< Format string ("LD A,#", "JP NZ,@", ...) nebo NULL. */
    u8 t_states;            /**< Pocet T-stavu (zakladni doba provedeni). */
    u8 t_states2;           /**< T-stavy pri vetveni (0 = neni vetveni). */
    u8 flags_affected;      /**< Bitova maska ovlivnenych flagu (Z80_FLAG_*). */
    u16 regs_read;          /**< Bitova maska ctenych registru (z80_reg_mask_t). */
    u16 regs_written;       /**< Bitova maska zapisovanych registru. */
    u8 flow_type;           /**< Typ toku rizeni (z80_flow_type_t). */
    u8 inst_class;          /**< Trida instrukce (z80_inst_class_t). */
} z80_dasm_opc_t;

/* ======================================================================
 * Opcode tabulky - externi deklarace
 * ====================================================================== */

/** Zakladni instrukce (bez prefixu), 256 zaznamu. */
extern const z80_dasm_opc_t z80_dasm_base[256];

/** CB prefix - bitove operace a rotace, 256 zaznamu. */
extern const z80_dasm_opc_t z80_dasm_cb[256];

/** ED prefix - rozsirene instrukce, 256 zaznamu. */
extern const z80_dasm_opc_t z80_dasm_ed[256];

/** DD prefix - IX operace, 256 zaznamu. NULL = fallback na base. */
extern const z80_dasm_opc_t z80_dasm_dd[256];

/** FD prefix - IY operace, 256 zaznamu. NULL = fallback na base. */
extern const z80_dasm_opc_t z80_dasm_fd[256];

/** DD CB prefix - indexovane bitove operace s IX, 256 zaznamu. */
extern const z80_dasm_opc_t z80_dasm_ddcb[256];

/** FD CB prefix - indexovane bitove operace s IY, 256 zaznamu. */
extern const z80_dasm_opc_t z80_dasm_fdcb[256];

/* ======================================================================
 * Zkratky pro pouziti v tabulkach
 * ====================================================================== */

/** @name Zkratky pro registrove masky */
/** @{ */
#define RA   Z80_REG_A
#define RF   Z80_REG_F
#define RB   Z80_REG_B
#define RC   Z80_REG_C
#define RD   Z80_REG_D
#define RE   Z80_REG_E
#define RH   Z80_REG_H
#define RL_  Z80_REG_L      /* RL_ kvuli kolizi s makrem RL v ai2-z80 */
#define RAF  Z80_REG_AF
#define RBC  Z80_REG_BC
#define RDE  Z80_REG_DE
#define RHL  Z80_REG_HL
#define RSP  Z80_REG_SP
#define RIXH Z80_REG_IXH
#define RIXL Z80_REG_IXL
#define RIYH Z80_REG_IYH
#define RIYL Z80_REG_IYL
#define RIX  Z80_REG_IX
#define RIY  Z80_REG_IY
#define RI   Z80_REG_I
#define RR_  Z80_REG_R
#define RWZ  Z80_REG_WZ
/** @} */

/** @name Zkratky pro flagove masky */
/** @{ */
#define FN    0x00  /**< Zadne flagy. */
#define FA    0xFF  /**< Vsechny flagy. */

/** Vsechny krome Carry (INC/DEC 8bit, BIT). */
#define FNC   (Z80_FLAG_S | Z80_FLAG_Z | Z80_FLAG_H | Z80_FLAG_PV | \
               Z80_FLAG_N | Z80_FLAG_3 | Z80_FLAG_5)

/** C, H, N, 3, 5 (rotace akumulatoru, ADD HL,rr, SCF, CCF). */
#define FCHN  (Z80_FLAG_C | Z80_FLAG_H | Z80_FLAG_N | Z80_FLAG_3 | Z80_FLAG_5)
/** @} */

/** @name Zkratky pro tok rizeni */
/** @{ */
#define TN   Z80_FLOW_NORMAL
#define TJ   Z80_FLOW_JUMP
#define TJC  Z80_FLOW_JUMP_COND
#define TC   Z80_FLOW_CALL
#define TCC  Z80_FLOW_CALL_COND
#define TR   Z80_FLOW_RET
#define TRC  Z80_FLOW_RET_COND
#define TRST Z80_FLOW_RST
#define TH   Z80_FLOW_HALT
#define TJI  Z80_FLOW_JUMP_INDIRECT
#define TRI  Z80_FLOW_RETI
#define TRN  Z80_FLOW_RETN
/** @} */

/** @name Zkratky pro tridu instrukce */
/** @{ */
#define CO   Z80_CLASS_OFFICIAL
#define CU   Z80_CLASS_UNDOCUMENTED
#define CI   Z80_CLASS_INVALID
/** @} */

/** Prazdny/neplatny zaznam v tabulce. */
#define INV  { NULL, 0, 0, FN, 0, 0, TN, CI }

/* ======================================================================
 * Interni funkce pro parsovani formatu
 * ====================================================================== */

/**
 * @brief Extrahuje zakladni mnemoniku z format stringu.
 *
 * Vraci ukazatel na staticky retezec obsahujici pouze mnemoniku
 * bez operandu (napr. "LD" z "LD A,#").
 *
 * @param[in] format Format string z opcode tabulky.
 * @return Ukazatel na staticky retezec s mnemonikou, nebo "???"
 *         pokud format je NULL.
 */
const char *z80_dasm_extract_mnemonic(const char *format);

/**
 * @brief Parsuje operandy z format stringu a bajtu instrukce.
 *
 * Naplni operandove struktury v inst na zaklade format stringu
 * a precteny bajtu z pameti.
 *
 * @param[in,out] inst       Struktura instrukce s vyplnenym addr a bytes.
 * @param[in]     format     Format string z opcode tabulky.
 * @param[in]     read_fn    Callback pro cteni pameti.
 * @param[in]     user_data  Uzivatelska data pro callback.
 * @param[in,out] addr       Aktualni adresa cteni (posouva se).
 * @param[in,out] bytes_read Pocet prectenych bajtu (zvysuje se).
 * @param[in]     have_disp  Zda jiz byl precten displacement (DDCB/FDCB).
 * @param[in]     disp_val   Hodnota displacementu pokud have_disp != 0.
 */
void z80_dasm_parse_operands(z80_dasm_inst_t *inst, const char *format,
                             z80_dasm_read_fn read_fn, void *user_data,
                             u16 *addr, int *bytes_read,
                             int have_disp, u8 disp_val);

#endif /* Z80_DASM_INTERNAL_H */
