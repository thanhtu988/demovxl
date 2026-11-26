/* STUB theo hop dong API muc 2.6 (file that cua A). Chi dung de build/test phan C. */
#ifndef SCHEDULER_H_
#define SCHEDULER_H_
#include <stdint.h>
#define SCH_MAX_TASKS 16
#define NO_TASK_ID    0
void     SCH_Init(void);
uint32_t SCH_Add_Task(void (*pFunc)(void), uint32_t delay, uint32_t period);
uint8_t  SCH_Delete_Task(uint32_t taskID);
void     SCH_Update(void);
void     SCH_Dispatch_Tasks(void);
#endif
