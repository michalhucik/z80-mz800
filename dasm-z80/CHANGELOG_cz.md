# Changelog

Všechny důležité změny v projektu dasm-z80 jsou dokumentovány v tomto souboru.

Formát vychází z [Keep a Changelog](https://keepachangelog.com/cs/1.1.0/).
Projekt používá [sémantické verzování](https://semver.org/lang/cs/).

## [0.1.3] - 2026-05-19

### Opraveno

- **RST symbol resolve v `z80_dasm_to_str_sym()`**: RST p má 8-bit
  operand (`Z80_OP_RST_VEC`), který `z80_dasm_to_str` formátuje jako
  `#%02x` (např. `#20`). Symtab substituce ale vždy sestavovala
  hledací klíč jako `#%04x` (např. `#0020`), takže `strstr()` nikdy
  neshledal shodu a symbol se tiše nenahradil. Přidána větev
  `inst->flow == Z80_FLOW_RST`, která pro všechny čtyři hex styly
  (`HASH` / `0X` / `DOLLAR` / `H_SUFFIX`) v lowercase i uppercase
  použije 2-místný hex formát.

### Přidáno

- Adresář `tests-dasm/` s standalone regresním testem
  (`test_dasm_sym.c`) pro symbol resolver, sestavovaný a spouštěný
  přes `cd tests-dasm && make run`. 12 testů pokrývajících CALL / JP /
  JP cc / JR / JR cc / DJNZ / RST resolve, no-match-zůstává-hex,
  filtr JP (HL)/(IX) a fallback při NULL symtab. Používá inline
  mini-framework ve stylu `tests/test_framework.h`, bez externích
  závislostí.

  Tímto je splněn slib z v0.1.2 o regresních testech - dorazil
  s jedním patch release zpožděním, aby mohl přijít ruku v ruce
  s pokrytím opravy RST.

## [0.1.2] - 2026-05-19

### Opraveno

- **Aliasing mnemoniky mezi po sobě jdoucími voláními `z80_dasm()`**:
  pole `z80_dasm_inst_t::mnemonic` bylo dosud `const char *`
  ukazující na jediný file-static buffer uvnitř
  `z80_dasm_extract_mnemonic()`. Dvě volání `z80_dasm()` na různé
  instrukce v rychlém sledu skončila se STEJNÝM pointerem - druhé
  volání tiše přepsalo text mnemoniky toho prvního, což rozbilo
  jakýkoli kód, který si disassemblovanou instrukci uložil k pozdějšímu
  použití. Nahrazeno embedded bufferem `char mnemonic[16]` (per-instance
  storage), takže každá struktura `z80_dasm_inst_t` nese vlastní kopii
  a je nyní bezpečné ji uchovávat, kopírovat a používat napříč vlákny.

### Přidáno

- `z80_dasm_extract_mnemonic_into(const char *format, char *buf,
  size_t buf_size)`: thread-safe varianta zapisující do bufferu
  volajícího.

### Deprecated

- `z80_dasm_extract_mnemonic()` zachován jako zpětně kompatibilní
  shim vracející pointer na vlastní file-static buffer. Nový kód má
  volat `z80_dasm_extract_mnemonic_into()`.

### Změněno

- Tři přiřazení `inst->mnemonic = "NOP*"` v `z80_dasm()` nahrazena
  za `strcpy(inst->mnemonic, "NOP*")`.
- Kontroly `inst->mnemonic` vs NULL v `z80_dasm_format.c` (3 místa)
  nahrazeny za `inst->mnemonic[0] != '\0'`, protože pole se rozpadá
  vždy na non-NULL pointer.

### Poznámka

V původní historii mz800new byl tento fix rozdělen do dvou commitů
(`7ef24f7` aliasing a změna typu, `30046a7` navazující úprava
NULL → empty-string checks ve `z80_dasm_format.c`). Sloučeny zde,
protože druhý commit existuje jen jako přímý důsledek změny typu
z prvního.

## [0.1.1] - 2026-05-19

### Opraveno

- **Build pod Linuxem/glibc**: `z80_dasm_internal.h` nyní explicitně
  vkládá `<stddef.h>` kvůli `NULL`. Na MinGW se tahalo tranzitivně
  přes `<stdint.h>`, ale glibc to negarantuje a makro `INV` (které
  expanduje na `NULL`) nebylo možné zkompilovat.
- **Zastaralá cesta v include po přejmenování upstreamu**: `z80_dasm.h`
  odkazoval na `../ai2-z80/utils/types.h`, který od přejmenování
  `ai2-z80` → `cpu-z80` neexistuje. Cesta opravena na
  `../cpu-z80/utils/types.h`.
- **Zbytky přejmenování v Makefile**: `-I../ai2-z80/utils` opraveno
  na `-I../cpu-z80/utils`; předtím knihovnu nešlo na čistém checkoutu
  vůbec sestavit.
- **Rozbitý `test` target v Makefile**: odstraněn; ukazoval na
  `../tests/test_dasm.c`, který v tomto repozitáři neexistuje.
  Regresní testy jsou plánovány pro v0.1.2.
- **MSYS2 temp adresář v Makefile**: doplněn export proměnných
  `TMPDIR/TMP/TEMP` na `/tmp`, aby GCC pod MSYS2 mohlo zapisovat
  dočasné soubory (jinak padá s "Permission denied" v `C:\WINDOWS\`).

### Změněno

- Komentář ve zdrojáku `z80_dasm_internal.h` o historickém makru
  `ai2-z80` aktualizován na `cpu-z80`.

## [0.1] - 2026-04-01

První verze knihovny. Kompletní implementace Z80 disassembleru
s pokročilými funkcemi pro debugger emulátoru MZ-800.

### Přidáno

#### Jádro disassembleru
- Dekódování všech 1792 instrukcí Z80 (7 opcode tabulek po 256 záznamech)
- Podpora všech dokumentovaných instrukcí dle Zilog UM0080
- Podpora nedokumentovaných instrukcí: SLL, IXH/IXL/IYH/IYL operace,
  DD CB/FD CB kopie do registru, IN F,(C), OUT (C),0, zrcadlené NEG/RETN/IM
- Detekce neplatných sekvencí (DD DD, DD FD, DD ED, neplatné ED) jako NOP*
- Strukturovaný výstup (z80_dasm_inst_t) s rozloženými operandy
- Hromadná disassemblace bloku (z80_dasm_block)
- Heuristické zpětné hledání začátku instrukce (z80_dasm_find_inst_start)

#### Metadata instrukcí
- Mapa čtených a zapisovaných registrů pro každou instrukci
- Mapa ovlivněných flagů pro každou instrukci
- Klasifikace instrukcí (official / undocumented / invalid)
- Typ toku řízení (12 kategorií: normal, jump, call, ret, rst, halt, ...)
- Časování: T-stavy základní i při větvení

#### Formátování výstupu
- 4 styly hexadecimálních čísel: #FF, 0xFF, $FF, FFh
- Přepínání velkých/malých písmen (LD A,B vs ld a,b)
- Volitelné zobrazení surových bajtů instrukce
- Volitelné zobrazení adresy
- Relativní skoky jako absolutní adresy nebo jako $+n/$-n
- 3 styly pojmenování IX půl-registrů (IXH/HX/XH)

#### Tabulka symbolů
- Mapování adres na symbolické názvy
- Binární vyhledávání O(log n)
- Automatické nahrazení adres symboly ve výstupu
- Rozlišení symbolů pro cílové adresy skoků a paměťové operandy

#### Analýza toku řízení
- Zjištění cílové adresy skoku/volání (z80_dasm_target_addr)
- Vyhodnocení podmínek větvení podle aktuálního stavu flagů (z80_dasm_branch_taken)
- Pohodlné obaly pro přístup k registrovým a flagovým maskám

#### Utility
- Převod relativního offsetu na absolutní adresu (z80_rel_to_abs)
- Převod absolutní adresy na relativní offset s kontrolou dosahu (z80_abs_to_rel)

#### Kompatibilita
- Drop-in náhrada za z80ex_dasm() se stejnou signaturou
- Podpora z80ex formátovacích flagů (WORDS_DEC, BYTES_DEC)
- Zachování z80ex konvencí pro T-stavy

#### Dokumentace a testy
- Kompletní Doxygen dokumentace všech veřejných i interních symbolů
- Uživatelská dokumentace (README.md) se 7 praktickými příklady
- 145 jednotkových testů pokrývajících všechny moduly
- Testování všech 252 základních opcode (bez prefixů)

#### Build systém
- Makefile pro GCC (MinGW64/MSYS2)
- Statická knihovna libdasm_z80.a
- Cílová platforma: MSYS2/MinGW64 na Windows
