/* STUB theo hop dong API muc 2.6 (file that cua B). */
#ifndef BUTTON_H_
#define BUTTON_H_
#include <stdint.h>
typedef enum { BTN_MODE, BTN_2, BTN_3, BTN_4, BTN_USER, NUM_BUTTONS } button_id_t;
void    button_scan(void);
uint8_t button_pressed(button_id_t b);
uint8_t button_held(button_id_t b);
void    button_clear_all(void);
#endif
