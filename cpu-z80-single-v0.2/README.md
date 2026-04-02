# cpu-z80 single-v0.2 - API Reference

Accurate and fast single-instance Zilog Z-80A processor emulator.

## Overview

The API is **identical** to [cpu-z80 multi-v0.2](../cpu-z80-multi-v0.2/README.md). See that README for complete documentation of all functions, types and callback signatures.

The only difference: single-v0.2 supports only **one active CPU instance**. Callbacks are stored globally for maximum performance.

## Usage

```c
z80_t *cpu = z80_create(mread, NULL, mwrite, NULL,
                         pread, NULL, pwrite, NULL,
                         NULL, NULL);
z80_execute(cpu, 69888);
z80_destroy(cpu);
```

## Switching to multi-instance

Replace `cpu-z80-single-v0.2` with `cpu-z80-multi-v0.2` in your include path. No source code changes needed.

## Constants

`CPU_Z80_VERSION` = `"single-v0.2"`

## License

MIT - See [LICENSE](../LICENSE)
