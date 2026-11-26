#ifndef FAKE_AB_H_
#define FAKE_AB_H_
#include <stdint.h>
#include "button.h"
#include "traffic_light.h"
int  fake_sch_count(void);
void fake_tick(int n);
extern uint32_t fake_phase_ms;
extern uint8_t fake_pressed[NUM_BUTTONS], fake_held[NUM_BUTTONS];
extern light_t fake_l1, fake_l2;
extern int fake_fault;
#endif
