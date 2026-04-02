# cpu-z80 - Přesný a rychlý emulátor procesoru Zilog Z-80A

Emulátor Z80A CPU a disassembler v přenositelném C99, vyvinutý jako CPU jádro pro [mz800emu](https://sourceforge.net/projects/mz800emu/) - emulátor počítačů Sharp MZ-800, MZ-700 a MZ-1500.

## Motivace

Projekt mz800emu původně používal upravenou verzi [z80ex](https://sourceforge.net/projects/z80ex/) jako CPU jádro. Přestože z80ex je solidní a dobře otestovaný emulátor, několik faktorů vedlo k vývoji vlastního náhradního jádra:

- **Výkon**: z80ex používá dispatch přes function pointery, kde každý opcode je samostatná funkce. cpu-z80 používá computed goto s lokální registrovou cache a dávkovým zpracováním instrukcí, čímž dosahuje **~3x vyšší propustnosti**.
- **Přesnost**: z80ex neemuluje hardwarový bug LD A,I/R (INT po LD A,I/R resetuje PF), což je důležité pro přesnou emulaci Z80A.
- **Licence**: z80ex je pod GPL-2.0. cpu-z80 je pod **licencí MIT**, což usnadňuje integraci do projektů s různými licenčními požadavky.
- **Velikost**: z80ex má ~13 500 řádků (většina generovaná Perl skriptem). cpu-z80 má ~2 000 řádků ručně psaného C kódu.

## Vlastnosti

- Kompletní instrukční sada Z80A (všechny dokumentované + nedokumentované instrukce)
- Prefixové instrukce CB, ED, DD/FD, DD CB/FD CB
- Nedokumentované: operace s IXH/IXL/IYH/IYL, SLL (CB 30-37), indexované bitové operace s kopírováním do registru
- Přesné počítání T-stavů pro každou instrukci
- Správné chování všech flagů včetně nedokumentovaných bitů F3 (bit 3) a F5 (bit 5)
- Interní registr MEMPTR/WZ se správným vlivem na F3/F5
- Interní Q registr pro správné chování F3/F5 u SCF/CCF (objeven Patrik Rak, 2018) *(pouze v0.2)*
- Přerušovací režimy IM0, IM1, IM2
- NMI se zachováním IFF2
- EI delay (přerušení odloženo o jednu instrukci po EI)
- Hardwarový bug LD A,I/R (INT po LD A,I/R resetuje PF na 0)
- HALT s probuzením přerušením
- 402 jednotkových testů (v0.1) / 410 testů (v0.2, včetně testů Q registru)
- **Validováno ZEXALL** - prochází všech 67 testů Z80 Instruction Exerciser (Frank Cringle / J.G. Harston), včetně nedokumentovaných instrukcí a flagů

### Výhody oproti z80ex

| Vlastnost | z80ex 1.1.21 | cpu-z80 |
|---|---|---|
| LD A,I/R INT bug | ne | **ano** |
| Q registr (SCF/CCF F3/F5) | ne | **ano** |
| Daisy chain (RETI callback) | ne | **ano** |
| INTACK callback | ne | **ano** |
| EI callback | ne | **ano** |
| Post-step callback | ne | **ano** |
| Wait states z callbacků | ne | **ano** |
| Dávkové zpracování (z80_execute) | ne | **ano** |
| Computed goto dispatch | ne | **ano** |
| Lokální registrová cache | ne | **ano** |
| DAA lookup tabulka | ne | **ano** |
| Zdrojový kód | ~13 500 řádků (generovaný) | ~2 000 řádků (ručně psaný) |
| Licence | GPL-2.0 | **MIT** |

## Varianty

Projekt poskytuje dvě varianty emulátoru s různými kompromisy:

### cpu-z80 v0.1 - Maximální výkon

Nejrychlejší varianta. Používá globální callback pointery s jednoduchou signaturou (`u8 fn(u16 addr)`) a CPU stav na zásobníku. Pouze jedna instance.

Záměrně neimplementuje Q registr pro zachování maximálního výkonu. Emulace Q registru vyžaduje sledování změn F registru v každém dispatch cyklu, což přidává ~3% overhead. Bez něj SCF/CCF F3/F5 flagy vždy používají `A | F` (správné po ALU instrukci, ale ne po LD/NOP/EX).

Vhodná pro projekty vyžadující maximální rychlost, které nepotřebují API kompatibilitu se z80ex, přesnost Q registru ani více CPU instancí.

```c
z80_t cpu;
z80_init(&cpu);
z80_set_mem_read(mem_read);
z80_set_mem_write(mem_write);
z80_set_mem_fetch(mem_read);
z80_execute(&cpu, 69888);
```

### cpu-z80 multi-v0.2 - Více instancí (Doporučená)

Každá CPU instance nese vlastní callbacky a `user_data` - více nezávislých instancí může běžet současně. Signatura callbacků je kompatibilní se z80ex (`cpu, addr, m1_state, user_data`).

Callback pointery jsou na začátku `z80_execute()` cachované do lokálních proměnných, což eliminuje overhead opakované dereference struktury. **Výkon na -O2 odpovídá single-instance variantě v0.1.**

```c
z80_t *cpu = z80_create(
    mem_read, NULL,    /* čtení paměti (fetch i data) */
    mem_write, NULL,   /* zápis do paměti */
    io_read, NULL,     /* čtení I/O portu */
    io_write, NULL,    /* zápis na I/O port */
    NULL, NULL         /* čtení vektoru přerušení (nepovinné) */
);
z80_execute(cpu, 69888);
z80_destroy(cpu);
```

### Rozšíření API (multi-v0.2)

API v0.2 rozšiřuje z80ex-kompatibilní základ o:

- `z80_execute(cpu, target_cycles)` - dávkové zpracování instrukcí (z80ex má jen single-step)
- `z80_irq(cpu, vector)` - explicitní vektor přerušení (navíc ke callback-based `z80_int()`)
- `z80_add_wait_states(cpu, wait)` - vložení extra T-stavů z callbacků (contended memory, PSG READY)
- `z80_set_post_step(cpu, fn, data)` - callback po každé instrukci (MZ-700 per-line WAIT timing)
- `z80_set_ei(cpu, fn, data)` - callback při instrukci EI (synchronizace přerušovací logiky)
- `z80_set_intack(cpu, fn, data)` - INTACK signál pro daisy chain periferie
- `z80_set_reti(cpu, fn, data)` - RETI notifikace pro daisy chain
- Dynamická změna callbacků za běhu přes `z80_set_mread()`, `z80_set_pwrite()` atd.

## Benchmark

Test: 16 777 216 iterací smyčky, mix instrukcí. GCC, MSYS2/MinGW64.

Všechny emulátory dosahují identický počet cyklů (2 214 609 436 T-stavů).

| Emulátor | -O2 (MHz) | vs z80ex | -O3 (MHz) |
|---|---|---|---|
| z80ex 1.1.21 | 1 057 | 1.0x | 1 120 |
| **cpu-z80 v0.1** | **3 020** | **2.86x** | **3 130** |
| **cpu-z80 multi-v0.2** | **3 040** | **2.88x** | **3 060** |

## Disassembler (dasm-z80)

Plnohodnotný Z80 disassembler s architekturou "parsuj jednou, dotazuj se mnohokrát". Navržen pro integraci debuggeru v emulátorech.

Vlastnosti:
- Všechny dokumentované i nedokumentované instrukce Z80
- Strukturovaný výstup (`z80_dasm_inst_t`) s operandy, časováním, typem toku řízení, mapou registrů/flagů
- Konfigurovatelné textové formátování (styly hex čísel, velká/malá písmena, zobrazení adresy/bajtů)
- Tabulka symbolů s rozlišením adresa -> název
- Analýza toku řízení (cílová adresa, predikce větvení)
- Zpětné hledání hranice instrukce (heuristika pro scrollování zpět v debuggeru)
- Drop-in kompatibilní wrapper `z80ex_dasm()`
- Thread-safe (žádný globální stav)

### Rychlý start

```c
#include "z80_dasm.h"

z80_dasm_inst_t inst;
z80_dasm(&inst, my_read_fn, NULL, 0x0000);

char buf[64];
z80_dasm_to_str(buf, sizeof(buf), &inst, NULL);
printf("%s\n", buf);  /* např. "LD A,#42" */
```

### API disassembleru

```c
/* Jádro */
int  z80_dasm(z80_dasm_inst_t *inst, z80_dasm_read_fn read_fn,
              void *user_data, u16 addr);
int  z80_dasm_block(z80_dasm_inst_t *out, int max_inst,
                    z80_dasm_read_fn read_fn, void *user_data,
                    u16 start_addr, u16 end_addr);
u16  z80_dasm_find_inst_start(z80_dasm_read_fn read_fn, void *user_data,
                              u16 target_addr, u16 search_from);

/* Formátování */
void z80_dasm_format_default(z80_dasm_format_t *fmt);
int  z80_dasm_to_str(char *buf, int buf_size,
                     const z80_dasm_inst_t *inst, const z80_dasm_format_t *fmt);
int  z80_dasm_to_str_sym(char *buf, int buf_size,
                         const z80_dasm_inst_t *inst, const z80_dasm_format_t *fmt,
                         const z80_symtab_t *symbols);

/* Tabulka symbolů */
z80_symtab_t *z80_symtab_create(void);
void z80_symtab_destroy(z80_symtab_t *tab);
int  z80_symtab_add(z80_symtab_t *tab, u16 addr, const char *name);
const char *z80_symtab_lookup(const z80_symtab_t *tab, u16 addr);

/* Analýza */
u16  z80_dasm_target_addr(const z80_dasm_inst_t *inst);
int  z80_dasm_branch_taken(const z80_dasm_inst_t *inst, u8 flags);
u16  z80_dasm_regs_read(const z80_dasm_inst_t *inst);
u16  z80_dasm_regs_written(const z80_dasm_inst_t *inst);

/* z80ex kompatibilita */
int  z80ex_dasm(char *output, int output_size, unsigned flags,
                int *t_states, int *t_states2,
                z80ex_dasm_readbyte_cb readbyte_cb,
                Z80EX_WORD addr, void *user_data);
```

## Struktura projektu

```
cpu-z80/                  v0.1 - jedna instance, původní API
cpu-z80-multi-v0.2/       více instancí, nové API (doporučená)
dasm-z80/                 knihovna Z80 disassembleru
tests/                    402 testů pro cpu-z80 v0.1
tests-multi/              410 testů pro multi-v0.2
bench/                    benchmarková sada
docs/                     referenční dokumentace, výsledky benchmarků
```

## Sestavení

Požadavky: GCC nebo Clang (pro computed goto), C99, little-endian platforma.

```bash
# Spuštění testů
cd tests-multi && make run     # 410 testů

# Spuštění benchmarků
cd bench && make compare       # všechny varianty, -O2 a -O3
```

Každá varianta cpu-z80 je jediná kompilační jednotka (`cpu/z80.c` + `cpu/z80.h` + `utils/types.h`). Není potřeba žádný build systém - stačí přidat do projektu:

```bash
gcc -O2 -I cesta/k/cpu-z80-multi-v0.2 -c cpu/z80.c -o z80.o
```

## Dokumentace

Každá varianta emulátoru obsahuje `API_en.txt` / `API_cz.txt` a `CHANGELOG_en.txt` / `CHANGELOG_cz.txt` s kompletní API referencí a historií verzí v angličtině i češtině.

## Licence

MIT

## Autor

Michal Hucik - [z80-mz800](https://github.com/michalhucik/z80-mz800)
