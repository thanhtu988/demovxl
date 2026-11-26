/* STUB theo hop dong API muc 2.6 (file that cua B). */
#ifndef SOFTWARE_TIMER_H_
#define SOFTWARE_TIMER_H_
#include <stdint.h>
enum { TMR_PHASE, TMR_MANUAL_LOCK, TMR_SPARE, NUM_TIMERS };
void     timer_set(uint8_t id, uint32_t ms);
uint8_t  timer_expired(uint8_t id);
uint32_t timer_remaining_ms(uint8_t id);
void     timer_run(void);
#endif
