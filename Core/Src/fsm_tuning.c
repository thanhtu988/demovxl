/*
 * fsm_tuning.c  (C) -- may trang thai che do chinh gio
 *
 * KHONG co lenh HAL trong file nay: doc nut qua button.h (B), hien thi qua display.h,
 * thoi gian dang ap dung nam trong global.h (g_time_red/green/amber).
 *
 *   +-----------+   B2   +------------+   B2   +------------+
 *   | TUNE_RED  | -----> | TUNE_GREEN | -----> | TUNE_AMBER |
 *   +-----------+        +------------+        +------------+
 *         ^                                          | B2
 *         +------------------------------------------+
 *   Trong moi trang thai: B3/B4 = +/-1 s gia tri dang chon (ban tam),
 *                         USER  = Luu (kiem tra hop le), o lai trang thai cu.
 */
#include "fsm_tuning.h"
#include "button.h"
#include "display.h"
#include "global.h"

static tune_state_t s_state = TUNE_RED;
static uint8_t      s_tmp[3];          /* ban sao tam: [0] Do, [1] Xanh, [2] Vang */
static uint8_t      s_unsaved = 0;     /* ban tam khac gia tri dang ap dung       */

/* ---------------- ham thuan ---------------- */

tune_result_t fsm_tuning_validate(uint8_t red, uint8_t green, uint8_t amber)
{
    if (green < 1U || amber < 1U) {
        return TUNE_ERR_MIN;
    }
    if (red > TUNE_VALUE_MAX) {
        return TUNE_ERR_MAX;
    }
    if ((uint16_t)red != (uint16_t)green + (uint16_t)amber) {
        return TUNE_ERR_SUM;
    }
    return TUNE_OK;
}

/* ---------------- noi bo ---------------- */

static void refresh_display(void)
{
    s_unsaved = (uint8_t)(s_tmp[TUNE_RED]   != g_time_red   ||
                          s_tmp[TUNE_GREEN] != g_time_green ||
                          s_tmp[TUNE_AMBER] != g_time_amber);
    display_set_tuning((display_sel_t)s_state, s_tmp[TUNE_RED], s_tmp[TUNE_GREEN],
                       s_tmp[TUNE_AMBER], s_unsaved);
}

/* cong/tru co chan 2 dau (khong quay vong: 99 + 1 van la 99, 0 - 1 van la 0) */
static void adjust(uint8_t idx, int8_t delta)
{
    int16_t v = (int16_t)s_tmp[idx] + delta;
    if (v < (int16_t)TUNE_VALUE_MIN) {
        v = (int16_t)TUNE_VALUE_MIN;
    }
    if (v > (int16_t)TUNE_VALUE_MAX) {
        v = (int16_t)TUNE_VALUE_MAX;
    }
    s_tmp[idx] = (uint8_t)v;
}

static void save(void)
{
    switch (fsm_tuning_validate(s_tmp[TUNE_RED], s_tmp[TUNE_GREEN], s_tmp[TUNE_AMBER])) {
    case TUNE_OK:
        /* "Luu" = ghi vao gio dang ap dung. Che do TUNING khong chay chu trinh den,
         * nen gia tri moi chi co hieu luc khi fsm_mode vao lai AUTO (reset timer pha). */
        g_time_red   = s_tmp[TUNE_RED];
        g_time_green = s_tmp[TUNE_GREEN];
        g_time_amber = s_tmp[TUNE_AMBER];
        refresh_display();
        display_show_message("SAVED", TUNE_MSG_MS);
        break;
    case TUNE_ERR_MIN:
        display_show_message("ERR: G,Y >= 1s", TUNE_MSG_MS);
        break;
    case TUNE_ERR_MAX:
        display_show_message("ERR: R > 99s", TUNE_MSG_MS);
        break;
    case TUNE_ERR_SUM:
    default:
        display_show_message("ERR: R!=G+Y", TUNE_MSG_MS);
        break;
    }
    /* loi: g_time_* KHONG doi, ban tam van giu de nguoi dung sua tiep */
}

/* xu ly chung cho moi trang thai: B3/B4 tren gia tri dang chon, USER = Luu */
static void handle_edit(uint8_t idx)
{
    uint8_t changed = 0;

    if (button_pressed(BTN_3) || button_held(BTN_3)) {
        adjust(idx, +1);
        changed = 1;
    }
    if (button_pressed(BTN_4) || button_held(BTN_4)) {
        adjust(idx, -1);
        changed = 1;
    }
    if (changed) {
        refresh_display();
    }
    if (button_pressed(BTN_USER)) {
        save();
    }
}

/* ---------------- API ---------------- */

void fsm_tuning_enter(void)
{
    s_tmp[TUNE_RED]   = g_time_red;     /* sao chep gio dang dung ra ban tam */
    s_tmp[TUNE_GREEN] = g_time_green;
    s_tmp[TUNE_AMBER] = g_time_amber;
    s_state = TUNE_RED;
    display_clear_message();
    refresh_display();
}

void fsm_tuning_run(void)
{
    switch (s_state) {
    case TUNE_RED:
        handle_edit(TUNE_RED);
        if (button_pressed(BTN_2)) {
            s_state = TUNE_GREEN;
            refresh_display();
        }
        break;

    case TUNE_GREEN:
        handle_edit(TUNE_GREEN);
        if (button_pressed(BTN_2)) {
            s_state = TUNE_AMBER;
            refresh_display();
        }
        break;

    case TUNE_AMBER:
        handle_edit(TUNE_AMBER);
        if (button_pressed(BTN_2)) {
            s_state = TUNE_RED;
            refresh_display();
        }
        break;

    default:                 /* trang thai la (nhieu bo nho) => ve trang thai an toan */
        s_state = TUNE_RED;
        refresh_display();
        break;
    }
}

void fsm_tuning_exit(void)
{
    /* ban tam bi bo; lan vao TUNING sau se sao chep lai tu g_time_* */
    s_unsaved = 0;
    display_clear_message();
}

tune_state_t fsm_tuning_state(void)
{
    return s_state;
}
