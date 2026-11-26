/*
 * test_c.c -- kiem thu phan C tren PC (gcc), khong can mach.
 * Build & chay:  make -C test/pc    (hoac: sh test/pc/run.sh)
 */
#include <stdio.h>
#include <string.h>
#include "fake_hw.h"
#include "fake_ab.h"
#include "scheduler.h"
#include "global.h"
#include "lcd.h"
#include "dht20.h"
#include "display.h"
#include "fsm_tuning.h"

static int g_pass = 0, g_fail = 0;

#define CHECK(cond, ...) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } \
} while (0)

static void row(int r, char out[17]) { fake_lcd_read_row(r, out); }

static void expect_lcd(const char *l1, const char *l2, int line)
{
    char want[2][17], got[2][17];
    snprintf(want[0], 17, "%-16s", l1);
    snprintf(want[1], 17, "%-16s", l2);
    row(0, got[0]);
    row(1, got[1]);
    if (strcmp(want[0], got[0]) == 0 && strcmp(want[1], got[1]) == 0) { g_pass++; return; }
    g_fail++;
    printf("  FAIL line %d:\n    want [%s] [%s]\n    got  [%s] [%s]\n", line, want[0], want[1], got[0], got[1]);
}
#define EXPECT_LCD(a, b) expect_lcd(a, b, __LINE__)

/* main.c (A) gia: dung bang task de xuat o muc 2.5 + task lcd_flush (C de xuat them) */
static uint32_t g_display_task = 0;
static void boot(void)
{
    fake_hw_reset();
    g_fake_ms = 0;
    g_mode = MODE_INIT;
    g_time_red = 5; g_time_green = 3; g_time_amber = 2;
    fake_l1 = LIGHT_RED; fake_l2 = LIGHT_RED; fake_fault = 0;
    fake_dht_present = 1; fake_dht_corrupt_crc = 0; fake_dht_measure_ms = 80;
    fake_dht_set(28.5, 65.2);
    SCH_Init();
    lcd_init();
    dht20_init();
    display_init();
    lcd_flush_all();
    g_display_task = SCH_Add_Task(display_update, 3, 25);
    SCH_Add_Task(dht20_trigger, 100, 200);
    SCH_Add_Task(lcd_flush, 5, 1);
    fake_i2c_max_tx_bytes = 0;
}

static void test_lcd_driver(void)
{
    printf("[1] LCD driver (PCF8574 4-bit, frame buffer, flush co gioi han)\n");
    boot();
    CHECK(lcd_get_address() == 0x27, "dia chi LCD = 0x%02X", lcd_get_address());
    CHECK(fake_lcd_display_on, "LCD chua bat display");
    EXPECT_LCD("STARTING...", "T:--.-C H:--.-%");   /* chua do lan nao */
    SCH_Delete_Task(g_display_task);                 /* test driver tho, khong de display ve de */

    lcd_print_line(0, "0123456789ABCDEF-qua-dai");
    lcd_print_line(1, "ab");
    fake_tick(10);          /* lcd_flush bat dau tu tick 5, can 4 lan cho 2 dong */
    EXPECT_LCD("0123456789ABCDEF", "ab");
    CHECK(fake_i2c_max_tx_bytes <= 36, "1 giao dich I2C = %u byte (> 36)", fake_i2c_max_tx_bytes);
    printf("    giao dich lon nhat: %u byte ~ %.1f ms o 100 kHz (1 tick = 10 ms)\n",
           fake_i2c_max_tx_bytes, (fake_i2c_max_tx_bytes + 1) * 9 / 100.0);

    /* chi doi 2 ky tu -> chi gui 1 lenh con tro + 2 ky tu = 12 byte */
    lcd_print_line(0, "0123456789ABCDXY");
    fake_i2c_total_bytes = 0;
    lcd_flush();
    CHECK(fake_i2c_total_bytes == 12, "doi 2 ky tu ton %u byte, mong doi 12", fake_i2c_total_bytes);
    fake_i2c_total_bytes = 0;
    lcd_flush();
    CHECK(fake_i2c_total_bytes == 0, "khong doi gi van gui %u byte", fake_i2c_total_bytes);
    CHECK(lcd_is_synced(), "chua dong bo");
}

static void test_dht20(void)
{
    int16_t t; uint16_t h;
    printf("[2] DHT20 non-blocking (trigger -> one-shot 80 ms -> doc, busy + CRC)\n");

    /* cong thuc + CRC voi vector tinh tay */
    uint8_t v[7] = { 0x1C, 0x80, 0x00, 0x06, 0x66, 0x66, 0 };   /* RH = 50.0 %, T = 30.0 C */
    v[6] = dht20_crc8(v, 6);
    dht20_convert(v, &t, &h);
    CHECK(h == 500 && t == 300, "convert: RH=%u T=%d", h, t);
    uint8_t c9[] = { 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39 };
    CHECK(dht20_crc8(c9, 9) == 0xF7, "CRC-8/NRSC-5 check value = 0x%02X (mong doi 0xF7)", dht20_crc8(c9, 9));

    boot();
    g_mode = MODE_AUTO;
    CHECK(!dht20_get(&t, &h), "chua do ma da co so lieu");
    fake_tick(100);                              /* t = 1.00 s: trigger */
    CHECK(fake_dht_trigger_count == 1, "trigger %d lan", fake_dht_trigger_count);
    fake_tick(7);
    CHECK(!dht20_get(&t, &h), "doc truoc 80 ms");
    fake_tick(1);                                /* +80 ms: one-shot doc */
    CHECK(dht20_get(&t, &h) && t == 285 && h == 652, "doc: T=%d RH=%u", t, h);
    CHECK(dht20_last_status() == DHT20_ST_OK, "status %d", dht20_last_status());
    fake_tick(30);
    char r1[17]; row(1, r1);
    CHECK(strncmp(r1, "T:28.5C H:65.2%", 15) == 0, "LCD dong 2 = [%s]", r1);

    /* CRC sai: bo so lieu, giu gia tri cu; 3 chu ky lien tiep sai => "--" */
    fake_dht_set(31.0, 70.0);
    fake_dht_corrupt_crc = 1;
    fake_tick(200);
    CHECK(dht20_last_status() == DHT20_ST_ERR_CRC, "status %d", dht20_last_status());
    CHECK(dht20_get(&t, &h) && t == 285, "CRC sai nhung gia tri cu bi doi: %d", t);
    fake_tick(400);
    CHECK(!dht20_get(&t, &h), "sau 3 lan loi van bao hop le");
    fake_tick(30);
    row(1, r1);
    CHECK(strncmp(r1, "T:--.-C H:--.-%", 15) == 0, "LCD khi mat cam bien = [%s]", r1);
    fake_dht_corrupt_crc = 0;
    fake_tick(200);
    CHECK(dht20_get(&t, &h) && t == 310 && h == 700, "phuc hoi: T=%d RH=%u", t, h);

    /* cam bien cham (95 ms): bit busy => doc lai sau 1 tick */
    fake_dht_measure_ms = 95;
    fake_dht_set(-5.2, 40.0);
    fake_tick(200);
    CHECK(dht20_get(&t, &h) && t == -52 && h == 400, "busy retry: T=%d RH=%u st=%d", t, h, dht20_last_status());
    fake_tick(30);
    row(1, r1);
    CHECK(strncmp(r1, "T:-5.2C H:40.0%", 15) == 0, "nhiet do am = [%s]", r1);

    /* mat cam bien hoan toan: khong treo, task khong ro ri */
    fake_dht_present = 0;
    int before = fake_sch_count();
    fake_tick(2000);
    CHECK(fake_sch_count() == before, "so task %d -> %d", before, fake_sch_count());
    CHECK(fake_recover_calls > 0, "khong goi khoi phuc bus khi I2C loi lien tiep");
    fake_dht_present = 1;
}

static void test_countdown(void)
{
    printf("[3] Dem nguoc (cong thuc muc 2.3)\n");
    CHECK(display_countdown_s(LIGHT_GREEN, LIGHT_RED, 2300, 2) == 3, "Xanh 2.3 s");
    CHECK(display_countdown_s(LIGHT_RED, LIGHT_GREEN, 2300, 2) == 5, "Do khi kia Xanh = pha + T_V");
    CHECK(display_countdown_s(LIGHT_RED, LIGHT_AMBER, 1500, 2) == 2, "Do khi kia Vang = pha");
    CHECK(display_countdown_s(LIGHT_AMBER, LIGHT_RED, 1, 2) == 1, "lam tron len");
    CHECK(display_countdown_s(LIGHT_GREEN, LIGHT_RED, 0, 2) == 0, "het gio");
    CHECK(display_countdown_s(LIGHT_RED, LIGHT_GREEN, 98000, 9) == 99, "kep 99");
}

static void test_screens(void)
{
    printf("[4] Man hinh AUTO / MANUAL / trang 2 / SAFETY\n");
    boot();
    fake_tick(110);                       /* co so lieu DHT20 */
    g_mode = MODE_AUTO;
    fake_l1 = LIGHT_RED; fake_l2 = LIGHT_GREEN; fake_phase_ms = 2300;
    fake_tick(30);
    EXPECT_LCD("AUTO 1:R05 2:G03", "T:28.5C H:65.2%");
    fake_l1 = LIGHT_RED; fake_l2 = LIGHT_AMBER; fake_phase_ms = 1200;
    fake_tick(30);
    EXPECT_LCD("AUTO 1:R02 2:Y02", "T:28.5C H:65.2%");

    display_next_page();
    fake_tick(5);
    EXPECT_LCD("AUTO 1:R02 2:Y02", "SET R05 G03 Y02");
    display_next_page();

    g_mode = MODE_MANUAL;
    fake_l1 = LIGHT_GREEN; fake_l2 = LIGHT_RED;
    fake_tick(30);
    EXPECT_LCD("MAN G1 - R2", "T:28.5C H:65.2%");

    fake_fault = 1; fake_l1 = LIGHT_RED; fake_l2 = LIGHT_RED;
    fake_tick(30);
    EXPECT_LCD("SAFETY ERR", "T:28.5C H:65.2%");
    fake_fault = 0;
}

static void press(button_id_t b) { fake_pressed[b] = 1; fsm_tuning_run(); fake_tick(5); }
static void hold(button_id_t b, int n) { for (int i = 0; i < n; i++) { fake_held[b] = 1; fsm_tuning_run(); fake_tick(20); } }

static void test_tuning(void)
{
    printf("[5] FSM chinh gio + kiem tra hop le\n");
    CHECK(fsm_tuning_validate(5, 3, 2) == TUNE_OK, "5=3+2");
    CHECK(fsm_tuning_validate(6, 3, 2) == TUNE_ERR_SUM, "6!=3+2");
    CHECK(fsm_tuning_validate(3, 3, 0) == TUNE_ERR_MIN, "vang 0");
    CHECK(fsm_tuning_validate(2, 0, 2) == TUNE_ERR_MIN, "xanh 0");
    CHECK(fsm_tuning_validate(99, 97, 2) == TUNE_OK, "bien 99");

    boot();
    fake_tick(110);
    g_mode = MODE_TUNING;                 /* fsm_mode (A): entry action */
    fsm_tuning_enter();
    fake_tick(5);
    EXPECT_LCD("TUNE >RED  : 05s", "R05 G03 Y02");
    CHECK(fsm_tuning_state() == TUNE_RED, "state");

    press(BTN_3);                         /* Do 6 -> sai rang buoc */
    EXPECT_LCD("TUNE >RED  : 06s", "R06 G03 Y02 *");
    press(BTN_USER);
    EXPECT_LCD("TUNE >RED  : 06s", "ERR: R!=G+Y");
    CHECK(g_time_red == 5 && g_time_green == 3 && g_time_amber == 2, "luu sai ma g_time bi doi");
    fake_tick(200);                       /* sau 2 s thong bao tu xoa */
    EXPECT_LCD("TUNE >RED  : 06s", "R06 G03 Y02 *");

    press(BTN_2);                         /* chon Xanh, tang len 4 */
    press(BTN_3);
    EXPECT_LCD("TUNE >GREEN: 04s", "R06 G04 Y02 *");
    press(BTN_USER);
    EXPECT_LCD("TUNE >GREEN: 04s", "SAVED");
    CHECK(g_time_red == 6 && g_time_green == 4 && g_time_amber == 2, "luu dung: %u %u %u",
          g_time_red, g_time_green, g_time_amber);
    fake_tick(200);
    EXPECT_LCD("TUNE >GREEN: 04s", "R06 G04 Y02");

    press(BTN_2);                         /* Vang, giam ve 0 va thu luu */
    hold(BTN_4, 5);
    EXPECT_LCD("TUNE >AMBER: 00s", "R06 G04 Y00 *");
    press(BTN_USER);
    EXPECT_LCD("TUNE >AMBER: 00s", "ERR: G,Y >= 1s");
    CHECK(g_time_amber == 2, "vang 0 da bi luu");

    /* thong bao moi de thong bao cu: one-shot cu phai bi xoa */
    fake_tick(100);
    press(BTN_USER);                      /* ERR lan 2 sau 1 s */
    fake_tick(150);                       /* 1.5 s sau: van con */
    char r1[17]; row(1, r1);
    CHECK(strncmp(r1, "ERR: G,Y >= 1s", 14) == 0, "thong bao bi xoa som: [%s]", r1);
    fake_tick(60);
    row(1, r1);
    CHECK(strncmp(r1, "R06 G04 Y00 *", 13) == 0, "thong bao khong tu xoa: [%s]", r1);

    press(BTN_2);                         /* vong ve Do, giu tang den kep 99 */
    CHECK(fsm_tuning_state() == TUNE_RED, "B2 tu Vang khong ve Do");
    hold(BTN_3, 120);
    EXPECT_LCD("TUNE >RED  : 99s", "R99 G04 Y00 *");

    /* ra TUNING khi chua luu: bo ban tam, gio dang dung giu nguyen */
    fsm_tuning_exit();
    g_mode = MODE_AUTO;
    CHECK(g_time_red == 6 && g_time_green == 4 && g_time_amber == 2, "thoat ma gio bi doi");
    g_mode = MODE_TUNING;
    fsm_tuning_enter();
    fake_tick(5);
    EXPECT_LCD("TUNE >RED  : 06s", "R06 G04 Y02");
}

static void test_no_lcd(void)
{
    printf("[6] Khong co LCD: khong treo\n");
    fake_lcd_present = 0;
    boot();
    CHECK(lcd_get_address() == 0, "tim thay LCD ma");
    fake_tick(500);
    CHECK(1, "ok");
    fake_lcd_present = 1;
}

static void test_long_run(void)
{
    printf("[7] Chay 30 phut mo phong: so task on dinh\n");
    boot();
    g_mode = MODE_AUTO;
    int n0 = -1, maxn = 0;
    for (int i = 0; i < 180000; i++) {
        fake_tick(1);
        if (i == 1000) n0 = fake_sch_count();
        if (fake_sch_count() > maxn) maxn = fake_sch_count();
        if (i % 777 == 0) display_show_message("SAVED", 2000);
    }
    CHECK(fake_sch_count() <= n0 + 2, "task ro ri: %d -> %d", n0, fake_sch_count());
    CHECK(maxn <= 6, "dinh %d task", maxn);
    printf("    so task: on dinh %d, toi da %d (SCH_MAX_TASKS = 16)\n", n0, maxn);
}

int main(void)
{
    test_lcd_driver();
    test_dht20();
    test_countdown();
    test_screens();
    test_tuning();
    test_no_lcd();
    test_long_run();
    printf("\nKET QUA: %d PASS, %d FAIL\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
