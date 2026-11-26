/* STUB theo hop dong API muc 2.6 (file that cua B). */
#ifndef TRAFFIC_LIGHT_H_
#define TRAFFIC_LIGHT_H_
#include <stdint.h>
typedef enum { LIGHT_OFF, LIGHT_RED, LIGHT_AMBER, LIGHT_GREEN } light_t;
uint8_t traffic_set_pair(light_t r1, light_t r2);
light_t traffic_get(uint8_t route);
#endif
