/**
 * @file test_cpu_ctrl_event.c
 * @brief Testy callbacku z80_set_cpu_ctrl_event (HALT enter/exit, RST nn).
 *
 * Pokriva:
 *   - HALT entry (opcode 0x76) -> HALT_ENTER + pc za HALT
 *   - HALT exit pri IM1 INT ack -> HALT_EXIT + pc za HALT
 *   - HALT exit pri NMI ack -> HALT_EXIT + pc za HALT
 *   - RST 00h (0xC7) -> RST_00 + pc za RST
 *   - RST 38h (0xFF) -> RST_38 + pc za RST
 *   - NULL callback hot path = no crash, zadny event nezaznamenan
 *   - Bezna instrukce (LD/ADD) negeneruje cpu_ctrl_event
 *   - INT mezi beznymi instrukcemi NEgeneruje HALT_EXIT
 *   - z80_reset zachova nastaveny callback (save/restore)
 */

#include "test_framework.h"

/* ========== Zaznam eventu ========== */

/** Maximalni pocet zaznamenanych eventu na jeden test. */
#define MAX_EVENTS 16

typedef struct {
    u8  event;
    u16 pc;
} ctrl_event_record_t;

static ctrl_event_record_t g_events[MAX_EVENTS];
static int g_event_count;

/**
 * @brief Callback pro z80_set_cpu_ctrl_event - zaznamenava do g_events.
 */
static void record_ctrl_event(struct z80_s *cpu, u8 event, u16 pc, void *user_data) {
    (void)cpu; (void)user_data;
    if (g_event_count < MAX_EVENTS) {
        g_events[g_event_count].event = event;
        g_events[g_event_count].pc = pc;
        g_event_count++;
    }
}

/**
 * @brief Reset event recorderu - volat pred kazdym sub-testem.
 */
static void reset_events(void) {
    g_event_count = 0;
    memset(g_events, 0, sizeof(g_events));
}

/* ========== Testy ========== */

/**
 * @brief HALT (0x76) musi fire HALT_ENTER s PC za HALT.
 */
static void test_halt_entry_fires_halt_enter(void) {
    TEST_BEGIN("HALT entry fires HALT_ENTER with PC after HALT");
    setup();
    reset_events();
    z80_set_cpu_ctrl_event(cpu, record_ctrl_event, NULL);
    cpu->pc = 0x1000;
    test_ram[0x1000] = 0x76;  /* HALT */
    int cyc = step();
    ASSERT_EQ(g_event_count, 1);
    ASSERT_EQ(g_events[0].event, Z80_CPU_CTRL_HALT_ENTER);
    ASSERT_EQ(g_events[0].pc, 0x1001);  /* za HALT */
    ASSERT_EQ(cpu->halted, true);
    ASSERT_CYCLES(cyc, 4);
    TEST_END();
}

/**
 * @brief HALT exit pri IM1 INT ack musi fire HALT_EXIT s PC za HALT.
 */
static void test_halt_exit_im1_int(void) {
    TEST_BEGIN("HALT exit on IM1 INT fires HALT_EXIT");
    setup();
    reset_events();
    z80_set_cpu_ctrl_event(cpu, record_ctrl_event, NULL);
    cpu->sp = 0xFFFE;
    cpu->iff1 = 1; cpu->iff2 = 1; cpu->im = 1;
    cpu->pc = 0x1000;
    test_ram[0x1000] = 0x76;  /* HALT */
    test_ram[0x0038] = 0x00;  /* IM1 vektor: NOP */
    /* Krok 1: HALT - cpu->halted = true, HALT_ENTER fire */
    (void)step();
    ASSERT_EQ(g_event_count, 1);
    ASSERT_EQ(g_events[0].event, Z80_CPU_CTRL_HALT_ENTER);
    /* Krok 2: vyvolat INT - dispatch handler clearuje halted, fire HALT_EXIT */
    z80_int(cpu);
    (void)step();
    /* Mezi krokem 2 mohou prijit dalsi eventy (jen HALT_EXIT) */
    ASSERT_TRUE(g_event_count >= 2);
    ASSERT_EQ(g_events[1].event, Z80_CPU_CTRL_HALT_EXIT);
    ASSERT_EQ(g_events[1].pc, 0x1001);  /* za HALT */
    ASSERT_EQ(cpu->halted, false);
    TEST_END();
}

/**
 * @brief HALT exit pri NMI ack musi fire HALT_EXIT s PC za HALT.
 */
static void test_halt_exit_nmi(void) {
    TEST_BEGIN("HALT exit on NMI fires HALT_EXIT");
    setup();
    reset_events();
    z80_set_cpu_ctrl_event(cpu, record_ctrl_event, NULL);
    cpu->sp = 0xFFFE;
    cpu->pc = 0x2000;
    test_ram[0x2000] = 0x76;  /* HALT */
    test_ram[0x0066] = 0x00;  /* NMI vektor */
    (void)step();  /* HALT_ENTER */
    ASSERT_EQ(g_events[0].event, Z80_CPU_CTRL_HALT_ENTER);
    z80_nmi(cpu);
    (void)step();
    ASSERT_TRUE(g_event_count >= 2);
    ASSERT_EQ(g_events[1].event, Z80_CPU_CTRL_HALT_EXIT);
    ASSERT_EQ(g_events[1].pc, 0x2001);  /* za HALT */
    ASSERT_EQ(cpu->halted, false);
    TEST_END();
}

/**
 * @brief RST 00h (0xC7) musi fire RST_00 s PC za RST.
 */
static void test_rst_00(void) {
    TEST_BEGIN("RST 00h fires RST_00 with PC after RST");
    setup();
    reset_events();
    z80_set_cpu_ctrl_event(cpu, record_ctrl_event, NULL);
    cpu->sp = 0xFFFE;
    cpu->pc = 0x3000;
    test_ram[0x3000] = 0xC7;  /* RST 00h */
    test_ram[0x0000] = 0x00;  /* RST vektor */
    int cyc = step();
    ASSERT_EQ(g_event_count, 1);
    ASSERT_EQ(g_events[0].event, Z80_CPU_CTRL_RST_00);
    ASSERT_EQ(g_events[0].pc, 0x3001);  /* za RST */
    ASSERT_EQ(cpu->pc, 0x0000);
    ASSERT_CYCLES(cyc, 11);
    TEST_END();
}

/**
 * @brief RST 38h (0xFF) musi fire RST_38 s PC za RST.
 */
static void test_rst_38(void) {
    TEST_BEGIN("RST 38h fires RST_38 with PC after RST");
    setup();
    reset_events();
    z80_set_cpu_ctrl_event(cpu, record_ctrl_event, NULL);
    cpu->sp = 0xFFFE;
    cpu->pc = 0x4000;
    test_ram[0x4000] = 0xFF;  /* RST 38h */
    int cyc = step();
    ASSERT_EQ(g_event_count, 1);
    ASSERT_EQ(g_events[0].event, Z80_CPU_CTRL_RST_38);
    ASSERT_EQ(g_events[0].pc, 0x4001);
    ASSERT_EQ(cpu->pc, 0x0038);
    ASSERT_CYCLES(cyc, 11);
    TEST_END();
}

/**
 * @brief NULL callback v hot path - HALT/RST nesmi zhavarovat.
 */
static void test_null_cb_no_crash(void) {
    TEST_BEGIN("NULL cb - HALT and RST do not crash");
    setup();
    reset_events();
    z80_set_cpu_ctrl_event(cpu, NULL, NULL);
    cpu->sp = 0xFFFE;
    cpu->pc = 0x5000;
    test_ram[0x5000] = 0xC7;  /* RST 00 */
    test_ram[0x0000] = 0x76;  /* HALT na vektoru */
    (void)step();   /* RST 00 - dispatch bez cb */
    (void)step();   /* HALT bez cb */
    ASSERT_EQ(g_event_count, 0);  /* zadny event nezaznamenan */
    ASSERT_EQ(cpu->halted, true);
    TEST_END();
}

/**
 * @brief Bezne LD/ADD nesmi fire cpu_ctrl_event.
 */
static void test_normal_instruction_no_event(void) {
    TEST_BEGIN("Normal instruction (LD/ADD) generates no event");
    setup();
    reset_events();
    z80_set_cpu_ctrl_event(cpu, record_ctrl_event, NULL);
    cpu->pc = 0x6000;
    test_ram[0x6000] = 0x3E; test_ram[0x6001] = 0x42;  /* LD A, 0x42 */
    test_ram[0x6002] = 0x80;                            /* ADD A, B */
    (void)step();
    (void)step();
    ASSERT_EQ(g_event_count, 0);
    TEST_END();
}

/**
 * @brief INT prijaty mezi beznymi instrukcemi (ne v HALT) nesmi
 *        fire HALT_EXIT.
 */
static void test_int_between_normal_no_halt_exit(void) {
    TEST_BEGIN("INT between normal instructions: no HALT_EXIT");
    setup();
    reset_events();
    z80_set_cpu_ctrl_event(cpu, record_ctrl_event, NULL);
    cpu->sp = 0xFFFE;
    cpu->iff1 = 1; cpu->iff2 = 1; cpu->im = 1;
    cpu->pc = 0x7000;
    test_ram[0x7000] = 0x00;  /* NOP */
    test_ram[0x7001] = 0x00;  /* NOP */
    test_ram[0x0038] = 0x00;  /* IM1 vektor */
    (void)step();  /* NOP - zadny event */
    ASSERT_EQ(g_event_count, 0);
    /* Vyvolat INT mezi beznymi instrukcemi */
    z80_int(cpu);
    (void)step();
    /* HALT_EXIT NESMI fire, protoze cpu nebylo v HALT */
    for (int i = 0; i < g_event_count; i++) {
        ASSERT_TRUE(g_events[i].event != Z80_CPU_CTRL_HALT_EXIT);
    }
    TEST_END();
}

/**
 * @brief z80_reset musi zachovat nastaveny callback (save/restore).
 */
static void test_reset_preserves_cb(void) {
    TEST_BEGIN("z80_reset preserves cpu_ctrl_event callback");
    setup();
    reset_events();
    z80_set_cpu_ctrl_event(cpu, record_ctrl_event, NULL);
    z80_reset(cpu);
    /* Po reset musi callback dal fungovat */
    cpu->pc = 0x8000;
    test_ram[0x8000] = 0xC7;  /* RST 00 */
    (void)step();
    ASSERT_TRUE(g_event_count >= 1);
    int found_rst = 0;
    for (int i = 0; i < g_event_count; i++) {
        if (g_events[i].event == Z80_CPU_CTRL_RST_00) found_rst = 1;
    }
    ASSERT_TRUE(found_rst);
    TEST_END();
}

/* ========== Hlavni runner ========== */

/**
 * @brief Spusti vsechny testy cpu_ctrl_event subsystemu.
 */
void test_cpu_ctrl_event_all(void) {
    test_halt_entry_fires_halt_enter();
    test_halt_exit_im1_int();
    test_halt_exit_nmi();
    test_rst_00();
    test_rst_38();
    test_null_cb_no_crash();
    test_normal_instruction_no_event();
    test_int_between_normal_no_halt_exit();
    test_reset_preserves_cb();
}
