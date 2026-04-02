/*
 * Copyright (c) 2026 Michal Hucik
 * SPDX-License-Identifier: MIT
 * https://github.com/michalhucik/z80-mz800
 */
/**
 * @file z80_dasm_compat.c
 * @brief Kompatibilni obalka pro z80ex_dasm() funkci.
 * @version 0.1
 *
 * Poskytuje drop-in nahradu za puvodni z80ex_dasm() s identickou
 * signaturou a vystupnim formatem. Interne pouziva novy disassembler
 * (z80_dasm() + z80_dasm_to_str()).
 *
 * Mapovani z80ex flagu na format:
 * - Z80EX_DASM_WORDS_DEC (1): 16bitove hodnoty desitkov (4660 misto #1234)
 * - Z80EX_DASM_BYTES_DEC (2): 8bitove hodnoty desitkove (255 misto #FF)
 * - 0: vychozi hex format s prefixem '#' a velkymi pismeny
 *
 * Konvence T-stavu z80ex:
 * - t_states = zakladni pocet T-stavu
 * - t_states2 = T-stavy pri splneni podminky vetveni
 * - Pokud instrukce nema vetveni (t_states == t_states2), z80ex nastavi
 *   t_states2 = 0
 */

#include <stdio.h>
#include <string.h>
#include "z80_dasm.h"

/* ======================================================================
 * Interni typy a pomocne funkce
 * ====================================================================== */

/**
 * @brief Kontextova struktura pro adaptaci z80ex callbacku.
 *
 * Zapouzdruje puvodni z80ex callback a jeho user_data, aby bylo mozne
 * predat je do z80_dasm_read_fn rozhrani.
 *
 * @invariant original_cb != NULL
 */
typedef struct {
    z80ex_dasm_readbyte_cb original_cb;  /**< Puvodni z80ex callback. */
    void *original_user_data;            /**< Puvodni uzivatelska data. */
} z80ex_cb_wrapper_t;

/**
 * @brief Adapter callbacku z z80ex signatury na z80_dasm_read_fn.
 *
 * Prevadi volani z80_dasm_read_fn na z80ex_dasm_readbyte_cb.
 * Pouziva kontextovou strukturu z80ex_cb_wrapper_t predanou
 * pres user_data.
 *
 * @param[in] addr      Adresa bajtu ke cteni (0x0000-0xFFFF).
 * @param[in] user_data Ukazatel na z80ex_cb_wrapper_t. Nesmi byt NULL.
 * @return Hodnota bajtu na dane adrese.
 *
 * @pre user_data ukazuje na platnou z80ex_cb_wrapper_t.
 * @post Funkce nesmi modifikovat stav emulatoru.
 */
static u8 readbyte_adapter(u16 addr, void *user_data)
{
    z80ex_cb_wrapper_t *wrapper = (z80ex_cb_wrapper_t *)user_data;
    return wrapper->original_cb((Z80EX_WORD)addr, wrapper->original_user_data);
}

/**
 * @brief Formatuje 8bitovou hodnotu desitkove do bufferu.
 *
 * @param[out] buf      Vystupni buffer. Nesmi byt NULL.
 * @param[in]  buf_size Velikost bufferu v bajtech.
 * @param[in]  val      Hodnota k formatovani (0-255).
 * @return Pocet zapsanych znaku (bez null terminatoru).
 *
 * @pre buf != NULL
 * @pre buf_size >= 1
 */
static int format_byte_dec(char *buf, int buf_size, u8 val)
{
    return snprintf(buf, (size_t)buf_size, "%u", val);
}

/**
 * @brief Formatuje 16bitovou hodnotu desitkove do bufferu.
 *
 * @param[out] buf      Vystupni buffer. Nesmi byt NULL.
 * @param[in]  buf_size Velikost bufferu v bajtech.
 * @param[in]  val      Hodnota k formatovani (0-65535).
 * @return Pocet zapsanych znaku (bez null terminatoru).
 *
 * @pre buf != NULL
 * @pre buf_size >= 1
 */
static int format_word_dec(char *buf, int buf_size, u16 val)
{
    return snprintf(buf, (size_t)buf_size, "%u", val);
}

/**
 * @brief Nahradi hexadecimalni hodnoty desitkovymi ve vystupnim retezci.
 *
 * Prochazi buffer a nahradi hex hodnoty formatu "#XX" nebo "#XXXX"
 * jejich desitkovymi ekvivalenty podle zadanych flagu.
 *
 * @param[in,out] buf       Textovy buffer s instrukci. Nesmi byt NULL.
 * @param[in]     buf_size  Velikost bufferu v bajtech.
 * @param[in]     inst      Dekodovana instrukce (pro urceni typu operandu).
 * @param[in]     flags     Formatovaci flagy (Z80EX_DASM_WORDS_DEC, Z80EX_DASM_BYTES_DEC).
 *
 * @pre buf != NULL
 * @pre buf_size >= 1
 * @pre inst != NULL
 */
static void apply_decimal_flags(char *buf, int buf_size,
                                const z80_dasm_inst_t *inst,
                                unsigned flags)
{
    /*
     * Strategie: prochazime operandy instrukce a pokud je pritomen
     * flag pro desitkovy format, hledame odpovidajici hex hodnotu
     * v bufferu a nahradime ji desitkovou.
     */
    char tmp[256];
    int words_dec = (flags & Z80EX_DASM_WORDS_DEC) != 0;
    int bytes_dec = (flags & Z80EX_DASM_BYTES_DEC) != 0;

    if (!words_dec && !bytes_dec) return;

    const z80_operand_t *ops[2] = { &inst->op1, &inst->op2 };

    for (int i = 0; i < 2; i++) {
        char hex_str[16];
        char dec_str[16];
        int is_word = 0;
        int is_byte = 0;

        switch (ops[i]->type) {
        case Z80_OP_IMM16:
            /* 16bitova konstanta: napr. LD BC,#1234 */
            if (words_dec) {
                snprintf(hex_str, sizeof(hex_str), "#%04X", ops[i]->val.imm16);
                format_word_dec(dec_str, sizeof(dec_str), ops[i]->val.imm16);
                is_word = 1;
            }
            break;

        case Z80_OP_MEM_IMM16:
            /* Prima adresa pameti: napr. LD A,(#1234) */
            if (words_dec) {
                snprintf(hex_str, sizeof(hex_str), "#%04X", ops[i]->val.imm16);
                format_word_dec(dec_str, sizeof(dec_str), ops[i]->val.imm16);
                is_word = 1;
            }
            break;

        case Z80_OP_IMM8:
            /* 8bitova konstanta: napr. LD A,#FF */
            if (bytes_dec) {
                snprintf(hex_str, sizeof(hex_str), "#%02X", ops[i]->val.imm8);
                format_byte_dec(dec_str, sizeof(dec_str), ops[i]->val.imm8);
                is_byte = 1;
            }
            break;

        case Z80_OP_MEM_IMM8:
            /* I/O port s primou adresou: IN A,(#FE) */
            if (bytes_dec) {
                snprintf(hex_str, sizeof(hex_str), "#%02X", ops[i]->val.imm8);
                format_byte_dec(dec_str, sizeof(dec_str), ops[i]->val.imm8);
                is_byte = 1;
            }
            break;

        case Z80_OP_REL8:
            /*
             * Relativni skok - zobrazeny jako absolutni adresa (16bit).
             * Presto je to "slovo" z hlediska z80ex flagu.
             */
            if (words_dec) {
                u16 abs_addr = z80_rel_to_abs(inst->addr,
                                               ops[i]->val.displacement);
                snprintf(hex_str, sizeof(hex_str), "#%04X", abs_addr);
                format_word_dec(dec_str, sizeof(dec_str), abs_addr);
                is_word = 1;
            }
            break;

        default:
            break;
        }

        if (!is_word && !is_byte) continue;

        /* Nahrazeni hex retezce desitkovym v bufferu */
        char *pos = strstr(buf, hex_str);
        if (!pos) continue;

        int hex_len = (int)strlen(hex_str);
        int dec_len = (int)strlen(dec_str);
        int before = (int)(pos - buf);
        int after_start = before + hex_len;
        int remaining = (int)strlen(buf + after_start);
        int total = before + dec_len + remaining;

        if (total < (int)sizeof(tmp) && total < buf_size) {
            memcpy(tmp, buf, (size_t)before);
            memcpy(tmp + before, dec_str, (size_t)dec_len);
            memcpy(tmp + before + dec_len, buf + after_start,
                   (size_t)remaining + 1);
            memcpy(buf, tmp, (size_t)total + 1);
        }
    }
}

/* ======================================================================
 * Verejne API: z80ex kompatibilni obalka
 * ====================================================================== */

int z80ex_dasm(char *output, int output_size, unsigned flags,
               int *t_states, int *t_states2,
               z80ex_dasm_readbyte_cb readbyte_cb,
               Z80EX_WORD addr, void *user_data)
{
    z80_dasm_inst_t inst;
    z80ex_cb_wrapper_t wrapper;
    z80_dasm_format_t fmt;
    int len;

    /* Pripravime adapter callbacku */
    wrapper.original_cb = readbyte_cb;
    wrapper.original_user_data = user_data;

    /* Dekodujeme instrukci */
    len = z80_dasm(&inst, readbyte_adapter, &wrapper, addr);

    /* Nastavime format: z80ex pouziva '#' prefix, velka pismena */
    z80_dasm_format_default(&fmt);
    /* Vychozi format je jiz Z80_HEX_HASH, uppercase=1, rel_as_absolute=1 */

    /* Formatujeme do textu */
    z80_dasm_to_str(output, output_size, &inst, &fmt);

    /* Aplikujeme desitkove flagy pokud jsou pozadovany */
    if (flags & (Z80EX_DASM_WORDS_DEC | Z80EX_DASM_BYTES_DEC)) {
        apply_decimal_flags(output, output_size, &inst, flags);
    }

    /* Nastavime T-stavy podle z80ex konvence:
     * - t_states = zakladni pocet T-stavu
     * - t_states2 = T-stavy pri splneni podminky vetveni
     * - Pokud t_states == t_states2, z80ex nastavuje t_states2 = 0 */
    *t_states = inst.t_states;

    if (inst.t_states2 != 0 && inst.t_states2 != inst.t_states) {
        *t_states2 = inst.t_states2;
    } else {
        *t_states2 = 0;
    }

    return len;
}
