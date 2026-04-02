/*
 * Copyright (c) 2026 Michal Hucik
 * SPDX-License-Identifier: MIT
 * https://github.com/michalhucik/z80-mz800
 */
/**
 * @file z80_dasm_tables.c
 * @brief Opcode tabulky pro Z80 disassembler.
 * @version 0.1
 *
 * Obsahuje 7 tabulek po 256 zaznamech pro vsechny skupiny instrukci Z80
 * vcetne nedokumentovanych. Kazdy zaznam obsahuje format string (mnemoniku
 * s operandy), casovani (T-stavy), mapu pristupu k registrum a flagum,
 * typ toku rizeni a klasifikaci instrukce.
 *
 * Zdroje dat:
 * - Zilog Z80 CPU User Manual (UM0080)
 * - z80ex-mz800/opcodes/opcodes_dasm.c.h (T-stavy, reference)
 * - docs/z80/z80_opcode_list.txt (kompletni seznam vcetne undocumented)
 * - docs/z80/z80_flag_affection.html (vliv na flagy)
 *
 * Zastupne znaky ve format stringu:
 * - '@' = 16bitove slovo (little-endian, 2 bajty)
 * - '#' = 8bitovy bajt
 * - '$' = znamenkovy displacement pro IX/IY+d
 * - '%%' = relativni offset (JR/DJNZ), ve vystupu jako absolutni adresa
 */

#include "z80_dasm_internal.h"

/* ======================================================================
 * Zakladni instrukce (bez prefixu)
 * ====================================================================== */

const z80_dasm_opc_t z80_dasm_base[256] = {
    /* 00-0F */
    { "NOP",            4,  0, FN,   0,       0,       TN,  CO }, /* 00 */
    { "LD BC,@",       10,  0, FN,   0,       RBC,     TN,  CO }, /* 01 */
    { "LD (BC),A",      7,  0, FN,   RBC|RA,  0,       TN,  CO }, /* 02 */
    { "INC BC",         6,  0, FN,   RBC,     RBC,     TN,  CO }, /* 03 */
    { "INC B",          4,  0, FNC,  RB,      RB|RF,   TN,  CO }, /* 04 */
    { "DEC B",          4,  0, FNC,  RB,      RB|RF,   TN,  CO }, /* 05 */
    { "LD B,#",         7,  0, FN,   0,       RB,      TN,  CO }, /* 06 */
    { "RLCA",           4,  0, FCHN, RA,      RA|RF,   TN,  CO }, /* 07 */
    { "EX AF,AF'",      4,  0, FA,   RAF,     RAF,     TN,  CO }, /* 08 */
    { "ADD HL,BC",     11,  0, FCHN, RHL|RBC, RHL|RF,  TN,  CO }, /* 09 */
    { "LD A,(BC)",      7,  0, FN,   RBC,     RA,      TN,  CO }, /* 0A */
    { "DEC BC",         6,  0, FN,   RBC,     RBC,     TN,  CO }, /* 0B */
    { "INC C",          4,  0, FNC,  RC,      RC|RF,   TN,  CO }, /* 0C */
    { "DEC C",          4,  0, FNC,  RC,      RC|RF,   TN,  CO }, /* 0D */
    { "LD C,#",         7,  0, FN,   0,       RC,      TN,  CO }, /* 0E */
    { "RRCA",           4,  0, FCHN, RA,      RA|RF,   TN,  CO }, /* 0F */

    /* 10-1F */
    { "DJNZ %",        8, 13, FN,   RB,      RB,      TJC, CO }, /* 10 */
    { "LD DE,@",       10,  0, FN,   0,       RDE,     TN,  CO }, /* 11 */
    { "LD (DE),A",      7,  0, FN,   RDE|RA,  0,       TN,  CO }, /* 12 */
    { "INC DE",         6,  0, FN,   RDE,     RDE,     TN,  CO }, /* 13 */
    { "INC D",          4,  0, FNC,  RD,      RD|RF,   TN,  CO }, /* 14 */
    { "DEC D",          4,  0, FNC,  RD,      RD|RF,   TN,  CO }, /* 15 */
    { "LD D,#",         7,  0, FN,   0,       RD,      TN,  CO }, /* 16 */
    { "RLA",            4,  0, FCHN, RA|RF,   RA|RF,   TN,  CO }, /* 17 */
    { "JR %",          12,  0, FN,   0,       0,       TJ,  CO }, /* 18 */
    { "ADD HL,DE",     11,  0, FCHN, RHL|RDE, RHL|RF,  TN,  CO }, /* 19 */
    { "LD A,(DE)",      7,  0, FN,   RDE,     RA,      TN,  CO }, /* 1A */
    { "DEC DE",         6,  0, FN,   RDE,     RDE,     TN,  CO }, /* 1B */
    { "INC E",          4,  0, FNC,  RE,      RE|RF,   TN,  CO }, /* 1C */
    { "DEC E",          4,  0, FNC,  RE,      RE|RF,   TN,  CO }, /* 1D */
    { "LD E,#",         7,  0, FN,   0,       RE,      TN,  CO }, /* 1E */
    { "RRA",            4,  0, FCHN, RA|RF,   RA|RF,   TN,  CO }, /* 1F */

    /* 20-2F */
    { "JR NZ,%",        7, 12, FN,   RF,      0,       TJC, CO }, /* 20 */
    { "LD HL,@",       10,  0, FN,   0,       RHL,     TN,  CO }, /* 21 */
    { "LD (@),HL",     16,  0, FN,   RHL,     0,       TN,  CO }, /* 22 */
    { "INC HL",         6,  0, FN,   RHL,     RHL,     TN,  CO }, /* 23 */
    { "INC H",          4,  0, FNC,  RH,      RH|RF,   TN,  CO }, /* 24 */
    { "DEC H",          4,  0, FNC,  RH,      RH|RF,   TN,  CO }, /* 25 */
    { "LD H,#",         7,  0, FN,   0,       RH,      TN,  CO }, /* 26 */
    { "DAA",            4,  0, FA,   RA|RF,   RA|RF,   TN,  CO }, /* 27 */
    { "JR Z,%",         7, 12, FN,   RF,      0,       TJC, CO }, /* 28 */
    { "ADD HL,HL",     11,  0, FCHN, RHL,     RHL|RF,  TN,  CO }, /* 29 */
    { "LD HL,(@)",     16,  0, FN,   0,       RHL,     TN,  CO }, /* 2A */
    { "DEC HL",         6,  0, FN,   RHL,     RHL,     TN,  CO }, /* 2B */
    { "INC L",          4,  0, FNC,  RL_,     RL_|RF,  TN,  CO }, /* 2C */
    { "DEC L",          4,  0, FNC,  RL_,     RL_|RF,  TN,  CO }, /* 2D */
    { "LD L,#",         7,  0, FN,   0,       RL_,     TN,  CO }, /* 2E */
    { "CPL",            4,  0, FCHN & ~Z80_FLAG_C, RA, RA|RF, TN, CO }, /* 2F: H=1,N=1,3,5 */

    /* 30-3F */
    { "JR NC,%",        7, 12, FN,   RF,      0,       TJC, CO }, /* 30 */
    { "LD SP,@",       10,  0, FN,   0,       RSP,     TN,  CO }, /* 31 */
    { "LD (@),A",      13,  0, FN,   RA,      0,       TN,  CO }, /* 32 */
    { "INC SP",         6,  0, FN,   RSP,     RSP,     TN,  CO }, /* 33 */
    { "INC (HL)",      11,  0, FNC,  RHL,     RF,      TN,  CO }, /* 34 */
    { "DEC (HL)",      11,  0, FNC,  RHL,     RF,      TN,  CO }, /* 35 */
    { "LD (HL),#",     10,  0, FN,   RHL,     0,       TN,  CO }, /* 36 */
    { "SCF",            4,  0, FCHN, 0,       RF,      TN,  CO }, /* 37 */
    { "JR C,%",         7, 12, FN,   RF,      0,       TJC, CO }, /* 38 */
    { "ADD HL,SP",     11,  0, FCHN, RHL|RSP, RHL|RF,  TN,  CO }, /* 39 */
    { "LD A,(@)",      13,  0, FN,   0,       RA,      TN,  CO }, /* 3A */
    { "DEC SP",         6,  0, FN,   RSP,     RSP,     TN,  CO }, /* 3B */
    { "INC A",          4,  0, FNC,  RA,      RA|RF,   TN,  CO }, /* 3C */
    { "DEC A",          4,  0, FNC,  RA,      RA|RF,   TN,  CO }, /* 3D */
    { "LD A,#",         7,  0, FN,   0,       RA,      TN,  CO }, /* 3E */
    { "CCF",            4,  0, FCHN, RF,      RF,      TN,  CO }, /* 3F */

    /* 40-4F: LD r,r' */
    { "LD B,B",         4,  0, FN,   RB,      RB,      TN,  CO }, /* 40 */
    { "LD B,C",         4,  0, FN,   RC,      RB,      TN,  CO }, /* 41 */
    { "LD B,D",         4,  0, FN,   RD,      RB,      TN,  CO }, /* 42 */
    { "LD B,E",         4,  0, FN,   RE,      RB,      TN,  CO }, /* 43 */
    { "LD B,H",         4,  0, FN,   RH,      RB,      TN,  CO }, /* 44 */
    { "LD B,L",         4,  0, FN,   RL_,     RB,      TN,  CO }, /* 45 */
    { "LD B,(HL)",      7,  0, FN,   RHL,     RB,      TN,  CO }, /* 46 */
    { "LD B,A",         4,  0, FN,   RA,      RB,      TN,  CO }, /* 47 */
    { "LD C,B",         4,  0, FN,   RB,      RC,      TN,  CO }, /* 48 */
    { "LD C,C",         4,  0, FN,   RC,      RC,      TN,  CO }, /* 49 */
    { "LD C,D",         4,  0, FN,   RD,      RC,      TN,  CO }, /* 4A */
    { "LD C,E",         4,  0, FN,   RE,      RC,      TN,  CO }, /* 4B */
    { "LD C,H",         4,  0, FN,   RH,      RC,      TN,  CO }, /* 4C */
    { "LD C,L",         4,  0, FN,   RL_,     RC,      TN,  CO }, /* 4D */
    { "LD C,(HL)",      7,  0, FN,   RHL,     RC,      TN,  CO }, /* 4E */
    { "LD C,A",         4,  0, FN,   RA,      RC,      TN,  CO }, /* 4F */

    /* 50-5F: LD r,r' */
    { "LD D,B",         4,  0, FN,   RB,      RD,      TN,  CO }, /* 50 */
    { "LD D,C",         4,  0, FN,   RC,      RD,      TN,  CO }, /* 51 */
    { "LD D,D",         4,  0, FN,   RD,      RD,      TN,  CO }, /* 52 */
    { "LD D,E",         4,  0, FN,   RE,      RD,      TN,  CO }, /* 53 */
    { "LD D,H",         4,  0, FN,   RH,      RD,      TN,  CO }, /* 54 */
    { "LD D,L",         4,  0, FN,   RL_,     RD,      TN,  CO }, /* 55 */
    { "LD D,(HL)",      7,  0, FN,   RHL,     RD,      TN,  CO }, /* 56 */
    { "LD D,A",         4,  0, FN,   RA,      RD,      TN,  CO }, /* 57 */
    { "LD E,B",         4,  0, FN,   RB,      RE,      TN,  CO }, /* 58 */
    { "LD E,C",         4,  0, FN,   RC,      RE,      TN,  CO }, /* 59 */
    { "LD E,D",         4,  0, FN,   RD,      RE,      TN,  CO }, /* 5A */
    { "LD E,E",         4,  0, FN,   RE,      RE,      TN,  CO }, /* 5B */
    { "LD E,H",         4,  0, FN,   RH,      RE,      TN,  CO }, /* 5C */
    { "LD E,L",         4,  0, FN,   RL_,     RE,      TN,  CO }, /* 5D */
    { "LD E,(HL)",      7,  0, FN,   RHL,     RE,      TN,  CO }, /* 5E */
    { "LD E,A",         4,  0, FN,   RA,      RE,      TN,  CO }, /* 5F */

    /* 60-6F: LD r,r' */
    { "LD H,B",         4,  0, FN,   RB,      RH,      TN,  CO }, /* 60 */
    { "LD H,C",         4,  0, FN,   RC,      RH,      TN,  CO }, /* 61 */
    { "LD H,D",         4,  0, FN,   RD,      RH,      TN,  CO }, /* 62 */
    { "LD H,E",         4,  0, FN,   RE,      RH,      TN,  CO }, /* 63 */
    { "LD H,H",         4,  0, FN,   RH,      RH,      TN,  CO }, /* 64 */
    { "LD H,L",         4,  0, FN,   RL_,     RH,      TN,  CO }, /* 65 */
    { "LD H,(HL)",      7,  0, FN,   RHL,     RH,      TN,  CO }, /* 66 */
    { "LD H,A",         4,  0, FN,   RA,      RH,      TN,  CO }, /* 67 */
    { "LD L,B",         4,  0, FN,   RB,      RL_,     TN,  CO }, /* 68 */
    { "LD L,C",         4,  0, FN,   RC,      RL_,     TN,  CO }, /* 69 */
    { "LD L,D",         4,  0, FN,   RD,      RL_,     TN,  CO }, /* 6A */
    { "LD L,E",         4,  0, FN,   RE,      RL_,     TN,  CO }, /* 6B */
    { "LD L,H",         4,  0, FN,   RH,      RL_,     TN,  CO }, /* 6C */
    { "LD L,L",         4,  0, FN,   RL_,     RL_,     TN,  CO }, /* 6D */
    { "LD L,(HL)",      7,  0, FN,   RHL,     RL_,     TN,  CO }, /* 6E */
    { "LD L,A",         4,  0, FN,   RA,      RL_,     TN,  CO }, /* 6F */

    /* 70-7F: LD (HL),r a LD A,r */
    { "LD (HL),B",      7,  0, FN,   RHL|RB,  0,       TN,  CO }, /* 70 */
    { "LD (HL),C",      7,  0, FN,   RHL|RC,  0,       TN,  CO }, /* 71 */
    { "LD (HL),D",      7,  0, FN,   RHL|RD,  0,       TN,  CO }, /* 72 */
    { "LD (HL),E",      7,  0, FN,   RHL|RE,  0,       TN,  CO }, /* 73 */
    { "LD (HL),H",      7,  0, FN,   RHL|RH,  0,       TN,  CO }, /* 74 */
    { "LD (HL),L",      7,  0, FN,   RHL|RL_, 0,       TN,  CO }, /* 75 */
    { "HALT",           4,  0, FN,   0,       0,       TH,  CO }, /* 76 */
    { "LD (HL),A",      7,  0, FN,   RHL|RA,  0,       TN,  CO }, /* 77 */
    { "LD A,B",         4,  0, FN,   RB,      RA,      TN,  CO }, /* 78 */
    { "LD A,C",         4,  0, FN,   RC,      RA,      TN,  CO }, /* 79 */
    { "LD A,D",         4,  0, FN,   RD,      RA,      TN,  CO }, /* 7A */
    { "LD A,E",         4,  0, FN,   RE,      RA,      TN,  CO }, /* 7B */
    { "LD A,H",         4,  0, FN,   RH,      RA,      TN,  CO }, /* 7C */
    { "LD A,L",         4,  0, FN,   RL_,     RA,      TN,  CO }, /* 7D */
    { "LD A,(HL)",      7,  0, FN,   RHL,     RA,      TN,  CO }, /* 7E */
    { "LD A,A",         4,  0, FN,   RA,      RA,      TN,  CO }, /* 7F */

    /* 80-87: ADD A,r */
    { "ADD A,B",        4,  0, FA,   RA|RB,   RA|RF,   TN,  CO }, /* 80 */
    { "ADD A,C",        4,  0, FA,   RA|RC,   RA|RF,   TN,  CO }, /* 81 */
    { "ADD A,D",        4,  0, FA,   RA|RD,   RA|RF,   TN,  CO }, /* 82 */
    { "ADD A,E",        4,  0, FA,   RA|RE,   RA|RF,   TN,  CO }, /* 83 */
    { "ADD A,H",        4,  0, FA,   RA|RH,   RA|RF,   TN,  CO }, /* 84 */
    { "ADD A,L",        4,  0, FA,   RA|RL_,  RA|RF,   TN,  CO }, /* 85 */
    { "ADD A,(HL)",     7,  0, FA,   RA|RHL,  RA|RF,   TN,  CO }, /* 86 */
    { "ADD A,A",        4,  0, FA,   RA,      RA|RF,   TN,  CO }, /* 87 */

    /* 88-8F: ADC A,r */
    { "ADC A,B",        4,  0, FA,   RA|RB|RF, RA|RF,  TN,  CO }, /* 88 */
    { "ADC A,C",        4,  0, FA,   RA|RC|RF, RA|RF,  TN,  CO }, /* 89 */
    { "ADC A,D",        4,  0, FA,   RA|RD|RF, RA|RF,  TN,  CO }, /* 8A */
    { "ADC A,E",        4,  0, FA,   RA|RE|RF, RA|RF,  TN,  CO }, /* 8B */
    { "ADC A,H",        4,  0, FA,   RA|RH|RF, RA|RF,  TN,  CO }, /* 8C */
    { "ADC A,L",        4,  0, FA,   RA|RL_|RF,RA|RF,  TN,  CO }, /* 8D */
    { "ADC A,(HL)",     7,  0, FA,   RA|RHL|RF,RA|RF,  TN,  CO }, /* 8E */
    { "ADC A,A",        4,  0, FA,   RA|RF,   RA|RF,   TN,  CO }, /* 8F */

    /* 90-97: SUB r */
    { "SUB B",          4,  0, FA,   RA|RB,   RA|RF,   TN,  CO }, /* 90 */
    { "SUB C",          4,  0, FA,   RA|RC,   RA|RF,   TN,  CO }, /* 91 */
    { "SUB D",          4,  0, FA,   RA|RD,   RA|RF,   TN,  CO }, /* 92 */
    { "SUB E",          4,  0, FA,   RA|RE,   RA|RF,   TN,  CO }, /* 93 */
    { "SUB H",          4,  0, FA,   RA|RH,   RA|RF,   TN,  CO }, /* 94 */
    { "SUB L",          4,  0, FA,   RA|RL_,  RA|RF,   TN,  CO }, /* 95 */
    { "SUB (HL)",       7,  0, FA,   RA|RHL,  RA|RF,   TN,  CO }, /* 96 */
    { "SUB A",          4,  0, FA,   RA,      RA|RF,   TN,  CO }, /* 97 */

    /* 98-9F: SBC A,r */
    { "SBC A,B",        4,  0, FA,   RA|RB|RF, RA|RF,  TN,  CO }, /* 98 */
    { "SBC A,C",        4,  0, FA,   RA|RC|RF, RA|RF,  TN,  CO }, /* 99 */
    { "SBC A,D",        4,  0, FA,   RA|RD|RF, RA|RF,  TN,  CO }, /* 9A */
    { "SBC A,E",        4,  0, FA,   RA|RE|RF, RA|RF,  TN,  CO }, /* 9B */
    { "SBC A,H",        4,  0, FA,   RA|RH|RF, RA|RF,  TN,  CO }, /* 9C */
    { "SBC A,L",        4,  0, FA,   RA|RL_|RF,RA|RF,  TN,  CO }, /* 9D */
    { "SBC A,(HL)",     7,  0, FA,   RA|RHL|RF,RA|RF,  TN,  CO }, /* 9E */
    { "SBC A,A",        4,  0, FA,   RA|RF,   RA|RF,   TN,  CO }, /* 9F */

    /* A0-A7: AND r */
    { "AND B",          4,  0, FA,   RA|RB,   RA|RF,   TN,  CO }, /* A0 */
    { "AND C",          4,  0, FA,   RA|RC,   RA|RF,   TN,  CO }, /* A1 */
    { "AND D",          4,  0, FA,   RA|RD,   RA|RF,   TN,  CO }, /* A2 */
    { "AND E",          4,  0, FA,   RA|RE,   RA|RF,   TN,  CO }, /* A3 */
    { "AND H",          4,  0, FA,   RA|RH,   RA|RF,   TN,  CO }, /* A4 */
    { "AND L",          4,  0, FA,   RA|RL_,  RA|RF,   TN,  CO }, /* A5 */
    { "AND (HL)",       7,  0, FA,   RA|RHL,  RA|RF,   TN,  CO }, /* A6 */
    { "AND A",          4,  0, FA,   RA,      RA|RF,   TN,  CO }, /* A7 */

    /* A8-AF: XOR r */
    { "XOR B",          4,  0, FA,   RA|RB,   RA|RF,   TN,  CO }, /* A8 */
    { "XOR C",          4,  0, FA,   RA|RC,   RA|RF,   TN,  CO }, /* A9 */
    { "XOR D",          4,  0, FA,   RA|RD,   RA|RF,   TN,  CO }, /* AA */
    { "XOR E",          4,  0, FA,   RA|RE,   RA|RF,   TN,  CO }, /* AB */
    { "XOR H",          4,  0, FA,   RA|RH,   RA|RF,   TN,  CO }, /* AC */
    { "XOR L",          4,  0, FA,   RA|RL_,  RA|RF,   TN,  CO }, /* AD */
    { "XOR (HL)",       7,  0, FA,   RA|RHL,  RA|RF,   TN,  CO }, /* AE */
    { "XOR A",          4,  0, FA,   RA,      RA|RF,   TN,  CO }, /* AF */

    /* B0-B7: OR r */
    { "OR B",           4,  0, FA,   RA|RB,   RA|RF,   TN,  CO }, /* B0 */
    { "OR C",           4,  0, FA,   RA|RC,   RA|RF,   TN,  CO }, /* B1 */
    { "OR D",           4,  0, FA,   RA|RD,   RA|RF,   TN,  CO }, /* B2 */
    { "OR E",           4,  0, FA,   RA|RE,   RA|RF,   TN,  CO }, /* B3 */
    { "OR H",           4,  0, FA,   RA|RH,   RA|RF,   TN,  CO }, /* B4 */
    { "OR L",           4,  0, FA,   RA|RL_,  RA|RF,   TN,  CO }, /* B5 */
    { "OR (HL)",        7,  0, FA,   RA|RHL,  RA|RF,   TN,  CO }, /* B6 */
    { "OR A",           4,  0, FA,   RA,      RA|RF,   TN,  CO }, /* B7 */

    /* B8-BF: CP r */
    { "CP B",           4,  0, FA,   RA|RB,   RF,      TN,  CO }, /* B8 */
    { "CP C",           4,  0, FA,   RA|RC,   RF,      TN,  CO }, /* B9 */
    { "CP D",           4,  0, FA,   RA|RD,   RF,      TN,  CO }, /* BA */
    { "CP E",           4,  0, FA,   RA|RE,   RF,      TN,  CO }, /* BB */
    { "CP H",           4,  0, FA,   RA|RH,   RF,      TN,  CO }, /* BC */
    { "CP L",           4,  0, FA,   RA|RL_,  RF,      TN,  CO }, /* BD */
    { "CP (HL)",        7,  0, FA,   RA|RHL,  RF,      TN,  CO }, /* BE */
    { "CP A",           4,  0, FA,   RA,      RF,      TN,  CO }, /* BF */

    /* C0-CF */
    { "RET NZ",         5, 11, FN,   RSP|RF,  RSP,     TRC, CO }, /* C0 */
    { "POP BC",        10,  0, FN,   RSP,     RBC|RSP, TN,  CO }, /* C1 */
    { "JP NZ,@",       10,  0, FN,   RF,      0,       TJC, CO }, /* C2 */
    { "JP @",          10,  0, FN,   0,       0,       TJ,  CO }, /* C3 */
    { "CALL NZ,@",     10, 17, FN,   RSP|RF,  RSP,     TCC, CO }, /* C4 */
    { "PUSH BC",       11,  0, FN,   RBC|RSP, RSP,     TN,  CO }, /* C5 */
    { "ADD A,#",        7,  0, FA,   RA,      RA|RF,   TN,  CO }, /* C6 */
    { "RST 00",        11,  0, FN,   RSP,     RSP,     TRST,CO }, /* C7 */
    { "RET Z",          5, 11, FN,   RSP|RF,  RSP,     TRC, CO }, /* C8 */
    { "RET",           10,  0, FN,   RSP,     RSP,     TR,  CO }, /* C9 */
    { "JP Z,@",        10,  0, FN,   RF,      0,       TJC, CO }, /* CA */
    { NULL,             0,  0, FN,   0,       0,       TN,  CO }, /* CB - prefix */
    { "CALL Z,@",      10, 17, FN,   RSP|RF,  RSP,     TCC, CO }, /* CC */
    { "CALL @",        17,  0, FN,   RSP,     RSP,     TC,  CO }, /* CD */
    { "ADC A,#",        7,  0, FA,   RA|RF,   RA|RF,   TN,  CO }, /* CE */
    { "RST 08",        11,  0, FN,   RSP,     RSP,     TRST,CO }, /* CF */

    /* D0-DF */
    { "RET NC",         5, 11, FN,   RSP|RF,  RSP,     TRC, CO }, /* D0 */
    { "POP DE",        10,  0, FN,   RSP,     RDE|RSP, TN,  CO }, /* D1 */
    { "JP NC,@",       10,  0, FN,   RF,      0,       TJC, CO }, /* D2 */
    { "OUT (#),A",     11,  0, FN,   RA,      0,       TN,  CO }, /* D3 */
    { "CALL NC,@",     10, 17, FN,   RSP|RF,  RSP,     TCC, CO }, /* D4 */
    { "PUSH DE",       11,  0, FN,   RDE|RSP, RSP,     TN,  CO }, /* D5 */
    { "SUB #",          7,  0, FA,   RA,      RA|RF,   TN,  CO }, /* D6 */
    { "RST 10",        11,  0, FN,   RSP,     RSP,     TRST,CO }, /* D7 */
    { "RET C",          5, 11, FN,   RSP|RF,  RSP,     TRC, CO }, /* D8 */
    { "EXX",            4,  0, FN,   RBC|RDE|RHL, RBC|RDE|RHL, TN, CO }, /* D9 */
    { "JP C,@",        10,  0, FN,   RF,      0,       TJC, CO }, /* DA */
    { "IN A,(#)",      11,  0, FN,   RA,      RA,      TN,  CO }, /* DB */
    { "CALL C,@",      10, 17, FN,   RSP|RF,  RSP,     TCC, CO }, /* DC */
    { NULL,             0,  0, FN,   0,       0,       TN,  CO }, /* DD - prefix */
    { "SBC A,#",        7,  0, FA,   RA|RF,   RA|RF,   TN,  CO }, /* DE */
    { "RST 18",        11,  0, FN,   RSP,     RSP,     TRST,CO }, /* DF */

    /* E0-EF */
    { "RET PO",         5, 11, FN,   RSP|RF,  RSP,     TRC, CO }, /* E0 */
    { "POP HL",        10,  0, FN,   RSP,     RHL|RSP, TN,  CO }, /* E1 */
    { "JP PO,@",       10,  0, FN,   RF,      0,       TJC, CO }, /* E2 */
    { "EX (SP),HL",    19,  0, FN,   RSP|RHL, RHL,     TN,  CO }, /* E3 */
    { "CALL PO,@",     10, 17, FN,   RSP|RF,  RSP,     TCC, CO }, /* E4 */
    { "PUSH HL",       11,  0, FN,   RHL|RSP, RSP,     TN,  CO }, /* E5 */
    { "AND #",          7,  0, FA,   RA,      RA|RF,   TN,  CO }, /* E6 */
    { "RST 20",        11,  0, FN,   RSP,     RSP,     TRST,CO }, /* E7 */
    { "RET PE",         5, 11, FN,   RSP|RF,  RSP,     TRC, CO }, /* E8 */
    { "JP (HL)",        4,  0, FN,   RHL,     0,       TJI, CO }, /* E9 */
    { "JP PE,@",       10,  0, FN,   RF,      0,       TJC, CO }, /* EA */
    { "EX DE,HL",       4,  0, FN,   RDE|RHL, RDE|RHL, TN,  CO }, /* EB */
    { "CALL PE,@",     10, 17, FN,   RSP|RF,  RSP,     TCC, CO }, /* EC */
    { NULL,             0,  0, FN,   0,       0,       TN,  CO }, /* ED - prefix */
    { "XOR #",          7,  0, FA,   RA,      RA|RF,   TN,  CO }, /* EE */
    { "RST 28",        11,  0, FN,   RSP,     RSP,     TRST,CO }, /* EF */

    /* F0-FF */
    { "RET P",          5, 11, FN,   RSP|RF,  RSP,     TRC, CO }, /* F0 */
    { "POP AF",        10,  0, FN,   RSP,     RAF|RSP, TN,  CO }, /* F1 */
    { "JP P,@",        10,  0, FN,   RF,      0,       TJC, CO }, /* F2 */
    { "DI",             4,  0, FN,   0,       0,       TN,  CO }, /* F3 */
    { "CALL P,@",      10, 17, FN,   RSP|RF,  RSP,     TCC, CO }, /* F4 */
    { "PUSH AF",       11,  0, FN,   RAF|RSP, RSP,     TN,  CO }, /* F5 */
    { "OR #",           7,  0, FA,   RA,      RA|RF,   TN,  CO }, /* F6 */
    { "RST 30",        11,  0, FN,   RSP,     RSP,     TRST,CO }, /* F7 */
    { "RET M",          5, 11, FN,   RSP|RF,  RSP,     TRC, CO }, /* F8 */
    { "LD SP,HL",       6,  0, FN,   RHL,     RSP,     TN,  CO }, /* F9 */
    { "JP M,@",        10,  0, FN,   RF,      0,       TJC, CO }, /* FA */
    { "EI",             4,  0, FN,   0,       0,       TN,  CO }, /* FB */
    { "CALL M,@",      10, 17, FN,   RSP|RF,  RSP,     TCC, CO }, /* FC */
    { NULL,             0,  0, FN,   0,       0,       TN,  CO }, /* FD - prefix */
    { "CP #",           7,  0, FA,   RA,      RF,      TN,  CO }, /* FE */
    { "RST 38",        11,  0, FN,   RSP,     RSP,     TRST,CO }, /* FF */
};

/* ======================================================================
 * CB prefix - bitove operace a rotace
 *
 * Struktura: 00-07 RLC, 08-0F RRC, 10-17 RL, 18-1F RR,
 *            20-27 SLA, 28-2F SRA, 30-37 SLL*, 38-3F SRL,
 *            40-7F BIT, 80-BF RES, C0-FF SET
 *
 * Poradi registru v kazde osmici: B,C,D,E,H,L,(HL),A
 * *SLL (30-37) je nedokumentovana instrukce
 * ====================================================================== */

/* Makra pro CB rotace/shifty: 8T pro registry, 15T pro (HL) */
#define CB_R(mn, r, rr, rw) { mn " " r,          8, 0, FA, rr, rw|RF, TN, CO }
#define CB_M(mn)            { mn " (HL)",        15, 0, FA, RHL, RF,   TN, CO }
#define CB_RU(mn, r, rr, rw) { mn " " r,         8, 0, FA, rr, rw|RF, TN, CU }
#define CB_MU(mn)            { mn " (HL)",       15, 0, FA, RHL, RF,   TN, CU }

/* Makra pro BIT: 8T pro registry, 12T pro (HL), nemeni C */
#define CB_BIT_R(n, r, rr) { "BIT " n "," r,     8, 0, FNC, rr, RF, TN, CO }
#define CB_BIT_M(n)        { "BIT " n ",(HL)",   12, 0, FNC, RHL, RF, TN, CO }

/* Makra pro RES/SET: 8T pro registry, 15T pro (HL), zadne flagy */
#define CB_RS_R(mn, n, r, rr, rw) { mn " " n "," r,    8, 0, FN, rr, rw, TN, CO }
#define CB_RS_M(mn, n)            { mn " " n ",(HL)",  15, 0, FN, RHL, 0,  TN, CO }

/* Sada 8 instrukci pro jednu operaci (B,C,D,E,H,L,(HL),A) */
#define CB_SHIFT_SET(mn) \
    CB_R(mn, "B", RB, RB), CB_R(mn, "C", RC, RC), \
    CB_R(mn, "D", RD, RD), CB_R(mn, "E", RE, RE), \
    CB_R(mn, "H", RH, RH), CB_R(mn, "L", RL_, RL_), \
    CB_M(mn), CB_R(mn, "A", RA, RA)

#define CB_SHIFT_SET_U(mn) \
    CB_RU(mn, "B", RB, RB), CB_RU(mn, "C", RC, RC), \
    CB_RU(mn, "D", RD, RD), CB_RU(mn, "E", RE, RE), \
    CB_RU(mn, "H", RH, RH), CB_RU(mn, "L", RL_, RL_), \
    CB_MU(mn), CB_RU(mn, "A", RA, RA)

#define CB_BIT_SET(n) \
    CB_BIT_R(n, "B", RB), CB_BIT_R(n, "C", RC), \
    CB_BIT_R(n, "D", RD), CB_BIT_R(n, "E", RE), \
    CB_BIT_R(n, "H", RH), CB_BIT_R(n, "L", RL_), \
    CB_BIT_M(n), CB_BIT_R(n, "A", RA)

#define CB_RES_SET(n) \
    CB_RS_R("RES", n, "B", RB, RB), CB_RS_R("RES", n, "C", RC, RC), \
    CB_RS_R("RES", n, "D", RD, RD), CB_RS_R("RES", n, "E", RE, RE), \
    CB_RS_R("RES", n, "H", RH, RH), CB_RS_R("RES", n, "L", RL_, RL_), \
    CB_RS_M("RES", n), CB_RS_R("RES", n, "A", RA, RA)

#define CB_SET_SET(n) \
    CB_RS_R("SET", n, "B", RB, RB), CB_RS_R("SET", n, "C", RC, RC), \
    CB_RS_R("SET", n, "D", RD, RD), CB_RS_R("SET", n, "E", RE, RE), \
    CB_RS_R("SET", n, "H", RH, RH), CB_RS_R("SET", n, "L", RL_, RL_), \
    CB_RS_M("SET", n), CB_RS_R("SET", n, "A", RA, RA)

const z80_dasm_opc_t z80_dasm_cb[256] = {
    /* 00-3F: rotace a shifty */
    CB_SHIFT_SET("RLC"),  /* 00-07 */
    CB_SHIFT_SET("RRC"),  /* 08-0F */
    CB_SHIFT_SET("RL"),   /* 10-17 */
    CB_SHIFT_SET("RR"),   /* 18-1F */
    CB_SHIFT_SET("SLA"),  /* 20-27 */
    CB_SHIFT_SET("SRA"),  /* 28-2F */
    CB_SHIFT_SET_U("SLL"),/* 30-37 (undocumented) */
    CB_SHIFT_SET("SRL"),  /* 38-3F */

    /* 40-7F: BIT */
    CB_BIT_SET("0"),      /* 40-47 */
    CB_BIT_SET("1"),      /* 48-4F */
    CB_BIT_SET("2"),      /* 50-57 */
    CB_BIT_SET("3"),      /* 58-5F */
    CB_BIT_SET("4"),      /* 60-67 */
    CB_BIT_SET("5"),      /* 68-6F */
    CB_BIT_SET("6"),      /* 70-77 */
    CB_BIT_SET("7"),      /* 78-7F */

    /* 80-BF: RES */
    CB_RES_SET("0"),      /* 80-87 */
    CB_RES_SET("1"),      /* 88-8F */
    CB_RES_SET("2"),      /* 90-97 */
    CB_RES_SET("3"),      /* 98-9F */
    CB_RES_SET("4"),      /* A0-A7 */
    CB_RES_SET("5"),      /* A8-AF */
    CB_RES_SET("6"),      /* B0-B7 */
    CB_RES_SET("7"),      /* B8-BF */

    /* C0-FF: SET */
    CB_SET_SET("0"),      /* C0-C7 */
    CB_SET_SET("1"),      /* C8-CF */
    CB_SET_SET("2"),      /* D0-D7 */
    CB_SET_SET("3"),      /* D8-DF */
    CB_SET_SET("4"),      /* E0-E7 */
    CB_SET_SET("5"),      /* E8-EF */
    CB_SET_SET("6"),      /* F0-F7 */
    CB_SET_SET("7"),      /* F8-FF */
};

/* ======================================================================
 * ED prefix - rozsirene instrukce
 * ====================================================================== */

const z80_dasm_opc_t z80_dasm_ed[256] = {
    /* 00-3F: vsechny neplatne */
    INV, INV, INV, INV, INV, INV, INV, INV, /* 00-07 */
    INV, INV, INV, INV, INV, INV, INV, INV, /* 08-0F */
    INV, INV, INV, INV, INV, INV, INV, INV, /* 10-17 */
    INV, INV, INV, INV, INV, INV, INV, INV, /* 18-1F */
    INV, INV, INV, INV, INV, INV, INV, INV, /* 20-27 */
    INV, INV, INV, INV, INV, INV, INV, INV, /* 28-2F */
    INV, INV, INV, INV, INV, INV, INV, INV, /* 30-37 */
    INV, INV, INV, INV, INV, INV, INV, INV, /* 38-3F */

    /* 40-7F: I/O, 16bit aritmetika, LD specialni */
    { "IN B,(C)",      12,  0, FNC,  RBC,     RB|RF,   TN,  CO }, /* 40 */
    { "OUT (C),B",     12,  0, FN,   RBC|RB,  0,       TN,  CO }, /* 41 */
    { "SBC HL,BC",     15,  0, FA,   RHL|RBC|RF, RHL|RF, TN, CO }, /* 42 */
    { "LD (@),BC",     20,  0, FN,   RBC,     0,       TN,  CO }, /* 43 */
    { "NEG",            8,  0, FA,   RA,      RA|RF,   TN,  CO }, /* 44 */
    { "RETN",          14,  0, FN,   RSP,     RSP,     TRN, CO }, /* 45 */
    { "IM 0",           8,  0, FN,   0,       0,       TN,  CO }, /* 46 */
    { "LD I,A",         9,  0, FN,   RA,      RI,      TN,  CO }, /* 47 */
    { "IN C,(C)",      12,  0, FNC,  RBC,     RC|RF,   TN,  CO }, /* 48 */
    { "OUT (C),C",     12,  0, FN,   RBC|RC,  0,       TN,  CO }, /* 49 */
    { "ADC HL,BC",     15,  0, FA,   RHL|RBC|RF, RHL|RF, TN, CO }, /* 4A */
    { "LD BC,(@)",     20,  0, FN,   0,       RBC,     TN,  CO }, /* 4B */
    { "NEG",            8,  0, FA,   RA,      RA|RF,   TN,  CU }, /* 4C: undoc mirror */
    { "RETI",          14,  0, FN,   RSP,     RSP,     TRI, CO }, /* 4D */
    { "IM 0",           8,  0, FN,   0,       0,       TN,  CU }, /* 4E: undoc mirror */
    { "LD R,A",         9,  0, FN,   RA,      RR_,     TN,  CO }, /* 4F */

    { "IN D,(C)",      12,  0, FNC,  RBC,     RD|RF,   TN,  CO }, /* 50 */
    { "OUT (C),D",     12,  0, FN,   RBC|RD,  0,       TN,  CO }, /* 51 */
    { "SBC HL,DE",     15,  0, FA,   RHL|RDE|RF, RHL|RF, TN, CO }, /* 52 */
    { "LD (@),DE",     20,  0, FN,   RDE,     0,       TN,  CO }, /* 53 */
    { "NEG",            8,  0, FA,   RA,      RA|RF,   TN,  CU }, /* 54: undoc mirror */
    { "RETN",          14,  0, FN,   RSP,     RSP,     TRN, CU }, /* 55: undoc mirror */
    { "IM 1",           8,  0, FN,   0,       0,       TN,  CO }, /* 56 */
    { "LD A,I",         9,  0, FNC,  RI,      RA|RF,   TN,  CO }, /* 57 */
    { "IN E,(C)",      12,  0, FNC,  RBC,     RE|RF,   TN,  CO }, /* 58 */
    { "OUT (C),E",     12,  0, FN,   RBC|RE,  0,       TN,  CO }, /* 59 */
    { "ADC HL,DE",     15,  0, FA,   RHL|RDE|RF, RHL|RF, TN, CO }, /* 5A */
    { "LD DE,(@)",     20,  0, FN,   0,       RDE,     TN,  CO }, /* 5B */
    { "NEG",            8,  0, FA,   RA,      RA|RF,   TN,  CU }, /* 5C: undoc mirror */
    { "RETN",          14,  0, FN,   RSP,     RSP,     TRN, CU }, /* 5D: undoc mirror */
    { "IM 2",           8,  0, FN,   0,       0,       TN,  CO }, /* 5E */
    { "LD A,R",         9,  0, FNC,  RR_,     RA|RF,   TN,  CO }, /* 5F */

    { "IN H,(C)",      12,  0, FNC,  RBC,     RH|RF,   TN,  CO }, /* 60 */
    { "OUT (C),H",     12,  0, FN,   RBC|RH,  0,       TN,  CO }, /* 61 */
    { "SBC HL,HL",     15,  0, FA,   RHL|RF,  RHL|RF,  TN,  CO }, /* 62 */
    { "LD (@),HL",     20,  0, FN,   RHL,     0,       TN,  CO }, /* 63 */
    { "NEG",            8,  0, FA,   RA,      RA|RF,   TN,  CU }, /* 64: undoc mirror */
    { "RETN",          14,  0, FN,   RSP,     RSP,     TRN, CU }, /* 65: undoc mirror */
    { "IM 0",           8,  0, FN,   0,       0,       TN,  CU }, /* 66: undoc mirror */
    { "RRD",           18,  0, FNC,  RA|RHL,  RA|RF,   TN,  CO }, /* 67 */
    { "IN L,(C)",      12,  0, FNC,  RBC,     RL_|RF,  TN,  CO }, /* 68 */
    { "OUT (C),L",     12,  0, FN,   RBC|RL_, 0,       TN,  CO }, /* 69 */
    { "ADC HL,HL",     15,  0, FA,   RHL|RF,  RHL|RF,  TN,  CO }, /* 6A */
    { "LD HL,(@)",     20,  0, FN,   0,       RHL,     TN,  CO }, /* 6B */
    { "NEG",            8,  0, FA,   RA,      RA|RF,   TN,  CU }, /* 6C: undoc mirror */
    { "RETN",          14,  0, FN,   RSP,     RSP,     TRN, CU }, /* 6D: undoc mirror */
    { "IM 0",           8,  0, FN,   0,       0,       TN,  CU }, /* 6E: undoc mirror */
    { "RLD",           18,  0, FNC,  RA|RHL,  RA|RF,   TN,  CO }, /* 6F */

    { "IN F,(C)",      12,  0, FNC,  RBC,     RF,      TN,  CU }, /* 70: undoc IN (C) */
    { "OUT (C),0",     12,  0, FN,   RBC,     0,       TN,  CU }, /* 71: undoc OUT (C),0 */
    { "SBC HL,SP",     15,  0, FA,   RHL|RSP|RF, RHL|RF, TN, CO }, /* 72 */
    { "LD (@),SP",     20,  0, FN,   RSP,     0,       TN,  CO }, /* 73 */
    { "NEG",            8,  0, FA,   RA,      RA|RF,   TN,  CU }, /* 74: undoc mirror */
    { "RETN",          14,  0, FN,   RSP,     RSP,     TRN, CU }, /* 75: undoc mirror */
    { "IM 1",           8,  0, FN,   0,       0,       TN,  CU }, /* 76: undoc mirror */
    INV,                                                            /* 77: neplatna */
    { "IN A,(C)",      12,  0, FNC,  RBC,     RA|RF,   TN,  CO }, /* 78 */
    { "OUT (C),A",     12,  0, FN,   RBC|RA,  0,       TN,  CO }, /* 79 */
    { "ADC HL,SP",     15,  0, FA,   RHL|RSP|RF, RHL|RF, TN, CO }, /* 7A */
    { "LD SP,(@)",     20,  0, FN,   0,       RSP,     TN,  CO }, /* 7B */
    { "NEG",            8,  0, FA,   RA,      RA|RF,   TN,  CU }, /* 7C: undoc mirror */
    { "RETN",          14,  0, FN,   RSP,     RSP,     TRN, CU }, /* 7D: undoc mirror */
    { "IM 2",           8,  0, FN,   0,       0,       TN,  CU }, /* 7E: undoc mirror */
    INV,                                                            /* 7F: neplatna */

    /* 80-9F: vsechny neplatne */
    INV, INV, INV, INV, INV, INV, INV, INV, /* 80-87 */
    INV, INV, INV, INV, INV, INV, INV, INV, /* 88-8F */
    INV, INV, INV, INV, INV, INV, INV, INV, /* 90-97 */
    INV, INV, INV, INV, INV, INV, INV, INV, /* 98-9F */

    /* A0-AF: blokove instrukce */
    { "LDI",           16,  0, FNC & ~Z80_FLAG_S & ~Z80_FLAG_Z, RA|RHL|RDE|RBC, RHL|RDE|RBC|RF, TN, CO }, /* A0 */
    { "CPI",           16,  0, FNC,  RA|RHL|RBC, RHL|RBC|RF, TN, CO }, /* A1 */
    { "INI",           16,  0, FNC,  RBC|RHL, RB|RHL|RF, TN, CO }, /* A2 */
    { "OUTI",          16,  0, FNC,  RBC|RHL, RB|RHL|RF, TN, CO }, /* A3 */
    INV, INV, INV, INV,                                             /* A4-A7 */
    { "LDD",           16,  0, FNC & ~Z80_FLAG_S & ~Z80_FLAG_Z, RA|RHL|RDE|RBC, RHL|RDE|RBC|RF, TN, CO }, /* A8 */
    { "CPD",           16,  0, FNC,  RA|RHL|RBC, RHL|RBC|RF, TN, CO }, /* A9 */
    { "IND",           16,  0, FNC,  RBC|RHL, RB|RHL|RF, TN, CO }, /* AA */
    { "OUTD",          16,  0, FNC,  RBC|RHL, RB|RHL|RF, TN, CO }, /* AB */
    INV, INV, INV, INV,                                             /* AC-AF */

    /* B0-BF: blokove instrukce s opakovanim */
    { "LDIR",          16, 21, FNC & ~Z80_FLAG_S & ~Z80_FLAG_Z, RA|RHL|RDE|RBC, RHL|RDE|RBC|RF, TN, CO }, /* B0 */
    { "CPIR",          16, 21, FNC, RA|RHL|RBC, RHL|RBC|RF, TN, CO }, /* B1 */
    { "INIR",          16, 21, FNC, RBC|RHL, RB|RHL|RF, TN, CO }, /* B2 */
    { "OTIR",          16, 21, FNC, RBC|RHL, RB|RHL|RF, TN, CO }, /* B3 */
    INV, INV, INV, INV,                                             /* B4-B7 */
    { "LDDR",          16, 21, FNC & ~Z80_FLAG_S & ~Z80_FLAG_Z, RA|RHL|RDE|RBC, RHL|RDE|RBC|RF, TN, CO }, /* B8 */
    { "CPDR",          16, 21, FNC, RA|RHL|RBC, RHL|RBC|RF, TN, CO }, /* B9 */
    { "INDR",          16, 21, FNC, RBC|RHL, RB|RHL|RF, TN, CO }, /* BA */
    { "OTDR",          16, 21, FNC, RBC|RHL, RB|RHL|RF, TN, CO }, /* BB */
    INV, INV, INV, INV,                                             /* BC-BF */

    /* C0-FF: vsechny neplatne */
    INV, INV, INV, INV, INV, INV, INV, INV, /* C0-C7 */
    INV, INV, INV, INV, INV, INV, INV, INV, /* C8-CF */
    INV, INV, INV, INV, INV, INV, INV, INV, /* D0-D7 */
    INV, INV, INV, INV, INV, INV, INV, INV, /* D8-DF */
    INV, INV, INV, INV, INV, INV, INV, INV, /* E0-E7 */
    INV, INV, INV, INV, INV, INV, INV, INV, /* E8-EF */
    INV, INV, INV, INV, INV, INV, INV, INV, /* F0-F7 */
    INV, INV, INV, INV, INV, INV, INV, INV, /* F8-FF */
};

/* ======================================================================
 * DD prefix - IX operace
 *
 * Vetsina zaznamu je NULL (fallback na base tabulku).
 * Pouze instrukce specifike pro IX maji platny zaznam.
 * T-stavy zahrnuji DD prefix (+4T oproti base).
 * ====================================================================== */

const z80_dasm_opc_t z80_dasm_dd[256] = {
    /* 00-08 */
    INV, INV, INV, INV, INV, INV, INV, INV, /* 00-07 */
    INV,                                                                    /* 08 */
    { "ADD IX,BC",     15,  0, FCHN, RIX|RBC, RIX|RF,   TN, CO },         /* 09 */
    INV, INV, INV, INV, INV, INV,                                           /* 0A-0F */

    INV, INV, INV, INV, INV, INV, INV, INV, INV,                           /* 10-18 */
    { "ADD IX,DE",     15,  0, FCHN, RIX|RDE, RIX|RF,   TN, CO },         /* 19 */
    INV, INV, INV, INV, INV, INV,                                           /* 1A-1F */

    INV,                                                                    /* 20 */
    { "LD IX,@",       14,  0, FN,   0,       RIX,      TN, CO },         /* 21 */
    { "LD (@),IX",     20,  0, FN,   RIX,     0,        TN, CO },         /* 22 */
    { "INC IX",        10,  0, FN,   RIX,     RIX,      TN, CO },         /* 23 */
    { "INC IXH",        8,  0, FNC,  RIXH,    RIXH|RF,  TN, CU },        /* 24 */
    { "DEC IXH",        8,  0, FNC,  RIXH,    RIXH|RF,  TN, CU },        /* 25 */
    { "LD IXH,#",      11,  0, FN,   0,       RIXH,     TN, CU },        /* 26 */
    INV,                                                                    /* 27 */
    INV,                                                                    /* 28 */
    { "ADD IX,IX",     15,  0, FCHN, RIX,     RIX|RF,   TN, CO },         /* 29 */
    { "LD IX,(@)",     20,  0, FN,   0,       RIX,      TN, CO },         /* 2A */
    { "DEC IX",        10,  0, FN,   RIX,     RIX,      TN, CO },         /* 2B */
    { "INC IXL",        8,  0, FNC,  RIXL,    RIXL|RF,  TN, CU },        /* 2C */
    { "DEC IXL",        8,  0, FNC,  RIXL,    RIXL|RF,  TN, CU },        /* 2D */
    { "LD IXL,#",      11,  0, FN,   0,       RIXL,     TN, CU },        /* 2E */
    INV,                                                                    /* 2F */

    INV, INV, INV, INV,                                                     /* 30-33 */
    { "INC (IX+$)",    23,  0, FNC,  RIX,     RF,       TN, CO },         /* 34 */
    { "DEC (IX+$)",    23,  0, FNC,  RIX,     RF,       TN, CO },         /* 35 */
    { "LD (IX+$),#",   19,  0, FN,   RIX,     0,        TN, CO },         /* 36 */
    INV, INV,                                                               /* 37-38 */
    { "ADD IX,SP",     15,  0, FCHN, RIX|RSP, RIX|RF,   TN, CO },         /* 39 */
    INV, INV, INV, INV, INV, INV,                                           /* 3A-3F */

    INV, INV, INV, INV,                                                     /* 40-43 */
    { "LD B,IXH",      8,  0, FN,   RIXH,    RB,       TN, CU },         /* 44 */
    { "LD B,IXL",      8,  0, FN,   RIXL,    RB,       TN, CU },         /* 45 */
    { "LD B,(IX+$)",   19,  0, FN,   RIX,     RB,       TN, CO },         /* 46 */
    INV,                                                                    /* 47 */
    INV, INV, INV, INV,                                                     /* 48-4B */
    { "LD C,IXH",      8,  0, FN,   RIXH,    RC,       TN, CU },         /* 4C */
    { "LD C,IXL",      8,  0, FN,   RIXL,    RC,       TN, CU },         /* 4D */
    { "LD C,(IX+$)",   19,  0, FN,   RIX,     RC,       TN, CO },         /* 4E */
    INV,                                                                    /* 4F */

    INV, INV, INV, INV,                                                     /* 50-53 */
    { "LD D,IXH",      8,  0, FN,   RIXH,    RD,       TN, CU },         /* 54 */
    { "LD D,IXL",      8,  0, FN,   RIXL,    RD,       TN, CU },         /* 55 */
    { "LD D,(IX+$)",   19,  0, FN,   RIX,     RD,       TN, CO },         /* 56 */
    INV,                                                                    /* 57 */
    INV, INV, INV, INV,                                                     /* 58-5B */
    { "LD E,IXH",      8,  0, FN,   RIXH,    RE,       TN, CU },         /* 5C */
    { "LD E,IXL",      8,  0, FN,   RIXL,    RE,       TN, CU },         /* 5D */
    { "LD E,(IX+$)",   19,  0, FN,   RIX,     RE,       TN, CO },         /* 5E */
    INV,                                                                    /* 5F */

    { "LD IXH,B",      8,  0, FN,   RB,      RIXH,     TN, CU },         /* 60 */
    { "LD IXH,C",      8,  0, FN,   RC,      RIXH,     TN, CU },         /* 61 */
    { "LD IXH,D",      8,  0, FN,   RD,      RIXH,     TN, CU },         /* 62 */
    { "LD IXH,E",      8,  0, FN,   RE,      RIXH,     TN, CU },         /* 63 */
    { "LD IXH,IXH",    8,  0, FN,   RIXH,    RIXH,     TN, CU },         /* 64 */
    { "LD IXH,IXL",    8,  0, FN,   RIXL,    RIXH,     TN, CU },         /* 65 */
    { "LD H,(IX+$)",   19,  0, FN,   RIX,     RH,       TN, CO },         /* 66 */
    { "LD IXH,A",      8,  0, FN,   RA,      RIXH,     TN, CU },         /* 67 */
    { "LD IXL,B",      8,  0, FN,   RB,      RIXL,     TN, CU },         /* 68 */
    { "LD IXL,C",      8,  0, FN,   RC,      RIXL,     TN, CU },         /* 69 */
    { "LD IXL,D",      8,  0, FN,   RD,      RIXL,     TN, CU },         /* 6A */
    { "LD IXL,E",      8,  0, FN,   RE,      RIXL,     TN, CU },         /* 6B */
    { "LD IXL,IXH",    8,  0, FN,   RIXH,    RIXL,     TN, CU },         /* 6C */
    { "LD IXL,IXL",    8,  0, FN,   RIXL,    RIXL,     TN, CU },         /* 6D */
    { "LD L,(IX+$)",   19,  0, FN,   RIX,     RL_,      TN, CO },         /* 6E */
    { "LD IXL,A",      8,  0, FN,   RA,      RIXL,     TN, CU },         /* 6F */

    { "LD (IX+$),B",   19,  0, FN,   RIX|RB,  0,        TN, CO },         /* 70 */
    { "LD (IX+$),C",   19,  0, FN,   RIX|RC,  0,        TN, CO },         /* 71 */
    { "LD (IX+$),D",   19,  0, FN,   RIX|RD,  0,        TN, CO },         /* 72 */
    { "LD (IX+$),E",   19,  0, FN,   RIX|RE,  0,        TN, CO },         /* 73 */
    { "LD (IX+$),H",   19,  0, FN,   RIX|RH,  0,        TN, CO },         /* 74 */
    { "LD (IX+$),L",   19,  0, FN,   RIX|RL_, 0,        TN, CO },         /* 75 */
    INV,                                                                    /* 76 */
    { "LD (IX+$),A",   19,  0, FN,   RIX|RA,  0,        TN, CO },         /* 77 */
    INV, INV, INV, INV,                                                     /* 78-7B */
    { "LD A,IXH",      8,  0, FN,   RIXH,    RA,       TN, CU },         /* 7C */
    { "LD A,IXL",      8,  0, FN,   RIXL,    RA,       TN, CU },         /* 7D */
    { "LD A,(IX+$)",   19,  0, FN,   RIX,     RA,       TN, CO },         /* 7E */
    INV,                                                                    /* 7F */

    INV, INV, INV, INV,                                                     /* 80-83 */
    { "ADD A,IXH",      8,  0, FA,   RA|RIXH, RA|RF,   TN, CU },         /* 84 */
    { "ADD A,IXL",      8,  0, FA,   RA|RIXL, RA|RF,   TN, CU },         /* 85 */
    { "ADD A,(IX+$)",  19,  0, FA,   RA|RIX,  RA|RF,    TN, CO },         /* 86 */
    INV,                                                                    /* 87 */
    INV, INV, INV, INV,                                                     /* 88-8B */
    { "ADC A,IXH",      8,  0, FA,   RA|RIXH|RF, RA|RF, TN, CU },        /* 8C */
    { "ADC A,IXL",      8,  0, FA,   RA|RIXL|RF, RA|RF, TN, CU },        /* 8D */
    { "ADC A,(IX+$)",  19,  0, FA,   RA|RIX|RF, RA|RF,  TN, CO },         /* 8E */
    INV,                                                                    /* 8F */

    INV, INV, INV, INV,                                                     /* 90-93 */
    { "SUB IXH",        8,  0, FA,   RA|RIXH, RA|RF,   TN, CU },         /* 94 */
    { "SUB IXL",        8,  0, FA,   RA|RIXL, RA|RF,   TN, CU },         /* 95 */
    { "SUB (IX+$)",    19,  0, FA,   RA|RIX,  RA|RF,    TN, CO },         /* 96 */
    INV,                                                                    /* 97 */
    INV, INV, INV, INV,                                                     /* 98-9B */
    { "SBC A,IXH",      8,  0, FA,   RA|RIXH|RF, RA|RF, TN, CU },        /* 9C */
    { "SBC A,IXL",      8,  0, FA,   RA|RIXL|RF, RA|RF, TN, CU },        /* 9D */
    { "SBC A,(IX+$)",  19,  0, FA,   RA|RIX|RF, RA|RF,  TN, CO },         /* 9E */
    INV,                                                                    /* 9F */

    INV, INV, INV, INV,                                                     /* A0-A3 */
    { "AND IXH",        8,  0, FA,   RA|RIXH, RA|RF,   TN, CU },         /* A4 */
    { "AND IXL",        8,  0, FA,   RA|RIXL, RA|RF,   TN, CU },         /* A5 */
    { "AND (IX+$)",    19,  0, FA,   RA|RIX,  RA|RF,    TN, CO },         /* A6 */
    INV,                                                                    /* A7 */
    INV, INV, INV, INV,                                                     /* A8-AB */
    { "XOR IXH",        8,  0, FA,   RA|RIXH, RA|RF,   TN, CU },         /* AC */
    { "XOR IXL",        8,  0, FA,   RA|RIXL, RA|RF,   TN, CU },         /* AD */
    { "XOR (IX+$)",    19,  0, FA,   RA|RIX,  RA|RF,    TN, CO },         /* AE */
    INV,                                                                    /* AF */

    INV, INV, INV, INV,                                                     /* B0-B3 */
    { "OR IXH",         8,  0, FA,   RA|RIXH, RA|RF,   TN, CU },         /* B4 */
    { "OR IXL",         8,  0, FA,   RA|RIXL, RA|RF,   TN, CU },         /* B5 */
    { "OR (IX+$)",     19,  0, FA,   RA|RIX,  RA|RF,    TN, CO },         /* B6 */
    INV,                                                                    /* B7 */
    INV, INV, INV, INV,                                                     /* B8-BB */
    { "CP IXH",         8,  0, FA,   RA|RIXH, RF,      TN, CU },         /* BC */
    { "CP IXL",         8,  0, FA,   RA|RIXL, RF,      TN, CU },         /* BD */
    { "CP (IX+$)",     19,  0, FA,   RA|RIX,  RF,       TN, CO },         /* BE */
    INV,                                                                    /* BF */

    INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,                 /* C0-CA */
    INV,                                                                    /* CB -> DDCB */
    INV, INV, INV, INV,                                                     /* CC-CF */

    INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,           /* D0-DB */
    INV, INV, INV, INV,                                                     /* DC-DF */

    INV,                                                                    /* E0 */
    { "POP IX",        14,  0, FN,   RSP,     RIX|RSP,  TN, CO },         /* E1 */
    INV,                                                                    /* E2 */
    { "EX (SP),IX",    23,  0, FN,   RSP|RIX, RIX,      TN, CO },         /* E3 */
    INV,                                                                    /* E4 */
    { "PUSH IX",       15,  0, FN,   RIX|RSP, RSP,      TN, CO },         /* E5 */
    INV, INV, INV,                                                          /* E6-E8 */
    { "JP (IX)",        8,  0, FN,   RIX,     0,        TJI, CO },         /* E9 */
    INV, INV, INV, INV, INV, INV,                                           /* EA-EF */

    INV, INV, INV, INV, INV, INV, INV, INV, INV,                           /* F0-F8 */
    { "LD SP,IX",      10,  0, FN,   RIX,     RSP,      TN, CO },         /* F9 */
    INV, INV, INV, INV, INV, INV,                                           /* FA-FF */
};

/* ======================================================================
 * FD prefix - IY operace
 *
 * Zrcadlo DD tabulky s IX -> IY.
 * ====================================================================== */

const z80_dasm_opc_t z80_dasm_fd[256] = {
    /* 00-08 */
    INV, INV, INV, INV, INV, INV, INV, INV,
    INV,
    { "ADD IY,BC",     15,  0, FCHN, RIY|RBC, RIY|RF,   TN, CO },         /* 09 */
    INV, INV, INV, INV, INV, INV,

    INV, INV, INV, INV, INV, INV, INV, INV, INV,
    { "ADD IY,DE",     15,  0, FCHN, RIY|RDE, RIY|RF,   TN, CO },         /* 19 */
    INV, INV, INV, INV, INV, INV,

    INV,
    { "LD IY,@",       14,  0, FN,   0,       RIY,      TN, CO },         /* 21 */
    { "LD (@),IY",     20,  0, FN,   RIY,     0,        TN, CO },         /* 22 */
    { "INC IY",        10,  0, FN,   RIY,     RIY,      TN, CO },         /* 23 */
    { "INC IYH",        8,  0, FNC,  RIYH,    RIYH|RF,  TN, CU },        /* 24 */
    { "DEC IYH",        8,  0, FNC,  RIYH,    RIYH|RF,  TN, CU },        /* 25 */
    { "LD IYH,#",      11,  0, FN,   0,       RIYH,     TN, CU },        /* 26 */
    INV,
    INV,
    { "ADD IY,IY",     15,  0, FCHN, RIY,     RIY|RF,   TN, CO },         /* 29 */
    { "LD IY,(@)",     20,  0, FN,   0,       RIY,      TN, CO },         /* 2A */
    { "DEC IY",        10,  0, FN,   RIY,     RIY,      TN, CO },         /* 2B */
    { "INC IYL",        8,  0, FNC,  RIYL,    RIYL|RF,  TN, CU },        /* 2C */
    { "DEC IYL",        8,  0, FNC,  RIYL,    RIYL|RF,  TN, CU },        /* 2D */
    { "LD IYL,#",      11,  0, FN,   0,       RIYL,     TN, CU },        /* 2E */
    INV,

    INV, INV, INV, INV,
    { "INC (IY+$)",    23,  0, FNC,  RIY,     RF,       TN, CO },         /* 34 */
    { "DEC (IY+$)",    23,  0, FNC,  RIY,     RF,       TN, CO },         /* 35 */
    { "LD (IY+$),#",   19,  0, FN,   RIY,     0,        TN, CO },         /* 36 */
    INV, INV,
    { "ADD IY,SP",     15,  0, FCHN, RIY|RSP, RIY|RF,   TN, CO },         /* 39 */
    INV, INV, INV, INV, INV, INV,

    INV, INV, INV, INV,
    { "LD B,IYH",      8,  0, FN,   RIYH,    RB,       TN, CU },         /* 44 */
    { "LD B,IYL",      8,  0, FN,   RIYL,    RB,       TN, CU },         /* 45 */
    { "LD B,(IY+$)",   19,  0, FN,   RIY,     RB,       TN, CO },         /* 46 */
    INV,
    INV, INV, INV, INV,
    { "LD C,IYH",      8,  0, FN,   RIYH,    RC,       TN, CU },         /* 4C */
    { "LD C,IYL",      8,  0, FN,   RIYL,    RC,       TN, CU },         /* 4D */
    { "LD C,(IY+$)",   19,  0, FN,   RIY,     RC,       TN, CO },         /* 4E */
    INV,

    INV, INV, INV, INV,
    { "LD D,IYH",      8,  0, FN,   RIYH,    RD,       TN, CU },         /* 54 */
    { "LD D,IYL",      8,  0, FN,   RIYL,    RD,       TN, CU },         /* 55 */
    { "LD D,(IY+$)",   19,  0, FN,   RIY,     RD,       TN, CO },         /* 56 */
    INV,
    INV, INV, INV, INV,
    { "LD E,IYH",      8,  0, FN,   RIYH,    RE,       TN, CU },         /* 5C */
    { "LD E,IYL",      8,  0, FN,   RIYL,    RE,       TN, CU },         /* 5D */
    { "LD E,(IY+$)",   19,  0, FN,   RIY,     RE,       TN, CO },         /* 5E */
    INV,

    { "LD IYH,B",      8,  0, FN,   RB,      RIYH,     TN, CU },         /* 60 */
    { "LD IYH,C",      8,  0, FN,   RC,      RIYH,     TN, CU },         /* 61 */
    { "LD IYH,D",      8,  0, FN,   RD,      RIYH,     TN, CU },         /* 62 */
    { "LD IYH,E",      8,  0, FN,   RE,      RIYH,     TN, CU },         /* 63 */
    { "LD IYH,IYH",    8,  0, FN,   RIYH,    RIYH,     TN, CU },         /* 64 */
    { "LD IYH,IYL",    8,  0, FN,   RIYL,    RIYH,     TN, CU },         /* 65 */
    { "LD H,(IY+$)",   19,  0, FN,   RIY,     RH,       TN, CO },         /* 66 */
    { "LD IYH,A",      8,  0, FN,   RA,      RIYH,     TN, CU },         /* 67 */
    { "LD IYL,B",      8,  0, FN,   RB,      RIYL,     TN, CU },         /* 68 */
    { "LD IYL,C",      8,  0, FN,   RC,      RIYL,     TN, CU },         /* 69 */
    { "LD IYL,D",      8,  0, FN,   RD,      RIYL,     TN, CU },         /* 6A */
    { "LD IYL,E",      8,  0, FN,   RE,      RIYL,     TN, CU },         /* 6B */
    { "LD IYL,IYH",    8,  0, FN,   RIYH,    RIYL,     TN, CU },         /* 6C */
    { "LD IYL,IYL",    8,  0, FN,   RIYL,    RIYL,     TN, CU },         /* 6D */
    { "LD L,(IY+$)",   19,  0, FN,   RIY,     RL_,      TN, CO },         /* 6E */
    { "LD IYL,A",      8,  0, FN,   RA,      RIYL,     TN, CU },         /* 6F */

    { "LD (IY+$),B",   19,  0, FN,   RIY|RB,  0,        TN, CO },
    { "LD (IY+$),C",   19,  0, FN,   RIY|RC,  0,        TN, CO },
    { "LD (IY+$),D",   19,  0, FN,   RIY|RD,  0,        TN, CO },
    { "LD (IY+$),E",   19,  0, FN,   RIY|RE,  0,        TN, CO },
    { "LD (IY+$),H",   19,  0, FN,   RIY|RH,  0,        TN, CO },
    { "LD (IY+$),L",   19,  0, FN,   RIY|RL_, 0,        TN, CO },
    INV,
    { "LD (IY+$),A",   19,  0, FN,   RIY|RA,  0,        TN, CO },
    INV, INV, INV, INV,
    { "LD A,IYH",      8,  0, FN,   RIYH,    RA,       TN, CU },         /* 7C */
    { "LD A,IYL",      8,  0, FN,   RIYL,    RA,       TN, CU },         /* 7D */
    { "LD A,(IY+$)",   19,  0, FN,   RIY,     RA,       TN, CO },         /* 7E */
    INV,

    INV, INV, INV, INV,
    { "ADD A,IYH",      8,  0, FA,   RA|RIYH, RA|RF,   TN, CU },
    { "ADD A,IYL",      8,  0, FA,   RA|RIYL, RA|RF,   TN, CU },
    { "ADD A,(IY+$)",  19,  0, FA,   RA|RIY,  RA|RF,    TN, CO },
    INV,
    INV, INV, INV, INV,
    { "ADC A,IYH",      8,  0, FA,   RA|RIYH|RF, RA|RF, TN, CU },
    { "ADC A,IYL",      8,  0, FA,   RA|RIYL|RF, RA|RF, TN, CU },
    { "ADC A,(IY+$)",  19,  0, FA,   RA|RIY|RF, RA|RF,  TN, CO },
    INV,

    INV, INV, INV, INV,
    { "SUB IYH",        8,  0, FA,   RA|RIYH, RA|RF,   TN, CU },
    { "SUB IYL",        8,  0, FA,   RA|RIYL, RA|RF,   TN, CU },
    { "SUB (IY+$)",    19,  0, FA,   RA|RIY,  RA|RF,    TN, CO },
    INV,
    INV, INV, INV, INV,
    { "SBC A,IYH",      8,  0, FA,   RA|RIYH|RF, RA|RF, TN, CU },
    { "SBC A,IYL",      8,  0, FA,   RA|RIYL|RF, RA|RF, TN, CU },
    { "SBC A,(IY+$)",  19,  0, FA,   RA|RIY|RF, RA|RF,  TN, CO },
    INV,

    INV, INV, INV, INV,
    { "AND IYH",        8,  0, FA,   RA|RIYH, RA|RF,   TN, CU },
    { "AND IYL",        8,  0, FA,   RA|RIYL, RA|RF,   TN, CU },
    { "AND (IY+$)",    19,  0, FA,   RA|RIY,  RA|RF,    TN, CO },
    INV,
    INV, INV, INV, INV,
    { "XOR IYH",        8,  0, FA,   RA|RIYH, RA|RF,   TN, CU },
    { "XOR IYL",        8,  0, FA,   RA|RIYL, RA|RF,   TN, CU },
    { "XOR (IY+$)",    19,  0, FA,   RA|RIY,  RA|RF,    TN, CO },
    INV,

    INV, INV, INV, INV,
    { "OR IYH",         8,  0, FA,   RA|RIYH, RA|RF,   TN, CU },
    { "OR IYL",         8,  0, FA,   RA|RIYL, RA|RF,   TN, CU },
    { "OR (IY+$)",     19,  0, FA,   RA|RIY,  RA|RF,    TN, CO },
    INV,
    INV, INV, INV, INV,
    { "CP IYH",         8,  0, FA,   RA|RIYH, RF,      TN, CU },
    { "CP IYL",         8,  0, FA,   RA|RIYL, RF,      TN, CU },
    { "CP (IY+$)",     19,  0, FA,   RA|RIY,  RF,       TN, CO },
    INV,

    INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
    INV, /* CB -> FDCB */
    INV, INV, INV, INV,

    INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV, INV,
    INV, INV, INV, INV,

    INV,
    { "POP IY",        14,  0, FN,   RSP,     RIY|RSP,  TN, CO },
    INV,
    { "EX (SP),IY",    23,  0, FN,   RSP|RIY, RIY,      TN, CO },
    INV,
    { "PUSH IY",       15,  0, FN,   RIY|RSP, RSP,      TN, CO },
    INV, INV, INV,
    { "JP (IY)",        8,  0, FN,   RIY,     0,        TJI, CO },
    INV, INV, INV, INV, INV, INV,

    INV, INV, INV, INV, INV, INV, INV, INV, INV,
    { "LD SP,IY",      10,  0, FN,   RIY,     RSP,      TN, CO },
    INV, INV, INV, INV, INV, INV,
};

/* ======================================================================
 * DD CB prefix - indexovane bitove operace s IX
 *
 * Kazda instrukce je 4 bajty: DD CB d opcode.
 * Rotace/shift: 23T, BIT: 20T, RES/SET: 23T.
 *
 * Nedokumentovane varianty (ne-6 a ne-E v dolnich 3 bitech)
 * provedou operaci nad (IX+d) a vysledek zaroven ulozi do registru.
 * ====================================================================== */

/* Makra pro DDCB: rotace/shift na (IX+d) s kopirovani do registru */
#define DDCB_S(mn, r, rw)  { mn " (IX+$)," r,  23, 0, FA, RIX, rw|RF, TN, CU }
#define DDCB_SM(mn)         { mn " (IX+$)",     23, 0, FA, RIX, RF,    TN, CO }
#define DDCB_SHIFT(mn) \
    DDCB_S(mn,"B",RB), DDCB_S(mn,"C",RC), DDCB_S(mn,"D",RD), DDCB_S(mn,"E",RE), \
    DDCB_S(mn,"H",RH), DDCB_S(mn,"L",RL_), DDCB_SM(mn), DDCB_S(mn,"A",RA)

#define DDCB_SHIFT_U(mn) \
    DDCB_S(mn,"B",RB), DDCB_S(mn,"C",RC), DDCB_S(mn,"D",RD), DDCB_S(mn,"E",RE), \
    DDCB_S(mn,"H",RH), DDCB_S(mn,"L",RL_), \
    { mn " (IX+$)", 23, 0, FA, RIX, RF, TN, CU }, \
    DDCB_S(mn,"A",RA)

/* BIT na (IX+d) - vsech 8 variant se chova stejne */
#define DDCB_BIT(n) \
    { "BIT " n ",(IX+$)", 20, 0, FNC, RIX, RF, TN, CO }, \
    { "BIT " n ",(IX+$)", 20, 0, FNC, RIX, RF, TN, CO }, \
    { "BIT " n ",(IX+$)", 20, 0, FNC, RIX, RF, TN, CO }, \
    { "BIT " n ",(IX+$)", 20, 0, FNC, RIX, RF, TN, CO }, \
    { "BIT " n ",(IX+$)", 20, 0, FNC, RIX, RF, TN, CO }, \
    { "BIT " n ",(IX+$)", 20, 0, FNC, RIX, RF, TN, CO }, \
    { "BIT " n ",(IX+$)", 20, 0, FNC, RIX, RF, TN, CO }, \
    { "BIT " n ",(IX+$)", 20, 0, FNC, RIX, RF, TN, CO }

/* RES/SET na (IX+d) s kopirovani do registru */
#define DDCB_RS(mn, n, r, rw) { mn " " n ",(IX+$)," r, 23, 0, FN, RIX, rw, TN, CU }
#define DDCB_RSM(mn, n)       { mn " " n ",(IX+$)",    23, 0, FN, RIX, 0,  TN, CO }
#define DDCB_RESSET(mn, n) \
    DDCB_RS(mn,n,"B",RB), DDCB_RS(mn,n,"C",RC), DDCB_RS(mn,n,"D",RD), DDCB_RS(mn,n,"E",RE), \
    DDCB_RS(mn,n,"H",RH), DDCB_RS(mn,n,"L",RL_), DDCB_RSM(mn,n), DDCB_RS(mn,n,"A",RA)

const z80_dasm_opc_t z80_dasm_ddcb[256] = {
    DDCB_SHIFT("RLC"),    /* 00-07 */
    DDCB_SHIFT("RRC"),    /* 08-0F */
    DDCB_SHIFT("RL"),     /* 10-17 */
    DDCB_SHIFT("RR"),     /* 18-1F */
    DDCB_SHIFT("SLA"),    /* 20-27 */
    DDCB_SHIFT("SRA"),    /* 28-2F */
    DDCB_SHIFT_U("SLL"),  /* 30-37 */
    DDCB_SHIFT("SRL"),    /* 38-3F */

    DDCB_BIT("0"),        /* 40-47 */
    DDCB_BIT("1"),        /* 48-4F */
    DDCB_BIT("2"),        /* 50-57 */
    DDCB_BIT("3"),        /* 58-5F */
    DDCB_BIT("4"),        /* 60-67 */
    DDCB_BIT("5"),        /* 68-6F */
    DDCB_BIT("6"),        /* 70-77 */
    DDCB_BIT("7"),        /* 78-7F */

    DDCB_RESSET("RES","0"), /* 80-87 */
    DDCB_RESSET("RES","1"), /* 88-8F */
    DDCB_RESSET("RES","2"), /* 90-97 */
    DDCB_RESSET("RES","3"), /* 98-9F */
    DDCB_RESSET("RES","4"), /* A0-A7 */
    DDCB_RESSET("RES","5"), /* A8-AF */
    DDCB_RESSET("RES","6"), /* B0-B7 */
    DDCB_RESSET("RES","7"), /* B8-BF */

    DDCB_RESSET("SET","0"), /* C0-C7 */
    DDCB_RESSET("SET","1"), /* C8-CF */
    DDCB_RESSET("SET","2"), /* D0-D7 */
    DDCB_RESSET("SET","3"), /* D8-DF */
    DDCB_RESSET("SET","4"), /* E0-E7 */
    DDCB_RESSET("SET","5"), /* E8-EF */
    DDCB_RESSET("SET","6"), /* F0-F7 */
    DDCB_RESSET("SET","7"), /* F8-FF */
};

/* ======================================================================
 * FD CB prefix - indexovane bitove operace s IY
 *
 * Zrcadlo DDCB s IX -> IY.
 * ====================================================================== */

#define FDCB_S(mn, r, rw)  { mn " (IY+$)," r,  23, 0, FA, RIY, rw|RF, TN, CU }
#define FDCB_SM(mn)         { mn " (IY+$)",     23, 0, FA, RIY, RF,    TN, CO }
#define FDCB_SHIFT(mn) \
    FDCB_S(mn,"B",RB), FDCB_S(mn,"C",RC), FDCB_S(mn,"D",RD), FDCB_S(mn,"E",RE), \
    FDCB_S(mn,"H",RH), FDCB_S(mn,"L",RL_), FDCB_SM(mn), FDCB_S(mn,"A",RA)

#define FDCB_SHIFT_U(mn) \
    FDCB_S(mn,"B",RB), FDCB_S(mn,"C",RC), FDCB_S(mn,"D",RD), FDCB_S(mn,"E",RE), \
    FDCB_S(mn,"H",RH), FDCB_S(mn,"L",RL_), \
    { mn " (IY+$)", 23, 0, FA, RIY, RF, TN, CU }, \
    FDCB_S(mn,"A",RA)

#define FDCB_BIT(n) \
    { "BIT " n ",(IY+$)", 20, 0, FNC, RIY, RF, TN, CO }, \
    { "BIT " n ",(IY+$)", 20, 0, FNC, RIY, RF, TN, CO }, \
    { "BIT " n ",(IY+$)", 20, 0, FNC, RIY, RF, TN, CO }, \
    { "BIT " n ",(IY+$)", 20, 0, FNC, RIY, RF, TN, CO }, \
    { "BIT " n ",(IY+$)", 20, 0, FNC, RIY, RF, TN, CO }, \
    { "BIT " n ",(IY+$)", 20, 0, FNC, RIY, RF, TN, CO }, \
    { "BIT " n ",(IY+$)", 20, 0, FNC, RIY, RF, TN, CO }, \
    { "BIT " n ",(IY+$)", 20, 0, FNC, RIY, RF, TN, CO }

#define FDCB_RS(mn, n, r, rw) { mn " " n ",(IY+$)," r, 23, 0, FN, RIY, rw, TN, CU }
#define FDCB_RSM(mn, n)       { mn " " n ",(IY+$)",    23, 0, FN, RIY, 0,  TN, CO }
#define FDCB_RESSET(mn, n) \
    FDCB_RS(mn,n,"B",RB), FDCB_RS(mn,n,"C",RC), FDCB_RS(mn,n,"D",RD), FDCB_RS(mn,n,"E",RE), \
    FDCB_RS(mn,n,"H",RH), FDCB_RS(mn,n,"L",RL_), FDCB_RSM(mn,n), FDCB_RS(mn,n,"A",RA)

const z80_dasm_opc_t z80_dasm_fdcb[256] = {
    FDCB_SHIFT("RLC"),    /* 00-07 */
    FDCB_SHIFT("RRC"),    /* 08-0F */
    FDCB_SHIFT("RL"),     /* 10-17 */
    FDCB_SHIFT("RR"),     /* 18-1F */
    FDCB_SHIFT("SLA"),    /* 20-27 */
    FDCB_SHIFT("SRA"),    /* 28-2F */
    FDCB_SHIFT_U("SLL"),  /* 30-37 */
    FDCB_SHIFT("SRL"),    /* 38-3F */

    FDCB_BIT("0"),        /* 40-47 */
    FDCB_BIT("1"),        /* 48-4F */
    FDCB_BIT("2"),        /* 50-57 */
    FDCB_BIT("3"),        /* 58-5F */
    FDCB_BIT("4"),        /* 60-67 */
    FDCB_BIT("5"),        /* 68-6F */
    FDCB_BIT("6"),        /* 70-77 */
    FDCB_BIT("7"),        /* 78-7F */

    FDCB_RESSET("RES","0"),
    FDCB_RESSET("RES","1"),
    FDCB_RESSET("RES","2"),
    FDCB_RESSET("RES","3"),
    FDCB_RESSET("RES","4"),
    FDCB_RESSET("RES","5"),
    FDCB_RESSET("RES","6"),
    FDCB_RESSET("RES","7"),

    FDCB_RESSET("SET","0"),
    FDCB_RESSET("SET","1"),
    FDCB_RESSET("SET","2"),
    FDCB_RESSET("SET","3"),
    FDCB_RESSET("SET","4"),
    FDCB_RESSET("SET","5"),
    FDCB_RESSET("SET","6"),
    FDCB_RESSET("SET","7"),
};
