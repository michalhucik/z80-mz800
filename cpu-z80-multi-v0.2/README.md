# cpu-z80 multi-v0.2 - API Reference

Accurate and fast multi-instance Zilog Z-80A processor emulator.

## Overview

Multi-instance Z80A emulator in ANSI C. Each CPU instance carries its own callbacks and `user_data` - multiple independent instances can run simultaneously.

- Complete instruction set including undocumented instructions
- Precise T-state (cycle) counting
- Correct behavior of all flags including undocumented F3/F5
- Internal MEMPTR/WZ register
- Interrupt modes IM0, IM1, IM2 and NMI
- Callback system with user_data for memory, I/O and interrupts
- API compatible with cpu-z80 single-v0.2 (drop-in replacement)

**Files:**

| File | Description |
|---|---|
| `cpu/z80.h` | Public API (types, structures, functions) |
| `cpu/z80.c` | Implementation |
| `utils/types.h` | Type aliases (u8, u16, u32, s8, ...) |

## Basic Usage

```c
#include "cpu/z80.h"

static u8 ram[65536];

static u8 mem_read(z80_t *cpu, u16 addr, int m1_state, void *data) {
    (void)cpu; (void)m1_state; (void)data;
    return ram[addr];
}

static void mem_write(z80_t *cpu, u16 addr, u8 value, void *data) {
    (void)cpu; (void)data;
    ram[addr] = value;
}

static u8 io_read(z80_t *cpu, u16 port, void *data) {
    (void)cpu; (void)data;
    return 0xFF;
}

static void io_write(z80_t *cpu, u16 port, u8 value, void *data) {
    (void)cpu; (void)port; (void)value; (void)data;
}

int main(void) {
    z80_t *cpu = z80_create(
        mem_read, NULL,
        mem_write, NULL,
        io_read, NULL,
        io_write, NULL,
        NULL, NULL       /* intread - not needed */
    );

    /* Load program into RAM... */

    /* Emulate 1 frame (69888 T-states for ZX Spectrum) */
    z80_execute(cpu, 69888);

    z80_destroy(cpu);
    return 0;
}
```

## Types

### Basic types (utils/types.h)

| Type | Description |
|---|---|
| `u8, u16, u32, u64` | Unsigned types (uint8_t ... uint64_t) |
| `s8, s16, s32` | Signed types (int8_t ... int32_t) |

### Callback types

```c
typedef u8   (*z80_mread_cb)(z80_t *cpu, u16 addr, int m1_state, void *user_data);
typedef void (*z80_mwrite_cb)(z80_t *cpu, u16 addr, u8 value, void *user_data);
typedef u8   (*z80_pread_cb)(z80_t *cpu, u16 port, void *user_data);
typedef void (*z80_pwrite_cb)(z80_t *cpu, u16 port, u8 value, void *user_data);
typedef u8   (*z80_intread_cb)(z80_t *cpu, void *user_data);
typedef void (*z80_intack_cb)(z80_t *cpu, void *user_data);
typedef void (*z80_reti_cb)(z80_t *cpu, void *user_data);
typedef void (*z80_ei_cb)(z80_t *cpu, void *user_data);
```

### z80_reg_t

Enum for `z80_get_reg()` / `z80_set_reg()`:

`Z80_REG_AF`, `Z80_REG_BC`, `Z80_REG_DE`, `Z80_REG_HL`, `Z80_REG_AF2`, `Z80_REG_BC2`, `Z80_REG_DE2`, `Z80_REG_HL2`, `Z80_REG_IX`, `Z80_REG_IY`, `Z80_REG_SP`, `Z80_REG_PC`, `Z80_REG_WZ`, `Z80_REG_IR`

### z80_t

CPU state structure. Directly accessible registers:

| Member | Description |
|---|---|
| `z80_pair_t af, bc, de, hl` | Main set |
| `z80_pair_t af2, bc2, de2, hl2` | Alternate set |
| `z80_pair_t ix, iy, wz` | Index + MEMPTR |
| `u16 sp, pc` | Stack Pointer, Program Counter |
| `u8 i, r` | Interrupt Vector, Refresh |
| `u8 iff1, iff2, im` | Interrupt system |
| `bool halted, int_pending, nmi_pending` | State flags |
| `u32 cycles, total_cycles` | T-state counters |
| `int wait_cycles` | Extra wait states |

## Functions

### Lifecycle

- **`z80_t *z80_create(mread, mread_data, mwrite, mwrite_data, pread, pread_data, pwrite, pwrite_data, intread, intread_data)`** - Create CPU instance. `intread` may be NULL. Returns NULL on failure.
- **`void z80_destroy(z80_t *cpu)`** - Destroy instance. `cpu` may be NULL.
- **`void z80_reset(z80_t *cpu)`** - Reset CPU. Preserves callbacks.

### Emulation

- **`int z80_step(z80_t *cpu)`** - Execute one instruction. Returns T-states.
- **`int z80_execute(z80_t *cpu, int target_cycles)`** - Batch execute until target T-states. Returns actual count (>= target).

### Interrupts

- **`void z80_int(z80_t *cpu)`** - Trigger maskable interrupt. Vector read via `intread` callback.
- **`void z80_irq(z80_t *cpu, u8 vector)`** - Trigger maskable interrupt with explicit vector. IM0: `vector & 0x38`, IM1: ignored, IM2: `(I << 8) | (vector & 0xFE)`.
- **`void z80_nmi(z80_t *cpu)`** - Trigger NMI. Jump to 0x0066. 11T.

### Register access

- **`u16 z80_get_reg(z80_t *cpu, z80_reg_t reg)`**
- **`void z80_set_reg(z80_t *cpu, z80_reg_t reg, u16 value)`**
- **`bool z80_is_halted(z80_t *cpu)`**

Registers are also directly accessible via `cpu->af.w`, `cpu->bc.h` etc.

### Dynamic callback changes

```c
void z80_set_mread(z80_t *cpu, z80_mread_cb fn, void *data);
void z80_set_mwrite(z80_t *cpu, z80_mwrite_cb fn, void *data);
void z80_set_pread(z80_t *cpu, z80_pread_cb fn, void *data);
void z80_set_pwrite(z80_t *cpu, z80_pwrite_cb fn, void *data);
void z80_set_intread(z80_t *cpu, z80_intread_cb fn, void *data);
void z80_set_intack(z80_t *cpu, z80_intack_cb fn, void *data);
void z80_set_reti(z80_t *cpu, z80_reti_cb fn, void *data);
void z80_set_ei(z80_t *cpu, z80_ei_cb fn, void *data);
void z80_set_post_step(z80_t *cpu, void (*fn)(z80_t *, void *), void *data);
```

### Utility

- **`void z80_add_wait_states(z80_t *cpu, int wait)`** - Add extra T-states from callbacks.

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

Access: `cpu->af.l`

## Constants

`CPU_Z80_VERSION` = `"multi-v0.2"`

## Integration

```bash
gcc -O2 -I /path/to/cpu-z80-multi-v0.2 -c cpu/z80.c -o z80.o
gcc -o emulator emulator.c z80.o
```

Requirements: C99+, GCC/Clang (computed goto), little-endian platform.

## License

MIT - See [LICENSE](../LICENSE)
