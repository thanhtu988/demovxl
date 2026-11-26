/*
 * display.h  (C) -- giao dien LCD cho 3 che do (xem docs/lcd_screens.md)
 *
 *            Dong 1 (trang thai, luon hien)   Dong 2 (trang 0 / trang 1 / thong bao)
 *   INIT     STARTING...                      T:28.5C H:65.2%
 *   AUTO     AUTO 1:R07 2:G05                 T:28.5C H:65.2%  |  SET R05 G03 Y02
 *   MANUAL   MAN R1 - G2                      T:28.5C H:65.2%  |  SET R05 G03 Y02
 *   TUNING   TUNE >RED  : 05s                 R05 G03 Y02 *
 *   loi      SAFETY ERR                       (giu dong 2)
 *   thong bao (2 s, de len dong 2):  SAVED  /  ERR: R!=G+Y  /  ERR: G,Y >= 1s
 *
 * Module nay KHONG goi HAL: chi dung lcd, dht20, software_timer, traffic_light, global.
 * Moi ham ve deu re (chi ghi bo dem RAM cua lcd), nen duoc goi ngay khi co thay doi
 * thay vi doi den chu ky 250 ms ke tiep.
 */
#ifndef INC_DISPLAY_H_
#define INC_DISPLAY_H_

#include <stdint.h>
#include "traffic_light.h"   /* light_t (B) */

#define DISPLAY_TICK_MS      10U    /* 1 tick scheduler                   */
#define DISPLAY_MSG_MAX      16U

typedef enum {
    DISPLAY_SEL_RED = 0,
    DISPLAY_SEL_GREEN,
    DISPLAY_SEL_AMBER
} display_sel_t;

void display_init(void);                                 /* goi sau lcd_init()        */
void display_update(void);                               /* task 250 ms               */
void display_show_message(const char *msg, uint32_t ms); /* de len dong 2, tu xoa     */
void display_clear_message(void);
void display_next_page(void);                            /* nut USER o AUTO / MANUAL  */
void display_set_tuning(display_sel_t sel, uint8_t red, uint8_t green, uint8_t amber,
                        uint8_t unsaved);                /* fsm_tuning day du lieu    */

/* ham thuan, tach ra de test tren PC: tinh so giay dem nguoc cua 1 tuyen */
uint8_t display_countdown_s(light_t mine, light_t other,
                            uint32_t phase_remaining_ms, uint8_t time_amber_s);

#endif /* INC_DISPLAY_H_ */
