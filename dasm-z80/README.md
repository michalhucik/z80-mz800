# dasm-z80 v0.1 - Z80 Disassembler for the MZ-800 Emulator

Complete Z80 disassembler with support for all documented and undocumented
instructions. Designed for debugger integration in the Sharp MZ-800 emulator.

Current version: **0.1** (2026-04-01) - see [CHANGELOG.md](CHANGELOG.md)

## Features

- **Structured output** - each instruction decoded into a structure with parsed
  operands, timing, register and flag maps
- **Control flow analysis** - detection of jumps, calls, returns; condition evaluation
- **Symbol table** - replace addresses with symbolic names (ROM routines, I/O ports)
- **Undocumented instructions** - SLL, IXH/IXL/IYH/IYL, DD CB register copy
- **Flexible formatting** - 4 hex styles, uppercase/lowercase, raw bytes
- **Backward compatibility** - drop-in replacement for z80ex_dasm()
- **Thread-safe** - no global variables or static buffers
- **Zero dependencies** - the only external include is `types.h` (u8, u16, s8)

## Quick Start

### 1. Basic usage

```c
#include "z80_dasm.h"

/* Memory read callback for the emulator */
u8 my_read(u16 addr, void *user_data) {
    u8 *mem = (u8 *)user_data;
    return mem[addr];
}

/* Disassemble one instruction */
z80_dasm_inst_t inst;
int len = z80_dasm(&inst, my_read, memory, 0x0100);

/* Format to text */
char buf[64];
z80_dasm_format_t fmt;
z80_dasm_format_default(&fmt);
z80_dasm_to_str(buf, sizeof(buf), &inst, &fmt);

printf("%04X: %s\n", inst.addr, buf);
/* Output: 0100: LD A,#FF */
```

### 2. Debugger usage

```c
/* Display a block of instructions */
z80_dasm_inst_t insts[20];
int count = z80_dasm_block(insts, 20, my_read, memory, pc, pc + 0x40);

for (int i = 0; i < count; i++) {
    char line[64];
    z80_dasm_to_str(line, sizeof(line), &insts[i], &fmt);

    /* Highlight current instruction */
    const char *marker = (insts[i].addr == pc) ? ">>>" : "   ";
    printf("%s %04X: %s\n", marker, insts[i].addr, line);
}
```

### 3. Control flow analysis

```c
z80_dasm(&inst, my_read, memory, pc);

/* Where does the instruction jump? */
u16 target = z80_dasm_target_addr(&inst);
if (target != 0xFFFF) {
    printf("Jump target: %04X\n", target);
}

/* Will the conditional jump be taken? */
u8 flags = cpu.af.b.l;  /* current F register */
if (z80_dasm_branch_taken(&inst, flags)) {
    printf("Branch TAKEN\n");
} else {
    printf("Branch NOT taken\n");
}

/* Which registers are modified? */
u16 written = z80_dasm_regs_written(&inst);
if (written & Z80_REG_A)  printf("Modifies A\n");
if (written & Z80_REG_HL) printf("Modifies HL\n");
if (written & Z80_REG_F)  printf("Modifies flags\n");
```

### 4. Symbol table

```c
/* Create table with MZ-800 system addresses */
z80_symtab_t *sym = z80_symtab_create();
z80_symtab_add(sym, 0x0000, "reset");
z80_symtab_add(sym, 0x0038, "irq_handler");
z80_symtab_add(sym, 0x00AD, "rom_getchar");
z80_symtab_add(sym, 0x0012, "rom_putchar");
z80_symtab_add(sym, 0xD000, "video_ram");

/* Disassemble with symbols */
z80_dasm(&inst, my_read, memory, 0x0100);
char buf[64];
z80_dasm_to_str_sym(buf, sizeof(buf), &inst, &fmt, sym);
/* "CALL rom_getchar" instead of "CALL #00AD" */

/* Manual symbol resolution */
z80_dasm_symbols_t syms;
z80_dasm_resolve_symbols(&inst, sym, &syms);
if (syms.target_sym) printf("Calls: %s\n", syms.target_sym);
if (syms.mem_sym)    printf("Accesses: %s\n", syms.mem_sym);

/* Cleanup */
z80_symtab_destroy(sym);
```

### 5. Format configuration

```c
z80_dasm_format_t fmt;
z80_dasm_format_default(&fmt);

/* Intel/Zilog style: FFh, 1234h */
fmt.hex_style = Z80_HEX_H_SUFFIX;

/* Lowercase: ld a,0ffh */
fmt.uppercase = 0;

/* Show raw bytes: "3E FF  LD A,#FF" */
fmt.show_bytes = 1;

/* Show address: "0100  LD A,#FF" */
fmt.show_addr = 1;

/* JR as relative offset: "JR $+5" instead of "JR #0107" */
fmt.rel_as_absolute = 0;

/* Alternative IX half-register names: HX/LX instead of IXH/IXL */
fmt.undoc_ix_style = 1;
```

### 6. Relative address conversion

```c
/* Where does JR at address 0x100 with offset +5 jump? */
u16 target = z80_rel_to_abs(0x0100, 5);  /* -> 0x0107 */

/* What offset is needed to jump from 0x100 to 0x120? */
s8 offset;
if (z80_abs_to_rel(0x0100, 0x0120, &offset) == 0) {
    printf("Offset: %d\n", offset);  /* 30 */
} else {
    printf("Target out of JR range!\n");
}
```

### 7. z80ex compatibility mode

```c
/* Drop-in replacement for the original z80ex_dasm() */
char output[64];
int t_states, t_states2;

int len = z80ex_dasm(output, sizeof(output), 0,
                     &t_states, &t_states2,
                     my_z80ex_readbyte_cb, addr, user_data);

/* Output identical to original z80ex */
```

## API Overview

### Core

| Function | Description |
|----------|-------------|
| `z80_dasm()` | Disassemble one instruction into `z80_dasm_inst_t` |
| `z80_dasm_block()` | Disassemble a block of instructions in address range |
| `z80_dasm_find_inst_start()` | Heuristically find instruction start (backward scrolling) |

### Formatting

| Function | Description |
|----------|-------------|
| `z80_dasm_format_default()` | Initialize format to default values |
| `z80_dasm_to_str()` | Format instruction into text buffer |
| `z80_dasm_to_str_sym()` | Format with address-to-symbol replacement |

### Symbol Table

| Function | Description |
|----------|-------------|
| `z80_symtab_create()` | Create new table |
| `z80_symtab_destroy()` | Destroy table and free memory |
| `z80_symtab_add()` | Add symbol (address -> name) |
| `z80_symtab_remove()` | Remove symbol by address |
| `z80_symtab_clear()` | Remove all symbols |
| `z80_symtab_lookup()` | Look up symbol by address |
| `z80_symtab_count()` | Number of symbols in table |
| `z80_dasm_resolve_symbols()` | Resolve symbols for an instruction |

### Analysis

| Function | Description |
|----------|-------------|
| `z80_dasm_target_addr()` | Target address of jump/call |
| `z80_dasm_branch_taken()` | Evaluate branch condition |
| `z80_dasm_regs_read()` | Bitmask of registers read |
| `z80_dasm_regs_written()` | Bitmask of registers written |
| `z80_dasm_flags_affected()` | Bitmask of flags affected |

### Utilities

| Function | Description |
|----------|-------------|
| `z80_rel_to_abs()` | Relative offset -> absolute address |
| `z80_abs_to_rel()` | Absolute address -> relative offset |

### Compatibility

| Function | Description |
|----------|-------------|
| `z80ex_dasm()` | Drop-in replacement for original z80ex_dasm() |

## Key Data Types

### z80_dasm_inst_t - disassembly result

```c
typedef struct {
    u16 addr;               /* instruction address */
    u8  bytes[4];           /* raw bytes (max 4) */
    u8  length;             /* instruction length (1-4) */
    u8  t_states;           /* T-states (base) */
    u8  t_states2;          /* T-states on branch (0 = none) */
    z80_flow_type_t flow;   /* control flow type */
    z80_inst_class_t cls;   /* official / undocumented / invalid */
    z80_operand_t op1;      /* first operand */
    z80_operand_t op2;      /* second operand */
    const char *mnemonic;   /* "LD", "ADD", "JP", ... */
    u8  flags_affected;     /* bitmask of affected flags */
    u16 regs_read;          /* bitmask of registers read */
    u16 regs_written;       /* bitmask of registers written */
} z80_dasm_inst_t;
```

### z80_flow_type_t - control flow

| Value | Description | Examples |
|-------|-------------|----------|
| `Z80_FLOW_NORMAL` | Normal instruction | LD, ADD, AND, NOP |
| `Z80_FLOW_JUMP` | Unconditional jump | JP nn, JR e |
| `Z80_FLOW_JUMP_COND` | Conditional jump | JP NZ,nn / JR Z,e / DJNZ |
| `Z80_FLOW_CALL` | Unconditional call | CALL nn |
| `Z80_FLOW_CALL_COND` | Conditional call | CALL NZ,nn |
| `Z80_FLOW_RET` | Unconditional return | RET |
| `Z80_FLOW_RET_COND` | Conditional return | RET NZ |
| `Z80_FLOW_RST` | Restart vector | RST 38 |
| `Z80_FLOW_HALT` | CPU halted | HALT |
| `Z80_FLOW_JUMP_INDIRECT` | Indirect jump | JP (HL), JP (IX) |
| `Z80_FLOW_RETI` | Return from IRQ | RETI |
| `Z80_FLOW_RETN` | Return from NMI | RETN |

### z80_inst_class_t - instruction classification

| Value | Description | Examples |
|-------|-------------|----------|
| `Z80_CLASS_OFFICIAL` | Documented (Zilog manual) | LD A,B / ADD HL,BC |
| `Z80_CLASS_UNDOCUMENTED` | Undocumented but functional | SLL, IXH, DD CB copy |
| `Z80_CLASS_INVALID` | Invalid sequence | DD DD, ED 00 (shown as NOP*) |

## Build

```bash
cd dasm-z80
make           # build libdasm_z80.a
make test      # build and run tests
make clean     # remove build artifacts
```

Requirements: GCC (MinGW64 / MSYS2), GNU Make.

## Architecture

```
z80_dasm()          ->  z80_dasm_inst_t  (structured result)
                              |
          +-------------------+-------------------+
          |                   |                   |
  z80_dasm_to_str()   z80_dasm_target_addr()  z80_dasm_regs_*()
  z80_dasm_to_str_sym()  z80_dasm_branch_taken()  z80_dasm_flags_affected()
          |
  z80_dasm_format_t
  z80_symtab_t
```

Principle: **parse once, query many**. `z80_dasm()` decodes the instruction once. All other functions only read data from the structure.

## Files

| File | Description |
|------|-------------|
| `z80_dasm.h` | Public API (the only header for users) |
| `z80_dasm_internal.h` | Internal types (opcode table, helper macros) |
| `z80_dasm.c` | Decoder core + utilities (rel/abs conversion) |
| `z80_dasm_tables.c` | 7 opcode tables (1792 entries) |
| `z80_dasm_format.c` | Text output formatting |
| `z80_dasm_symtab.c` | Symbol table |
| `z80_dasm_analysis.c` | Control flow analysis |
| `z80_dasm_compat.c` | z80ex compatibility wrapper |

## License

MIT - See [LICENSE](../LICENSE)
