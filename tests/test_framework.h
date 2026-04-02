/**
 * @file test_framework.h
 * @brief Minimalni testovaci framework pro Z80 emulator.
 *
 * Poskytuje makra pro definici testu, aserce a souhrn vysledku.
 * Kazdy test ma pristup k sdilene RAM a CPU instanci.
 */

#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cpu/z80.h"

/* ========== Pocitadla (definovana v test_main.c) ========== */

/** Celkovy pocet spustenych testu. */
extern int tests_run;
/** Pocet uspesnych testu. */
extern int tests_passed;
/** Pocet neuspesnych testu. */
extern int tests_failed;
/** Nazev aktualne beziciho testu. */
extern const char *current_test_name;
/** Priznak selhani aktualniho testu. */
extern int current_test_failed;

/* ========== 64KB RAM a I/O ========== */

/** 64KB pametovy prostor. */
static u8 test_ram[65536];

/** I/O port data - posledni zapsana hodnota. */
static u8 io_ports[256];
/** I/O port - posledni ctena hodnota (nastavitelna testem). */
static u8 io_read_data[256];
/** Posledni port na ktery se zapisovalo. */
static u16 last_io_write_port = 0;
/** Posledni port ze ktereho se cetlo. */
static u16 last_io_read_port = 0;
/** Pocet I/O zapisu. */
static int io_write_count = 0;
/** Pocet I/O cteni. */
static int io_read_count = 0;

/* ========== Callbacky ========== */

/**
 * @brief Callback pro cteni z pameti.
 * @param addr 16bitova adresa.
 * @return Bajt na dane adrese.
 */
static u8 test_mem_read(u16 addr) {
    return test_ram[addr];
}

/**
 * @brief Callback pro zapis do pameti.
 * @param addr 16bitova adresa.
 * @param data Zapisovany bajt.
 */
static void test_mem_write(u16 addr, u8 data) {
    test_ram[addr] = data;
}

/**
 * @brief Callback pro cteni z I/O portu.
 * @param port 16bitova adresa portu.
 * @return Bajt z portu.
 */
static u8 test_io_read(u16 port) {
    last_io_read_port = port;
    io_read_count++;
    return io_read_data[port & 0xFF];
}

/**
 * @brief Callback pro zapis na I/O port.
 * @param port 16bitova adresa portu.
 * @param data Zapisovany bajt.
 */
static void test_io_write(u16 port, u8 data) {
    last_io_write_port = port;
    io_ports[port & 0xFF] = data;
    io_write_count++;
}

/* ========== Intack/RETI callbacky ========== */

/** Pocet vyvolanych INTACK signalu. */
static int intack_count = 0;
/** Pocet vyvolanych RETI signalu. */
static int reti_count = 0;

/**
 * @brief Callback pro INTACK signal.
 */
static void test_intack(void) {
    intack_count++;
}

/**
 * @brief Callback pro RETI signal.
 */
static void test_reti(void) {
    reti_count++;
}

/* ========== CPU instance ========== */

/** Globalni CPU instance pro testy. */
static z80_t cpu;

/* ========== Pomocne funkce ========== */

/**
 * @brief Inicializace testovaci CPU a pameti.
 *
 * Vynuluje RAM, resetuje I/O countery, inicializuje CPU a nastavi callbacky.
 */
static void setup(void) {
    memset(test_ram, 0, sizeof(test_ram));
    memset(io_ports, 0, sizeof(io_ports));
    memset(io_read_data, 0, sizeof(io_read_data));
    last_io_write_port = 0;
    last_io_read_port = 0;
    io_write_count = 0;
    io_read_count = 0;
    intack_count = 0;
    reti_count = 0;
    z80_init(&cpu);
    z80_set_mem_read(test_mem_read);
    z80_set_mem_write(test_mem_write);
    z80_set_mem_fetch(test_mem_read);
    z80_set_io_read(test_io_read);
    z80_set_io_write(test_io_write);
    z80_set_intack(test_intack);
    z80_set_reti_fn(test_reti);
}

/**
 * @brief Nacte sekvenci bajtu do RAM na danou adresu.
 * @param addr Cilova adresa.
 * @param data Pole bajtu.
 * @param len Pocet bajtu.
 */
static void load_bytes(u16 addr, const u8 *data, int len) {
    for (int i = 0; i < len; i++) {
        test_ram[(u16)(addr + i)] = data[i];
    }
}

/**
 * @brief Provede jednu instrukci a vrati pocet T-stavu.
 * @return Pocet T-stavu spotrebovanych instrukci.
 */
static int step(void) {
    return z80_step(&cpu);
}

/* ========== Testovaci makra ========== */

/**
 * Zahajeni testu s danym nazvem.
 */
#define TEST_BEGIN(name) do { \
    current_test_name = (name); \
    current_test_failed = 0; \
    tests_run++; \
} while(0)

/**
 * Ukonceni testu - vypise PASS/FAIL.
 */
#define TEST_END() do { \
    if (current_test_failed) { \
        tests_failed++; \
    } else { \
        tests_passed++; \
    } \
} while(0)

/**
 * Aserze rovnosti dvou hodnot.
 * Pri selhani vypise ocekavanou a skutecnou hodnotu.
 */
#define ASSERT_EQ(actual, expected) do { \
    long long _a = (long long)(actual); \
    long long _e = (long long)(expected); \
    if (_a != _e) { \
        printf("  FAIL [%s]: %s == 0x%llX, ocekavano 0x%llX (radek %d)\n", \
               current_test_name, #actual, _a, _e, __LINE__); \
        current_test_failed = 1; \
    } \
} while(0)

/**
 * Aserze boolove podminky.
 */
#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        printf("  FAIL [%s]: %s neni true (radek %d)\n", \
               current_test_name, #cond, __LINE__); \
        current_test_failed = 1; \
    } \
} while(0)

/**
 * Aserze neplatnosti boolove podminky.
 */
#define ASSERT_FALSE(cond) do { \
    if (cond) { \
        printf("  FAIL [%s]: %s neni false (radek %d)\n", \
               current_test_name, #cond, __LINE__); \
        current_test_failed = 1; \
    } \
} while(0)

/**
 * Kontrola konkretniho flagu.
 */
#define ASSERT_FLAG_SET(flag) do { \
    if (!(cpu.af.l & (flag))) { \
        printf("  FAIL [%s]: flag %s neni nastaven (F=0x%02X, radek %d)\n", \
               current_test_name, #flag, cpu.af.l, __LINE__); \
        current_test_failed = 1; \
    } \
} while(0)

#define ASSERT_FLAG_CLEAR(flag) do { \
    if (cpu.af.l & (flag)) { \
        printf("  FAIL [%s]: flag %s je nastaven (F=0x%02X, radek %d)\n", \
               current_test_name, #flag, cpu.af.l, __LINE__); \
        current_test_failed = 1; \
    } \
} while(0)

/**
 * Kontrola presne hodnoty F registru.
 */
#define ASSERT_FLAGS(expected) do { \
    u8 _ef = (u8)(expected); \
    if (cpu.af.l != _ef) { \
        printf("  FAIL [%s]: F == 0x%02X, ocekavano 0x%02X (radek %d)\n", \
               current_test_name, cpu.af.l, _ef, __LINE__); \
        current_test_failed = 1; \
    } \
} while(0)

/**
 * Kontrola T-stavu instrukce.
 */
#define ASSERT_CYCLES(actual, expected) do { \
    int _ac = (actual); \
    int _ec = (expected); \
    if (_ac != _ec) { \
        printf("  FAIL [%s]: cykly == %d, ocekavano %d (radek %d)\n", \
               current_test_name, _ac, _ec, __LINE__); \
        current_test_failed = 1; \
    } \
} while(0)

/**
 * Vypise souhrn vysledku testu a vrati exit code.
 */
#define TEST_SUMMARY() do { \
    printf("\n========================================\n"); \
    printf("Vysledky: %d testu, %d PASS, %d FAIL\n", \
           tests_run, tests_passed, tests_failed); \
    printf("========================================\n"); \
    return (tests_failed > 0) ? 1 : 0; \
} while(0)

/**
 * Spusti testovaci funkci a vypise nazev sekce.
 */
#define RUN_SUITE(fn) do { \
    printf("\n--- %s ---\n", #fn); \
    fn(); \
} while(0)

#endif /* TEST_FRAMEWORK_H */
