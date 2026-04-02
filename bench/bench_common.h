/**
 * @file bench_common.h
 * @brief Spolecny testovaci program a pomocne funkce pro benchmark Z80.
 *
 * Obsahuje Z80 testovaci program jako pole bajtu a sdilenou RAM.
 * Testovaci program provadi 256 * 65536 = 16 777 216 iteraci smycky
 * s mixem instrukci: aritmetika, logika, pamet, CB prefix, DD prefix,
 * PUSH/POP, podmineny skok.
 */

#ifndef BENCH_COMMON_H
#define BENCH_COMMON_H

#include <stdio.h>
#include <string.h>
#include <time.h>

/** 64KB RAM sdilena mezi benchmarky */
static unsigned char ram[65536];

/**
 * Z80 testovaci program - mix instrukci.
 *
 * Vnejsi smycka: IX = 256x, vnitrni: BC = 65536x = 16 777 216 iteraci.
 * Vnitrni smycka obsahuje:
 *   - pristup do pameti (LD A,(HL), LD (HL),A)
 *   - 8bit aritmetiku (ADD, SUB, AND, OR, XOR)
 *   - rotace (RLCA, RRCA)
 *   - 16bit aritmetiku (ADD HL,DE, INC HL, DEC HL)
 *   - zasobnik (PUSH AF, POP AF)
 *   - CB prefix (BIT, SET)
 *   - podmineny skok (JP NZ)
 * Vnejsi smycka pouziva DD prefix (DEC IX, LD A,IXH, OR IXL).
 */
static const unsigned char test_program[] = {
    /* 0x0000 */ 0x31, 0xFE, 0xFF,             /* LD SP, $FFFE          */
    /* 0x0003 */ 0xDD, 0x21, 0x00, 0x01,       /* LD IX, $0100          */
    /* outer: */
    /* 0x0007 */ 0x01, 0x00, 0x00,             /* LD BC, $0000 (=65536) */
    /* 0x000A */ 0x21, 0x00, 0x80,             /* LD HL, $8000          */
    /* 0x000D */ 0x11, 0x00, 0xC0,             /* LD DE, $C000          */
    /* inner: */
    /* 0x0010 */ 0x7E,                         /* LD A, (HL)            */
    /* 0x0011 */ 0xC6, 0x2A,                   /* ADD A, $2A            */
    /* 0x0013 */ 0x93,                         /* SUB E                 */
    /* 0x0014 */ 0xE6, 0xF0,                   /* AND $F0               */
    /* 0x0016 */ 0xB5,                         /* OR L                  */
    /* 0x0017 */ 0xAA,                         /* XOR D                 */
    /* 0x0018 */ 0x07,                         /* RLCA                  */
    /* 0x0019 */ 0x0F,                         /* RRCA                  */
    /* 0x001A */ 0x77,                         /* LD (HL), A            */
    /* 0x001B */ 0x23,                         /* INC HL                */
    /* 0x001C */ 0x2B,                         /* DEC HL                */
    /* 0x001D */ 0xF5,                         /* PUSH AF               */
    /* 0x001E */ 0xF1,                         /* POP AF                */
    /* 0x001F */ 0xCB, 0x5F,                   /* BIT 3, A              */
    /* 0x0021 */ 0xCB, 0xEF,                   /* SET 5, A              */
    /* 0x0023 */ 0x19,                         /* ADD HL, DE            */
    /* 0x0024 */ 0x0B,                         /* DEC BC                */
    /* 0x0025 */ 0x78,                         /* LD A, B               */
    /* 0x0026 */ 0xB1,                         /* OR C                  */
    /* 0x0027 */ 0xC2, 0x10, 0x00,             /* JP NZ, $0010          */
    /* outer dec: */
    /* 0x002A */ 0xDD, 0x2B,                   /* DEC IX                */
    /* 0x002C */ 0xDD, 0x7C,                   /* LD A, IXH             */
    /* 0x002E */ 0xDD, 0xB5,                   /* OR IXL                */
    /* 0x0030 */ 0xC2, 0x07, 0x00,             /* JP NZ, $0007          */
    /* 0x0033 */ 0x76                          /* HALT                  */
};

/**
 * Nacte testovaci program do RAM.
 *
 * Vynuluje celou RAM a nakopiruje test_program na adresu 0x0000.
 */
static void load_test_program(void) {
    memset(ram, 0, sizeof(ram));
    memcpy(ram, test_program, sizeof(test_program));
}

#endif /* BENCH_COMMON_H */
