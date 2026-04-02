/**
 * @file test_timing.c
 * @brief Testy presneho casovani (T-stavu) vsech Z80 instrukci.
 *
 * Overuje, ze kazda instrukce spotrebuje spravny pocet T-stavu
 * podle specifikace Z80A.
 */

#include "test_framework.h"

/**
 * @brief Pomocna funkce - nacte instrukci a overi T-stavy.
 * @param name Nazev testu.
 * @param code Pole bajtu instrukce.
 * @param len Delka instrukce.
 * @param expected_cycles Ocekavany pocet T-stavu.
 */
static void check_timing(const char *name, const u8 *code, int len, int expected_cycles) {
    TEST_BEGIN(name);
    setup();
    /* Nastavime registry tak, aby podminene instrukce fungovaly */
    cpu.sp = 0xFFF0;
    cpu.hl.w = 0x8000;
    cpu.bc.w = 0x0100;
    cpu.de.w = 0x2000;
    cpu.ix.w = 0x5000;
    cpu.iy.w = 0x6000;
    /* Flagy: Z=0, C=0 (podminene skoky "taken") */
    cpu.af.l = 0;
    cpu.af.h = 0x42;
    /* Na zasobniku neco rozumneho */
    test_ram[0xFFF0] = 0x00;
    test_ram[0xFFF1] = 0x01;
    /* Na (HL) neco */
    test_ram[0x8000] = 0x42;

    load_bytes(0, code, len);
    int cyc = step();
    ASSERT_CYCLES(cyc, expected_cycles);
    TEST_END();
}

/**
 * @brief Overeni T-stavu zakladnich instrukci (opkody 0x00-0x3F).
 */
static void test_timing_basic(void) {
    u8 c[4];

    c[0] = 0x00; check_timing("NOP", c, 1, 4);

    c[0] = 0x01; c[1] = 0x34; c[2] = 0x12;
    check_timing("LD BC,nn", c, 3, 10);

    c[0] = 0x02; check_timing("LD (BC),A", c, 1, 7);
    c[0] = 0x03; check_timing("INC BC", c, 1, 6);
    c[0] = 0x04; check_timing("INC B", c, 1, 4);
    c[0] = 0x05; check_timing("DEC B", c, 1, 4);

    c[0] = 0x06; c[1] = 0x42;
    check_timing("LD B,n", c, 2, 7);

    c[0] = 0x07; check_timing("RLCA", c, 1, 4);
    c[0] = 0x08; check_timing("EX AF,AF'", c, 1, 4);
    c[0] = 0x09; check_timing("ADD HL,BC", c, 1, 11);
    c[0] = 0x0A; check_timing("LD A,(BC)", c, 1, 7);
    c[0] = 0x0B; check_timing("DEC BC", c, 1, 6);
    c[0] = 0x0C; check_timing("INC C", c, 1, 4);
    c[0] = 0x0D; check_timing("DEC C", c, 1, 4);

    c[0] = 0x0E; c[1] = 0x42;
    check_timing("LD C,n", c, 2, 7);

    c[0] = 0x0F; check_timing("RRCA", c, 1, 4);

    /* DJNZ: B=1 -> not taken = 8T */
    {
        TEST_BEGIN("DJNZ (not taken)");
        setup(); cpu.bc.h = 1;
        test_ram[0] = 0x10; test_ram[1] = 0x05;
        ASSERT_CYCLES(step(), 8);
        TEST_END();
    }
    /* DJNZ: B=2 -> taken = 13T */
    {
        TEST_BEGIN("DJNZ (taken)");
        setup(); cpu.bc.h = 2;
        test_ram[0] = 0x10; test_ram[1] = 0x05;
        ASSERT_CYCLES(step(), 13);
        TEST_END();
    }

    c[0] = 0x11; c[1] = 0x00; c[2] = 0x00;
    check_timing("LD DE,nn", c, 3, 10);

    c[0] = 0x12; check_timing("LD (DE),A", c, 1, 7);
    c[0] = 0x13; check_timing("INC DE", c, 1, 6);
    c[0] = 0x14; check_timing("INC D", c, 1, 4);
    c[0] = 0x15; check_timing("DEC D", c, 1, 4);

    c[0] = 0x16; c[1] = 0x42;
    check_timing("LD D,n", c, 2, 7);

    c[0] = 0x17; check_timing("RLA", c, 1, 4);

    c[0] = 0x18; c[1] = 0x05;
    check_timing("JR d", c, 2, 12);

    c[0] = 0x19; check_timing("ADD HL,DE", c, 1, 11);
    c[0] = 0x1A; check_timing("LD A,(DE)", c, 1, 7);
    c[0] = 0x1B; check_timing("DEC DE", c, 1, 6);
    c[0] = 0x1C; check_timing("INC E", c, 1, 4);
    c[0] = 0x1D; check_timing("DEC E", c, 1, 4);

    c[0] = 0x1E; c[1] = 0x42;
    check_timing("LD E,n", c, 2, 7);

    c[0] = 0x1F; check_timing("RRA", c, 1, 4);

    /* JR NZ taken (Z=0) = 12T */
    c[0] = 0x20; c[1] = 0x05;
    check_timing("JR NZ taken", c, 2, 12);

    /* JR NZ not taken (Z=1) = 7T */
    {
        TEST_BEGIN("JR NZ not taken");
        setup(); cpu.af.l = Z80_FLAG_Z;
        test_ram[0] = 0x20; test_ram[1] = 0x05;
        ASSERT_CYCLES(step(), 7);
        TEST_END();
    }

    c[0] = 0x21; c[1] = 0x00; c[2] = 0x80;
    check_timing("LD HL,nn", c, 3, 10);

    c[0] = 0x22; c[1] = 0x00; c[2] = 0x90;
    check_timing("LD (nn),HL", c, 3, 16);

    c[0] = 0x23; check_timing("INC HL", c, 1, 6);
    c[0] = 0x24; check_timing("INC H", c, 1, 4);
    c[0] = 0x25; check_timing("DEC H", c, 1, 4);

    c[0] = 0x26; c[1] = 0x42;
    check_timing("LD H,n", c, 2, 7);

    c[0] = 0x27; check_timing("DAA", c, 1, 4);
    c[0] = 0x29; check_timing("ADD HL,HL", c, 1, 11);

    c[0] = 0x2A; c[1] = 0x00; c[2] = 0x90;
    check_timing("LD HL,(nn)", c, 3, 16);

    c[0] = 0x2B; check_timing("DEC HL", c, 1, 6);
    c[0] = 0x2C; check_timing("INC L", c, 1, 4);
    c[0] = 0x2D; check_timing("DEC L", c, 1, 4);

    c[0] = 0x2E; c[1] = 0x42;
    check_timing("LD L,n", c, 2, 7);

    c[0] = 0x2F; check_timing("CPL", c, 1, 4);

    c[0] = 0x31; c[1] = 0x00; c[2] = 0xFF;
    check_timing("LD SP,nn", c, 3, 10);

    c[0] = 0x32; c[1] = 0x00; c[2] = 0x90;
    check_timing("LD (nn),A", c, 3, 13);

    c[0] = 0x33; check_timing("INC SP", c, 1, 6);
    c[0] = 0x34; check_timing("INC (HL)", c, 1, 11);
    c[0] = 0x35; check_timing("DEC (HL)", c, 1, 11);

    c[0] = 0x36; c[1] = 0x42;
    check_timing("LD (HL),n", c, 2, 10);

    c[0] = 0x37; check_timing("SCF", c, 1, 4);
    c[0] = 0x39; check_timing("ADD HL,SP", c, 1, 11);

    c[0] = 0x3A; c[1] = 0x00; c[2] = 0x90;
    check_timing("LD A,(nn)", c, 3, 13);

    c[0] = 0x3B; check_timing("DEC SP", c, 1, 6);
    c[0] = 0x3C; check_timing("INC A", c, 1, 4);
    c[0] = 0x3D; check_timing("DEC A", c, 1, 4);

    c[0] = 0x3E; c[1] = 0x42;
    check_timing("LD A,n", c, 2, 7);

    c[0] = 0x3F; check_timing("CCF", c, 1, 4);
}

/**
 * @brief Overeni T-stavu LD r,r' a aritmetiky (0x40-0xBF).
 */
static void test_timing_ld_alu(void) {
    u8 c[1];

    /* LD r,r' = 4T, LD r,(HL) = 7T, LD (HL),r = 7T */
    c[0] = 0x40; check_timing("LD B,B", c, 1, 4);
    c[0] = 0x46; check_timing("LD B,(HL)", c, 1, 7);
    c[0] = 0x70; check_timing("LD (HL),B", c, 1, 7);

    /* Aritmetika reg = 4T, (HL) = 7T */
    c[0] = 0x80; check_timing("ADD A,B", c, 1, 4);
    c[0] = 0x86; check_timing("ADD A,(HL)", c, 1, 7);
    c[0] = 0x90; check_timing("SUB B", c, 1, 4);
    c[0] = 0x96; check_timing("SUB (HL)", c, 1, 7);
    c[0] = 0xA0; check_timing("AND B", c, 1, 4);
    c[0] = 0xA6; check_timing("AND (HL)", c, 1, 7);
    c[0] = 0xA8; check_timing("XOR B", c, 1, 4);
    c[0] = 0xB0; check_timing("OR B", c, 1, 4);
    c[0] = 0xB8; check_timing("CP B", c, 1, 4);
    c[0] = 0xBE; check_timing("CP (HL)", c, 1, 7);
}

/**
 * @brief Overeni T-stavu instrukci 0xC0-0xFF.
 */
static void test_timing_high(void) {
    u8 c[3];

    /* RET cc: taken=11T, not taken=5T */
    {
        TEST_BEGIN("RET NZ taken"); setup();
        cpu.sp = 0xFFF0; cpu.af.l = 0;
        test_ram[0xFFF0] = 0x00; test_ram[0xFFF1] = 0x01;
        test_ram[0] = 0xC0;
        ASSERT_CYCLES(step(), 11); TEST_END();
    }
    {
        TEST_BEGIN("RET NZ not taken"); setup();
        cpu.sp = 0xFFF0; cpu.af.l = Z80_FLAG_Z;
        test_ram[0] = 0xC0;
        ASSERT_CYCLES(step(), 5); TEST_END();
    }

    /* POP = 10T, PUSH = 11T */
    c[0] = 0xC1; check_timing("POP BC", c, 1, 10);
    c[0] = 0xC5; check_timing("PUSH BC", c, 1, 11);

    /* JP cc = 10T (vzdy) */
    c[0] = 0xC2; c[1] = 0x00; c[2] = 0x50;
    check_timing("JP NZ,nn", c, 3, 10);

    /* JP = 10T */
    c[0] = 0xC3; c[1] = 0x00; c[2] = 0x50;
    check_timing("JP nn", c, 3, 10);

    /* CALL cc: taken=17T, not taken=10T */
    {
        TEST_BEGIN("CALL NZ taken"); setup();
        cpu.sp = 0xFFFE; cpu.af.l = 0;
        test_ram[0] = 0xC4; test_ram[1] = 0x00; test_ram[2] = 0x50;
        ASSERT_CYCLES(step(), 17); TEST_END();
    }
    {
        TEST_BEGIN("CALL NZ not taken"); setup();
        cpu.sp = 0xFFFE; cpu.af.l = Z80_FLAG_Z;
        test_ram[0] = 0xC4; test_ram[1] = 0x00; test_ram[2] = 0x50;
        ASSERT_CYCLES(step(), 10); TEST_END();
    }

    /* Aritmetika imm = 7T */
    c[0] = 0xC6; c[1] = 0x01;
    check_timing("ADD A,n", c, 2, 7);

    /* RST = 11T */
    c[0] = 0xC7; check_timing("RST 0", c, 1, 11);

    /* RET = 10T */
    c[0] = 0xC9; check_timing("RET", c, 1, 10);

    /* CALL = 17T */
    c[0] = 0xCD; c[1] = 0x00; c[2] = 0x50;
    check_timing("CALL nn", c, 3, 17);

    /* OUT (n),A = 11T */
    c[0] = 0xD3; c[1] = 0x10;
    check_timing("OUT (n),A", c, 2, 11);

    /* IN A,(n) = 11T */
    c[0] = 0xDB; c[1] = 0x10;
    check_timing("IN A,(n)", c, 2, 11);

    /* EX DE,HL = 4T */
    c[0] = 0xEB; check_timing("EX DE,HL", c, 1, 4);

    /* EX (SP),HL = 19T */
    c[0] = 0xE3; check_timing("EX (SP),HL", c, 1, 19);

    /* JP (HL) = 4T */
    c[0] = 0xE9; check_timing("JP (HL)", c, 1, 4);

    /* LD SP,HL = 6T */
    c[0] = 0xF9; check_timing("LD SP,HL", c, 1, 6);

    /* EXX = 4T */
    c[0] = 0xD9; check_timing("EXX", c, 1, 4);
}

/**
 * @brief Overeni T-stavu CB prefixovych instrukci.
 */
static void test_timing_cb(void) {
    u8 c[2];

    /* CB rotace/posuvy reg = 8T */
    c[0] = 0xCB; c[1] = 0x00;
    check_timing("RLC B", c, 2, 8);

    c[0] = 0xCB; c[1] = 0x38;
    check_timing("SRL B", c, 2, 8);

    /* CB rotace/posuvy (HL) = 15T */
    c[0] = 0xCB; c[1] = 0x06;
    check_timing("RLC (HL)", c, 2, 15);

    /* BIT reg = 8T, BIT (HL) = 12T */
    c[0] = 0xCB; c[1] = 0x40;
    check_timing("BIT 0,B", c, 2, 8);

    c[0] = 0xCB; c[1] = 0x46;
    check_timing("BIT 0,(HL)", c, 2, 12);

    /* SET/RES reg = 8T, (HL) = 15T */
    c[0] = 0xCB; c[1] = 0xC0;
    check_timing("SET 0,B", c, 2, 8);

    c[0] = 0xCB; c[1] = 0xC6;
    check_timing("SET 0,(HL)", c, 2, 15);

    c[0] = 0xCB; c[1] = 0x80;
    check_timing("RES 0,B", c, 2, 8);

    c[0] = 0xCB; c[1] = 0x86;
    check_timing("RES 0,(HL)", c, 2, 15);
}

/**
 * @brief Overeni T-stavu ED prefixovych instrukci.
 */
static void test_timing_ed(void) {
    u8 c[4];

    /* IN r,(C) = 12T */
    c[0] = 0xED; c[1] = 0x40;
    check_timing("IN B,(C)", c, 2, 12);

    /* OUT (C),r = 12T */
    c[0] = 0xED; c[1] = 0x41;
    check_timing("OUT (C),B", c, 2, 12);

    /* SBC HL,rr = 15T */
    c[0] = 0xED; c[1] = 0x42;
    check_timing("SBC HL,BC", c, 2, 15);

    /* ADC HL,rr = 15T */
    c[0] = 0xED; c[1] = 0x4A;
    check_timing("ADC HL,BC", c, 2, 15);

    /* LD (nn),rr = 20T */
    c[0] = 0xED; c[1] = 0x43; c[2] = 0x00; c[3] = 0x90;
    check_timing("LD (nn),BC", c, 4, 20);

    /* LD rr,(nn) = 20T */
    c[0] = 0xED; c[1] = 0x4B; c[2] = 0x00; c[3] = 0x90;
    check_timing("LD BC,(nn)", c, 4, 20);

    /* NEG = 8T */
    c[0] = 0xED; c[1] = 0x44;
    check_timing("NEG", c, 2, 8);

    /* IM = 8T */
    c[0] = 0xED; c[1] = 0x46;
    check_timing("IM 0", c, 2, 8);

    /* LD I,A = 9T */
    c[0] = 0xED; c[1] = 0x47;
    check_timing("LD I,A", c, 2, 9);

    /* RRD = 18T */
    c[0] = 0xED; c[1] = 0x67;
    check_timing("RRD", c, 2, 18);

    /* RLD = 18T */
    c[0] = 0xED; c[1] = 0x6F;
    check_timing("RLD", c, 2, 18);

    /* LDI = 16T */
    c[0] = 0xED; c[1] = 0xA0;
    check_timing("LDI", c, 2, 16);

    /* CPI = 16T */
    c[0] = 0xED; c[1] = 0xA1;
    check_timing("CPI", c, 2, 16);

    /* INI = 16T */
    c[0] = 0xED; c[1] = 0xA2;
    check_timing("INI", c, 2, 16);

    /* OUTI = 16T */
    c[0] = 0xED; c[1] = 0xA3;
    check_timing("OUTI", c, 2, 16);

    /* Nedokumentovany ED opcode = 8T (NOP) */
    c[0] = 0xED; c[1] = 0x00;
    check_timing("ED NOP (undoc)", c, 2, 8);
}

/**
 * @brief Overeni T-stavu DD/FD prefixovych instrukci.
 */
static void test_timing_dd(void) {
    u8 c[4];

    /* ADD IX,rr = 15T */
    c[0] = 0xDD; c[1] = 0x09;
    check_timing("ADD IX,BC", c, 2, 15);

    /* LD IX,nn = 14T */
    c[0] = 0xDD; c[1] = 0x21; c[2] = 0x00; c[3] = 0x50;
    check_timing("LD IX,nn", c, 4, 14);

    /* INC IX = 10T */
    c[0] = 0xDD; c[1] = 0x23;
    check_timing("INC IX", c, 2, 10);

    /* INC IXH = 8T */
    c[0] = 0xDD; c[1] = 0x24;
    check_timing("INC IXH", c, 2, 8);

    /* LD r,(IX+d) = 19T */
    c[0] = 0xDD; c[1] = 0x7E; c[2] = 0x00;
    check_timing("LD A,(IX+0)", c, 3, 19);

    /* LD (IX+d),r = 19T */
    c[0] = 0xDD; c[1] = 0x77; c[2] = 0x00;
    check_timing("LD (IX+0),A", c, 3, 19);

    /* LD (IX+d),n = 19T */
    c[0] = 0xDD; c[1] = 0x36; c[2] = 0x00; c[3] = 0x42;
    check_timing("LD (IX+0),n", c, 4, 19);

    /* INC (IX+d) = 23T */
    c[0] = 0xDD; c[1] = 0x34; c[2] = 0x00;
    check_timing("INC (IX+0)", c, 3, 23);

    /* ADD A,IXH = 8T */
    c[0] = 0xDD; c[1] = 0x84;
    check_timing("ADD A,IXH", c, 2, 8);

    /* ADD A,(IX+d) = 19T */
    c[0] = 0xDD; c[1] = 0x86; c[2] = 0x00;
    check_timing("ADD A,(IX+0)", c, 3, 19);

    /* PUSH IX = 15T */
    c[0] = 0xDD; c[1] = 0xE5;
    check_timing("PUSH IX", c, 2, 15);

    /* POP IX = 14T */
    c[0] = 0xDD; c[1] = 0xE1;
    check_timing("POP IX", c, 2, 14);

    /* JP (IX) = 8T */
    c[0] = 0xDD; c[1] = 0xE9;
    check_timing("JP (IX)", c, 2, 8);

    /* LD SP,IX = 10T */
    c[0] = 0xDD; c[1] = 0xF9;
    check_timing("LD SP,IX", c, 2, 10);

    /* EX (SP),IX = 23T */
    c[0] = 0xDD; c[1] = 0xE3;
    check_timing("EX (SP),IX", c, 2, 23);

    /* DD CB: shift/set/res (IX+d) = 23T, BIT (IX+d) = 20T */
    c[0] = 0xDD; c[1] = 0xCB; c[2] = 0x00; c[3] = 0x06;
    check_timing("DD CB RLC (IX+0)", c, 4, 23);

    c[0] = 0xDD; c[1] = 0xCB; c[2] = 0x00; c[3] = 0x46;
    check_timing("DD CB BIT 0,(IX+0)", c, 4, 20);

    c[0] = 0xDD; c[1] = 0xCB; c[2] = 0x00; c[3] = 0xC6;
    check_timing("DD CB SET 0,(IX+0)", c, 4, 23);

    c[0] = 0xDD; c[1] = 0xCB; c[2] = 0x00; c[3] = 0x86;
    check_timing("DD CB RES 0,(IX+0)", c, 4, 23);
}

/* ========== Vstupni bod ========== */

/**
 * @brief Spusti vsechny testy casovani.
 */
void test_timing_all(void) {
    test_timing_basic();
    test_timing_ld_alu();
    test_timing_high();
    test_timing_cb();
    test_timing_ed();
    test_timing_dd();
}
