/*
 * Copyright (c) 2026 Michal Hucik
 * SPDX-License-Identifier: MIT
 * https://github.com/michalhucik/z80-mz800
 */
/**
 * @file z80_dasm.c
 * @brief Jadro Z80 disassembleru - dekodovani instrukci a utility.
 * @version 0.1
 *
 * Implementuje hlavni disassemblovaci funkce z80_dasm() a z80_dasm_block(),
 * heuristicky z80_dasm_find_inst_start() a prevody relativnich adres.
 *
 * Dekoder cte bajty pres uzivatelsky callback, vyhleda odpovidajici
 * zaznam v opcode tabulce a vyplni strukturu z80_dasm_inst_t vcetne
 * rozlozenych operandu.
 */

#include <string.h>
#include "z80_dasm_internal.h"

/* ======================================================================
 * Interni: parsovani operandu z format stringu
 * ====================================================================== */

/**
 * @brief Rozpozna 8bitovy registr podle retezce.
 *
 * @param[in] s   Retezec k rozpoznani.
 * @param[out] len Delka rozpoznaneho tokenu.
 * @return Index registru (z80_reg8_t) nebo -1 pri neuspechu.
 */
static int parse_reg8(const char *s, int *len)
{
    /* Vicepismenne registry (musi byt pred jednopismenymi) */
    if (s[0] == 'I' && s[1] == 'X' && s[2] == 'H') { *len = 3; return Z80_R8_IXH; }
    if (s[0] == 'I' && s[1] == 'X' && s[2] == 'L') { *len = 3; return Z80_R8_IXL; }
    if (s[0] == 'I' && s[1] == 'Y' && s[2] == 'H') { *len = 3; return Z80_R8_IYH; }
    if (s[0] == 'I' && s[1] == 'Y' && s[2] == 'L') { *len = 3; return Z80_R8_IYL; }

    *len = 1;
    switch (s[0]) {
        case 'B': return Z80_R8_B;
        case 'C': return Z80_R8_C;
        case 'D': return Z80_R8_D;
        case 'E': return Z80_R8_E;
        case 'H': return Z80_R8_H;
        case 'L': return Z80_R8_L;
        case 'A': return Z80_R8_A;
        case 'F': return Z80_R8_F;
        case 'I': *len = 1; return Z80_R8_I;
        case 'R': *len = 1; return Z80_R8_R;
        default:  return -1;
    }
}

/**
 * @brief Rozpozna 16bitovy registr podle retezce.
 *
 * @param[in] s   Retezec k rozpoznani.
 * @param[out] len Delka rozpoznaneho tokenu.
 * @return Index registru (z80_reg16_t) nebo -1 pri neuspechu.
 */
static int parse_reg16(const char *s, int *len)
{
    *len = 2;
    if (s[0] == 'B' && s[1] == 'C') return Z80_R16_BC;
    if (s[0] == 'D' && s[1] == 'E') return Z80_R16_DE;
    if (s[0] == 'H' && s[1] == 'L') return Z80_R16_HL;
    if (s[0] == 'S' && s[1] == 'P') return Z80_R16_SP;
    if (s[0] == 'A' && s[1] == 'F') return Z80_R16_AF;
    if (s[0] == 'I' && s[1] == 'X') return Z80_R16_IX;
    if (s[0] == 'I' && s[1] == 'Y') return Z80_R16_IY;
    return -1;
}

/**
 * @brief Rozpozna podminku vetveni podle retezce.
 *
 * @param[in] s   Retezec k rozpoznani.
 * @param[out] len Delka rozpoznaneho tokenu.
 * @return Index podminky (z80_condition_t) nebo -1 pri neuspechu.
 */
static int parse_condition(const char *s, int *len)
{
    /* Dvouznakove podminky maji prednost */
    if (s[0] == 'N' && s[1] == 'Z') { *len = 2; return Z80_CC_NZ; }
    if (s[0] == 'N' && s[1] == 'C') { *len = 2; return Z80_CC_NC; }
    if (s[0] == 'P' && s[1] == 'O') { *len = 2; return Z80_CC_PO; }
    if (s[0] == 'P' && s[1] == 'E') { *len = 2; return Z80_CC_PE; }

    *len = 1;
    if (s[0] == 'Z')  return Z80_CC_Z;
    if (s[0] == 'C')  return Z80_CC_C;
    if (s[0] == 'P')  return Z80_CC_P;
    if (s[0] == 'M')  return Z80_CC_M;
    return -1;
}

/**
 * @brief Urcuje, zda je instrukce podminena (ma podminku jako operand).
 *
 * Podminene instrukce: JP cc, JR cc, CALL cc, RET cc, DJNZ.
 * Detekce podle flow_type z tabulky.
 */
static int is_conditional_flow(u8 flow_type)
{
    return flow_type == TJC || flow_type == TCC || flow_type == TRC;
}

/**
 * @brief Parsuje jeden operand z format stringu.
 *
 * Cte format string od pozice *pos a plni operandovou strukturu.
 * Pokud operand obsahuje zastupny znak, cte bajty z pameti.
 *
 * @param[out]    op         Operand k naplneni.
 * @param[in]     fmt        Format string.
 * @param[in,out] pos        Aktualni pozice ve format stringu.
 * @param[in]     read_fn    Callback pro cteni pameti.
 * @param[in]     user_data  Uzivatelska data pro callback.
 * @param[in,out] addr       Aktualni adresa cteni.
 * @param[in,out] bytes_read Pocet prectenych bajtu.
 * @param[in]     have_disp  Zda byl displacement jiz precten (DDCB/FDCB).
 * @param[in]     disp_val   Hodnota predcteneho displacementu.
 * @param[in]     flow_type  Typ toku (pro rozliseni podminky vs registr C).
 * @param[in]     is_first   Zda jde o prvni operand (pro podminky).
 */
static void parse_one_operand(z80_operand_t *op, const char *fmt, int *pos,
                              z80_dasm_read_fn read_fn, void *user_data,
                              u16 *addr, int *bytes_read,
                              int have_disp, u8 disp_val,
                              u8 flow_type, int is_first)
{
    const char *s = fmt + *pos;
    int len;

    op->type = Z80_OP_NONE;

    /* Preskoc mezery */
    while (*s == ' ') { s++; (*pos)++; }

    if (*s == '\0' || *s == ',') return;

    /* Zastupne znaky pro operandy z pameti */
    if (*s == '@') {
        /* 16bitove slovo */
        u8 lo = read_fn((*addr)++, user_data);
        u8 hi = read_fn((*addr)++, user_data);
        *bytes_read += 2;
        op->type = Z80_OP_IMM16;
        op->val.imm16 = (u16)(lo | (hi << 8));
        (*pos)++;
        return;
    }

    if (*s == '#') {
        /* 8bitovy bajt */
        u8 val = read_fn((*addr)++, user_data);
        *bytes_read += 1;
        op->type = Z80_OP_IMM8;
        op->val.imm8 = val;
        (*pos)++;
        return;
    }

    if (*s == '%') {
        /* Relativni offset (JR/DJNZ) */
        u8 raw = read_fn((*addr)++, user_data);
        *bytes_read += 1;
        op->type = Z80_OP_REL8;
        op->val.displacement = (s8)raw;
        (*pos)++;
        return;
    }

    /* Pametove operandy v zavorkach */
    if (*s == '(') {
        s++; (*pos)++;

        /* (@) - prime adresovani */
        if (*s == '@') {
            u8 lo = read_fn((*addr)++, user_data);
            u8 hi = read_fn((*addr)++, user_data);
            *bytes_read += 2;
            op->type = Z80_OP_MEM_IMM16;
            op->val.imm16 = (u16)(lo | (hi << 8));
            (*pos) += 2; /* @ a ) */
            return;
        }

        /* (#) - I/O port s primou adresou */
        if (*s == '#') {
            u8 val = read_fn((*addr)++, user_data);
            *bytes_read += 1;
            op->type = Z80_OP_MEM_IMM8;
            op->val.imm8 = val;
            (*pos) += 2;
            return;
        }

        /* (IX+$) nebo (IY+$) */
        if (s[0] == 'I' && (s[1] == 'X' || s[1] == 'Y') && s[2] == '+') {
            s8 disp;
            if (have_disp) {
                disp = (s8)disp_val;
            } else {
                u8 raw = read_fn((*addr)++, user_data);
                *bytes_read += 1;
                disp = (s8)raw;
            }
            op->type = (s[1] == 'X') ? Z80_OP_MEM_IX_D : Z80_OP_MEM_IY_D;
            op->val.displacement = disp;
            /* Preskoc "IX+$)" nebo "IY+$)" */
            *pos += 5;
            return;
        }

        /* (BC), (DE), (HL), (SP), (IX), (IY) */
        int r16 = parse_reg16(s, &len);
        if (r16 >= 0) {
            op->type = Z80_OP_MEM_REG16;
            op->val.reg16 = (u8)r16;
            *pos += len + 1; /* registr + ')' */
            return;
        }

        /* (C) - I/O port z registru C (IN/OUT instrukce) */
        if (s[0] == 'C' && s[1] == ')') {
            op->type = Z80_OP_MEM_REG16;
            op->val.reg16 = Z80_R16_BC; /* IN r,(C) pouziva cely BC jako adresu */
            *pos += 2;
            return;
        }

        return;
    }

    /* Vicemistny hex literal (RST vektory: "00","08","10","18","20","28","30","38")
       a cislo bitu 0-7 (BIT/SET/RES) */
    if (*s >= '0' && *s <= '9') {
        /* Pokud nasleduje dalsi hex cislice -> vicemistne cislo (RST vektor) */
        if ((s[1] >= '0' && s[1] <= '9') ||
            (s[1] >= 'A' && s[1] <= 'F') ||
            (s[1] >= 'a' && s[1] <= 'f')) {
            u8 vec = 0;
            while ((*s >= '0' && *s <= '9') ||
                   (*s >= 'A' && *s <= 'F') ||
                   (*s >= 'a' && *s <= 'f')) {
                if (*s >= '0' && *s <= '9')
                    vec = vec * 16 + (*s - '0');
                else
                    vec = vec * 16 + ((*s & 0xDF) - 'A' + 10);
                s++; (*pos)++;
            }
            op->type = Z80_OP_RST_VEC;
            op->val.imm8 = vec;
            return;
        }

        /* Jednociferny: cislo bitu 0-7 (BIT/SET/RES) */
        if (*s >= '0' && *s <= '7' && (s[1] == ',' || s[1] == '\0')) {
        op->type = Z80_OP_BIT_INDEX;
            op->type = Z80_OP_BIT_INDEX;
            op->val.bit_index = (u8)(*s - '0');
            (*pos)++;
            return;
        }

        return; /* nerozpoznany ciselny literal */
    }

    /* Podminky: NZ, Z, NC, C, PO, PE, P, M
       Podminky se vyskytuji pouze jako prvni operand podminenych instrukci.
       "C" je nejednoznacne (registr vs podminka) - rozlisujeme podle flow_type. */
    if (is_first && is_conditional_flow(flow_type)) {
        int cc = parse_condition(s, &len);
        if (cc >= 0) {
            op->type = Z80_OP_CONDITION;
            op->val.condition = (u8)cc;
            *pos += len;
            return;
        }
    }

    /* 16bitovy registr (musi byt pred 8bitovym kvuli BC, DE, HL...) */
    int r16 = parse_reg16(s, &len);
    if (r16 >= 0 && (s[len] == ',' || s[len] == '\0' || s[len] == '\'')) {
        op->type = Z80_OP_REG16;
        op->val.reg16 = (u8)r16;
        *pos += len;
        /* AF' - preskoc apostrof */
        if (s[len] == '\'') (*pos)++;
        return;
    }

    /* 8bitovy registr */
    int r8 = parse_reg8(s, &len);
    if (r8 >= 0) {
        op->type = Z80_OP_REG8;
        op->val.reg8 = (u8)r8;
        *pos += len;
        return;
    }

    /* Literal "0" v "OUT (C),0" */
    if (*s == '0' && (s[1] == ',' || s[1] == '\0')) {
        op->type = Z80_OP_IMM8;
        op->val.imm8 = 0;
        (*pos)++;
        return;
    }
}

/**
 * @brief Extrahuje zakladni mnemoniku z format stringu.
 *
 * Vraci ukazatel na interní staticky retezec.
 */
const char *z80_dasm_extract_mnemonic(const char *format)
{
    static char buf[16];
    int i = 0;

    if (!format) return "???";

    while (format[i] && format[i] != ' ' && i < 15) {
        buf[i] = format[i];
        i++;
    }
    buf[i] = '\0';
    return buf;
}

/* ======================================================================
 * Verejne API: jadro disassembleru
 * ====================================================================== */

int z80_dasm(z80_dasm_inst_t *inst,
             z80_dasm_read_fn read_fn, void *user_data,
             u16 addr)
{
    u8 opc, next;
    u8 disp_u = 0;
    int have_disp = 0;
    int bytes = 0;
    u16 start_addr = addr;
    const z80_dasm_opc_t *entry = NULL;

    memset(inst, 0, sizeof(*inst));
    inst->addr = start_addr;

    /* Cti prvni bajt */
    opc = read_fn(addr++, user_data);
    inst->bytes[bytes++] = opc;

    switch (opc) {
    case 0xDD:
    case 0xFD:
        next = read_fn(addr++, user_data);
        inst->bytes[bytes++] = next;

        /* Neplatne sekvence: DD DD, DD FD, FD DD, FD FD, DD ED, FD ED */
        if ((next | 0x20) == 0xFD || next == 0xED) {
            /* Vrat prefix jako 1bajtovou NOP* instrukci */
            inst->length = 1;
            inst->t_states = 4;
            inst->t_states2 = 0;
            inst->flow = Z80_FLOW_NORMAL;
            inst->cls = Z80_CLASS_INVALID;
            inst->mnemonic = "NOP*";
            inst->op1.type = Z80_OP_NONE;
            inst->op2.type = Z80_OP_NONE;
            return 1;
        }

        if (next == 0xCB) {
            /* DD CB d opcode / FD CB d opcode */
            disp_u = read_fn(addr++, user_data);
            inst->bytes[bytes++] = disp_u;
            u8 final_opc = read_fn(addr++, user_data);
            inst->bytes[bytes++] = final_opc;
            bytes = 4;
            have_disp = 1;
            entry = (opc == 0xDD) ? &z80_dasm_ddcb[final_opc]
                                  : &z80_dasm_fdcb[final_opc];
        } else {
            /* Normalni DD/FD instrukce */
            entry = (opc == 0xDD) ? &z80_dasm_dd[next]
                                  : &z80_dasm_fd[next];
            /* NULL format = fallback na base tabulku (zrcadlena instrukce) */
            if (entry->format == NULL) {
                entry = &z80_dasm_base[next];
                /* Zrcadlena instrukce: DD prefix prida 4T */
                inst->t_states = 4;
            }
        }
        break;

    case 0xED:
        next = read_fn(addr++, user_data);
        inst->bytes[bytes++] = next;
        entry = &z80_dasm_ed[next];
        if (entry->format == NULL) {
            /* Neplatna ED instrukce */
            inst->length = 2;
            inst->t_states = 8;
            inst->t_states2 = 0;
            inst->flow = Z80_FLOW_NORMAL;
            inst->cls = Z80_CLASS_INVALID;
            inst->mnemonic = "NOP*";
            inst->op1.type = Z80_OP_NONE;
            inst->op2.type = Z80_OP_NONE;
            return 2;
        }
        break;

    case 0xCB:
        next = read_fn(addr++, user_data);
        inst->bytes[bytes++] = next;
        entry = &z80_dasm_cb[next];
        break;

    default:
        /* Zakladni instrukce */
        entry = &z80_dasm_base[opc];
        /* Prefixy CB, DD, ED, FD maji NULL format v base tabulce */
        if (entry->format == NULL) {
            inst->length = 1;
            inst->t_states = 4;
            inst->t_states2 = 0;
            inst->flow = Z80_FLOW_NORMAL;
            inst->cls = Z80_CLASS_INVALID;
            inst->mnemonic = "NOP*";
            inst->op1.type = Z80_OP_NONE;
            inst->op2.type = Z80_OP_NONE;
            return 1;
        }
        break;
    }

    /* Vyplneni metadata z tabulky */
    inst->t_states += entry->t_states;
    inst->t_states2 = entry->t_states2;
    inst->flags_affected = entry->flags_affected;
    inst->regs_read = entry->regs_read;
    inst->regs_written = entry->regs_written;
    inst->flow = (z80_flow_type_t)entry->flow_type;
    inst->cls = (z80_inst_class_t)entry->inst_class;

    /* Extrakce mnemoniky */
    inst->mnemonic = z80_dasm_extract_mnemonic(entry->format);

    /* Parsovani operandu z format stringu */
    const char *fmt = entry->format;
    int pos = 0;

    /* Preskoc mnemoniku (vse pred prvni mezerou) */
    while (fmt[pos] && fmt[pos] != ' ') pos++;
    if (fmt[pos] == ' ') pos++;

    /* Prvni operand */
    if (fmt[pos] && fmt[pos] != '\0') {
        parse_one_operand(&inst->op1, fmt, &pos, read_fn, user_data,
                          &addr, &bytes, have_disp, disp_u,
                          entry->flow_type, 1);
    }

    /* Carka mezi operandy */
    if (fmt[pos] == ',') pos++;

    /* Druhy operand */
    if (fmt[pos] && fmt[pos] != '\0') {
        parse_one_operand(&inst->op2, fmt, &pos, read_fn, user_data,
                          &addr, &bytes, have_disp, disp_u,
                          entry->flow_type, 0);
    }

    inst->length = (u8)bytes;

    /* Doplneni surovych bajtu (operandy prectenych navic) */
    /* bytes[0..prefix_len-1] uz jsou vyplneny, zbytek doplnime */
    /* Prepocitame bajty z adresy */
    for (int i = inst->bytes[0] == 0xDD || inst->bytes[0] == 0xFD ||
                 inst->bytes[0] == 0xED || inst->bytes[0] == 0xCB ? 2 : 1;
         i < bytes && i < 4; i++) {
        if (inst->bytes[i] == 0 && i >= 2) {
            /* Bajty operandu - precteme je znovu */
            inst->bytes[i] = read_fn((u16)(start_addr + i), user_data);
        }
    }

    /* Korektni vyplneni vsech bajtu */
    for (int i = 0; i < (int)inst->length && i < 4; i++) {
        inst->bytes[i] = read_fn((u16)(start_addr + i), user_data);
    }

    return (int)inst->length;
}

int z80_dasm_block(z80_dasm_inst_t *out, int max_inst,
                   z80_dasm_read_fn read_fn, void *user_data,
                   u16 start_addr, u16 end_addr)
{
    int count = 0;
    u16 addr = start_addr;

    if (start_addr > end_addr) return 0;

    while (count < max_inst && addr <= end_addr) {
        int len = z80_dasm(&out[count], read_fn, user_data, addr);

        /* Ochrana proti preteceni u16 */
        u32 next = (u32)addr + (u32)len;
        if (next > 0xFFFF && end_addr != 0xFFFF) break;

        count++;
        addr = (u16)next;

        /* Zastaveni pokud dalsi instrukce by zacala za end_addr */
        if (addr > end_addr && addr != 0) break;
    }

    return count;
}

u16 z80_dasm_find_inst_start(z80_dasm_read_fn read_fn, void *user_data,
                             u16 target_addr, u16 search_from)
{
    /*
     * Heuristika: disassembluj z vice startovnich bodu
     * a sleduj, ktera adresa se objevuje nejcasteji
     * jako zacatek instrukce tesne pred target_addr.
     *
     * Z80 instrukce maji delku 1-4 bajty, proto hledame
     * konsensus z az 16 ruznych startovnich bodu.
     */
    u16 votes[4] = {0};  /* az 4 kandidati */
    int vote_count[4] = {0};
    int num_candidates = 0;
    z80_dasm_inst_t tmp;

    if (search_from >= target_addr) return target_addr;

    /* Zkusime startovat z kazde adresy v rozsahu [search_from, target_addr) */
    for (u16 start = search_from; start < target_addr; start++) {
        u16 a = start;

        /* Disassembluj dokud nedosahnes target_addr */
        while (a < target_addr) {
            int len = z80_dasm(&tmp, read_fn, user_data, a);
            u16 next_a = (u16)(a + len);

            if (next_a == target_addr) {
                /* Tato cesta dosla presne na target_addr - dobry znak */
                /* Posledni instrukce pred cilem je nas kandidat */
                int found = 0;
                for (int i = 0; i < num_candidates; i++) {
                    if (votes[i] == a) {
                        vote_count[i]++;
                        found = 1;
                        break;
                    }
                }
                if (!found && num_candidates < 4) {
                    votes[num_candidates] = a;
                    vote_count[num_candidates] = 1;
                    num_candidates++;
                }
                break;
            }

            if (next_a > target_addr) break; /* presah */
            a = next_a;
        }
    }

    /* Najdi kandidata s nejvice hlasy */
    int best = 0;
    for (int i = 1; i < num_candidates; i++) {
        if (vote_count[i] > vote_count[best]) best = i;
    }

    return (num_candidates > 0) ? votes[best] : target_addr;
}

/* ======================================================================
 * Konverze relativnich adres
 * ====================================================================== */

u16 z80_rel_to_abs(u16 addr, s8 offset)
{
    return (u16)(addr + 2 + offset);
}

int z80_abs_to_rel(u16 addr, u16 target, s8 *offset)
{
    int diff = (int)target - (int)addr - 2;

    if (diff < -128 || diff > 127) return -1;

    *offset = (s8)diff;
    return 0;
}
