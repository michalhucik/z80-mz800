/**
 * @file test_iff_change.c
 * @brief Testy callbacku změny IFF1/IFF2 (z80_set_iff_change).
 *
 * Pokrývá:
 *   - DI -> reason DI, IFF1 = 0, IFF2 = 0
 *   - EI -> reason EI, IFF1 = 1, IFF2 = 1
 *   - přijetí INT v IM 1, IM 2 a IM 0 (RST 38h) -> reason INT_ACK,
 *     IFF1 = 0, IFF2 = 0
 *   - přijetí NMI -> reason NMI_ACK, IFF1 = 0, IFF2 = původní IFF1
 *   - RETN po NMI -> reason RETN, IFF1 <- IFF2
 *   - RETI -> reason RETI, IFF1 <- IFF2
 *   - z80_reset -> reason RESET, IFF1 = 0, IFF2 = 0, user_data zachována
 *   - NULL callback -> žádný pád, instrukce proběhne normálně
 *   - dílčí callbacky ei/di/intack/reti/nmi dál volané beze změny
 */

#include "test_framework.h"

/** Záznam volání callbacků během testu. */
typedef struct {
    int   fire_count;       /**< Počet volání iff_change_cb celkem. */
    u8    last_iff1;        /**< Poslední new_iff1. */
    u8    last_iff2;        /**< Poslední new_iff2. */
    u8    last_reason;      /**< Poslední reason (z80_iff_change_reason_t). */
    void *last_user_data;   /**< Poslední user_data. */
    int   ei_count;         /**< Počet volání ei_cb. */
    int   di_count;         /**< Počet volání di_cb. */
    int   intack_count;     /**< Počet volání intack_cb. */
    int   reti_count;       /**< Počet volání reti_cb. */
    int   nmi_assert_count; /**< Počet volání nmi_cb (z80_nmi). */
} iff_track_t;

/** Záznam volání aktuálního testu. */
static iff_track_t iff_track;

/** Hodnota user_data předávaná callbacku iff_change. */
#define IFF_USER_DATA ((void *)0xC0FFEE)

/**
 * @brief Callback změny IFF - zaznamená poslední hodnoty.
 * @param cpu       Ukazatel na CPU instanci (nepoužit).
 * @param new_iff1  Nová hodnota IFF1.
 * @param new_iff2  Nová hodnota IFF2.
 * @param reason    Důvod změny.
 * @param user_data Uživatelská data předaná při registraci.
 */
static void iff_cb(struct z80_s *cpu, u8 new_iff1, u8 new_iff2, u8 reason, void *user_data) {
    (void)cpu;
    iff_track.fire_count++;
    iff_track.last_iff1 = new_iff1;
    iff_track.last_iff2 = new_iff2;
    iff_track.last_reason = reason;
    iff_track.last_user_data = user_data;
}

/** @brief Callback EI - počítá volání. */
static void iff_ei_cb(struct z80_s *cpu, void *user_data) {
    (void)cpu; (void)user_data;
    iff_track.ei_count++;
}

/** @brief Callback DI - počítá volání. */
static void iff_di_cb(struct z80_s *cpu, void *user_data) {
    (void)cpu; (void)user_data;
    iff_track.di_count++;
}

/** @brief Callback INTACK - počítá volání. */
static void iff_intack_cb(struct z80_s *cpu, void *user_data) {
    (void)cpu; (void)user_data;
    iff_track.intack_count++;
}

/** @brief Callback RETI - počítá volání. */
static void iff_reti_cb(struct z80_s *cpu, void *user_data) {
    (void)cpu; (void)user_data;
    iff_track.reti_count++;
}

/** @brief Callback NMI assertion (z80_nmi) - počítá volání. */
static void iff_nmi_cb(struct z80_s *cpu, void *user_data) {
    (void)cpu; (void)user_data;
    iff_track.nmi_assert_count++;
}

/**
 * @brief Nová CPU instance se všemi sledovanými callbacky.
 * @post Paměť vynulovaná, iff_track vynulovaný, iff_change_cb nastaven
 *       s user_data IFF_USER_DATA.
 */
static void iff_setup(void) {
    setup();
    memset(&iff_track, 0, sizeof(iff_track));
    z80_set_ei(cpu, iff_ei_cb, NULL);
    z80_set_di(cpu, iff_di_cb, NULL);
    z80_set_intack(cpu, iff_intack_cb, NULL);
    z80_set_reti(cpu, iff_reti_cb, NULL);
    z80_set_nmi_cb(cpu, iff_nmi_cb, NULL);
    z80_set_iff_change(cpu, iff_cb, IFF_USER_DATA);
}

/**
 * @brief Připraví program IM n, EI, NOP, NOP a provede první tři instrukce.
 *
 * Po návratu je IFF1 = IFF2 = 1, ei_delay smazaný a PC = 0x0004, takže
 * vyvolané INT se přijme v příštím kroku.
 *
 * @param im_opcode Druhý bajt instrukce IM (0x46 = IM 0, 0x56 = IM 1, 0x5E = IM 2).
 */
static void iff_prepare_int(u8 im_opcode) {
    test_ram[0x0000] = 0xED;
    test_ram[0x0001] = im_opcode;  /* IM n */
    test_ram[0x0002] = 0xFB;       /* EI */
    test_ram[0x0003] = 0x00;       /* NOP - první instrukce po EI, blokuje INT */
    test_ram[0x0004] = 0x00;       /* NOP - tady už INT může být přijato */
    cpu->pc = 0x0000;
    cpu->sp = 0xFFFE;
    (void)step();  /* IM n */
    (void)step();  /* EI */
    (void)step();  /* NOP - smaže ei_delay */
}

/** @brief DI volá callback s reason DI, IFF1 = IFF2 = 0; di_cb dál volán. */
static void test_di_fires_reason_di(void) {
    TEST_BEGIN("iff_change: DI fires reason DI");
    iff_setup();
    test_ram[0x0000] = 0xFB;  /* EI */
    test_ram[0x0001] = 0xF3;  /* DI */
    cpu->pc = 0x0000;
    (void)step();  /* EI */
    (void)step();  /* DI */
    ASSERT_EQ(cpu->iff1, 0);
    ASSERT_EQ(cpu->iff2, 0);
    ASSERT_EQ(iff_track.last_reason, Z80_IFF_REASON_DI);
    ASSERT_EQ(iff_track.last_iff1, 0);
    ASSERT_EQ(iff_track.last_iff2, 0);
    ASSERT_EQ(iff_track.fire_count, 2);  /* EI + DI */
    ASSERT_EQ(iff_track.di_count, 1);
    ASSERT_EQ(iff_track.ei_count, 1);
    ASSERT_TRUE(iff_track.last_user_data == IFF_USER_DATA);
    TEST_END();
}

/** @brief EI volá callback s reason EI, IFF1 = IFF2 = 1. */
static void test_ei_fires_reason_ei(void) {
    TEST_BEGIN("iff_change: EI fires reason EI");
    iff_setup();
    test_ram[0x0000] = 0xFB;  /* EI */
    cpu->pc = 0x0000;
    (void)step();
    ASSERT_EQ(cpu->iff1, 1);
    ASSERT_EQ(cpu->iff2, 1);
    ASSERT_EQ(iff_track.last_reason, Z80_IFF_REASON_EI);
    ASSERT_EQ(iff_track.last_iff1, 1);
    ASSERT_EQ(iff_track.last_iff2, 1);
    ASSERT_EQ(iff_track.fire_count, 1);
    ASSERT_EQ(iff_track.ei_count, 1);
    TEST_END();
}

/**
 * @brief Přijetí INT v IM 1 volá callback s reason INT_ACK; intack_cb dál volán.
 *
 * Handler na 0x0038 je HALT, takže po kroku zůstane PC na adrese HALT.
 */
static void test_im1_int_ack_fires_int_ack(void) {
    TEST_BEGIN("iff_change: INT IM 1 acceptance fires reason INT_ACK");
    iff_setup();
    test_ram[0x0038] = 0x76;  /* HALT v handleru */
    iff_prepare_int(0x56);    /* IM 1 */
    int prev_fire = iff_track.fire_count;
    int prev_intack = iff_track.intack_count;
    z80_int(cpu);
    (void)step();
    ASSERT_EQ(cpu->iff1, 0);
    ASSERT_EQ(cpu->iff2, 0);
    ASSERT_EQ(iff_track.last_reason, Z80_IFF_REASON_INT_ACK);
    ASSERT_EQ(iff_track.last_iff1, 0);
    ASSERT_EQ(iff_track.last_iff2, 0);
    ASSERT_EQ(iff_track.fire_count, prev_fire + 1);
    ASSERT_EQ(iff_track.intack_count, prev_intack + 1);
    ASSERT_EQ(cpu->pc, 0x0038);  /* PC drží adresu HALT */
    ASSERT_TRUE(z80_is_halted(cpu));
    TEST_END();
}

/** @brief Přijetí INT v IM 2 (I = 0x40, vektor 0x80) volá callback s reason INT_ACK. */
static void test_im2_int_ack_fires_int_ack(void) {
    TEST_BEGIN("iff_change: INT IM 2 acceptance fires reason INT_ACK");
    iff_setup();
    test_ram[0x4080] = 0x34;  /* vektor: handler na 0x1234 */
    test_ram[0x4081] = 0x12;
    test_ram[0x1234] = 0x76;  /* HALT v handleru */
    cpu->i = 0x40;
    iff_prepare_int(0x5E);    /* IM 2 */
    int prev_fire = iff_track.fire_count;
    z80_irq(cpu, 0x80);
    (void)step();
    ASSERT_EQ(cpu->iff1, 0);
    ASSERT_EQ(cpu->iff2, 0);
    ASSERT_EQ(iff_track.last_reason, Z80_IFF_REASON_INT_ACK);
    ASSERT_EQ(iff_track.fire_count, prev_fire + 1);
    ASSERT_EQ(cpu->pc, 0x1234);  /* PC drží adresu HALT */
    ASSERT_TRUE(z80_is_halted(cpu));
    TEST_END();
}

/**
 * @brief Přijetí INT v IM 0 (RST 38h) volá callback s reason INT_ACK.
 *
 * Z80 při přijetí INT nuluje IFF1 i IFF2 vždy, nezávisle na IM; IM určuje
 * jen to, co se provede po přijetí.
 */
static void test_im0_int_ack_fires_int_ack(void) {
    TEST_BEGIN("iff_change: INT IM 0 acceptance fires reason INT_ACK");
    iff_setup();
    iff_prepare_int(0x46);  /* IM 0 */
    int prev_fire = iff_track.fire_count;
    z80_irq(cpu, 0xFF);     /* RST 38h */
    (void)step();
    ASSERT_EQ(cpu->iff1, 0);
    ASSERT_EQ(cpu->iff2, 0);
    ASSERT_EQ(iff_track.last_reason, Z80_IFF_REASON_INT_ACK);
    ASSERT_EQ(iff_track.fire_count, prev_fire + 1);
    TEST_END();
}

/**
 * @brief Přijetí NMI volá callback s reason NMI_ACK; IFF2 zachová původní IFF1.
 *
 * z80_nmi() sám volá jen nmi_cb (assertion), iff_change až při přijetí.
 */
static void test_nmi_ack_fires_nmi_ack(void) {
    TEST_BEGIN("iff_change: NMI acceptance fires reason NMI_ACK");
    iff_setup();
    test_ram[0x0000] = 0xFB;  /* EI - IFF1 = IFF2 = 1 */
    test_ram[0x0001] = 0x00;  /* NOP */
    test_ram[0x0066] = 0x76;  /* HALT v NMI handleru */
    cpu->pc = 0x0000;
    cpu->sp = 0xFFFE;
    (void)step();  /* EI */
    (void)step();  /* NOP */
    int prev_fire = iff_track.fire_count;
    int prev_nmi_assert = iff_track.nmi_assert_count;
    z80_nmi(cpu);
    ASSERT_EQ(iff_track.nmi_assert_count, prev_nmi_assert + 1);
    ASSERT_EQ(iff_track.fire_count, prev_fire);  /* zatím bez iff_change */
    (void)step();  /* přijetí NMI + HALT */
    ASSERT_EQ(cpu->iff1, 0);
    ASSERT_EQ(cpu->iff2, 1);  /* kopie původního IFF1 */
    ASSERT_EQ(iff_track.last_reason, Z80_IFF_REASON_NMI_ACK);
    ASSERT_EQ(iff_track.last_iff1, 0);
    ASSERT_EQ(iff_track.last_iff2, 1);
    ASSERT_EQ(iff_track.fire_count, prev_fire + 1);
    ASSERT_EQ(cpu->pc, 0x0066);  /* PC drží adresu HALT */
    ASSERT_TRUE(z80_is_halted(cpu));
    TEST_END();
}

/**
 * @brief RETN v NMI handleru volá callback s reason RETN, IFF1 <- IFF2.
 *
 * Krok po z80_nmi() provede přijetí NMI i RETN: dvě volání (NMI_ACK + RETN).
 */
static void test_retn_fires_retn(void) {
    TEST_BEGIN("iff_change: RETN fires reason RETN");
    iff_setup();
    test_ram[0x0000] = 0xFB;  /* EI */
    test_ram[0x0001] = 0x00;  /* NOP */
    test_ram[0x0066] = 0xED;  /* RETN */
    test_ram[0x0067] = 0x45;
    cpu->pc = 0x0000;
    cpu->sp = 0xFFFE;
    (void)step();  /* EI */
    (void)step();  /* NOP */
    int prev_fire = iff_track.fire_count;
    z80_nmi(cpu);
    (void)step();  /* přijetí NMI + RETN */
    ASSERT_EQ(cpu->iff1, 1);  /* IFF1 <- IFF2 (= stav před NMI) */
    ASSERT_EQ(cpu->iff2, 1);
    ASSERT_EQ(iff_track.last_reason, Z80_IFF_REASON_RETN);
    ASSERT_EQ(iff_track.fire_count, prev_fire + 2);  /* NMI_ACK + RETN */
    TEST_END();
}

/**
 * @brief RETI v INT handleru volá callback s reason RETI, IFF1 <- IFF2; reti_cb dál volán.
 *
 * Krok po z80_int() provede přijetí INT i RETI: dvě volání (INT_ACK + RETI).
 */
static void test_reti_fires_reti(void) {
    TEST_BEGIN("iff_change: RETI fires reason RETI");
    iff_setup();
    test_ram[0x0038] = 0xED;  /* RETI */
    test_ram[0x0039] = 0x4D;
    iff_prepare_int(0x56);    /* IM 1 */
    int prev_fire = iff_track.fire_count;
    int prev_reti = iff_track.reti_count;
    z80_int(cpu);
    (void)step();  /* přijetí INT + RETI */
    ASSERT_EQ(cpu->iff1, 0);
    ASSERT_EQ(cpu->iff2, 0);
    ASSERT_EQ(iff_track.last_reason, Z80_IFF_REASON_RETI);
    ASSERT_EQ(iff_track.fire_count, prev_fire + 2);  /* INT_ACK + RETI */
    ASSERT_EQ(iff_track.reti_count, prev_reti + 1);
    TEST_END();
}

/** @brief z80_reset volá callback s reason RESET; callback i user_data reset přežijí. */
static void test_reset_fires_reset(void) {
    TEST_BEGIN("iff_change: z80_reset fires reason RESET");
    iff_setup();
    int prev_fire = iff_track.fire_count;
    z80_reset(cpu);
    ASSERT_EQ(cpu->iff1, 0);
    ASSERT_EQ(cpu->iff2, 0);
    ASSERT_EQ(iff_track.last_reason, Z80_IFF_REASON_RESET);
    ASSERT_EQ(iff_track.fire_count, prev_fire + 1);
    ASSERT_TRUE(iff_track.last_user_data == IFF_USER_DATA);
    TEST_END();
}

/** @brief Bez callbacku (NULL) instrukce proběhne normálně a dílčí callbacky se volají. */
static void test_null_cb_no_crash(void) {
    TEST_BEGIN("iff_change: NULL callback does not crash");
    iff_setup();
    z80_set_iff_change(cpu, NULL, NULL);
    test_ram[0x0000] = 0xFB;  /* EI */
    test_ram[0x0001] = 0xF3;  /* DI */
    cpu->pc = 0x0000;
    (void)step();  /* EI */
    (void)step();  /* DI */
    ASSERT_EQ(iff_track.fire_count, 0);
    ASSERT_EQ(iff_track.ei_count, 1);
    ASSERT_EQ(iff_track.di_count, 1);
    ASSERT_EQ(cpu->iff1, 0);
    ASSERT_EQ(cpu->iff2, 0);
    TEST_END();
}

/** @brief ei/intack/reti callbacky se volají souběžně s iff_change beze změny. */
static void test_baseline_callbacks_unchanged(void) {
    TEST_BEGIN("iff_change: ei/intack/reti callbacks still fire");
    iff_setup();
    test_ram[0x0038] = 0xED;  /* RETI */
    test_ram[0x0039] = 0x4D;
    iff_prepare_int(0x56);    /* IM 1 */
    z80_int(cpu);
    (void)step();  /* přijetí INT (intack_cb) + RETI (reti_cb) */
    ASSERT_EQ(iff_track.ei_count, 1);
    ASSERT_EQ(iff_track.intack_count, 1);
    ASSERT_EQ(iff_track.reti_count, 1);
    ASSERT_EQ(iff_track.fire_count, 3);  /* EI + INT_ACK + RETI */
    TEST_END();
}

/* ========== Hlavní runner ========== */

/**
 * @brief Spustí všechny testy callbacku iff_change.
 */
void test_iff_change_all(void) {
    test_di_fires_reason_di();
    test_ei_fires_reason_ei();
    test_im1_int_ack_fires_int_ack();
    test_im2_int_ack_fires_int_ack();
    test_im0_int_ack_fires_int_ack();
    test_nmi_ack_fires_nmi_ack();
    test_retn_fires_retn();
    test_reti_fires_reti();
    test_reset_fires_reset();
    test_null_cb_no_crash();
    test_baseline_callbacks_unchanged();
}
