# Changelog

All notable changes to the dasm-z80 project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
This project uses [Semantic Versioning](https://semver.org/).

## [0.1.1] - 2026-05-19

### Fixed

- **Build under Linux/glibc**: `z80_dasm_internal.h` now explicitly
  includes `<stddef.h>` for `NULL`. On MinGW it was pulled in
  transitively via `<stdint.h>`, but glibc does not guarantee this and
  the `INV` macro (which expands to `NULL`) failed to compile.
- **Stale include path after upstream rename**: `z80_dasm.h` referenced
  `../ai2-z80/utils/types.h`, which has not existed since the
  `ai2-z80` → `cpu-z80` rename. Path updated to
  `../cpu-z80/utils/types.h`.
- **Makefile rename leftovers**: `-I../ai2-z80/utils` corrected to
  `-I../cpu-z80/utils`; previously the library could not be built
  at all on a fresh checkout.
- **Makefile broken `test` target**: removed; it referenced a
  `../tests/test_dasm.c` that does not exist in this repository.
  Regression tests are planned for v0.1.2.
- **Makefile MSYS2 temp directory**: added `TMPDIR/TMP/TEMP` exports
  set to `/tmp` so that GCC under MSYS2 can write its temporaries
  (otherwise it falls back to `C:\WINDOWS\` and fails with
  "Permission denied").

### Changed

- Source comment in `z80_dasm_internal.h` referring to the historic
  `ai2-z80` macro name updated to `cpu-z80`.

## [0.1] - 2026-04-01

First release. Complete Z80 disassembler implementation
with advanced features for the MZ-800 emulator debugger.

### Added

#### Disassembler core
- Decoding of all 1792 Z80 instructions (7 opcode tables of 256 entries each)
- Support for all documented instructions per Zilog UM0080
- Support for undocumented instructions: SLL, IXH/IXL/IYH/IYL operations,
  DD CB/FD CB register copy, IN F,(C), OUT (C),0, mirrored NEG/RETN/IM
- Detection of invalid sequences (DD DD, DD FD, DD ED, invalid ED) as NOP*
- Structured output (z80_dasm_inst_t) with parsed operands
- Block disassembly (z80_dasm_block)
- Heuristic backward instruction start search (z80_dasm_find_inst_start)

#### Instruction metadata
- Read/written register map for each instruction
- Affected flags map for each instruction
- Instruction classification (official / undocumented / invalid)
- Control flow type (12 categories: normal, jump, call, ret, rst, halt, ...)
- Timing: base T-states and branch T-states

#### Output formatting
- 4 hexadecimal number styles: #FF, 0xFF, $FF, FFh
- Uppercase/lowercase toggle (LD A,B vs ld a,b)
- Optional raw instruction bytes display
- Optional address display
- Relative jumps as absolute addresses or as $+n/$-n
- 3 IX half-register naming styles (IXH/HX/XH)

#### Symbol table
- Address-to-name mapping
- Binary search O(log n)
- Automatic address-to-symbol replacement in output
- Symbol resolution for jump targets and memory operands

#### Control flow analysis
- Jump/call target address retrieval (z80_dasm_target_addr)
- Branch condition evaluation from current flags state (z80_dasm_branch_taken)
- Convenience wrappers for register and flag mask access

#### Utilities
- Relative offset to absolute address conversion (z80_rel_to_abs)
- Absolute address to relative offset with range check (z80_abs_to_rel)

#### Compatibility
- Drop-in replacement for z80ex_dasm() with identical signature
- Support for z80ex formatting flags (WORDS_DEC, BYTES_DEC)
- Preserved z80ex T-state conventions

#### Documentation and tests
- Complete Doxygen documentation for all public and internal symbols
- User documentation (README.md) with 7 practical examples
- 145 unit tests covering all modules
- Testing of all 252 base opcodes (without prefixes)

#### Build system
- Makefile for GCC (MinGW64/MSYS2)
- Static library libdasm_z80.a
- Target platform: MSYS2/MinGW64 on Windows
