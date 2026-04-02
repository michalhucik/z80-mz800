/**
 * @file test_dasm.c
 * @brief Testy pro Z80 disassembler (dasm-z80).
 * @version 0.1
 *
 * Testuje dekodovani instrukci, formatovani vystupu, analyzu toku rizeni,
 * tabulku symbolu, konverze relativnich adres a z80ex kompatibilni obalku.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../dasm-z80/z80_dasm.h"

/* ======================================================================
 * Jednoduchy testovaci framework
 * ====================================================================== */

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_TRUE(cond, msg) do { \
    tests_run++; \
    if (cond) { tests_passed++; } \
    else { tests_failed++; printf("  FAIL: %s (radek %d)\n", msg, __LINE__); } \
} while(0)

#define ASSERT_EQ_INT(a, b, msg) do { \
    tests_run++; \
    if ((a) == (b)) { tests_passed++; } \
    else { tests_failed++; printf("  FAIL: %s: ocekavano %d, dostano %d (radek %d)\n", msg, (int)(b), (int)(a), __LINE__); } \
} while(0)

#define ASSERT_EQ_STR(a, b, msg) do { \
    tests_run++; \
    if (strcmp((a), (b)) == 0) { tests_passed++; } \
    else { tests_failed++; printf("  FAIL: %s: ocekavano \"%s\", dostano \"%s\" (radek %d)\n", msg, (b), (a), __LINE__); } \
} while(0)

/* ======================================================================
 * Testovaci pamet - pole bajtu s callbackem
 * ====================================================================== */

static u8 test_mem[65536];

/**
 * @brief Callback pro cteni z testovaci pameti.
 */
static u8 test_read(u16 addr, void *user_data)
{
    (void)user_data;
    return test_mem[addr];
}

/**
 * @brief Zapise bajty do testovaci pameti od dane adresy.
 */
static void write_bytes(u16 addr, const u8 *data, int len)
{
    for (int i = 0; i < len; i++) {
        test_mem[addr + i] = data[i];
    }
}

/* ======================================================================
 * Testy: zakladni dekodovani
 * ====================================================================== */

static void test_base_opcodes(void)
{
    z80_dasm_inst_t inst;
    char buf[64];
    z80_dasm_format_t fmt;
    z80_dasm_format_default(&fmt);

    printf("--- test_base_opcodes ---\n");

    /* NOP */
    test_mem[0x0000] = 0x00;
    z80_dasm(&inst, test_read, NULL, 0x0000);
    z80_dasm_to_str(buf, sizeof(buf), &inst, &fmt);
    ASSERT_EQ_INT(inst.length, 1, "NOP delka");
    ASSERT_EQ_INT(inst.t_states, 4, "NOP T-stavy");
    ASSERT_EQ_STR(buf, "NOP", "NOP format");
    ASSERT_EQ_INT(inst.flow, Z80_FLOW_NORMAL, "NOP flow");

    /* LD BC,1234h */
    u8 ld_bc[] = { 0x01, 0x34, 0x12 };
    write_bytes(0x0100, ld_bc, 3);
    z80_dasm(&inst, test_read, NULL, 0x0100);
    z80_dasm_to_str(buf, sizeof(buf), &inst, &fmt);
    ASSERT_EQ_INT(inst.length, 3, "LD BC,nn delka");
    ASSERT_EQ_INT(inst.t_states, 10, "LD BC,nn T-stavy");
    ASSERT_EQ_INT(inst.op2.type, Z80_OP_IMM16, "LD BC,nn op2 typ");
    ASSERT_EQ_INT(inst.op2.val.imm16, 0x1234, "LD BC,nn op2 hodnota");
    ASSERT_TRUE(strstr(buf, "LD") != NULL, "LD BC,nn obsahuje LD");

    /* HALT */
    test_mem[0x0200] = 0x76;
    z80_dasm(&inst, test_read, NULL, 0x0200);
    ASSERT_EQ_INT(inst.flow, Z80_FLOW_HALT, "HALT flow");
    ASSERT_EQ_INT(inst.length, 1, "HALT delka");

    /* JP 1234h */
    u8 jp[] = { 0xC3, 0x34, 0x12 };
    write_bytes(0x0300, jp, 3);
    z80_dasm(&inst, test_read, NULL, 0x0300);
    ASSERT_EQ_INT(inst.flow, Z80_FLOW_JUMP, "JP flow");
    ASSERT_EQ_INT(inst.length, 3, "JP delka");
    ASSERT_EQ_INT(inst.t_states, 10, "JP T-stavy");

    /* CALL 5678h */
    u8 call[] = { 0xCD, 0x78, 0x56 };
    write_bytes(0x0400, call, 3);
    z80_dasm(&inst, test_read, NULL, 0x0400);
    ASSERT_EQ_INT(inst.flow, Z80_FLOW_CALL, "CALL flow");
    ASSERT_EQ_INT(inst.t_states, 17, "CALL T-stavy");

    /* RET */
    test_mem[0x0500] = 0xC9;
    z80_dasm(&inst, test_read, NULL, 0x0500);
    ASSERT_EQ_INT(inst.flow, Z80_FLOW_RET, "RET flow");
    ASSERT_EQ_INT(inst.t_states, 10, "RET T-stavy");

    /* JR +5 */
    u8 jr[] = { 0x18, 0x05 };
    write_bytes(0x0600, jr, 2);
    z80_dasm(&inst, test_read, NULL, 0x0600);
    ASSERT_EQ_INT(inst.flow, Z80_FLOW_JUMP, "JR flow");
    ASSERT_EQ_INT(inst.length, 2, "JR delka");
    ASSERT_EQ_INT(inst.t_states, 12, "JR T-stavy");
    ASSERT_EQ_INT(inst.op1.type, Z80_OP_REL8, "JR op1 typ");
    ASSERT_EQ_INT(inst.op1.val.displacement, 5, "JR op1 displacement");

    /* DJNZ -2 (skok sam na sebe) */
    u8 djnz[] = { 0x10, 0xFE };
    write_bytes(0x0700, djnz, 2);
    z80_dasm(&inst, test_read, NULL, 0x0700);
    ASSERT_EQ_INT(inst.flow, Z80_FLOW_JUMP_COND, "DJNZ flow");
    ASSERT_EQ_INT(inst.t_states, 8, "DJNZ T-stavy zakladni");
    ASSERT_EQ_INT(inst.t_states2, 13, "DJNZ T-stavy pri skoku");

    /* RST 38h */
    test_mem[0x0800] = 0xFF;
    z80_dasm(&inst, test_read, NULL, 0x0800);
    ASSERT_EQ_INT(inst.flow, Z80_FLOW_RST, "RST flow");
    ASSERT_EQ_INT(inst.t_states, 11, "RST T-stavy");

    /* ADD A,B */
    test_mem[0x0900] = 0x80;
    z80_dasm(&inst, test_read, NULL, 0x0900);
    ASSERT_EQ_INT(inst.flags_affected, 0xFF, "ADD A,B flagy");
    ASSERT_TRUE(inst.regs_read & Z80_REG_A, "ADD A,B cte A");
    ASSERT_TRUE(inst.regs_read & Z80_REG_B, "ADD A,B cte B");
    ASSERT_TRUE(inst.regs_written & Z80_REG_A, "ADD A,B zapisuje A");
    ASSERT_TRUE(inst.regs_written & Z80_REG_F, "ADD A,B zapisuje F");

    /* LD A,(nn) */
    u8 ld_a_nn[] = { 0x3A, 0x00, 0x80 };
    write_bytes(0x0A00, ld_a_nn, 3);
    z80_dasm(&inst, test_read, NULL, 0x0A00);
    ASSERT_EQ_INT(inst.length, 3, "LD A,(nn) delka");
    ASSERT_EQ_INT(inst.op1.type, Z80_OP_REG8, "LD A,(nn) op1 typ");
    ASSERT_EQ_INT(inst.op2.type, Z80_OP_MEM_IMM16, "LD A,(nn) op2 typ");
    ASSERT_EQ_INT(inst.op2.val.imm16, 0x8000, "LD A,(nn) op2 adresa");

    /* Podmineny JP NZ */
    u8 jp_nz[] = { 0xC2, 0x00, 0x10 };
    write_bytes(0x0B00, jp_nz, 3);
    z80_dasm(&inst, test_read, NULL, 0x0B00);
    ASSERT_EQ_INT(inst.flow, Z80_FLOW_JUMP_COND, "JP NZ flow");
    ASSERT_EQ_INT(inst.op1.type, Z80_OP_CONDITION, "JP NZ op1 typ");
    ASSERT_EQ_INT(inst.op1.val.condition, Z80_CC_NZ, "JP NZ podminka");
}

/* ======================================================================
 * Testy: CB prefix
 * ====================================================================== */

static void test_cb_opcodes(void)
{
    z80_dasm_inst_t inst;

    printf("--- test_cb_opcodes ---\n");

    /* RLC B */
    u8 rlc_b[] = { 0xCB, 0x00 };
    write_bytes(0x1000, rlc_b, 2);
    z80_dasm(&inst, test_read, NULL, 0x1000);
    ASSERT_EQ_INT(inst.length, 2, "RLC B delka");
    ASSERT_EQ_INT(inst.t_states, 8, "RLC B T-stavy");
    ASSERT_EQ_INT(inst.flags_affected, 0xFF, "RLC B flagy");

    /* BIT 3,(HL) */
    u8 bit_hl[] = { 0xCB, 0x5E };
    write_bytes(0x1010, bit_hl, 2);
    z80_dasm(&inst, test_read, NULL, 0x1010);
    ASSERT_EQ_INT(inst.length, 2, "BIT 3,(HL) delka");
    ASSERT_EQ_INT(inst.t_states, 12, "BIT 3,(HL) T-stavy");
    ASSERT_EQ_INT(inst.op1.type, Z80_OP_BIT_INDEX, "BIT 3,(HL) op1 typ");
    ASSERT_EQ_INT(inst.op1.val.bit_index, 3, "BIT 3,(HL) op1 bit");

    /* SET 7,A */
    u8 set_a[] = { 0xCB, 0xFF };
    write_bytes(0x1020, set_a, 2);
    z80_dasm(&inst, test_read, NULL, 0x1020);
    ASSERT_EQ_INT(inst.length, 2, "SET 7,A delka");
    ASSERT_EQ_INT(inst.t_states, 8, "SET 7,A T-stavy");
    ASSERT_EQ_INT(inst.flags_affected, 0, "SET 7,A flagy (zadne)");

    /* SLL B (undocumented) */
    u8 sll[] = { 0xCB, 0x30 };
    write_bytes(0x1030, sll, 2);
    z80_dasm(&inst, test_read, NULL, 0x1030);
    ASSERT_EQ_INT(inst.cls, Z80_CLASS_UNDOCUMENTED, "SLL undocumented");
    ASSERT_EQ_INT(inst.length, 2, "SLL delka");
}

/* ======================================================================
 * Testy: ED prefix
 * ====================================================================== */

static void test_ed_opcodes(void)
{
    z80_dasm_inst_t inst;

    printf("--- test_ed_opcodes ---\n");

    /* LDIR */
    u8 ldir[] = { 0xED, 0xB0 };
    write_bytes(0x2000, ldir, 2);
    z80_dasm(&inst, test_read, NULL, 0x2000);
    ASSERT_EQ_INT(inst.length, 2, "LDIR delka");
    ASSERT_EQ_INT(inst.t_states, 16, "LDIR T-stavy");
    ASSERT_EQ_INT(inst.t_states2, 21, "LDIR T-stavy2");

    /* RETI */
    u8 reti[] = { 0xED, 0x4D };
    write_bytes(0x2010, reti, 2);
    z80_dasm(&inst, test_read, NULL, 0x2010);
    ASSERT_EQ_INT(inst.flow, Z80_FLOW_RETI, "RETI flow");

    /* RETN */
    u8 retn[] = { 0xED, 0x45 };
    write_bytes(0x2020, retn, 2);
    z80_dasm(&inst, test_read, NULL, 0x2020);
    ASSERT_EQ_INT(inst.flow, Z80_FLOW_RETN, "RETN flow");

    /* Neplatna ED instrukce */
    u8 ed_inv[] = { 0xED, 0x00 };
    write_bytes(0x2030, ed_inv, 2);
    z80_dasm(&inst, test_read, NULL, 0x2030);
    ASSERT_EQ_INT(inst.cls, Z80_CLASS_INVALID, "ED 00 invalid");
    ASSERT_EQ_INT(inst.length, 2, "ED 00 delka");
    ASSERT_EQ_INT(inst.t_states, 8, "ED 00 T-stavy");

    /* IN F,(C) - undocumented */
    u8 in_f[] = { 0xED, 0x70 };
    write_bytes(0x2040, in_f, 2);
    z80_dasm(&inst, test_read, NULL, 0x2040);
    ASSERT_EQ_INT(inst.cls, Z80_CLASS_UNDOCUMENTED, "IN F,(C) undocumented");
}

/* ======================================================================
 * Testy: DD/FD prefix (IX/IY)
 * ====================================================================== */

static void test_dd_fd_opcodes(void)
{
    z80_dasm_inst_t inst;
    char buf[64];
    z80_dasm_format_t fmt;
    z80_dasm_format_default(&fmt);

    printf("--- test_dd_fd_opcodes ---\n");

    /* LD IX,1234h */
    u8 ld_ix[] = { 0xDD, 0x21, 0x34, 0x12 };
    write_bytes(0x3000, ld_ix, 4);
    z80_dasm(&inst, test_read, NULL, 0x3000);
    ASSERT_EQ_INT(inst.length, 4, "LD IX,nn delka");
    ASSERT_EQ_INT(inst.t_states, 14, "LD IX,nn T-stavy");
    ASSERT_TRUE(inst.regs_written & Z80_REG_IX, "LD IX,nn zapisuje IX");

    /* LD A,(IX+5) */
    u8 ld_a_ix[] = { 0xDD, 0x7E, 0x05 };
    write_bytes(0x3010, ld_a_ix, 3);
    z80_dasm(&inst, test_read, NULL, 0x3010);
    z80_dasm_to_str(buf, sizeof(buf), &inst, &fmt);
    ASSERT_EQ_INT(inst.length, 3, "LD A,(IX+d) delka");
    ASSERT_EQ_INT(inst.op2.type, Z80_OP_MEM_IX_D, "LD A,(IX+d) op2 typ");
    ASSERT_EQ_INT(inst.op2.val.displacement, 5, "LD A,(IX+d) op2 disp");

    /* INC IXH (undocumented) */
    u8 inc_ixh[] = { 0xDD, 0x24 };
    write_bytes(0x3020, inc_ixh, 2);
    z80_dasm(&inst, test_read, NULL, 0x3020);
    ASSERT_EQ_INT(inst.cls, Z80_CLASS_UNDOCUMENTED, "INC IXH undocumented");

    /* JP (IX) */
    u8 jp_ix[] = { 0xDD, 0xE9 };
    write_bytes(0x3030, jp_ix, 2);
    z80_dasm(&inst, test_read, NULL, 0x3030);
    ASSERT_EQ_INT(inst.flow, Z80_FLOW_JUMP_INDIRECT, "JP (IX) flow");

    /* DD DD - neplatna sekvence -> NOP* delka 1 */
    u8 dd_dd[] = { 0xDD, 0xDD, 0x00 };
    write_bytes(0x3040, dd_dd, 3);
    z80_dasm(&inst, test_read, NULL, 0x3040);
    ASSERT_EQ_INT(inst.cls, Z80_CLASS_INVALID, "DD DD invalid");
    ASSERT_EQ_INT(inst.length, 1, "DD DD delka 1");
    ASSERT_EQ_INT(inst.t_states, 4, "DD DD T-stavy");

    /* DD zrcadlena instrukce (fallback na base): DD 00 = NOP s +4T */
    u8 dd_nop[] = { 0xDD, 0x00 };
    write_bytes(0x3050, dd_nop, 2);
    z80_dasm(&inst, test_read, NULL, 0x3050);
    ASSERT_EQ_INT(inst.length, 2, "DD NOP delka");
    ASSERT_EQ_INT(inst.t_states, 8, "DD NOP T-stavy (4+4)");

    /* LD A,(IY+FFh = -1) */
    u8 ld_a_iy[] = { 0xFD, 0x7E, 0xFF };
    write_bytes(0x3060, ld_a_iy, 3);
    z80_dasm(&inst, test_read, NULL, 0x3060);
    ASSERT_EQ_INT(inst.op2.type, Z80_OP_MEM_IY_D, "LD A,(IY+d) op2 typ");
    ASSERT_EQ_INT(inst.op2.val.displacement, -1, "LD A,(IY-1) op2 disp");
}

/* ======================================================================
 * Testy: DDCB/FDCB prefix
 * ====================================================================== */

static void test_ddcb_fdcb(void)
{
    z80_dasm_inst_t inst;

    printf("--- test_ddcb_fdcb ---\n");

    /* BIT 3,(IX+2) */
    u8 bit_ix[] = { 0xDD, 0xCB, 0x02, 0x5E };
    write_bytes(0x4000, bit_ix, 4);
    z80_dasm(&inst, test_read, NULL, 0x4000);
    ASSERT_EQ_INT(inst.length, 4, "BIT 3,(IX+2) delka");
    ASSERT_EQ_INT(inst.t_states, 20, "BIT 3,(IX+2) T-stavy");

    /* RLC (IX+3),B (undocumented - kopie do registru) */
    u8 rlc_ix_b[] = { 0xDD, 0xCB, 0x03, 0x00 };
    write_bytes(0x4010, rlc_ix_b, 4);
    z80_dasm(&inst, test_read, NULL, 0x4010);
    ASSERT_EQ_INT(inst.length, 4, "RLC (IX+d),B delka");
    ASSERT_EQ_INT(inst.cls, Z80_CLASS_UNDOCUMENTED, "RLC (IX+d),B undocumented");
    ASSERT_EQ_INT(inst.t_states, 23, "RLC (IX+d),B T-stavy");

    /* SET 7,(IY+0) */
    u8 set_iy[] = { 0xFD, 0xCB, 0x00, 0xFE };
    write_bytes(0x4020, set_iy, 4);
    z80_dasm(&inst, test_read, NULL, 0x4020);
    ASSERT_EQ_INT(inst.length, 4, "SET 7,(IY+0) delka");
    ASSERT_EQ_INT(inst.t_states, 23, "SET 7,(IY+0) T-stavy");
}

/* ======================================================================
 * Testy: formatovani vystupu
 * ====================================================================== */

static void test_formatting(void)
{
    z80_dasm_inst_t inst;
    char buf[64];
    z80_dasm_format_t fmt;

    printf("--- test_formatting ---\n");

    /* Test hex stylu: LD A,#FF */
    u8 ld_a_ff[] = { 0x3E, 0xFF };
    write_bytes(0x5000, ld_a_ff, 2);
    z80_dasm(&inst, test_read, NULL, 0x5000);

    /* Z80_HEX_HASH (vychozi) */
    z80_dasm_format_default(&fmt);
    z80_dasm_to_str(buf, sizeof(buf), &inst, &fmt);
    ASSERT_TRUE(strstr(buf, "#FF") != NULL || strstr(buf, "#ff") != NULL,
                "HEX_HASH format");

    /* Z80_HEX_0X */
    fmt.hex_style = Z80_HEX_0X;
    z80_dasm_to_str(buf, sizeof(buf), &inst, &fmt);
    ASSERT_TRUE(strstr(buf, "0XFF") != NULL || strstr(buf, "0xFF") != NULL ||
                strstr(buf, "0xff") != NULL,
                "HEX_0X format");

    /* Lowercase */
    z80_dasm_format_default(&fmt);
    fmt.uppercase = 0;
    z80_dasm_to_str(buf, sizeof(buf), &inst, &fmt);
    ASSERT_TRUE(strstr(buf, "ld") != NULL, "lowercase format");

    /* JP (HL) - spravny format (ne JP HL jako z80ex) */
    test_mem[0x5010] = 0xE9;
    z80_dasm(&inst, test_read, NULL, 0x5010);
    z80_dasm_format_default(&fmt);
    z80_dasm_to_str(buf, sizeof(buf), &inst, &fmt);
    ASSERT_TRUE(strstr(buf, "(HL)") != NULL, "JP (HL) format");
}

/* ======================================================================
 * Testy: tabulka symbolu
 * ====================================================================== */

static void test_symtab(void)
{
    printf("--- test_symtab ---\n");

    z80_symtab_t *tab = z80_symtab_create();
    ASSERT_TRUE(tab != NULL, "symtab create");
    ASSERT_EQ_INT(z80_symtab_count(tab), 0, "symtab pocatecni pocet");

    /* Pridani symbolu */
    z80_symtab_add(tab, 0x0000, "reset");
    z80_symtab_add(tab, 0x0038, "irq_handler");
    z80_symtab_add(tab, 0x00AD, "rom_getchar");
    ASSERT_EQ_INT(z80_symtab_count(tab), 3, "symtab pocet po pridani");

    /* Vyhledani */
    ASSERT_EQ_STR(z80_symtab_lookup(tab, 0x0000), "reset", "symtab lookup 0000");
    ASSERT_EQ_STR(z80_symtab_lookup(tab, 0x0038), "irq_handler", "symtab lookup 0038");
    ASSERT_EQ_STR(z80_symtab_lookup(tab, 0x00AD), "rom_getchar", "symtab lookup 00AD");
    ASSERT_TRUE(z80_symtab_lookup(tab, 0x0001) == NULL, "symtab lookup neexistujici");

    /* Odebrani */
    z80_symtab_remove(tab, 0x0038);
    ASSERT_EQ_INT(z80_symtab_count(tab), 2, "symtab pocet po odebrani");
    ASSERT_TRUE(z80_symtab_lookup(tab, 0x0038) == NULL, "symtab po odebrani");

    /* Prepis existujiciho */
    z80_symtab_add(tab, 0x0000, "cold_start");
    ASSERT_EQ_STR(z80_symtab_lookup(tab, 0x0000), "cold_start", "symtab prepis");
    ASSERT_EQ_INT(z80_symtab_count(tab), 2, "symtab pocet po prepisu");

    /* Smazani vsech */
    z80_symtab_clear(tab);
    ASSERT_EQ_INT(z80_symtab_count(tab), 0, "symtab po clear");

    z80_symtab_destroy(tab);
}

/* ======================================================================
 * Testy: symboly ve vystupu
 * ====================================================================== */

static void test_sym_formatting(void)
{
    z80_dasm_inst_t inst;
    char buf[64];
    z80_dasm_format_t fmt;
    z80_dasm_format_default(&fmt);

    printf("--- test_sym_formatting ---\n");

    z80_symtab_t *tab = z80_symtab_create();
    z80_symtab_add(tab, 0x00AD, "rom_getchar");
    z80_symtab_add(tab, 0x8000, "video_ram");

    /* CALL 00ADh -> CALL rom_getchar */
    u8 call[] = { 0xCD, 0xAD, 0x00 };
    write_bytes(0x6000, call, 3);
    z80_dasm(&inst, test_read, NULL, 0x6000);
    z80_dasm_to_str_sym(buf, sizeof(buf), &inst, &fmt, tab);
    ASSERT_TRUE(strstr(buf, "rom_getchar") != NULL, "CALL se symbolem");

    /* LD A,(8000h) -> LD A,(video_ram) */
    u8 ld_a[] = { 0x3A, 0x00, 0x80 };
    write_bytes(0x6010, ld_a, 3);
    z80_dasm(&inst, test_read, NULL, 0x6010);
    z80_dasm_to_str_sym(buf, sizeof(buf), &inst, &fmt, tab);
    ASSERT_TRUE(strstr(buf, "video_ram") != NULL, "LD A,(nn) se symbolem");

    /* resolve_symbols */
    z80_dasm_symbols_t syms;
    z80_dasm(&inst, test_read, NULL, 0x6000); /* CALL 00AD */
    z80_dasm_resolve_symbols(&inst, tab, &syms);
    ASSERT_TRUE(syms.target_sym != NULL, "resolve target sym");
    ASSERT_EQ_STR(syms.target_sym, "rom_getchar", "resolve target hodnota");

    z80_symtab_destroy(tab);
}

/* ======================================================================
 * Testy: analyza toku rizeni
 * ====================================================================== */

static void test_flow_analysis(void)
{
    z80_dasm_inst_t inst;

    printf("--- test_flow_analysis ---\n");

    /* JP 1234h - cilova adresa */
    u8 jp[] = { 0xC3, 0x34, 0x12 };
    write_bytes(0x7000, jp, 3);
    z80_dasm(&inst, test_read, NULL, 0x7000);
    ASSERT_EQ_INT(z80_dasm_target_addr(&inst), 0x1234, "JP target addr");

    /* JR +5 na adrese 0x7010 - cil = 0x7010 + 2 + 5 = 0x7017 */
    u8 jr[] = { 0x18, 0x05 };
    write_bytes(0x7010, jr, 2);
    z80_dasm(&inst, test_read, NULL, 0x7010);
    ASSERT_EQ_INT(z80_dasm_target_addr(&inst), 0x7017, "JR target addr");

    /* RST 38h */
    test_mem[0x7020] = 0xFF;
    z80_dasm(&inst, test_read, NULL, 0x7020);
    ASSERT_EQ_INT(z80_dasm_target_addr(&inst), 0x0038, "RST 38h target");

    /* JP (HL) - cil neznam */
    test_mem[0x7030] = 0xE9;
    z80_dasm(&inst, test_read, NULL, 0x7030);
    ASSERT_EQ_INT(z80_dasm_target_addr(&inst), 0xFFFF, "JP (HL) target unknown");

    /* RET - cil neznam */
    test_mem[0x7040] = 0xC9;
    z80_dasm(&inst, test_read, NULL, 0x7040);
    ASSERT_EQ_INT(z80_dasm_target_addr(&inst), 0xFFFF, "RET target unknown");
}

/* ======================================================================
 * Testy: vyhodnoceni podminek vetveni
 * ====================================================================== */

static void test_branch_taken(void)
{
    z80_dasm_inst_t inst;

    printf("--- test_branch_taken ---\n");

    /* JP NZ,1234h */
    u8 jp_nz[] = { 0xC2, 0x34, 0x12 };
    write_bytes(0x8000, jp_nz, 3);
    z80_dasm(&inst, test_read, NULL, 0x8000);

    /* Z flag = 0 -> NZ splnena -> skok */
    ASSERT_EQ_INT(z80_dasm_branch_taken(&inst, 0x00), 1, "JP NZ taken (Z=0)");
    /* Z flag = 1 -> NZ nesplnena -> bez skoku */
    ASSERT_EQ_INT(z80_dasm_branch_taken(&inst, Z80_FLAG_Z), 0, "JP NZ not taken (Z=1)");

    /* JP C,1234h */
    u8 jp_c[] = { 0xDA, 0x34, 0x12 };
    write_bytes(0x8010, jp_c, 3);
    z80_dasm(&inst, test_read, NULL, 0x8010);
    ASSERT_EQ_INT(z80_dasm_branch_taken(&inst, Z80_FLAG_C), 1, "JP C taken (C=1)");
    ASSERT_EQ_INT(z80_dasm_branch_taken(&inst, 0x00), 0, "JP C not taken (C=0)");

    /* Bezpodminecny JP - vzdy proveden */
    u8 jp[] = { 0xC3, 0x34, 0x12 };
    write_bytes(0x8020, jp, 3);
    z80_dasm(&inst, test_read, NULL, 0x8020);
    ASSERT_EQ_INT(z80_dasm_branch_taken(&inst, 0x00), 1, "JP vzdy taken");

    /* NOP - neni vetveni */
    test_mem[0x8030] = 0x00;
    z80_dasm(&inst, test_read, NULL, 0x8030);
    ASSERT_EQ_INT(z80_dasm_branch_taken(&inst, 0x00), 0, "NOP not taken");
}

/* ======================================================================
 * Testy: konverze relativnich adres
 * ====================================================================== */

static void test_rel_addr(void)
{
    s8 offset;

    printf("--- test_rel_addr ---\n");

    /* z80_rel_to_abs: addr=0x100, offset=+3 -> 0x105 */
    ASSERT_EQ_INT(z80_rel_to_abs(0x0100, 3), 0x0105, "rel_to_abs +3");

    /* z80_rel_to_abs: addr=0x100, offset=-2 -> 0x100 (skok na sebe) */
    ASSERT_EQ_INT(z80_rel_to_abs(0x0100, -2), 0x0100, "rel_to_abs -2 (self)");

    /* z80_rel_to_abs: addr=0x100, offset=0 -> 0x102 */
    ASSERT_EQ_INT(z80_rel_to_abs(0x0100, 0), 0x0102, "rel_to_abs 0");

    /* z80_rel_to_abs: wraparound */
    ASSERT_EQ_INT(z80_rel_to_abs(0xFFFE, 1), 0x0001, "rel_to_abs wraparound");

    /* z80_abs_to_rel: addr=0x100, target=0x105 -> offset=3 */
    ASSERT_EQ_INT(z80_abs_to_rel(0x0100, 0x0105, &offset), 0, "abs_to_rel ok");
    ASSERT_EQ_INT(offset, 3, "abs_to_rel offset +3");

    /* z80_abs_to_rel: skok na sebe */
    ASSERT_EQ_INT(z80_abs_to_rel(0x0100, 0x0100, &offset), 0, "abs_to_rel self");
    ASSERT_EQ_INT(offset, -2, "abs_to_rel offset -2");

    /* z80_abs_to_rel: mimo dosah */
    ASSERT_EQ_INT(z80_abs_to_rel(0x0100, 0x0200, &offset), -1, "abs_to_rel mimo dosah");

    /* z80_abs_to_rel: hranice +127 */
    ASSERT_EQ_INT(z80_abs_to_rel(0x0100, 0x0181, &offset), 0, "abs_to_rel max+");
    ASSERT_EQ_INT(offset, 127, "abs_to_rel offset +127");

    /* z80_abs_to_rel: hranice -128 */
    ASSERT_EQ_INT(z80_abs_to_rel(0x0100, 0x0082, &offset), 0, "abs_to_rel max-");
    ASSERT_EQ_INT(offset, -128, "abs_to_rel offset -128");
}

/* ======================================================================
 * Testy: hromadna disassemblace
 * ====================================================================== */

static void test_block_dasm(void)
{
    z80_dasm_inst_t insts[16];

    printf("--- test_block_dasm ---\n");

    /* Kratky blok: NOP, LD A,42h, RET */
    u8 code[] = { 0x00, 0x3E, 0x42, 0xC9 };
    write_bytes(0x9000, code, 4);

    int count = z80_dasm_block(insts, 16, test_read, NULL, 0x9000, 0x9003);
    ASSERT_EQ_INT(count, 3, "block pocet instrukci");
    ASSERT_EQ_INT(insts[0].length, 1, "block[0] NOP delka");
    ASSERT_EQ_INT(insts[1].length, 2, "block[1] LD A,n delka");
    ASSERT_EQ_INT(insts[2].length, 1, "block[2] RET delka");
    ASSERT_EQ_INT(insts[2].flow, Z80_FLOW_RET, "block[2] flow");
}

/* ======================================================================
 * Testy: z80ex kompatibilni obalka
 * ====================================================================== */

static Z80EX_BYTE compat_read(Z80EX_WORD addr, void *user_data)
{
    (void)user_data;
    return test_mem[addr];
}

static void test_compat(void)
{
    char output[128];
    int t_states, t_states2;

    printf("--- test_compat ---\n");

    /* NOP */
    test_mem[0xA000] = 0x00;
    int len = z80ex_dasm(output, sizeof(output), 0, &t_states, &t_states2,
                         compat_read, 0xA000, NULL);
    ASSERT_EQ_INT(len, 1, "compat NOP delka");
    ASSERT_EQ_INT(t_states, 4, "compat NOP T-stavy");
    ASSERT_EQ_INT(t_states2, 0, "compat NOP T-stavy2");
    ASSERT_TRUE(strstr(output, "NOP") != NULL, "compat NOP vystup");

    /* LD A,FFh */
    u8 ld_a_ff[] = { 0x3E, 0xFF };
    write_bytes(0xA010, ld_a_ff, 2);
    len = z80ex_dasm(output, sizeof(output), 0, &t_states, &t_states2,
                     compat_read, 0xA010, NULL);
    ASSERT_EQ_INT(len, 2, "compat LD A,FF delka");
    ASSERT_EQ_INT(t_states, 7, "compat LD A,FF T-stavy");

    /* JP NZ,1234h */
    u8 jp_nz[] = { 0xC2, 0x34, 0x12 };
    write_bytes(0xA020, jp_nz, 3);
    len = z80ex_dasm(output, sizeof(output), 0, &t_states, &t_states2,
                     compat_read, 0xA020, NULL);
    ASSERT_EQ_INT(len, 3, "compat JP NZ delka");
    ASSERT_EQ_INT(t_states, 10, "compat JP NZ T-stavy");
    /* JP cc,nn nema t_states2 (vzdy 10T) */
    ASSERT_EQ_INT(t_states2, 0, "compat JP NZ T-stavy2");

    /* DJNZ */
    u8 djnz[] = { 0x10, 0x05 };
    write_bytes(0xA030, djnz, 2);
    len = z80ex_dasm(output, sizeof(output), 0, &t_states, &t_states2,
                     compat_read, 0xA030, NULL);
    ASSERT_EQ_INT(len, 2, "compat DJNZ delka");
    ASSERT_EQ_INT(t_states, 8, "compat DJNZ T-stavy");
    ASSERT_EQ_INT(t_states2, 13, "compat DJNZ T-stavy2");
}

/* ======================================================================
 * Testy: uplny pruchod vsech 256 zakladnich opcode
 * ====================================================================== */

static void test_all_base_decode(void)
{
    z80_dasm_inst_t inst;

    printf("--- test_all_base_decode ---\n");

    /* Priprava: za kazdym opcodem nasleduji bajty FF FF FF
       pro pripadne operandy */
    for (int i = 0; i < 256; i++) {
        u16 addr = (u16)(0xC000 + i * 4);
        test_mem[addr] = (u8)i;
        test_mem[addr + 1] = 0xFF;
        test_mem[addr + 2] = 0xFF;
        test_mem[addr + 3] = 0xFF;
    }

    int ok_count = 0;
    for (int i = 0; i < 256; i++) {
        u16 addr = (u16)(0xC000 + i * 4);

        /* CB, DD, ED, FD jsou prefixy - preskocime */
        if (i == 0xCB || i == 0xDD || i == 0xED || i == 0xFD) continue;

        int len = z80_dasm(&inst, test_read, NULL, addr);

        /* Kazda instrukce musi mit platnou delku 1-4 */
        if (len >= 1 && len <= 4 && inst.length == (u8)len && inst.t_states > 0) {
            ok_count++;
        } else {
            printf("  FAIL: opcode %02X: len=%d, inst.len=%d, tstates=%d\n",
                   i, len, inst.length, inst.t_states);
            tests_failed++;
            tests_run++;
        }
    }

    tests_run++;
    if (ok_count == 252) { /* 256 - 4 prefixy */
        tests_passed++;
    } else {
        tests_failed++;
        printf("  FAIL: pouze %d/252 opcode dekodovano spravne\n", ok_count);
    }
}

/* ======================================================================
 * Main
 * ====================================================================== */

int main(void)
{
    printf("=== dasm-z80: testy ===\n\n");

    memset(test_mem, 0, sizeof(test_mem));

    test_base_opcodes();
    test_cb_opcodes();
    test_ed_opcodes();
    test_dd_fd_opcodes();
    test_ddcb_fdcb();
    test_formatting();
    test_symtab();
    test_sym_formatting();
    test_flow_analysis();
    test_branch_taken();
    test_rel_addr();
    test_block_dasm();
    test_compat();
    test_all_base_decode();

    printf("\n=== Vysledky: %d testu, %d ok, %d selhalo ===\n",
           tests_run, tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
