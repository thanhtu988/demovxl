/* STUB theo hop dong API muc 2.6 (file that cua A). */
#ifndef GLOBAL_H_
#define GLOBAL_H_
#include <stdint.h>
typedef enum { MODE_INIT, MODE_AUTO, MODE_MANUAL, MODE_TUNING } app_mode_t;
extern app_mode_t g_mode;
extern uint8_t    g_time_red, g_time_green, g_time_amber;
#endif
