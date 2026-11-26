# Ghép phần C vào project chung: việc A và B cần làm

Phần C gồm `lcd`, `dht20`, `display`, `fsm_tuning` và thêm `i2c_bus` (khôi phục lỗi I2C kẹt BUSY, rủi ro mục 6 thuộc về C). Code chỉ dựa vào hợp đồng API ở mục 2.6. Các file trong `test/stubs/` là bản giả **chỉ dùng để test**; **không** chép chúng vào `Core/`.

## 1. A – `.ioc` (chỉ A được sửa)

- Bật **I2C1**, Standard mode **100 kHz**. PCF8574 chỉ đảm bảo 100 kHz theo datasheet; nhờ cơ chế flush, 400 kHz không còn cần thiết.
- Chân SCL/SDA: lấy theo khe I2C màu đỏ trên schematic (PB6/PB7 mặc định, hoặc remap PB8/PB9). `i2c_bus_recover()` tự đọc `AFIO->MAPR`, nên chọn cách nào cũng không phải sửa code C.
- Không cần bật ngắt I2C hay DMA: C dùng chế độ polling, mỗi giao dịch ngắn hơn 3,5 ms.

## 2. A – `main.c` (đặt trong các vùng USER CODE)

```c
/* USER CODE BEGIN Includes */
#include "scheduler.h"
#include "i2c_bus.h"
#include "lcd.h"
#include "dht20.h"
#include "display.h"
/* USER CODE END Includes */

  /* USER CODE BEGIN 2 */          /* sau MX_I2C1_Init(), TRUOC SCH_Init/HAL_TIM_Base_Start_IT */
  i2c_bus_recover();               /* xoa BUSY neu MCU reset giua giao dich I2C */
  lcd_init();                      /* blocking ~60 ms, chi goi 1 lan o day     */
  dht20_init();                    /* blocking, cho >= 100 ms tu luc cap nguon */
  display_init();
  lcd_flush_all();                 /* hien "STARTING..." ngay                  */

  SCH_Init();
  /* ... task cua A, B ... */
  SCH_Add_Task(display_update, 3, 25);    /* 250 ms                            */
  SCH_Add_Task(dht20_trigger, 100, 200);  /* 2 s, tu them one-shot doc 80 ms   */
  SCH_Add_Task(lcd_flush,       5,  1);   /* MOI: 10 ms, <= 3.4 ms moi lan     */
  /* USER CODE END 2 */
```

**Thay đổi so với bảng task mục 2.5:** thêm `lcd_flush` (delay 5, period 1). Số task tối đa tăng từ khoảng 10 lên 11, vẫn dưới `SCH_MAX_TASKS = 16`. Delay 5 để không trùng tick với `fsm_mode_run` (delay 1) và `display_update` (delay 3).

## 3. A – `fsm_mode.c` (hành động entry/exit)

| Cung | Gọi hàm của C |
|---|---|
| MANUAL → TUNING (B1) | `button_clear_all(); fsm_tuning_enter();` (sao chép `g_time_*` ra bản tạm) |
| Trong TUNING | `fsm_tuning_run();` trong `fsm_mode_run()` khi `g_mode == MODE_TUNING` |
| TUNING → AUTO (B1) | `fsm_tuning_exit();` (bỏ bản tạm, xoá thông báo). `g_time_*` đã chứa giờ đã Lưu. |
| AUTO / MANUAL, nút USER | `display_next_page();` |

`fsm_tuning_run()` **không** xử lý B1. B1 do `fsm_mode` bắt, như bảng 2.1.

## 4. B – `traffic_light.c`

`display.c` cần biết khi lớp bảo vệ thứ 2 đã ép ĐỎ–ĐỎ, để hiện `SAFETY ERR`. B thêm vào `traffic_light.h/.c`:

```c
uint8_t traffic_is_fault(void);   /* 1 = da ep ALL RED do vi pham bat bien */
```

Trong `display.c` đang có một bản `__attribute__((weak))` trả về 0, để build được khi B chưa có hàm này. Khi B định nghĩa hàm thật, linker tự dùng bản của B.

## 5. B – `button.c`

`fsm_tuning` gọi `button_pressed(BTN_3) || button_held(BTN_3)` (B4 tương tự), nên cần đúng như hợp đồng:

- `button_pressed()` trả về 1 đúng một lần cho mỗi lần nhấn.
- `button_held()` trả về 1 một lần ở mốc giữ 1 s, rồi một lần sau mỗi 200 ms (mỗi lần trả 1 là xoá cờ).

## 6. Kiểm tra

- `sh test/pc/run.sh`: 64 ca test PC (LCD giả lập HD44780, DHT20 giả lập có CRC/busy, màn hình, FSM tuning, chạy mô phỏng 30 phút).
- `grep -n "HAL_" Core/Src/fsm_*.c Core/Src/display.c` phải rỗng.
