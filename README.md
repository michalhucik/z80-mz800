# cpu-z80 - Accurate and Fast Zilog Z-80A Emulator

A Z80A CPU emulator and disassembler in portable C99, developed as the CPU core for [mz800emu](https://sourceforge.net/projects/mz800emu/) - a cycle-accurate emulator of the Sharp MZ-800, MZ-700 and MZ-1500 home computers.

## Motivation

The mz800emu project originally used a modified version of [z80ex](https://sourceforge.net/projects/z80ex/) as its CPU core. While z80ex is a solid and well-tested emulator, several factors motivated developing a replacement:

- **Performance**: z80ex uses function-pointer dispatch with each opcode as a separate function. cpu-z80 uses computed goto, so its dispatch core is modestly faster than z80ex per-step (~1.05x), and its optional batch mode reaches **~2.6x** for non-cycle-accurate bulk execution. (The often-quoted "2.8x" is the batch figure; in the per-step mode a real cycle-accurate emulator uses, raw throughput vs z80ex is roughly a wash - see the [Benchmark](#benchmark) section for the honest, full breakdown.)
- **Accuracy**: z80ex does not emulate the LD A,I/R hardware bug, the Q register, or provide per-M-cycle T-state tracking.
- **License**: z80ex is GPL-2.0. cpu-z80 is **MIT licensed**, making it easier to integrate into projects with various licensing requirements.
- **Size**: z80ex is ~13,500 lines (mostly Perl-generated). cpu-z80 is ~3,500 lines of hand-written, heavily commented C.

## Features

- Complete Z80A instruction set (all documented + undocumented)
- CB, ED, DD/FD, DD CB/FD CB prefix instructions
- Undocumented: IXH/IXL/IYH/IYL operations, SLL (CB 30-37), indexed bit-op register copy
- Precise T-state counting for every instruction
- Per-M-cycle T-state tracking (`op_tstate`) for contended memory timing
- All flags correct including undocumented bits F3 (bit 3) and F5 (bit 5)
- Internal MEMPTR/WZ register with correct F3/F5 influence
- Internal Q register for correct SCF/CCF F3/F5 behavior (discovered by Patrik Rak, 2018)
- Interrupt modes IM0, IM1, IM2
- NMI with IFF2 preservation
- EI delay (interrupt deferred by one instruction after EI)
- LD A,I/R hardware bug (INT after LD A,I/R resets PF to 0)
- HALT with interrupt wakeup
- Multi-instance: multiple independent CPU instances with own callbacks and user_data
- Two execution cores from one shared source: batch (`z80_execute`, register cache) and per-step (`z80_step`, registers in `cpu->`) for cycle-accurate hosts - bit-identical
- Optional RAM fast-path (`-DCPU_Z80_RAM_FASTPATH`) - inline page-table memory access bypassing callbacks for clean RAM
- 441 unit tests (Q register, ei/di/im/halt/nmi, iff_change, cpu_ctrl_event, call/ret, HALT PC, EI-delay)
- **ZEXALL validated** - passes all 67 tests of the Z80 Instruction Exerciser (Frank Cringle / J.G. Harston) through **both** the batch and the per-step core, including undocumented instructions and flags

### Advantages over z80ex

| Feature | z80ex 1.1.21 | cpu-z80 |
|---|---|---|
| LD A,I/R INT bug | no | **yes** |
| Q register (SCF/CCF F3/F5) | no | **yes** |
| Per-M-cycle T-state tracking | no | **yes** |
| Daisy chain (RETI callback) | no | **yes** |
| INTACK callback | no | **yes** |
| EI callback | no | **yes** |
| Post-step callback | no | **yes** |
| Wait states from callbacks | no | **yes** |
| Batch execution (z80_execute) | no | **yes** |
| Dedicated per-step core (z80_step) | n/a (per-step only) | **yes** |
| Optional RAM fast-path | no | **yes** |
| Computed goto dispatch | no | **yes** |
| Local register cache | no | **yes** |
| DAA lookup table | no | **yes** |
| Source code | ~13,500 lines (generated) | ~3,500 lines (hand-written) |
| License | GPL-2.0 | **MIT** |

## Quick Start

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

### API Extensions over z80ex

- `z80_execute(cpu, target_cycles)` - batch execution (z80ex only has single-step)
- `z80_irq(cpu, vector)` - explicit interrupt vector (in addition to callback-based `z80_int()`)
- `z80_add_wait_states(cpu, wait)` - inject extra T-states from callbacks (contended memory, PSG READY)
- `z80_set_post_step(cpu, fn, data)` - per-instruction callback (WAIT timing)
- `z80_set_ei(cpu, fn, data)` - EI instruction callback (interrupt logic synchronization)
- `z80_set_intack(cpu, fn, data)` - INTACK signal for daisy chain peripherals
- `z80_set_reti(cpu, fn, data)` - RETI notification for daisy chain
- `op_tstate` field - T-states from instruction start, incremented by each M-cycle
- Dynamic callback changes at runtime via `z80_set_mread()`, `z80_set_pwrite()`, etc.

## Benchmark

Test: 16,777,216 loop iterations, mixed instruction workload. GCC 15.2.0, MSYS2/MinGW64. All emulators and all cpu-z80 modes produce identical cycle counts (2,214,609,436 T-states).

cpu-z80 can be driven two ways, and that choice dominates throughput. `z80_execute` (batch) runs many T-states in one internal loop - fastest, but a cycle-accurate emulator that synchronises peripherals mid-instruction cannot use it. `z80_step` (per-step) runs one instruction and returns - what a real emulator uses. z80ex is per-step only.

| Mode (same session, O2 avg) | MHz | vs z80ex |
|---|---|---|
| z80ex-mz800 (per-step) | 1,072 | 1.00x |
| cpu-z80 batch | 2,822 | 2.63x |
| cpu-z80 dispatch core ¹ | 1,123 | 1.05x |
| cpu-z80 per-step (`z80_step`) | ~830 | 0.77x |
| cpu-z80 per-step + fast-path ² | 627 | 0.58x |

¹ Per-step core with interrupt acceptance excluded - the fair comparison to `z80ex_step`, which also runs only the opcode. cpu-z80's dispatch is ~5% faster.
² Slower *here* only because this microbenchmark's memory callback is trivial; the fast-path pays off against an expensive (banked) callback, not a `return ram[addr]`.

Honest takeaway: the batch core is ~2.6x z80ex but a real emulator can't use it; the per-step *dispatch* is ~5% faster, but `z80_step` also integrates per-instruction interrupt + post-step handling that `z80ex_step` leaves to the caller, so full per-step throughput is roughly a wash. The per-step cost is a deliberate trade-off, not a defect: you forfeit the batch speedup to get cycle-accurate mid-instruction timing (the price of accuracy), and `z80_step` does integrated work z80ex offloads to the consumer (the price of built-in functionality). The accurate dispatch core itself is *faster* than z80ex - accuracy is not what slows cpu-z80 down. The real reasons to choose cpu-z80 are accuracy, the MIT license, the small source, and - for expensive memory maps - the RAM fast-path. Full methodology and raw data: [docs/benchmark_en.txt](docs/benchmark_en.txt).

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
cpu-z80/        Z80 emulator
dasm-z80/       Z80 disassembler library
tests/          cpu-z80 unit tests (441 cases)
tests-zexall/   cpu-z80 ZEXALL validation (67/67 PASS)
tests-dasm/     dasm-z80 regression tests (symbol resolver)
bench/          Benchmark suite
docs/           Reference documentation, benchmark results
```

## Building

Requires GCC or Clang (for computed goto), C99, little-endian platform.

```bash
# Run tests
cd tests && make run        # 441 cpu-z80 cases (per-step core via z80_step)
cd tests-zexall && make zexall  # ZEXALL via the batch core
cd tests-zexall && make zexall_z80_perstep.exe && ./zexall_z80_perstep.exe zexall.com  # ZEXALL via the per-step core
cd tests-dasm && make run   # dasm-z80 symbol resolver regression

# Run benchmarks (batch / per-step / step+fast-path / dispatch core)
cd bench && make compare    # -O2 and -O3
```

cpu-z80 is a single compilation unit (`cpu/z80.c`, which includes `cpu/z80_execute.inc`, plus `cpu/z80.h` and `utils/types.h`). No build system required - just add to your project:

```bash
gcc -O2 -I path/to/cpu-z80 -c cpu/z80.c -o z80.o
# optional inline RAM fast-path (see API §4.7):
gcc -O2 -DCPU_Z80_RAM_FASTPATH -I path/to/cpu-z80 -c cpu/z80.c -o z80.o
```

## Documentation

The emulator includes `API_en.txt` / `API_cz.txt` and `CHANGELOG_en.txt` / `CHANGELOG_cz.txt` with full API reference and version history in both English and Czech.

Note: In-code comments are in Czech. I apologize for the inconvenience - the project originated as a personal tool for the Czech mz800emu emulator and the comments reflect that heritage.

## License

MIT

## Author

Michal Hucik - [z80-mz800](https://github.com/michalhucik/z80-mz800)
