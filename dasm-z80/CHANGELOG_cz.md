# Changelog

Všechny důležité změny v projektu dasm-z80 jsou dokumentovány v tomto souboru.

Formát vychází z [Keep a Changelog](https://keepachangelog.com/cs/1.1.0/).
Projekt používá [sémantické verzování](https://semver.org/lang/cs/).

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
