/*
 * c_lab_demo.c -- doan code DAN TAM vao main.c de qua "cong" lab cua C
 * (I2C scan thay 2 thiet bi, LCD in chuoi, DHT20 cap nhat moi 2 s tren scheduler).
 * KHONG commit vao main.c chinh. Dan dung vao cac vung USER CODE tuong ung.
 */

/* ===== USER CODE BEGIN Includes ===== */
#include <stdio.h>
#include "scheduler.h"
#include "i2c_bus.h"
#include "lcd.h"
#include "dht20.h"
/* ===== USER CODE END Includes ===== */

/* ===== USER CODE BEGIN 0 ===== */
static uint8_t g_found[8];
static uint8_t g_found_n;

static void demo_show(void)            /* task 250 ms */
{
    char l2[17];
    int16_t t; uint16_t h;
    if (dht20_get(&t, &h)) {
        int at = (t < 0) ? -t : t;
        snprintf(l2, sizeof(l2), "T:%s%d.%dC H:%u.%u%%", (t < 0) ? "-" : "", at / 10, at % 10,
                 (unsigned)(h / 10U), (unsigned)(h % 10U));
    } else {
        snprintf(l2, sizeof(l2), "DHT20 st=%d", (int)dht20_last_status());
    }
    lcd_print_line(1, l2);
}
/* ===== USER CODE END 0 ===== */

/* ===== USER CODE BEGIN 2 ===== */
  i2c_bus_recover();
  g_found_n = i2c_bus_scan(g_found, 8);   /* dat breakpoint o day: mong doi 0x27 (hoac 0x3F) va 0x38 */
  lcd_init();
  dht20_init();
  {
      char l1[17];
      snprintf(l1, sizeof(l1), "I2C:%u %02X %02X", g_found_n, g_found[0], g_found[1]);
      lcd_print_line(0, l1);               /* vd "I2C:2 27 38" */
      lcd_print_line(1, "Hello CO3009");
      lcd_flush_all();
  }
  SCH_Init();
  SCH_Add_Task(dht20_trigger, 100, 200);
  SCH_Add_Task(demo_show, 3, 25);
  SCH_Add_Task(lcd_flush, 5, 1);
  HAL_TIM_Base_Start_IT(&htim2);
/* ===== USER CODE END 2 ===== */

/* trong while(1):   SCH_Dispatch_Tasks();
 * trong HAL_TIM_PeriodElapsedCallback:  if (htim->Instance == TIM2) SCH_Update();
 *
 * Kiem tra o lab:
 *   [ ] dong 1 "I2C:2 27 38"  (2 thiet bi)            -> ghi dia chi vao bao cao 3.1
 *   [ ] dong 2 doi tu "Hello CO3009" sang T/H sau ~1.1 s
 *   [ ] ha hoi vao DHT20: do am tang trong 2-4 s
 *   [ ] rut day DHT20: sau ~6 s hien "DHT20 st=2", cam lai tu phuc hoi
 */
