/*
 * display.c  (C) -- ve noi dung LCD theo che do hien tai (g_mode)
 *
 * Khong co lenh HAL o day. Thu tu uu tien dong 2:
 *   thong bao tam (SAVED / ERR, tu xoa bang one-shot)  >  noi dung theo che do/trang.
 * Dong 1 luon la trang thai den => khi demo luon nhin duoc den 2 tuyen ra sao.
 */
#include <stdio.h>
#include <string.h>
#include "display.h"
#include "lcd.h"
#include "dht20.h"
#include "global.h"
#include "scheduler.h"
#include "software_timer.h"
#include "traffic_light.h"

/* B can them ham nay vao traffic_light.c (tra 1 khi lop bao ve 2 da ep ALL RED).
 * Ban "weak" duoi day chi de build duoc khi B chua co; ban that cua B se thay the. */
__attribute__((weak)) uint8_t traffic_is_fault(void)
{
    return 0;
}

static char          s_msg[DISPLAY_MSG_MAX + 1U];
static uint8_t       s_msg_active = 0;
static uint32_t      s_msg_task   = NO_TASK_ID;
static uint8_t       s_page       = 0;            /* 0: T/H, 1: cau hinh gio */

static display_sel_t s_tune_sel   = DISPLAY_SEL_RED;
static uint8_t       s_tune_val[3] = { 5, 3, 2 };
static uint8_t       s_tune_unsaved = 0;

/* ---------------- ham thuan ---------------- */

static char light_char(light_t l)
{
    switch (l) {
    case LIGHT_RED:   return 'R';
    case LIGHT_AMBER: return 'Y';
    case LIGHT_GREEN: return 'G';
    default:          return '-';
    }
}

uint8_t display_countdown_s(light_t mine, light_t other,
                            uint32_t phase_remaining_ms, uint8_t time_amber_s)
{
    /* lam tron len: con 6.01 s hien 07, con 0.2 s hien 01 */
    uint32_t s = (phase_remaining_ms + 999UL) / 1000UL;

    /* tuyen dang Do ma tuyen kia dang Xanh: con phai cho them pha Vang cua tuyen kia */
    if (mine == LIGHT_RED && other == LIGHT_GREEN) {
        s += time_amber_s;
    }
    return (uint8_t)(s > 99UL ? 99UL : s);
}

/* "T:28.5C H:65.2%" ; nhiet do am: "T:-5.2C" ; chua co so lieu: "T:--.-C H:--.-%" */
static void format_env(char *out, size_t n)
{
    int16_t  t;
    uint16_t h;
    if (!dht20_get(&t, &h)) {
        snprintf(out, n, "T:--.-C H:--.-%%");
        return;
    }
    const char *sign = (t < 0) ? "-" : "";
    int32_t at = (t < 0) ? -(int32_t)t : (int32_t)t;
    snprintf(out, n, "T:%s%d.%dC H:%u.%u%%",
             sign, (int)(at / 10), (int)(at % 10), (unsigned)(h / 10U), (unsigned)(h % 10U));
}

static void format_config(char *out, size_t n)
{
    snprintf(out, n, "SET R%02u G%02u Y%02u",
             (unsigned)g_time_red, (unsigned)g_time_green, (unsigned)g_time_amber);
}

/* ---------------- ve man hinh ---------------- */

static void render(void)
{
    char l1[24];
    char l2[24];
    light_t a = traffic_get(1);
    light_t b = traffic_get(2);

    l1[0] = '\0';
    l2[0] = '\0';

    switch (g_mode) {
    case MODE_INIT:
        snprintf(l1, sizeof(l1), "STARTING...");
        format_env(l2, sizeof(l2));
        break;

    case MODE_AUTO: {
        uint32_t rem = timer_remaining_ms(TMR_PHASE);
        snprintf(l1, sizeof(l1), "AUTO 1:%c%02u 2:%c%02u",
                 light_char(a), (unsigned)display_countdown_s(a, b, rem, g_time_amber),
                 light_char(b), (unsigned)display_countdown_s(b, a, rem, g_time_amber));
        if (s_page == 0U) {
            format_env(l2, sizeof(l2));
        } else {
            format_config(l2, sizeof(l2));
        }
        break;
    }

    case MODE_MANUAL:
        snprintf(l1, sizeof(l1), "MAN %c1 - %c2", light_char(a), light_char(b));
        if (s_page == 0U) {
            format_env(l2, sizeof(l2));
        } else {
            format_config(l2, sizeof(l2));
        }
        break;

    case MODE_TUNING: {
        static const char *const k_name[3] = { "RED  ", "GREEN", "AMBER" };
        snprintf(l1, sizeof(l1), "TUNE >%s: %02us",
                 k_name[s_tune_sel], (unsigned)s_tune_val[s_tune_sel]);
        snprintf(l2, sizeof(l2), "R%02u G%02u Y%02u%s",
                 (unsigned)s_tune_val[DISPLAY_SEL_RED], (unsigned)s_tune_val[DISPLAY_SEL_GREEN],
                 (unsigned)s_tune_val[DISPLAY_SEL_AMBER], s_tune_unsaved ? " *" : "");
        break;
    }

    default:
        snprintf(l1, sizeof(l1), "MODE ?");
        break;
    }

    if (traffic_is_fault()) {
        snprintf(l1, sizeof(l1), "SAFETY ERR");
    }
    if (s_msg_active) {
        snprintf(l2, sizeof(l2), "%s", s_msg);
    }

    lcd_print_line(0, l1);   /* lcd chi gui I2C phan ky tu thuc su thay doi */
    lcd_print_line(1, l2);
}

/* ---------------- one-shot xoa thong bao ---------------- */

static void msg_clear(void)
{
    s_msg_task   = NO_TASK_ID;   /* one-shot da chay va tu bi xoa khoi danh sach */
    s_msg_active = 0;
    render();
}

/* ---------------- API ---------------- */

void display_init(void)
{
    s_page       = 0;
    s_msg_active = 0;
    s_msg_task   = NO_TASK_ID;
    render();
}

void display_update(void)
{
    render();
}

void display_show_message(const char *msg, uint32_t ms)
{
    /* thong bao moi de thong bao cu: xoa one-shot cu truoc, neu khong no se
     * xoa som thong bao moi */
    if (s_msg_task != NO_TASK_ID) {
        (void)SCH_Delete_Task(s_msg_task);
        s_msg_task = NO_TASK_ID;
    }
    snprintf(s_msg, sizeof(s_msg), "%s", (msg != 0) ? msg : "");
    s_msg_active = 1;

    uint32_t ticks = ms / DISPLAY_TICK_MS;
    if (ticks == 0U) {
        ticks = 1U;
    }
    s_msg_task = SCH_Add_Task(msg_clear, ticks, 0);
    if (s_msg_task == NO_TASK_ID) {
        s_msg_active = 0;        /* khong them duoc task thi khong de thong bao treo mai */
    }
    render();
}

void display_clear_message(void)
{
    if (s_msg_task != NO_TASK_ID) {
        (void)SCH_Delete_Task(s_msg_task);
        s_msg_task = NO_TASK_ID;
    }
    s_msg_active = 0;
    render();
}

void display_next_page(void)
{
    s_page = (uint8_t)((s_page + 1U) % 2U);
    render();
}

void display_set_tuning(display_sel_t sel, uint8_t red, uint8_t green, uint8_t amber,
                        uint8_t unsaved)
{
    s_tune_sel                     = (sel <= DISPLAY_SEL_AMBER) ? sel : DISPLAY_SEL_RED;
    s_tune_val[DISPLAY_SEL_RED]    = red;
    s_tune_val[DISPLAY_SEL_GREEN]  = green;
    s_tune_val[DISPLAY_SEL_AMBER]  = amber;
    s_tune_unsaved                 = unsaved;
    render();
}
