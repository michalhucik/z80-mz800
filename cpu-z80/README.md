# cpu-z80 v0.1 - API Reference

Accurate and fast Zilog Z-80A processor emulator.

## Overview

cpu-z80 is a Z80A processor emulator implemented in ANSI C. It provides:

- Complete instruction set including undocumented instructions
- Precise T-state (cycle) counting
- Correct behavior of all flags including undocumented F3/F5
- Internal MEMPTR/WZ register
- Interrupt modes IM0, IM1, IM2 and NMI
- Callback system for memory, I/O and interrupts

**Files:**

| File | Description |
|---|---|
| `cpu/z80.h` | Public API (types, structures, functions) |
| `cpu/z80.c` | Implementation |
| `utils/types.h` | Type aliases (u8, u16, u32, s8, ...) |

## Basic Usage

```c
#include "cpu/z80.h"

/* 64KB RAM */
static u8 ram[65536];

static u8 mem_read(u16 addr) { return ram[addr]; }
static void mem_write(u16 addr, u8 data) { ram[addr] = data; }

int main(void) {
    z80_t cpu;
    z80_init(&cpu);
    z80_set_mem_read(mem_read);
    z80_set_mem_write(mem_write);
    z80_set_mem_fetch(mem_read);

    /* Load program into RAM... */

    /* Emulate 1 frame (69888 T-states for ZX Spectrum) */
    z80_execute(&cpu, 69888);

    return 0;
}
```

## Types

### Basic types (utils/types.h)

| Type | Description |
|---|---|
| `u8` | unsigned 8-bit (uint8_t) |
| `u16` | unsigned 16-bit (uint16_t) |
| `u32` | unsigned 32-bit (uint32_t) |
| `u64` | unsigned 64-bit (uint64_t) |
| `s8` | signed 8-bit (int8_t) |
| `s16` | signed 16-bit (int16_t) |
| `s32` | signed 32-bit (int32_t) |

### z80_pair_t

```c
typedef union {
    u16 w;                    /* 16-bit access */
    struct { u8 l, h; };     /* 8-bit access (little-endian) */
} z80_pair_t;
```

Register pair with access to the full 16-bit value (`w`) or two 8-bit halves (`h` = high byte, `l` = low byte).

### z80_t

Main CPU state structure:

**Registers:**

| Member | Description |
|---|---|
| `z80_pair_t af, bc, de, hl` | Main register set |
| `z80_pair_t af2, bc2, de2, hl2` | Alternate set (EX/EXX) |
| `z80_pair_t ix, iy` | Index registers |
| `z80_pair_t wz` | Internal MEMPTR/WZ register |
| `u16 sp` | Stack Pointer |
| `u16 pc` | Program Counter |
| `u8 i` | Interrupt Vector (upper byte for IM2) |
| `u8 r` | Memory Refresh (lower 7 bits) |

**Interrupt system:**

| Member | Description |
|---|---|
| `u8 iff1, iff2` | Interrupt Flip-Flops |
| `u8 im` | Interrupt Mode (0, 1, 2) |
| `bool halted` | CPU is in HALT state |
| `bool int_pending` | Pending maskable interrupt |
| `bool nmi_pending` | Pending NMI |
| `bool ei_delay` | EI delay (blocks INT for 1 instruction) |
| `bool ld_a_ir` | LD A,I/R HW bug flag |
| `u8 int_vector` | Interrupt vector (for IM0/IM2) |

**Counters:**

| Member | Description |
|---|---|
| `u32 cycles` | T-states in current frame |
| `u32 total_cycles` | Total T-states since last reset |
| `int wait_cycles` | Extra wait states from I/O callbacks |

### Callback types

```c
typedef u8   (*z80_read_fn)(u16 addr);       /* memory/IO read */
typedef void (*z80_write_fn)(u16 addr, u8 data); /* memory/IO write */
typedef void (*z80_intack_fn)(void);          /* INTACK signal */
typedef void (*z80_reti_fn)(void);            /* RETI notification */
typedef bool (*z80_int_line_fn)(void);        /* /INT line polling */
typedef void (*z80_ei_fn)(void);              /* EI instruction */
```

## Functions

### Initialization and reset

- **`void z80_init(z80_t *cpu)`** - Initialize CPU instance. Must be called first. Initializes lookup tables on first call.
- **`void z80_reset(z80_t *cpu)`** - Reset CPU to default state. Does not reinitialize tables.

### Callback setup

- **`void z80_set_mem_read(z80_read_fn fn)`** - General memory read (LD, PUSH/POP, etc.)
- **`void z80_set_mem_write(z80_write_fn fn)`** - Memory write
- **`void z80_set_mem_fetch(z80_read_fn fn)`** - Opcode fetch (M1 cycle, separate for contended memory)
- **`void z80_set_io_read(z80_read_fn fn)`** - I/O port read (IN). Port is 16-bit: `IN A,(n)` -> `A:n`
- **`void z80_set_io_write(z80_write_fn fn)`** - I/O port write (OUT). Port is 16-bit.
- **`void z80_set_intack(z80_intack_fn fn)`** - INTACK signal for daisy chain
- **`void z80_set_reti_fn(z80_reti_fn fn)`** - RETI notification for daisy chain
- **`void z80_set_int_line(z80_int_line_fn fn)`** - Level-sensitive /INT line polling
- **`void z80_set_ei_fn(z80_ei_fn fn)`** - EI instruction callback
- **`void z80_set_post_step_fn(void (*fn)(void))`** - Per-instruction callback (MZ-700 WAIT)

### Emulation

- **`int z80_step(z80_t *cpu)`** - Execute one instruction. Returns T-states.
- **`int z80_execute(z80_t *cpu, int target_cycles)`** - Batch execute until target T-states reached. Returns actual count (>= target).

### Interrupts

- **`void z80_irq(z80_t *cpu, u8 vector)`** - Trigger maskable interrupt. IM0: `vector & 0x38`, IM1: ignored, IM2: `(I << 8) | (vector & 0xFE)`. T-states: IM0/1 = 13T, IM2 = 19T.
- **`void z80_nmi(z80_t *cpu)`** - Trigger NMI. Jump to 0x0066, IFF2 = IFF1, IFF1 = 0. 11T.

### Utility

- **`void z80_add_wait_states(int wait)`** - Add extra T-states from callbacks.

## Flags

| Constant | Value | Description |
|---|---|---|
| `Z80_FLAG_C` | 0x01 | Carry |
| `Z80_FLAG_N` | 0x02 | Subtract |
| `Z80_FLAG_PV` | 0x04 | Parity/Overflow |
| `Z80_FLAG_3` | 0x08 | Undocumented bit 3 |
| `Z80_FLAG_H` | 0x10 | Half Carry |
| `Z80_FLAG_5` | 0x20 | Undocumented bit 5 |
| `Z80_FLAG_Z` | 0x40 | Zero |
| `Z80_FLAG_S` | 0x80 | Sign |

Access: `cpu.af.l` contains the F register.

## Special Behavior

### MEMPTR/WZ register

Internal 16-bit register affecting undocumented flags F3/F5. Accessible as `cpu.wz`. See `API_en.txt` for complete setting rules.

### LD A,I/R HW bug

On real Z80A, if INT arrives immediately after LD A,I or LD A,R, PF is reset to 0 instead of IFF2.

### EI delay

After EI, interrupts are deferred by one instruction (allows `EI; RET` without risk).

### Undocumented instructions

IXH/IXL/IYH/IYL, SLL (CB 30-37), DD CB/FD CB register copy, IN F,(C), OUT (C),0.

## Integration

```bash
gcc -O2 -I /path/to/cpu-z80 -c cpu/z80.c -o z80.o
```

Requirements: C99+, GCC/Clang (computed goto), little-endian platform.

## License

MIT - See [LICENSE](../LICENSE)
