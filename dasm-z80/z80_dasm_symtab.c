/*
 * Copyright (c) 2026 Michal Hucik
 * SPDX-License-Identifier: MIT
 * https://github.com/michalhucik/z80-mz800
 */
/**
 * @file z80_dasm_symtab.c
 * @brief Tabulka symbolu pro Z80 disassembler.
 * @version 0.1
 *
 * Implementuje mapovani 16bitovych adres na symbolicky nazvy.
 * Interne pouziva serazene pole s binarnim vyhledavanim.
 *
 * Typicke pouziti: pojmenovani ROM rutin, I/O portu a systemovych
 * adres MZ-800 pro citelnejsi vystup debuggeru.
 *
 * Slozitost operaci:
 * - lookup:  O(log n) - binarni vyhledavani
 * - add:     O(n) - udrzeni serazeneho pole
 * - remove:  O(n) - posun prvku
 * - clear:   O(n) - uvolneni retezcu
 *
 * Pocatecni kapacita je 64 zaznamu, pri prekroceni se zdvojnasobuje.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "z80_dasm.h"

/* ======================================================================
 * Interni datove typy
 * ====================================================================== */

/** Pocatecni kapacita tabulky symbolu (pocet zaznamu). */
#define Z80_SYMTAB_INITIAL_CAPACITY 64

/**
 * @brief Jeden zaznam v tabulce symbolu.
 *
 * Par {adresa, nazev}. Tabulka vlastni kopii retezce name
 * (alokovany pres strdup).
 *
 * @invariant name != NULL && name[0] != '\0'
 * @invariant addr je platna 16bitova adresa (0x0000-0xFFFF).
 */
typedef struct {
    u16  addr;  /**< Adresa symbolu. */
    char *name; /**< Nazev symbolu (vlastnena kopie, nutno uvolnit). */
} z80_sym_entry_t;

/**
 * @brief Tabulka symbolu - serazene pole par {adresa, nazev}.
 *
 * Zaznamy jsou serazeny vzestupne podle adresy.
 * Vyhledavani pouziva binarni puleni.
 *
 * @invariant count <= capacity
 * @invariant entries != NULL pokud capacity > 0
 * @invariant entries[0..count-1] jsou serazeny vzestupne podle addr.
 * @invariant Zadne dve polozky nemaji stejnou adresu.
 */
struct z80_symtab {
    z80_sym_entry_t *entries;  /**< Pole zaznamu serazene podle adresy. */
    int count;                 /**< Pocet pouzitych zaznamu. */
    int capacity;              /**< Alokovana kapacita pole entries. */
};

/* ======================================================================
 * Interni pomocne funkce
 * ====================================================================== */

/**
 * @brief Binarni vyhledavani adresy v serazenem poli.
 *
 * Hleda pozici zaznamu s danou adresou. Pokud zaznam neexistuje,
 * vraci pozici kam by mel byt vlozen (insert point).
 *
 * @param[in]  tab   Tabulka symbolu. Nesmi byt NULL.
 * @param[in]  addr  Adresa k vyhledani.
 * @param[out] found Nastavi na 1 pokud zaznam nalezen, jinak 0. Nesmi byt NULL.
 * @return Index nalezeneho zaznamu, nebo index pro vlozeni.
 *
 * @pre tab != NULL
 * @pre found != NULL
 * @post *found == 1 => tab->entries[navratova_hodnota].addr == addr
 * @post *found == 0 => navratova_hodnota je spravny insert point
 */
static int symtab_bsearch(const z80_symtab_t *tab, u16 addr, int *found)
{
    int lo = 0;
    int hi = tab->count - 1;

    *found = 0;

    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        u16 mid_addr = tab->entries[mid].addr;

        if (mid_addr == addr) {
            *found = 1;
            return mid;
        } else if (mid_addr < addr) {
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }

    /* lo je insert point - pozice kam vlozit novy zaznam */
    return lo;
}

/**
 * @brief Zvysi kapacitu tabulky zdvojnasobenim.
 *
 * @param[in,out] tab Tabulka symbolu. Nesmi byt NULL.
 * @return 0 pri uspechu, -1 pri selhani alokace.
 *
 * @pre tab != NULL
 * @post Pri uspechu: tab->capacity >= 2 * puvodni_capacity
 */
static int symtab_grow(z80_symtab_t *tab)
{
    int new_cap = tab->capacity * 2;
    z80_sym_entry_t *new_entries;

    new_entries = (z80_sym_entry_t *)realloc(tab->entries,
                                              (size_t)new_cap * sizeof(z80_sym_entry_t));
    if (!new_entries) return -1;

    tab->entries = new_entries;
    tab->capacity = new_cap;
    return 0;
}

/* ======================================================================
 * Verejne API: sprava tabulky
 * ====================================================================== */

z80_symtab_t *z80_symtab_create(void)
{
    z80_symtab_t *tab = (z80_symtab_t *)malloc(sizeof(z80_symtab_t));
    if (!tab) return NULL;

    tab->entries = (z80_sym_entry_t *)malloc(
        Z80_SYMTAB_INITIAL_CAPACITY * sizeof(z80_sym_entry_t));
    if (!tab->entries) {
        free(tab);
        return NULL;
    }

    tab->count = 0;
    tab->capacity = Z80_SYMTAB_INITIAL_CAPACITY;
    return tab;
}

void z80_symtab_destroy(z80_symtab_t *tab)
{
    if (!tab) return;

    /* Uvolneni vsech kopirovanych retezcu */
    for (int i = 0; i < tab->count; i++) {
        free(tab->entries[i].name);
    }

    free(tab->entries);
    free(tab);
}

int z80_symtab_add(z80_symtab_t *tab, u16 addr, const char *name)
{
    int found;
    int idx;
    char *name_copy;

    idx = symtab_bsearch(tab, addr, &found);

    if (found) {
        /* Symbol na teto adrese jiz existuje - nahradime nazev */
        name_copy = strdup(name);
        if (!name_copy) return -1;

        free(tab->entries[idx].name);
        tab->entries[idx].name = name_copy;
        return 0;
    }

    /* Novy symbol - overime kapacitu */
    if (tab->count >= tab->capacity) {
        if (symtab_grow(tab) != 0) return -1;
    }

    /* Udelame kopii nazvu */
    name_copy = strdup(name);
    if (!name_copy) return -1;

    /* Posuneme prvky za insert pointem o jednu pozici doprava */
    if (idx < tab->count) {
        memmove(&tab->entries[idx + 1],
                &tab->entries[idx],
                (size_t)(tab->count - idx) * sizeof(z80_sym_entry_t));
    }

    /* Vlozime novy zaznam */
    tab->entries[idx].addr = addr;
    tab->entries[idx].name = name_copy;
    tab->count++;

    return 0;
}

void z80_symtab_remove(z80_symtab_t *tab, u16 addr)
{
    int found;
    int idx = symtab_bsearch(tab, addr, &found);

    if (!found) return;

    /* Uvolnime retezec */
    free(tab->entries[idx].name);

    /* Posuneme zbyvajici prvky doleva */
    if (idx < tab->count - 1) {
        memmove(&tab->entries[idx],
                &tab->entries[idx + 1],
                (size_t)(tab->count - 1 - idx) * sizeof(z80_sym_entry_t));
    }

    tab->count--;
}

void z80_symtab_clear(z80_symtab_t *tab)
{
    for (int i = 0; i < tab->count; i++) {
        free(tab->entries[i].name);
    }
    tab->count = 0;
}

const char *z80_symtab_lookup(const z80_symtab_t *tab, u16 addr)
{
    int found;
    int idx = symtab_bsearch(tab, addr, &found);

    if (!found) return NULL;
    return tab->entries[idx].name;
}

int z80_symtab_count(const z80_symtab_t *tab)
{
    return tab->count;
}

/* ======================================================================
 * Rozliseni symbolu pro instrukci
 * ====================================================================== */

/**
 * @brief Zjisti, zda operand obsahuje cilovou adresu skoku/volani.
 *
 * Kontroluje operandy instrukce a vraci cilovou adresu pokud existuje.
 * Pro relativni skoky (JR, DJNZ) vypocita absolutni adresu.
 *
 * @param[in] inst Dekodovana instrukce. Nesmi byt NULL.
 * @param[out] addr Nalezena cilova adresa. Nesmi byt NULL.
 * @return 1 pokud cilova adresa nalezena, 0 jinak.
 *
 * @pre inst != NULL
 * @pre addr != NULL
 */
static int find_target_addr(const z80_dasm_inst_t *inst, u16 *addr)
{
    /* Instrukce s cilovymi adresami: JP, JR, CALL, RST, DJNZ */
    switch (inst->flow) {
    case Z80_FLOW_JUMP:
    case Z80_FLOW_JUMP_COND:
    case Z80_FLOW_CALL:
    case Z80_FLOW_CALL_COND:
    case Z80_FLOW_RST:
        break;
    default:
        return 0;
    }

    /* RST vektor */
    if (inst->flow == Z80_FLOW_RST) {
        if (inst->op1.type == Z80_OP_RST_VEC) {
            *addr = (u16)inst->op1.val.imm8;
            return 1;
        }
        return 0;
    }

    /* Absolutni cilova adresa - hledame IMM16 operand */
    const z80_operand_t *ops[2] = { &inst->op1, &inst->op2 };
    for (int i = 0; i < 2; i++) {
        if (ops[i]->type == Z80_OP_IMM16) {
            *addr = ops[i]->val.imm16;
            return 1;
        }
        if (ops[i]->type == Z80_OP_REL8) {
            /* Relativni skok - prepocet na absolutni adresu */
            *addr = z80_rel_to_abs(inst->addr, ops[i]->val.displacement);
            return 1;
        }
    }

    return 0;
}

/**
 * @brief Zjisti, zda instrukce pristupuje k prime adrese pameti.
 *
 * Hleda operand typu Z80_OP_MEM_IMM16, ktery oznacuje primy pristup
 * do pameti (napr. LD A,(nn), LD (nn),HL).
 *
 * @param[in]  inst Dekodovana instrukce. Nesmi byt NULL.
 * @param[out] addr Nalezena adresa pameti. Nesmi byt NULL.
 * @return 1 pokud adresa nalezena, 0 jinak.
 *
 * @pre inst != NULL
 * @pre addr != NULL
 */
static int find_mem_addr(const z80_dasm_inst_t *inst, u16 *addr)
{
    const z80_operand_t *ops[2] = { &inst->op1, &inst->op2 };
    for (int i = 0; i < 2; i++) {
        if (ops[i]->type == Z80_OP_MEM_IMM16) {
            *addr = ops[i]->val.imm16;
            return 1;
        }
    }
    return 0;
}

void z80_dasm_resolve_symbols(const z80_dasm_inst_t *inst,
                              const z80_symtab_t *symbols,
                              z80_dasm_symbols_t *out)
{
    u16 addr;

    out->target_sym = NULL;
    out->mem_sym = NULL;

    /* Cilova adresa skoku/volani */
    if (find_target_addr(inst, &addr)) {
        out->target_sym = z80_symtab_lookup(symbols, addr);
    }

    /* Adresa prime pameti */
    if (find_mem_addr(inst, &addr)) {
        out->mem_sym = z80_symtab_lookup(symbols, addr);
    }
}

/* ======================================================================
 * Formatovani se symboly
 * ====================================================================== */

int z80_dasm_to_str_sym(char *buf, int buf_size,
                        const z80_dasm_inst_t *inst,
                        const z80_dasm_format_t *fmt,
                        const z80_symtab_t *symbols)
{
    int len;

    /* Nejprve formatujeme standardne */
    len = z80_dasm_to_str(buf, buf_size, inst, fmt);
    if (len < 0) return len;

    /* Pokud nemame symboly, vracime standardni vystup */
    if (!symbols || z80_symtab_count(symbols) == 0) return len;

    /* Rozlisime symboly pro tuto instrukci */
    z80_dasm_symbols_t syms;
    z80_dasm_resolve_symbols(inst, symbols, &syms);

    /* Pokud zadny symbol nenalezen, nic nemenim */
    if (!syms.target_sym && !syms.mem_sym) return len;

    /*
     * Nahrazeni adres symboly ve vystupnim retezci.
     *
     * Strategie: hledame hexadecimalni cisla ve formatu odpovidajicim
     * aktualnimu hex_style a nahrazujeme je symbolickymi nazvy.
     *
     * Pouzivame docasny buffer pro bezpecne nahrazeni.
     */
    char tmp[256];
    int tmp_len;

    /* Ziskame aktualni format pro urceni prefixu/suffixu */
    z80_dasm_format_t def_fmt;
    if (!fmt) {
        z80_dasm_format_default(&def_fmt);
        fmt = &def_fmt;
    }

    /* Nahrazeni cilove adresy symbolem */
    if (syms.target_sym) {
        u16 target;
        if (find_target_addr(inst, &target)) {
            /* Sestavime hexadecimalni reprezentaci adresy */
            char addr_str[16];
            int addr_len = 0;

            switch (fmt->hex_style) {
            case Z80_HEX_HASH:
                if (fmt->uppercase)
                    addr_len = snprintf(addr_str, sizeof(addr_str), "#%04X", target);
                else
                    addr_len = snprintf(addr_str, sizeof(addr_str), "#%04x", target);
                break;
            case Z80_HEX_0X:
                if (fmt->uppercase)
                    addr_len = snprintf(addr_str, sizeof(addr_str), "0x%04X", target);
                else
                    addr_len = snprintf(addr_str, sizeof(addr_str), "0x%04x", target);
                break;
            case Z80_HEX_DOLLAR:
                if (fmt->uppercase)
                    addr_len = snprintf(addr_str, sizeof(addr_str), "$%04X", target);
                else
                    addr_len = snprintf(addr_str, sizeof(addr_str), "$%04x", target);
                break;
            case Z80_HEX_H_SUFFIX:
                if (fmt->uppercase)
                    addr_len = snprintf(addr_str, sizeof(addr_str), "%04XH", target);
                else
                    addr_len = snprintf(addr_str, sizeof(addr_str), "%04xh", target);
                break;
            }
            (void)addr_len;

            /* Hledame adresni retezec v buf a nahradime ho symbolem */
            char *pos = strstr(buf, addr_str);
            if (pos) {
                int sym_len = (int)strlen(syms.target_sym);
                int old_len = (int)strlen(addr_str);
                int before = (int)(pos - buf);
                int after_start = before + old_len;
                int remaining = (int)strlen(buf + after_start);

                tmp_len = before + sym_len + remaining;
                if (tmp_len < (int)sizeof(tmp)) {
                    memcpy(tmp, buf, (size_t)before);
                    memcpy(tmp + before, syms.target_sym, (size_t)sym_len);
                    memcpy(tmp + before + sym_len, buf + after_start,
                           (size_t)remaining + 1);

                    if (tmp_len < buf_size) {
                        memcpy(buf, tmp, (size_t)tmp_len + 1);
                        len = tmp_len;
                    }
                }
            }
        }
    }

    /* Nahrazeni pametove adresy symbolem */
    if (syms.mem_sym) {
        u16 mem;
        if (find_mem_addr(inst, &mem)) {
            /* Sestavime hexadecimalni reprezentaci adresy v zavorkach: (#XXXX) */
            char addr_str[16];
            int addr_len = 0;

            switch (fmt->hex_style) {
            case Z80_HEX_HASH:
                if (fmt->uppercase)
                    addr_len = snprintf(addr_str, sizeof(addr_str), "(#%04X)", mem);
                else
                    addr_len = snprintf(addr_str, sizeof(addr_str), "(#%04x)", mem);
                break;
            case Z80_HEX_0X:
                if (fmt->uppercase)
                    addr_len = snprintf(addr_str, sizeof(addr_str), "(0x%04X)", mem);
                else
                    addr_len = snprintf(addr_str, sizeof(addr_str), "(0x%04x)", mem);
                break;
            case Z80_HEX_DOLLAR:
                if (fmt->uppercase)
                    addr_len = snprintf(addr_str, sizeof(addr_str), "($%04X)", mem);
                else
                    addr_len = snprintf(addr_str, sizeof(addr_str), "($%04x)", mem);
                break;
            case Z80_HEX_H_SUFFIX:
                if (fmt->uppercase)
                    addr_len = snprintf(addr_str, sizeof(addr_str), "(%04XH)", mem);
                else
                    addr_len = snprintf(addr_str, sizeof(addr_str), "(%04xh)", mem);
                break;
            }
            (void)addr_len;

            /* Nahrazeni vcetne zavorek: (nn) -> (symbol) */
            char *pos = strstr(buf, addr_str);
            if (pos) {
                /* Nahradime vcetne zavorek: "(#XXXX)" -> "(symbol)" */
                char sym_with_parens[256];
                int sym_plen = snprintf(sym_with_parens, sizeof(sym_with_parens),
                                        "(%s)", syms.mem_sym);

                int old_len = (int)strlen(addr_str);
                int before = (int)(pos - buf);
                int after_start = before + old_len;
                int remaining = (int)strlen(buf + after_start);

                tmp_len = before + sym_plen + remaining;
                if (tmp_len < (int)sizeof(tmp)) {
                    memcpy(tmp, buf, (size_t)before);
                    memcpy(tmp + before, sym_with_parens, (size_t)sym_plen);
                    memcpy(tmp + before + sym_plen, buf + after_start,
                           (size_t)remaining + 1);

                    if (tmp_len < buf_size) {
                        memcpy(buf, tmp, (size_t)tmp_len + 1);
                        len = tmp_len;
                    }
                }
            }
        }
    }

    return len;
}
