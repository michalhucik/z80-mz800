/**
 * @file test_call_ret_hooks.c
 * @brief Testy callbacku z80_set_call a z80_set_ret (CALL / RET dispatch).
 *
 * Pokriva:
 *   - CALL nn (0xCD) fire call_cb s call_site/target/return_addr
 *   - CALL cc, nn (taken) fire call_cb
 *   - CALL cc, nn (not taken) NEFIRE call_cb
 *   - RET (0xC9) fire ret_cb s pop_target/sp_before_pop
 *   - RET cc (taken) fire ret_cb
 *   - RET cc (not taken) NEFIRE ret_cb
 *   - NULL callback v hot path = no crash
 *   - Bezne instrukce (LD/ADD) negenerujou call/ret event
 *   - RETI (ED 4D) NEFIRE ret_cb (jen reti_cb / iff_change_cb)
 *   - RETN (ED 45) NEFIRE ret_cb (jen iff_change_cb)
 *   - z80_reset zachova oba callbacky (save/restore)
 */

#include "test_framework.h"

/* ========== Zaznam call/ret eventu ========== */

#define MAX_EVENTS 16

typedef struct {
    int  kind;        /* 1 = CALL, 2 = RET */
    u16  call_site;   /* CALL: adresa CALL opkodu */
    u16  target;      /* CALL: cilova adresa */
    u16  return_addr; /* CALL: za-CALL PC; RET: pop_target */
    u16  sp;          /* RET: sp_before_pop */
} call_ret_record_t;

static call_ret_record_t g_events[MAX_EVENTS];
static int g_event_count;

static void record_call(struct z80_s *cpu, u16 call_site, u16 target,
                        u16 return_addr, void *user_data) {
    (void)cpu; (void)user_data;
    if (g_event_count < MAX_EVENTS) {
        g_events[g_event_count].kind = 1;
        g_events[g_event_count].call_site = call_site;
        g_events[g_event_count].target = target;
        g_events[g_event_count].return_addr = return_addr;
        g_event_count++;
    }
}

static void record_ret(struct z80_s *cpu, u16 pop_target, u16 sp,
                       void *user_data) {
    (void)cpu; (void)user_data;
    if (g_event_count < MAX_EVENTS) {
        g_events[g_event_count].kind = 2;
        g_events[g_event_count].return_addr = pop_target;
        g_events[g_event_count].sp = sp;
        g_event_count++;
    }
}

static void reset_events(void) {
    g_event_count = 0;
    memset(g_events, 0, sizeof(g_events));
}

/* ========== Testy ========== */

/**
 * @brief CALL nn musi fire call_cb se spravnym call_site, target, return_addr.
 */
static void test_call_nn_fires_call_cb(void) {
    TEST_BEGIN("CALL nn fires call_cb");
    setup();
    reset_events();
    z80_set_call(cpu, record_call, NULL);
    z80_set_ret(cpu, record_ret, NULL);
    cpu->sp = 0xFFFE;
    cpu->pc = 0x1000;
    /* CALL 0x2345 - 3 bajty: CD 45 23 */
    test_ram[0x1000] = 0xCD;
    test_ram[0x1001] = 0x45;
    test_ram[0x1002] = 0x23;
    (void)step();
    ASSERT_EQ(g_event_count, 1);
    ASSERT_EQ(g_events[0].kind, 1);          /* CALL */
    ASSERT_EQ(g_events[0].call_site, 0x1000); /* adresa CD opkodu */
    ASSERT_EQ(g_events[0].target, 0x2345);   /* cilova adresa */
    ASSERT_EQ(g_events[0].return_addr, 0x1003); /* za CALL */
    ASSERT_EQ(cpu->pc, 0x2345);
    TEST_END();
}

/**
 * @brief CALL cc, nn (taken) musi fire call_cb.
 */
static void test_call_cc_taken_fires(void) {
    TEST_BEGIN("CALL Z, nn (taken) fires call_cb");
    setup();
    reset_events();
    z80_set_call(cpu, record_call, NULL);
    z80_set_ret(cpu, record_ret, NULL);
    cpu->sp = 0xFFFE;
    cpu->af.l = 0x40;  /* ZF = 1 */
    cpu->pc = 0x1000;
    /* CALL Z, 0x2345 - CC 45 23 */
    test_ram[0x1000] = 0xCC;
    test_ram[0x1001] = 0x45;
    test_ram[0x1002] = 0x23;
    (void)step();
    ASSERT_EQ(g_event_count, 1);
    ASSERT_EQ(g_events[0].kind, 1);
    ASSERT_EQ(g_events[0].call_site, 0x1000);
    ASSERT_EQ(g_events[0].target, 0x2345);
    ASSERT_EQ(g_events[0].return_addr, 0x1003);
    TEST_END();
}

/**
 * @brief CALL cc, nn (not taken) NESMI fire call_cb.
 */
static void test_call_cc_not_taken_no_fire(void) {
    TEST_BEGIN("CALL Z, nn (not taken) does not fire");
    setup();
    reset_events();
    z80_set_call(cpu, record_call, NULL);
    z80_set_ret(cpu, record_ret, NULL);
    cpu->sp = 0xFFFE;
    cpu->af.l = 0x00;  /* ZF = 0 */
    cpu->pc = 0x1000;
    test_ram[0x1000] = 0xCC;  /* CALL Z, 0x2345 */
    test_ram[0x1001] = 0x45;
    test_ram[0x1002] = 0x23;
    (void)step();
    ASSERT_EQ(g_event_count, 0);
    ASSERT_EQ(cpu->pc, 0x1003);  /* pokracovala za CALL bez skoku */
    TEST_END();
}

/**
 * @brief RET (0xC9) musi fire ret_cb s pop_target = vrch stacku.
 */
static void test_ret_fires_ret_cb(void) {
    TEST_BEGIN("RET fires ret_cb");
    setup();
    reset_events();
    z80_set_call(cpu, record_call, NULL);
    z80_set_ret(cpu, record_ret, NULL);
    /* Pripravime stack tak, ze na SP je 0x3456 */
    cpu->sp = 0x8000;
    test_ram[0x8000] = 0x56;  /* lo */
    test_ram[0x8001] = 0x34;  /* hi */
    cpu->pc = 0x1000;
    test_ram[0x1000] = 0xC9;  /* RET */
    u16 sp_before = cpu->sp;
    (void)step();
    ASSERT_EQ(g_event_count, 1);
    ASSERT_EQ(g_events[0].kind, 2);
    ASSERT_EQ(g_events[0].return_addr, 0x3456); /* pop_target */
    ASSERT_EQ(g_events[0].sp, sp_before);
    ASSERT_EQ(cpu->pc, 0x3456);
    TEST_END();
}

/**
 * @brief RET cc (taken) musi fire ret_cb.
 */
static void test_ret_cc_taken_fires(void) {
    TEST_BEGIN("RET Z (taken) fires ret_cb");
    setup();
    reset_events();
    z80_set_call(cpu, record_call, NULL);
    z80_set_ret(cpu, record_ret, NULL);
    cpu->sp = 0x8000;
    test_ram[0x8000] = 0x56;
    test_ram[0x8001] = 0x34;
    cpu->af.l = 0x40;  /* ZF = 1 */
    cpu->pc = 0x1000;
    test_ram[0x1000] = 0xC8;  /* RET Z */
    (void)step();
    ASSERT_EQ(g_event_count, 1);
    ASSERT_EQ(g_events[0].kind, 2);
    ASSERT_EQ(g_events[0].return_addr, 0x3456);
    ASSERT_EQ(cpu->pc, 0x3456);
    TEST_END();
}

/**
 * @brief RET cc (not taken) NESMI fire ret_cb.
 */
static void test_ret_cc_not_taken_no_fire(void) {
    TEST_BEGIN("RET Z (not taken) does not fire");
    setup();
    reset_events();
    z80_set_call(cpu, record_call, NULL);
    z80_set_ret(cpu, record_ret, NULL);
    cpu->sp = 0x8000;
    test_ram[0x8000] = 0x56;
    test_ram[0x8001] = 0x34;
    cpu->af.l = 0x00;  /* ZF = 0 */
    cpu->pc = 0x1000;
    test_ram[0x1000] = 0xC8;  /* RET Z */
    (void)step();
    ASSERT_EQ(g_event_count, 0);
    ASSERT_EQ(cpu->pc, 0x1001);  /* pokracovala za RET bez popu */
    ASSERT_EQ(cpu->sp, 0x8000);  /* SP se nezmenilo */
    TEST_END();
}

/**
 * @brief NULL callback v hot path nesmi zhavarovat.
 */
static void test_null_cb_no_crash(void) {
    TEST_BEGIN("NULL cb - CALL and RET do not crash");
    setup();
    reset_events();
    z80_set_call(cpu, NULL, NULL);
    z80_set_ret(cpu, NULL, NULL);
    cpu->sp = 0x8000;
    test_ram[0x8000] = 0x56;
    test_ram[0x8001] = 0x34;
    cpu->pc = 0x1000;
    test_ram[0x1000] = 0xCD;  /* CALL 0x3456 */
    test_ram[0x1001] = 0x56;
    test_ram[0x1002] = 0x34;
    test_ram[0x3456] = 0xC9;  /* RET */
    (void)step();  /* CALL */
    (void)step();  /* RET */
    ASSERT_EQ(g_event_count, 0);  /* zadny event */
    TEST_END();
}

/**
 * @brief Bezne LD/ADD nesmi fire call_cb ani ret_cb.
 */
static void test_normal_instructions_no_event(void) {
    TEST_BEGIN("Normal instructions generate no call/ret event");
    setup();
    reset_events();
    z80_set_call(cpu, record_call, NULL);
    z80_set_ret(cpu, record_ret, NULL);
    cpu->pc = 0x1000;
    test_ram[0x1000] = 0x3E; test_ram[0x1001] = 0x42;  /* LD A, 0x42 */
    test_ram[0x1002] = 0x80;                            /* ADD A, B */
    test_ram[0x1003] = 0x00;                            /* NOP */
    (void)step();
    (void)step();
    (void)step();
    ASSERT_EQ(g_event_count, 0);
    TEST_END();
}

/**
 * @brief RETI (ED 4D) NESMI fire ret_cb (ma vlastni reti_cb / iff_change_cb).
 */
static void test_reti_no_ret_cb(void) {
    TEST_BEGIN("RETI does not fire ret_cb");
    setup();
    reset_events();
    z80_set_call(cpu, record_call, NULL);
    z80_set_ret(cpu, record_ret, NULL);
    cpu->sp = 0x8000;
    test_ram[0x8000] = 0x56;
    test_ram[0x8001] = 0x34;
    cpu->pc = 0x1000;
    test_ram[0x1000] = 0xED;  /* RETI */
    test_ram[0x1001] = 0x4D;
    (void)step();
    ASSERT_EQ(g_event_count, 0);
    ASSERT_EQ(cpu->pc, 0x3456);  /* RETI se provedlo, jen bez ret_cb */
    TEST_END();
}

/**
 * @brief RETN (ED 45) NESMI fire ret_cb (jen iff_change_cb).
 */
static void test_retn_no_ret_cb(void) {
    TEST_BEGIN("RETN does not fire ret_cb");
    setup();
    reset_events();
    z80_set_call(cpu, record_call, NULL);
    z80_set_ret(cpu, record_ret, NULL);
    cpu->sp = 0x8000;
    test_ram[0x8000] = 0x56;
    test_ram[0x8001] = 0x34;
    cpu->pc = 0x1000;
    test_ram[0x1000] = 0xED;  /* RETN */
    test_ram[0x1001] = 0x45;
    (void)step();
    ASSERT_EQ(g_event_count, 0);
    ASSERT_EQ(cpu->pc, 0x3456);
    TEST_END();
}

/**
 * @brief z80_reset musi zachovat oba callbacky.
 */
static void test_reset_preserves_cbs(void) {
    TEST_BEGIN("z80_reset preserves call_cb and ret_cb");
    setup();
    reset_events();
    z80_set_call(cpu, record_call, NULL);
    z80_set_ret(cpu, record_ret, NULL);
    z80_reset(cpu);
    /* Po reset musi oba callbacky dal fungovat */
    cpu->sp = 0x8000;
    test_ram[0x8000] = 0x56;
    test_ram[0x8001] = 0x34;
    cpu->pc = 0x1000;
    test_ram[0x1000] = 0xC9;  /* RET */
    (void)step();
    int found_ret = 0;
    for (int i = 0; i < g_event_count; i++) {
        if (g_events[i].kind == 2) found_ret = 1;
    }
    ASSERT_TRUE(found_ret);
    TEST_END();
}

/* ========== Hlavni runner ========== */

void test_call_ret_hooks_all(void) {
    test_call_nn_fires_call_cb();
    test_call_cc_taken_fires();
    test_call_cc_not_taken_no_fire();
    test_ret_fires_ret_cb();
    test_ret_cc_taken_fires();
    test_ret_cc_not_taken_no_fire();
    test_null_cb_no_crash();
    test_normal_instructions_no_event();
    test_reti_no_ret_cb();
    test_retn_no_ret_cb();
    test_reset_preserves_cbs();
}
