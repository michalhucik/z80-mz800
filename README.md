# cpu-z80 - Accurate and Fast Zilog Z-80A Emulator

A Z80A CPU emulator and disassembler in portable C99, developed as the CPU core for [mz800emu](https://sourceforge.net/projects/mz800emu/) - a cycle-accurate emulator of the Sharp MZ-800, MZ-700 and MZ-1500 home computers.

## Motivation

The mz800emu project originally used a modified version of [z80ex](https://sourceforge.net/projects/z80ex/) as its CPU core. While z80ex is a solid and well-tested emulator, several factors motivated developing a replacement:

- **Performance**: z80ex uses function-pointer dispatch with each opcode as a separate function. cpu-z80 uses computed goto with a local register cache and batch execution, achieving **~3x the throughput**.
- **Accuracy**: z80ex does not emulate the LD A,I/R hardware bug (INT after LD A,I/R resets PF), which matters for accurate Z80A emulation.
- **License**: z80ex is GPL-2.0. cpu-z80 is **MIT licensed**, making it easier to integrate into projects with various licensing requirements.
- **Size**: z80ex is ~13,500 lines (mostly Perl-generated). cpu-z80 is ~2,000 lines of hand-written C.

## Features

- Complete Z80A instruction set (all documented + undocumented)
- CB, ED, DD/FD, DD CB/FD CB prefix instructions
- Undocumented: IXH/IXL/IYH/IYL operations, SLL (CB 30-37), indexed bit-op register copy
- Precise T-state counting for every instruction
- All flags correct including undocumented bits F3 (bit 3) and F5 (bit 5)
- Internal MEMPTR/WZ register with correct F3/F5 influence
- Interrupt modes IM0, IM1, IM2
- NMI with IFF2 preservation
- EI delay (interrupt deferred by one instruction after EI)
- LD A,I/R hardware bug (INT after LD A,I/R resets PF to 0)
- HALT with interrupt wakeup
- 402 unit tests covering all instructions, flags, timing, interrupts, and MEMPTR behavior
- **ZEXALL validated** - passes all 67 tests of the Z80 Instruction Exerciser (Frank Cringle / J.G. Harston), including undocumented instructions and flags

### Advantages over z80ex

| Feature | z80ex 1.1.21 | cpu-z80 |
|---|---|---|
| LD A,I/R INT bug | no | **yes** |
| Daisy chain (RETI callback) | no | **yes** |
| INTACK callback | no | **yes** |
| EI callback | no | **yes** |
| Post-step callback | no | **yes** |
| Wait states from callbacks | no | **yes** |
| Batch execution (z80_execute) | no | **yes** |
| Computed goto dispatch | no | **yes** |
| Local register cache | no | **yes** |
| DAA lookup table | no | **yes** |
| Source code | ~13,500 lines (generated) | ~2,000 lines (hand-written) |
| License | GPL-2.0 | **MIT** |

## Variants

The project provides three emulator variants with different trade-offs:

### cpu-z80 v0.1 - Maximum Performance

The fastest variant. Uses global callback pointers with a simple signature (`u8 fn(u16 addr)`) and stack-allocated CPU state. Single-instance only.

Best for projects that don't need z80ex API compatibility or multiple CPU instances.

```c
z80_t cpu;
z80_init(&cpu);
z80_set_mem_read(mem_read);
z80_set_mem_write(mem_write);
z80_set_mem_fetch(mem_read);
z80_execute(&cpu, 69888);
```

### cpu-z80 single-v0.2 - New API, Single Instance

Same new API as multi-v0.2 (see below), but stores callbacks globally for maximum speed. Only one active CPU instance at a time. Uses `FORCE_O2` attribute on the execute loop to prevent GCC -O3 regressions with global function pointers.

Drop-in replacement for multi-v0.2 - switch by changing the include path, no code changes needed.

### cpu-z80 multi-v0.2 - Multi-Instance (Recommended)

The most capable variant. Each CPU instance carries its own callbacks and `user_data` - multiple independent instances can run simultaneously. Callback signature is compatible with z80ex (`cpu, addr, m1_state, user_data`).

Callback pointers are cached in local variables at the top of `z80_execute()`, eliminating the overhead of repeated structure dereference. **Performance matches the v0.1 single-instance variant on -O2.**

```c
z80_t *cpu = z80_create(
    mem_read, NULL,    /* memory read (handles both fetch and data) */
    mem_write, NULL,   /* memory write */
    io_read, NULL,     /* I/O port read */
    io_write, NULL,    /* I/O port write */
    NULL, NULL         /* interrupt vector read (optional) */
);
z80_execute(cpu, 69888);
z80_destroy(cpu);
```

### API Extensions (v0.2 variants)

Both v0.2 variants extend the z80ex-compatible base with:

- `z80_execute(cpu, target_cycles)` - batch execution (z80ex only has single-step)
- `z80_irq(cpu, vector)` - explicit interrupt vector (in addition to callback-based `z80_int()`)
- `z80_add_wait_states(cpu, wait)` - inject extra T-states from callbacks (contended memory, PSG READY)
- `z80_set_post_step(cpu, fn, data)` - per-instruction callback (MZ-700 per-line WAIT timing)
- `z80_set_ei(cpu, fn, data)` - EI instruction callback (interrupt logic synchronization)
- `z80_set_intack(cpu, fn, data)` - INTACK signal for daisy chain peripherals
- `z80_set_reti(cpu, fn, data)` - RETI notification for daisy chain
- Dynamic callback changes at runtime via `z80_set_mread()`, `z80_set_pwrite()`, etc.

## Benchmark

Test: 16,777,216 loop iterations, mixed instruction workload. GCC, MSYS2/MinGW64.

All emulators produce identical cycle counts (2,214,609,436 T-states).

| Emulator | -O2 (MHz) | vs z80ex | -O3 (MHz) |
|---|---|---|---|
| z80ex 1.1.21 | 1,057 | 1.0x | 1,120 |
| **cpu-z80 v0.1** | **3,118** | **2.95x** | **3,192** |
| **cpu-z80 multi-v0.2** | **3,080** | **2.91x** | 2,954 |
| **cpu-z80 single-v0.2** | **3,128** | **2.96x** | **3,129** |

## Disassembler (dasm-z80)

A full-featured Z80 disassembler library with a "parse once, query many" architecture. Designed for debugger integration in emulators.

Features:
- All documented and undocumented Z80 instructions
- Structured output (`z80_dasm_inst_t`) with operands, timing, flow type, register/flag maps
- Configurable text formatting (hex styles, uppercase, address/byte display)
- Symbol table with address-to-name resolution
- Control flow analysis (target address, branch prediction)
- Backward instruction boundary search (heuristic, for scroll-back in debugger view)
- Drop-in `z80ex_dasm()` compatibility wrapper
- Thread-safe (no global state)

### Quick Start

```c
#include "z80_dasm.h"

z80_dasm_inst_t inst;
z80_dasm(&inst, my_read_fn, NULL, 0x0000);

char buf[64];
z80_dasm_to_str(buf, sizeof(buf), &inst, NULL);
printf("%s\n", buf);  /* e.g. "LD A,#42" */
```

### Disassembler API

```c
/* Core */
int  z80_dasm(z80_dasm_inst_t *inst, z80_dasm_read_fn read_fn,
              void *user_data, u16 addr);
int  z80_dasm_block(z80_dasm_inst_t *out, int max_inst,
                    z80_dasm_read_fn read_fn, void *user_data,
                    u16 start_addr, u16 end_addr);
u16  z80_dasm_find_inst_start(z80_dasm_read_fn read_fn, void *user_data,
                              u16 target_addr, u16 search_from);

/* Formatting */
void z80_dasm_format_default(z80_dasm_format_t *fmt);
int  z80_dasm_to_str(char *buf, int buf_size,
                     const z80_dasm_inst_t *inst, const z80_dasm_format_t *fmt);
int  z80_dasm_to_str_sym(char *buf, int buf_size,
                         const z80_dasm_inst_t *inst, const z80_dasm_format_t *fmt,
                         const z80_symtab_t *symbols);

/* Symbol table */
z80_symtab_t *z80_symtab_create(void);
void z80_symtab_destroy(z80_symtab_t *tab);
int  z80_symtab_add(z80_symtab_t *tab, u16 addr, const char *name);
const char *z80_symtab_lookup(const z80_symtab_t *tab, u16 addr);

/* Analysis */
u16  z80_dasm_target_addr(const z80_dasm_inst_t *inst);
int  z80_dasm_branch_taken(const z80_dasm_inst_t *inst, u8 flags);
u16  z80_dasm_regs_read(const z80_dasm_inst_t *inst);
u16  z80_dasm_regs_written(const z80_dasm_inst_t *inst);

/* z80ex compatibility */
int  z80ex_dasm(char *output, int output_size, unsigned flags,
                int *t_states, int *t_states2,
                z80ex_dasm_readbyte_cb readbyte_cb,
                Z80EX_WORD addr, void *user_data);
```

## Project Structure

```
cpu-z80/                  v0.1 - single-instance, legacy API
cpu-z80-multi-v0.2/       multi-instance, new API (recommended)
cpu-z80-single-v0.2/      single-instance, new API
dasm-z80/                 Z80 disassembler library
tests/                    402 tests for cpu-z80 v0.1
tests-multi/              402 tests for multi-v0.2
tests-single/             402 tests for single-v0.2
bench/                    Benchmark suite
docs/                     Reference documentation, benchmark results
```

## Building

Requires GCC or Clang (for computed goto), C99, little-endian platform.

```bash
# Run tests
cd tests-multi && make run     # 402 tests

# Run benchmarks
cd bench && make compare       # all variants, -O2 and -O3
```

Each cpu-z80 variant is a single compilation unit (`cpu/z80.c` + `cpu/z80.h` + `utils/types.h`). No build system required - just add to your project:

```bash
gcc -O2 -I path/to/cpu-z80-multi-v0.2 -c cpu/z80.c -o z80.o
```

## Documentation

Each emulator variant includes `API_en.txt` / `API_cz.txt` and `CHANGELOG_en.txt` / `CHANGELOG_cz.txt` with full API reference and version history in both English and Czech.

Note: In-code comments are in Czech. I apologize for the inconvenience - the project originated as a personal tool for the Czech mz800emu emulator and the comments reflect that heritage.

## License

MIT

## Author

Michal Hucik - [z80-mz800](https://github.com/michalhucik/z80-mz800)
