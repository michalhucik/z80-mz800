# Changelog

All notable changes to the dasm-z80 project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
This project uses [Semantic Versioning](https://semver.org/).

## [0.1.5] - 2026-05-26

### Added

- **`z80_ix_style_t` enum and `z80_dasm_format_t.ix_iy_style` field**
  to select the syntax used for indexed addressing operands
  (`Z80_OP_MEM_IX_D`, `Z80_OP_MEM_IY_D`):

    - `Z80_IX_ZILOG` (= 0, default) - mainstream `(IX+5)` / `(IY-2)`
      syntax used by pasmo, sjasmplus, z80ex, z88dk-z80asm and the
      Zilog reference manual.
    - `Z80_IX_MOTOROLA` (= 1) - signed-decimal-first `(5,IX)` /
      `(-2,IY)` syntax required by SDCC's `sdas-z80` (asxxxx
      family). Both styles produce identical opcodes; the difference
      is purely syntactic.

- New file-static helper `fmt_displacement_dec_signed()` in
  `z80_dasm_format.c` that writes the displacement as a signed
  decimal (`5`, `-2`, `0`, `-128`, `127`). Used only on the Motorola
  path.

- `z80_dasm_format_default()` now initialises `ix_iy_style` to
  `Z80_IX_ZILOG`, so existing callers see no behaviour change.

- `tests-dasm/test_motorola_ix.c` regression suite (7 cases): positive
  / negative / zero / extreme +127 / extreme -128 Motorola
  displacements for both IX and IY, plus an explicit Zilog default
  baseline.

## [0.1.4] - 2026-05-26

### Fixed

- **`Z80_HEX_H_SUFFIX` symbol substitution left an orphan `0` in front
  of the symbol for any address whose top nibble is A-F**
  (`0xA000..0xFFFF`). The renderer in `fmt_hex16()` emits the form
  `0c000h` (with a leading `0` so an asm parser cannot mistake the
  number for an identifier), but `z80_dasm_to_str_sym()` was building
  the lookup key as `c000h`. `strstr()` found the substring INSIDE the
  rendered text and replaced just `c000h` with the symbol, leaving the
  orphan `0` behind. Result: `JP 0Lc000` instead of `JP Lc000`,
  `LD A,(0VRAM)` instead of `LD A,(VRAM)`, etc.

  Affected both substitution sites - control-flow target_sym
  (`JP/CALL/JR/...`) and direct-memory mem_sym (`LD A,(nn)` etc.).

  Fix: new static helper `h_suffix_leading_zero(u16 v)` that returns
  `"0"` for `v >= 0xA000` and `""` otherwise. Both H_SUFFIX call sites
  now build the lookup key with the same leading-zero rule as the
  renderer.

  Other hex styles (`HASH`, `0X`, `DOLLAR`) have unambiguous prefixes
  and were not affected. RST addresses are 8-bit (max `0x38`), so the
  leading-zero path never triggers for RST.

### Added

- 4 new regression cases in `tests-dasm/test_dasm_sym.c`:
  `JP 0xC000` (target_sym, top nibble C), `JP 0x1234` (target_sym,
  no leading-zero path), `LD A,(0xE000)` (mem_sym, top nibble E),
  and `RST 0x20` (RST path unaffected). Suite count rises from
  12 to 16 cases.

## [0.1.3] - 2026-05-19

### Fixed

- **RST symbol resolve in `z80_dasm_to_str_sym()`**: RST p has an
  8-bit operand (`Z80_OP_RST_VEC`) that `z80_dasm_to_str` formats as
  `#%02x` (e.g. `#20`). The symtab substitution path always built the
  lookup key as `#%04x` (e.g. `#0020`), so `strstr()` never matched
  and the symbol was silently not substituted. Added an
  `inst->flow == Z80_FLOW_RST` branch that uses the 2-digit hex format
  for all four hex styles (`HASH` / `0X` / `DOLLAR` / `H_SUFFIX`),
  both lower- and uppercase.

### Added

- `tests-dasm/` directory with a standalone regression test
  (`test_dasm_sym.c`) for the symbol resolver, built and run via
  `cd tests-dasm && make run`. 12 cases covering CALL / JP / JP cc /
  JR / JR cc / DJNZ / RST resolve, no-match-stays-hex, JP (HL)/(IX)
  filter and NULL symtab fallback. Uses an inline mini-framework in
  the style of `tests/test_framework.h`, no external dependencies.

  This is the v0.1.2 promise of regression tests, delivered one
  patch release late so the RST fix could land together with its
  coverage.

## [0.1.2] - 2026-05-19

### Fixed

- **Aliasing of the mnemonic string between consecutive `z80_dasm()`
  calls**: the field `z80_dasm_inst_t::mnemonic` used to be a
  `const char *` pointing at a single file-static buffer inside
  `z80_dasm_extract_mnemonic()`. Two `z80_dasm()` calls on different
  instructions in quick succession both ended up with the SAME pointer
  - the second call's mnemonic silently overwrote the first one's
  text, which broke any code that disassembled an instruction and
  kept the result around. Replaced with an embedded
  `char mnemonic[16]` buffer (per-instance storage), so each
  `z80_dasm_inst_t` carries its own copy and is now safe to keep,
  copy, and use across threads.

### Added

- `z80_dasm_extract_mnemonic_into(const char *format, char *buf,
  size_t buf_size)`: thread-safe variant that writes into the
  caller's buffer.

### Deprecated

- `z80_dasm_extract_mnemonic()` is kept as a backward-compatible
  shim returning a pointer to its own file-static buffer. New code
  should call `z80_dasm_extract_mnemonic_into()`.

### Changed

- Three `inst->mnemonic = "NOP*"` literal assignments in `z80_dasm()`
  replaced with `strcpy(inst->mnemonic, "NOP*")`.
- `inst->mnemonic` pointer-vs-NULL checks in `z80_dasm_format.c` (3
  call sites) replaced with `inst->mnemonic[0] != '\0'`, since arrays
  always decay to non-NULL pointers.

### Note

In the original mz800new history this fix was split across two
commits (`7ef24f7` aliasing + type change, `30046a7` follow-up
`z80_dasm_format.c` NULL → empty-string checks). They are bundled
here because the second commit only exists as a direct consequence
of the type change in the first.

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
