# cpu-z80 - Přesný a rychlý emulátor procesoru Zilog Z-80A

Emulátor Z80A CPU a disassembler v přenositelném C99, vyvinutý jako CPU jádro pro [mz800emu](https://sourceforge.net/projects/mz800emu/) - cycle-accurate emulátor počítačů Sharp MZ-800, MZ-700 a MZ-1500.

## Motivace

Projekt mz800emu původně používal upravenou verzi [z80ex](https://sourceforge.net/projects/z80ex/) jako CPU jádro. Přestože z80ex je solidní a dobře otestovaný emulátor, několik faktorů vedlo k vývoji vlastního náhradního jádra:

- **Výkon**: z80ex používá dispatch přes function pointery, kde každý opcode je samostatná funkce. cpu-z80 používá computed goto, takže jeho dispatch jádro je mírně rychlejší než z80ex v per-step režimu (~1.05x) a jeho volitelný dávkový režim dosahuje **~2.6x** pro ne-cycle-accurate dávkové provádění. (Často citovaných "2.8x" je dávkové číslo; v per-step režimu, který reálný cycle-accurate emulátor používá, je propustnost proti z80ex zhruba nerozhodná - viz sekce [Benchmark](#benchmark) pro poctivý úplný rozpad.)
- **Přesnost**: z80ex neemuluje hardwarový bug LD A,I/R, Q registr ani per-M-cycle T-state tracking.
- **Licence**: z80ex je pod GPL-2.0. cpu-z80 je pod **licencí MIT**, což usnadňuje integraci do projektů s různými licenčními požadavky.
- **Velikost**: z80ex má ~13 500 řádků (většina generovaná Perl skriptem). cpu-z80 má ~3 500 řádků ručně psaného, bohatě komentovaného C kódu.

## Vlastnosti

- Kompletní instrukční sada Z80A (všechny dokumentované + nedokumentované instrukce)
- Prefixové instrukce CB, ED, DD/FD, DD CB/FD CB
- Nedokumentované: operace s IXH/IXL/IYH/IYL, SLL (CB 30-37), indexované bitové operace s kopírováním do registru
- Přesné počítání T-stavů pro každou instrukci
- Per-M-cycle T-state tracking (`op_tstate`) pro contended memory timing
- Správné chování všech flagů včetně nedokumentovaných bitů F3 (bit 3) a F5 (bit 5)
- Interní registr MEMPTR/WZ se správným vlivem na F3/F5
- Interní Q registr pro správné chování F3/F5 u SCF/CCF (objeven Patrik Rak, 2018)
- Přerušovací režimy IM0, IM1, IM2
- NMI se zachováním IFF2
- EI delay (přerušení odloženo o jednu instrukci po EI)
- Hardwarový bug LD A,I/R (INT po LD A,I/R resetuje PF na 0)
- HALT s probuzením přerušením
- Multi-instance: více nezávislých CPU instancí s vlastními callbacky a user_data
- Dvě jádra provádění z jednoho sdíleného zdroje: dávkové (`z80_execute`, registrová cache) a per-step (`z80_step`, registry v `cpu->`) pro cycle-accurate hostitele - bit-identické
- Volitelný RAM fast-path (`-DCPU_Z80_RAM_FASTPATH`) - inline page-table přístup do paměti obcházející callbacky pro čistou RAM
- 441 jednotkových testů (Q registr, ei/di/im/halt/nmi, iff_change, cpu_ctrl_event, call/ret, HALT PC, EI-delay)
- **Validováno ZEXALL** - prochází všech 67 testů Z80 Instruction Exerciser (Frank Cringle / J.G. Harston) přes **obě** jádra (dávkové i per-step), včetně nedokumentovaných instrukcí a flagů

### Výhody oproti z80ex

| Vlastnost | z80ex 1.1.21 | cpu-z80 |
|---|---|---|
| LD A,I/R INT bug | ne | **ano** |
| Q registr (SCF/CCF F3/F5) | ne | **ano** |
| Per-M-cycle T-state tracking | ne | **ano** |
| Daisy chain (RETI callback) | ne | **ano** |
| INTACK callback | ne | **ano** |
| EI callback | ne | **ano** |
| Post-step callback | ne | **ano** |
| Wait states z callbacků | ne | **ano** |
| Dávkové zpracování (z80_execute) | ne | **ano** |
| Vyhrazené per-step jádro (z80_step) | nelze (jen per-step) | **ano** |
| Volitelný RAM fast-path | ne | **ano** |
| Computed goto dispatch | ne | **ano** |
| Lokální registrová cache | ne | **ano** |
| DAA lookup tabulka | ne | **ano** |
| Zdrojový kód | ~13 500 řádků (generovaný) | ~3 500 řádků (ručně psaný) |
| Licence | GPL-2.0 | **MIT** |

## Rychlý start

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

### Rozšíření API oproti z80ex

- `z80_execute(cpu, target_cycles)` - dávkové zpracování instrukcí (z80ex má jen single-step)
- `z80_irq(cpu, vector)` - explicitní vektor přerušení (navíc ke callback-based `z80_int()`)
- `z80_add_wait_states(cpu, wait)` - vložení extra T-stavů z callbacků (contended memory, PSG READY)
- `z80_set_post_step(cpu, fn, data)` - callback po každé instrukci (WAIT timing)
- `z80_set_ei(cpu, fn, data)` - callback při instrukci EI (synchronizace přerušovací logiky)
- `z80_set_intack(cpu, fn, data)` - INTACK signál pro daisy chain periferie
- `z80_set_reti(cpu, fn, data)` - RETI notifikace pro daisy chain
- `op_tstate` pole - T-stavy od začátku instrukce, inkrementovány každým M-cyklem
- Dynamická změna callbacků za běhu přes `z80_set_mread()`, `z80_set_pwrite()` atd.

## Benchmark

Test: 16 777 216 iterací smyčky, mix instrukcí. GCC 15.2.0, MSYS2/MinGW64. Všechny emulátory i všechny režimy cpu-z80 dosahují identický počet cyklů (2 214 609 436 T-stavů).

cpu-z80 lze pohánět dvěma způsoby a tato volba určuje propustnost. `z80_execute` (dávkově) provede mnoho T-stavů v jedné interní smyčce - nejrychlejší, ale cycle-accurate emulátor, který synchronizuje periferie uprostřed instrukce, ho použít nemůže. `z80_step` (per-step) provede jednu instrukci a vrátí se - to, co reálný emulátor používá. z80ex je jen per-step.

| Režim (stejná session, O2 průměr) | MHz | vs z80ex |
|---|---|---|
| z80ex-mz800 (per-step) | 1 072 | 1.00x |
| cpu-z80 batch | 2 822 | 2.63x |
| cpu-z80 dispatch jádro ¹ | 1 123 | 1.05x |
| cpu-z80 per-step (`z80_step`) | ~830 | 0.77x |
| cpu-z80 per-step + fast-path ² | 627 | 0.58x |

¹ Per-step jádro s vyloučenou obsluhou přerušení - férové srovnání s `z80ex_step`, který také provádí jen opcode. Dispatch cpu-z80 je ~5 % rychlejší.
² Pomalejší *zde* jen proto, že memory callback tohoto microbenchmarku je triviální; fast-path se vyplatí proti drahému (bankovanému) callbacku, ne proti `return ram[addr]`.

Poctivý závěr: dávkové jádro je ~2.6x z80ex, ale reálný emulátor ho použít nemůže; per-step *dispatch* je ~5 % rychlejší, ale `z80_step` navíc integruje per-instruction obsluhu přerušení + post-step, kterou `z80ex_step` nechává na volajícím, takže plná per-step propustnost je zhruba nerozhodná. Per-step cena je záměrný trade-off, ne vada: vzdáváš se dávkového zrychlení výměnou za cycle-accurate mid-instruction časování (daň za přesnost) a `z80_step` dělá integrovanou práci, kterou z80ex nechává na consumerovi (daň za zabudovanou funkčnost). Samotné přesné dispatch jádro je naopak *rychlejší* než z80ex - přesnost cpu-z80 nezpomaluje. Skutečné důvody pro volbu cpu-z80 jsou přesnost, licence MIT, malý zdroj a - pro drahé pamětové mapy - RAM fast-path. Úplná metodika a surová data: [docs/benchmark_cz.txt](docs/benchmark_cz.txt).

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
cpu-z80/        Z80 emulátor
dasm-z80/       knihovna Z80 disassembleru
tests/          jednotkové testy cpu-z80 (441 případů)
tests-zexall/   ZEXALL validace cpu-z80 (67/67 PASS)
tests-dasm/     regresní testy dasm-z80 (resolver symbolů)
bench/          benchmarková sada
docs/           referenční dokumentace, výsledky benchmarků
```

## Sestavení

Požadavky: GCC nebo Clang (pro computed goto), C99, little-endian platforma.

```bash
# Spuštění testů
cd tests && make run        # 441 případů cpu-z80 (per-step jádro přes z80_step)
cd tests-zexall && make zexall  # ZEXALL přes dávkové jádro
cd tests-zexall && make zexall_z80_perstep.exe && ./zexall_z80_perstep.exe zexall.com  # ZEXALL přes per-step jádro
cd tests-dasm && make run   # regrese resolveru symbolů dasm-z80

# Spuštění benchmarků (batch / per-step / step+fast-path / dispatch jádro)
cd bench && make compare    # -O2 a -O3
```

cpu-z80 je jediná kompilační jednotka (`cpu/z80.c`, který includuje `cpu/z80_execute.inc`, plus `cpu/z80.h` a `utils/types.h`). Není potřeba žádný build systém - stačí přidat do projektu:

```bash
gcc -O2 -I cesta/k/cpu-z80 -c cpu/z80.c -o z80.o
# volitelný inline RAM fast-path (viz API §4.7):
gcc -O2 -DCPU_Z80_RAM_FASTPATH -I cesta/k/cpu-z80 -c cpu/z80.c -o z80.o
```

## Dokumentace

Emulátor obsahuje `API_en.txt` / `API_cz.txt` a `CHANGELOG_en.txt` / `CHANGELOG_cz.txt` s kompletní API referencí a historií verzí v angličtině i češtině.

## Licence

MIT

## Autor

Michal Hucik - [z80-mz800](https://github.com/michalhucik/z80-mz800)
