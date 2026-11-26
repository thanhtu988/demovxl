/* Ban gia cua cac module A, B cho test PC (scheduler don gian, nut/timer/den gia). */
#include <string.h>
#include "fake_hw.h"
#include "scheduler.h"
#include "global.h"
#include "software_timer.h"
#include "button.h"
#include "traffic_light.h"
#include "fake_ab.h"

/* ---- global (A) ---- */
app_mode_t g_mode = MODE_INIT;
uint8_t g_time_red = 5, g_time_green = 3, g_time_amber = 2;

/* ---- scheduler gia: mang + delay tuyet doi, du cho test logic C ---- */
typedef struct { void (*f)(void); uint32_t delay, period, id; uint8_t used, run; } t_t;
static t_t T[SCH_MAX_TASKS];
static uint32_t next_id = 1;
int fake_sch_count(void) { int n = 0; for (int i = 0; i < SCH_MAX_TASKS; i++) n += T[i].used; return n; }
void SCH_Init(void) { memset(T, 0, sizeof(T)); next_id = 1; }
uint32_t SCH_Add_Task(void (*f)(void), uint32_t d, uint32_t p)
{
    for (int i = 0; i < SCH_MAX_TASKS; i++) if (!T[i].used) {
        T[i] = (t_t){ f, d, p, next_id++, 1, 0 };
        return T[i].id;
    }
    return NO_TASK_ID;
}
uint8_t SCH_Delete_Task(uint32_t id)
{
    for (int i = 0; i < SCH_MAX_TASKS; i++) if (T[i].used && T[i].id == id) { T[i].used = 0; return 1; }
    return 0;
}
void SCH_Update(void)
{
    for (int i = 0; i < SCH_MAX_TASKS; i++) if (T[i].used) {
        if (T[i].delay > 0) T[i].delay--;
        if (T[i].delay == 0) T[i].run = 1;
    }
}
void SCH_Dispatch_Tasks(void)
{
    for (int i = 0; i < SCH_MAX_TASKS; i++) if (T[i].used && T[i].run) {
        void (*f)(void) = T[i].f;
        T[i].run = 0;
        if (T[i].period) T[i].delay = T[i].period; else T[i].used = 0;
        f();
    }
}
void fake_tick(int n)
{
    while (n--) { g_fake_ms += 10; SCH_Update(); SCH_Dispatch_Tasks(); }
}

/* ---- software timer (B) ---- */
uint32_t fake_phase_ms = 0;
void timer_set(uint8_t id, uint32_t ms) { if (id == TMR_PHASE) fake_phase_ms = ms; }
uint8_t timer_expired(uint8_t id) { (void)id; return 0; }
uint32_t timer_remaining_ms(uint8_t id) { return id == TMR_PHASE ? fake_phase_ms : 0; }
void timer_run(void) {}

/* ---- button (B): test dat co, doc 1 lan la mat ---- */
uint8_t fake_pressed[NUM_BUTTONS], fake_held[NUM_BUTTONS];
void button_scan(void) {}
uint8_t button_pressed(button_id_t b) { uint8_t v = fake_pressed[b]; fake_pressed[b] = 0; return v; }
uint8_t button_held(button_id_t b) { uint8_t v = fake_held[b]; fake_held[b] = 0; return v; }
void button_clear_all(void) { memset(fake_pressed, 0, sizeof(fake_pressed)); memset(fake_held, 0, sizeof(fake_held)); }

/* ---- traffic light (B) ---- */
light_t fake_l1 = LIGHT_RED, fake_l2 = LIGHT_RED;
int fake_fault = 0;
uint8_t traffic_set_pair(light_t a, light_t b) { fake_l1 = a; fake_l2 = b; return 1; }
light_t traffic_get(uint8_t r) { return r == 1 ? fake_l1 : fake_l2; }
uint8_t traffic_is_fault(void) { return (uint8_t)fake_fault; }   /* ban "that" cua B */
