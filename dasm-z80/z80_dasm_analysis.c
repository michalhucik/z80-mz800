/*
 * Copyright (c) 2026 Michal Hucik
 * SPDX-License-Identifier: MIT
 * https://github.com/michalhucik/z80-mz800
 */
/**
 * @file z80_dasm_analysis.c
 * @brief Analyticky modul Z80 disassembleru - analyza toku rizeni a registru.
 * @version 0.1
 *
 * Implementuje funkce pro analyzu dekodovanych instrukci:
 * - Zjisteni cilove adresy skoku/volani (z80_dasm_target_addr)
 * - Vyhodnoceni podminek vetveni (z80_dasm_branch_taken)
 * - Pristup k mapam ctenych/zapisovanych registru a ovlivnenych flagu
 *
 * Vsechny funkce pracuji nad jiz dekodovanou instrukci (z80_dasm_inst_t)
 * a neprovadeji zadne cteni z pameti.
 */

#include "z80_dasm_internal.h"

/* ======================================================================
 * Pomocne interni funkce
 * ====================================================================== */

/**
 * @brief Najde operand daneho typu v instrukci.
 *
 * Prohledava op1 a op2 instrukce a vraci ukazatel na prvni operand
 * odpovidajiciho typu.
 *
 * @param[in] inst Dekodovana instrukce. Nesmi byt NULL.
 * @param[in] type Hledany typ operandu.
 * @return Ukazatel na nalezeny operand, nebo NULL pokud takovy operand
 *         v instrukci neni.
 *
 * @pre inst != NULL
 */
static const z80_operand_t *find_operand(const z80_dasm_inst_t *inst,
                                         z80_operand_type_t type)
{
    if (inst->op1.type == type) {
        return &inst->op1;
    }
    if (inst->op2.type == type) {
        return &inst->op2;
    }
    return NULL;
}

/**
 * @brief Vyhodnoti podminku vetveni podle hodnoty registru F.
 *
 * Kontroluje konkretni flag odpovidajici dane podmince a vraci,
 * zda je podminka splnena.
 *
 * @param[in] condition Podminka vetveni (z80_condition_t).
 * @param[in] flags     Aktualni hodnota registru F.
 * @return 1 pokud je podminka splnena, 0 pokud ne.
 */
static int eval_condition(u8 condition, u8 flags)
{
    switch ((z80_condition_t)condition) {
    case Z80_CC_NZ: return !(flags & Z80_FLAG_Z);
    case Z80_CC_Z:  return !!(flags & Z80_FLAG_Z);
    case Z80_CC_NC: return !(flags & Z80_FLAG_C);
    case Z80_CC_C:  return !!(flags & Z80_FLAG_C);
    case Z80_CC_PO: return !(flags & Z80_FLAG_PV);
    case Z80_CC_PE: return !!(flags & Z80_FLAG_PV);
    case Z80_CC_P:  return !(flags & Z80_FLAG_S);
    case Z80_CC_M:  return !!(flags & Z80_FLAG_S);
    default:        return 0;
    }
}

/* ======================================================================
 * Verejne funkce - analyza toku rizeni
 * ====================================================================== */

/**
 * @brief Zjisti cilovou adresu instrukce skoku nebo volani.
 *
 * Pro instrukce menici tok rizeni (JP, JR, CALL, RST, DJNZ)
 * vraci cilovou adresu. Pro neprimi skoky (JP (HL), JP (IX), JP (IY)),
 * navraty (RET, RETI, RETN) a bezne instrukce vraci (u16)-1.
 *
 * @param[in] inst Dekodovana instrukce. Nesmi byt NULL.
 * @return Cilova adresa, nebo (u16)-1 pokud cil neni staticky znamy.
 *
 * @pre inst != NULL
 */
u16 z80_dasm_target_addr(const z80_dasm_inst_t *inst)
{
    const z80_operand_t *op;

    switch (inst->flow) {
    case Z80_FLOW_JUMP:
    case Z80_FLOW_JUMP_COND:
    case Z80_FLOW_CALL:
    case Z80_FLOW_CALL_COND:
        /* JP nn, JP cc,nn, CALL nn, CALL cc,nn - hledame 16bitovy immediate */
        op = find_operand(inst, Z80_OP_IMM16);
        if (op) {
            return op->val.imm16;
        }

        /* JR e, JR cc,e, DJNZ e - relativni offset */
        op = find_operand(inst, Z80_OP_REL8);
        if (op) {
            return z80_rel_to_abs(inst->addr, op->val.displacement);
        }

        /* Pokud nenalezeno, cil neni znamy */
        return (u16)-1;

    case Z80_FLOW_RST:
        /* RST p - restart vektor */
        op = find_operand(inst, Z80_OP_RST_VEC);
        if (op) {
            return (u16)op->val.imm8;
        }
        return (u16)-1;

    case Z80_FLOW_JUMP_INDIRECT:
        /* JP (HL), JP (IX), JP (IY) - cil zavisi na stavu registru */
        return (u16)-1;

    case Z80_FLOW_RET:
    case Z80_FLOW_RET_COND:
    case Z80_FLOW_RETI:
    case Z80_FLOW_RETN:
        /* Navratove instrukce - cil je na zasobniku */
        return (u16)-1;

    case Z80_FLOW_NORMAL:
    case Z80_FLOW_HALT:
    default:
        /* Bezne instrukce nemaji cilovou adresu */
        return (u16)-1;
    }
}

/**
 * @brief Vyhodnot podminku vetveni podle aktualniho stavu flagu.
 *
 * Pro podminecne instrukce (JP cc, JR cc, CALL cc, RET cc) vyhodnoti,
 * zda by se skok provedl pri danych hodnotach registru flagu.
 *
 * Pro bezpodminecne instrukce menici tok (JP, CALL, RST, RET, RETI, RETN)
 * vraci vzdy 1. Pro bezne instrukce (LD, ADD, ...) vraci vzdy 0.
 *
 * Specialni pripad DJNZ: podminka zavisi na registru B, ne na flagech.
 * Protoze nemame k dispozici hodnotu B, vracime 1.
 *
 * @param[in] inst  Dekodovana instrukce. Nesmi byt NULL.
 * @param[in] flags Aktualni hodnota registru F procesoru Z80.
 * @return 1 pokud by se skok provedl, 0 pokud ne.
 *
 * @pre inst != NULL
 */
int z80_dasm_branch_taken(const z80_dasm_inst_t *inst, u8 flags)
{
    const z80_operand_t *cond_op;

    switch (inst->flow) {
    case Z80_FLOW_JUMP:
    case Z80_FLOW_CALL:
    case Z80_FLOW_RST:
    case Z80_FLOW_JUMP_INDIRECT:
    case Z80_FLOW_RET:
    case Z80_FLOW_RETI:
    case Z80_FLOW_RETN:
        /* Bezpodminecne instrukce menici tok - vzdy se provedou */
        return 1;

    case Z80_FLOW_JUMP_COND:
    case Z80_FLOW_CALL_COND:
    case Z80_FLOW_RET_COND:
        /* Podminecne instrukce - hledame operand s podminkou */
        cond_op = find_operand(inst, Z80_OP_CONDITION);
        if (cond_op) {
            return eval_condition(cond_op->val.condition, flags);
        }
        /*
         * Zadny operand Z80_OP_CONDITION nenalezen.
         * Toto nastava u DJNZ, kde podminka zavisi na registru B,
         * ne na flagech. Bez znalosti B vracime 1 (optimisticka predpoved).
         */
        return 1;

    case Z80_FLOW_NORMAL:
    case Z80_FLOW_HALT:
    default:
        /* Instrukce nemeni tok rizeni */
        return 0;
    }
}

/* ======================================================================
 * Verejne funkce - pristup k registrum a flagum
 * ====================================================================== */

/**
 * @brief Vrati bitovou masku registru, ktere instrukce cte.
 *
 * Pohodlny obal pro pristup k inst->regs_read. Poskytuje explicitni
 * rozhrani pro zapouzdreni.
 *
 * @param[in] inst Dekodovana instrukce. Nesmi byt NULL.
 * @return Bitova maska ctenych registru (z80_reg_mask_t hodnoty).
 *
 * @pre inst != NULL
 */
u16 z80_dasm_regs_read(const z80_dasm_inst_t *inst)
{
    return inst->regs_read;
}

/**
 * @brief Vrati bitovou masku registru, ktere instrukce zapisuje.
 *
 * Pohodlny obal pro pristup k inst->regs_written. Poskytuje explicitni
 * rozhrani pro zapouzdreni.
 *
 * @param[in] inst Dekodovana instrukce. Nesmi byt NULL.
 * @return Bitova maska zapisovanych registru (z80_reg_mask_t hodnoty).
 *
 * @pre inst != NULL
 */
u16 z80_dasm_regs_written(const z80_dasm_inst_t *inst)
{
    return inst->regs_written;
}

/**
 * @brief Vrati bitovou masku flagu, ktere instrukce ovlivnuje.
 *
 * Pohodlny obal pro pristup k inst->flags_affected. Poskytuje explicitni
 * rozhrani pro zapouzdreni.
 *
 * @param[in] inst Dekodovana instrukce. Nesmi byt NULL.
 * @return Bitova maska ovlivnenych flagu (Z80_FLAG_* konstanty).
 *
 * @pre inst != NULL
 */
u8 z80_dasm_flags_affected(const z80_dasm_inst_t *inst)
{
    return inst->flags_affected;
}
