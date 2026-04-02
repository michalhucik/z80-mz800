# dasm-z80 v0.1 - Z80 disassembler pro emulátor MZ-800

Kompletní Z80 disassembler s podporou všech dokumentovaných i nedokumentovaných
instrukcí. Navržen pro použití v debuggeru emulátoru Sharp MZ-800.

Aktuální verze: **0.1** (2026-04-01) - viz [CHANGELOG.md](CHANGELOG.md)

## Vlastnosti

- **Strukturovaný výstup** - každá instrukce dekódovaná do struktury s rozloženými
  operandy, časováním, mapou registrů a flagů
- **Analýza toku řízení** - detekce skoků, volání, návratů; vyhodnocení podmínek
- **Tabulka symbolů** - nahrazení adres symbolickými názvy (ROM rutiny, I/O porty)
- **Nedokumentované instrukce** - SLL, IXH/IXL/IYH/IYL, DD CB kopie do registru
- **Flexibilní formátování** - 4 hex styly, uppercase/lowercase, surové bajty
- **Zpětná kompatibilita** - drop-in náhrada za z80ex_dasm()
- **Thread-safe** - žádné globální proměnné ani statické buffery
- **Nulové závislosti** - jediný externí include je `types.h` (u8, u16, s8)

## Rychlý start

### 1. Základní použití

```c
#include "z80_dasm.h"

/* Callback pro čtení z paměti emulátoru */
u8 my_read(u16 addr, void *user_data) {
    u8 *mem = (u8 *)user_data;
    return mem[addr];
}

/* Disassemblace jedné instrukce */
z80_dasm_inst_t inst;
int len = z80_dasm(&inst, my_read, memory, 0x0100);

/* Formátování do textu */
char buf[64];
z80_dasm_format_t fmt;
z80_dasm_format_default(&fmt);
z80_dasm_to_str(buf, sizeof(buf), &inst, &fmt);

printf("%04X: %s\n", inst.addr, buf);
/* Výstup: 0100: LD A,#FF */
```

### 2. Použití v debuggeru

```c
/* Zobrazení bloku instrukcí */
z80_dasm_inst_t insts[20];
int count = z80_dasm_block(insts, 20, my_read, memory, pc, pc + 0x40);

for (int i = 0; i < count; i++) {
    char line[64];
    z80_dasm_to_str(line, sizeof(line), &insts[i], &fmt);

    /* Zvýraznění aktuální instrukce */
    const char *marker = (insts[i].addr == pc) ? ">>>" : "   ";
    printf("%s %04X: %s\n", marker, insts[i].addr, line);
}
```

### 3. Analýza toku řízení

```c
z80_dasm(&inst, my_read, memory, pc);

/* Kam instrukce skáče? */
u16 target = z80_dasm_target_addr(&inst);
if (target != 0xFFFF) {
    printf("Cíl skoku: %04X\n", target);
}

/* Provede se podmíněčný skok? */
u8 flags = cpu.af.b.l;  /* aktuální F registr */
if (z80_dasm_branch_taken(&inst, flags)) {
    printf("Skok se PROVEDE\n");
} else {
    printf("Skok se NEPROVEDE\n");
}

/* Které registry se změní? */
u16 written = z80_dasm_regs_written(&inst);
if (written & Z80_REG_A)  printf("Změní se A\n");
if (written & Z80_REG_HL) printf("Změní se HL\n");
if (written & Z80_REG_F)  printf("Změní se flagy\n");
```

### 4. Tabulka symbolů

```c
/* Vytvoření tabulky s MZ-800 systémovými adresami */
z80_symtab_t *sym = z80_symtab_create();
z80_symtab_add(sym, 0x0000, "reset");
z80_symtab_add(sym, 0x0038, "irq_handler");
z80_symtab_add(sym, 0x00AD, "rom_getchar");
z80_symtab_add(sym, 0x0012, "rom_putchar");
z80_symtab_add(sym, 0xD000, "video_ram");

/* Disassemblace se symboly */
z80_dasm(&inst, my_read, memory, 0x0100);
char buf[64];
z80_dasm_to_str_sym(buf, sizeof(buf), &inst, &fmt, sym);
/* "CALL rom_getchar" místo "CALL #00AD" */

/* Ruční rozlišení symbolů */
z80_dasm_symbols_t syms;
z80_dasm_resolve_symbols(&inst, sym, &syms);
if (syms.target_sym) printf("Volá: %s\n", syms.target_sym);
if (syms.mem_sym)    printf("Přistupuje: %s\n", syms.mem_sym);

/* Úklid */
z80_symtab_destroy(sym);
```

### 5. Konfigurace formátu

```c
z80_dasm_format_t fmt;
z80_dasm_format_default(&fmt);

/* Intel/Zilog styl: FFh, 1234h */
fmt.hex_style = Z80_HEX_H_SUFFIX;

/* Malá písmena: ld a,0ffh */
fmt.uppercase = 0;

/* Zobrazit surové bajty: "3E FF  LD A,#FF" */
fmt.show_bytes = 1;

/* Zobrazit adresu: "0100  LD A,#FF" */
fmt.show_addr = 1;

/* JR jako relativní offset: "JR $+5" místo "JR #0107" */
fmt.rel_as_absolute = 0;

/* Alternativní názvy IX půl-registrů: HX/LX místo IXH/IXL */
fmt.undoc_ix_style = 1;
```

### 6. Konverze relativních adres

```c
/* Kam skočí JR na adrese 0x100 s offsetem +5? */
u16 target = z80_rel_to_abs(0x0100, 5);  /* -> 0x0107 */

/* Jaký offset potřebuji pro skok z 0x100 na 0x120? */
s8 offset;
if (z80_abs_to_rel(0x0100, 0x0120, &offset) == 0) {
    printf("Offset: %d\n", offset);  /* 30 */
} else {
    printf("Cíl mimo dosah JR!\n");
}
```

### 7. z80ex kompatibilní režim

```c
/* Drop-in náhrada za původní z80ex_dasm() */
char output[64];
int t_states, t_states2;

int len = z80ex_dasm(output, sizeof(output), 0,
                     &t_states, &t_states2,
                     my_z80ex_readbyte_cb, addr, user_data);

/* Identický výstup jako původní z80ex */
```

## Přehled API

### Jádro

| Funkce | Popis |
|--------|-------|
| `z80_dasm()` | Disassembluj jednu instrukci do `z80_dasm_inst_t` |
| `z80_dasm_block()` | Disassembluj blok instrukcí v rozsahu adres |
| `z80_dasm_find_inst_start()` | Heuristicky najdi začátek instrukce (zpětné scrollování) |

### Formátování

| Funkce | Popis |
|--------|-------|
| `z80_dasm_format_default()` | Inicializuj formát na výchozí hodnoty |
| `z80_dasm_to_str()` | Formátuj instrukci do textového bufferu |
| `z80_dasm_to_str_sym()` | Formátuj s nahrazením adres symboly |

### Tabulka symbolů

| Funkce | Popis |
|--------|-------|
| `z80_symtab_create()` | Vytvoř novou tabulku |
| `z80_symtab_destroy()` | Zruš tabulku a uvolni paměť |
| `z80_symtab_add()` | Přidej symbol (adresa -> název) |
| `z80_symtab_remove()` | Odeber symbol podle adresy |
| `z80_symtab_clear()` | Odeber všechny symboly |
| `z80_symtab_lookup()` | Vyhledej symbol podle adresy |
| `z80_symtab_count()` | Počet symbolů v tabulce |
| `z80_dasm_resolve_symbols()` | Rozliš symboly pro instrukci |

### Analýza

| Funkce | Popis |
|--------|-------|
| `z80_dasm_target_addr()` | Cílová adresa skoku/volání |
| `z80_dasm_branch_taken()` | Vyhodnoť podmínku větvení |
| `z80_dasm_regs_read()` | Bitová maska čtených registrů |
| `z80_dasm_regs_written()` | Bitová maska zapisovaných registrů |
| `z80_dasm_flags_affected()` | Bitová maska ovlivněných flagů |

### Utility

| Funkce | Popis |
|--------|-------|
| `z80_rel_to_abs()` | Relativní offset -> absolutní adresa |
| `z80_abs_to_rel()` | Absolutní adresa -> relativní offset |

### Kompatibilita

| Funkce | Popis |
|--------|-------|
| `z80ex_dasm()` | Drop-in náhrada za původní z80ex_dasm() |

## Klíčové datové typy

### z80_dasm_inst_t - výsledek disassemblace

```c
typedef struct {
    u16 addr;               /* adresa instrukce */
    u8  bytes[4];           /* surové bajty (max 4) */
    u8  length;             /* délka instrukce (1-4) */
    u8  t_states;           /* T-stavy (základní) */
    u8  t_states2;          /* T-stavy při větvení (0 = není) */
    z80_flow_type_t flow;   /* typ toku řízení */
    z80_inst_class_t cls;   /* official / undocumented / invalid */
    z80_operand_t op1;      /* první operand */
    z80_operand_t op2;      /* druhý operand */
    const char *mnemonic;   /* "LD", "ADD", "JP", ... */
    u8  flags_affected;     /* bitová maska ovlivněných flagů */
    u16 regs_read;          /* bitová maska čtených registrů */
    u16 regs_written;       /* bitová maska zapisovaných registrů */
} z80_dasm_inst_t;
```

### z80_flow_type_t - tok řízení

| Hodnota | Popis | Příklady |
|---------|-------|----------|
| `Z80_FLOW_NORMAL` | Běžná instrukce | LD, ADD, AND, NOP |
| `Z80_FLOW_JUMP` | Bezpodmínečný skok | JP nn, JR e |
| `Z80_FLOW_JUMP_COND` | Podmíněčný skok | JP NZ,nn / JR Z,e / DJNZ |
| `Z80_FLOW_CALL` | Bezpodmínečné volání | CALL nn |
| `Z80_FLOW_CALL_COND` | Podmíněčné volání | CALL NZ,nn |
| `Z80_FLOW_RET` | Bezpodmínečný návrat | RET |
| `Z80_FLOW_RET_COND` | Podmíněčný návrat | RET NZ |
| `Z80_FLOW_RST` | Restart vektor | RST 38 |
| `Z80_FLOW_HALT` | CPU zastaven | HALT |
| `Z80_FLOW_JUMP_INDIRECT` | Nepřímí skok | JP (HL), JP (IX) |
| `Z80_FLOW_RETI` | Návrat z IRQ | RETI |
| `Z80_FLOW_RETN` | Návrat z NMI | RETN |

### z80_inst_class_t - klasifikace instrukce

| Hodnota | Popis | Příklady |
|---------|-------|----------|
| `Z80_CLASS_OFFICIAL` | Dokumentovaná (Zilog manual) | LD A,B / ADD HL,BC |
| `Z80_CLASS_UNDOCUMENTED` | Nedokumentovaná ale funkční | SLL, IXH, DD CB kopie |
| `Z80_CLASS_INVALID` | Neplatná sekvence | DD DD, ED 00 (zobrazeno jako NOP*) |

### Registrové masky

```c
Z80_REG_A, Z80_REG_F, Z80_REG_B, Z80_REG_C,
Z80_REG_D, Z80_REG_E, Z80_REG_H, Z80_REG_L,
Z80_REG_IXH, Z80_REG_IXL, Z80_REG_IYH, Z80_REG_IYL,
Z80_REG_SP, Z80_REG_I, Z80_REG_R, Z80_REG_WZ

/* Aliasy pro páry */
Z80_REG_AF, Z80_REG_BC, Z80_REG_DE, Z80_REG_HL,
Z80_REG_IX, Z80_REG_IY
```

### Flagové masky

```c
Z80_FLAG_C   (0x01)   /* Carry */
Z80_FLAG_N   (0x02)   /* Subtract */
Z80_FLAG_PV  (0x04)   /* Parity / Overflow */
Z80_FLAG_3   (0x08)   /* Undocumented bit 3 */
Z80_FLAG_H   (0x10)   /* Half-carry */
Z80_FLAG_5   (0x20)   /* Undocumented bit 5 */
Z80_FLAG_Z   (0x40)   /* Zero */
Z80_FLAG_S   (0x80)   /* Sign */
```

## Build

```bash
cd dasm-z80
make           # sestaví libdasm_z80.a
make test      # sestaví a spustí testy
make clean     # smaže build artefakty
```

Požadavky: GCC (MinGW64 / MSYS2), GNU Make.

## Architektura

```
z80_dasm()          ->  z80_dasm_inst_t  (strukturovaný výsledek)
                              |
          +-------------------+-------------------+
          |                   |                   |
  z80_dasm_to_str()   z80_dasm_target_addr()  z80_dasm_regs_*()
  z80_dasm_to_str_sym()  z80_dasm_branch_taken()  z80_dasm_flags_affected()
          |
  z80_dasm_format_t
  z80_symtab_t
```

Princip: **parsuj jednou, dotazuj se mnohokrát**. Funkce `z80_dasm()` dekóduje
instrukci jednou. Všechny další funkce pouze čtou data ze struktury.

## Soubory

| Soubor | Popis |
|--------|-------|
| `z80_dasm.h` | Veřejné API (jediný header pro uživatele) |
| `z80_dasm_internal.h` | Interní typy (opcode tabulka, helper makra) |
| `z80_dasm.c` | Jádro dekodéru + utility (rel/abs konverze) |
| `z80_dasm_tables.c` | 7 opcode tabulek (1792 záznamů) |
| `z80_dasm_format.c` | Formátování výstupu do textu |
| `z80_dasm_symtab.c` | Tabulka symbolů |
| `z80_dasm_analysis.c` | Analýza toku řízení |
| `z80_dasm_compat.c` | z80ex kompatibilní obálka |

## Licence

MIT - Viz [LICENSE](../LICENSE)
