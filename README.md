# BTL CO3009 – Phần việc của C

I2C drivers (DHT20, LCD), giao diện LCD và FSM chỉnh giờ, viết theo kế hoạch và hợp đồng API ở mục 2.6.

## Nội dung

```
Core/Inc, Core/Src     -> chép vào Core/ của project chung (nhánh feature/lcd, feature/dht20, feature/tuning)
  lcd.h/.c             LCD 16x2 qua PCF8574, frame buffer + task lcd_flush (<= 3.4 ms mỗi lần)
  dht20.h/.c           DHT20 non-blocking: trigger -> one-shot 80 ms -> đọc, kiểm tra busy + CRC-8
  display.h/.c         3 màn hình + thông báo tự xoá (không gọi HAL)
  fsm_tuning.h/.c      FSM chỉnh giờ trên bản tạm, kiểm tra R = G + Y (không gọi HAL)
  i2c_bus.h/.c         I2C scan + khôi phục lỗi kẹt BUSY của STM32F1 (rủi ro mục 6)
docs/
  integration_C.md     việc A, B cần làm để ghép (task lcd_flush, entry/exit, traffic_is_fault)
  lcd_screens.md       bảng màn hình LCD, cách tính đếm ngược, bảng thời gian I2C
  BaoCao_phanC_nhap.docx  nháp các mục báo cáo của C (1.2, 1.3, 2.1, 2.3.2, 2.3.3, 3.3.4, 3.4.3, 3.5, 4.1, 4.2)
  report_C.md + build_report.js   nguồn của file .docx (node build_report.js)
  *.dot, img/*.png     sơ đồ khối, FSM tuning, lưu đồ kiểm tra hợp lệ (Graphviz)
test/
  pc/                  test trên PC: sh test/pc/run.sh   (64 ca, HD44780 + DHT20 giả lập)
  stubs/               header GIẢ của A/B theo hợp đồng API, CHỈ để test; không chép vào Core/
  c_lab_demo.c         đoạn dán tạm vào main.c để qua cổng lab của C (I2C scan, LCD, DHT20)
```

## Đã kiểm tra

- `sh test/pc/run.sh`: 64/64 PASS, build với `-Wall -Wextra -Werror -fsanitize=address,undefined`.
- `arm-none-eabi-gcc -mcpu=cortex-m3 -Wall -Wextra -Wconversion` với HAL STM32F1 (STM32F103xB): 0 warning.
- `grep -n "HAL_" Core/Src/fsm_*.c Core/Src/display.c`: rỗng.
- Chưa chạy trên mạch thật. Các mục [CẦN ĐIỀN] trong báo cáo phải lấy số liệu ở lab.

## Commit

Commit bằng tài khoản GitHub của bạn (thầy chấm đóng góp theo lịch sử commit), chia nhỏ theo module, ví dụ:

```
i2c: add bus scan and BUSY recovery for STM32F1
lcd: add PCF8574 driver with frame buffer and bounded flush task
dht20: non-blocking read with busy retry and CRC-8 check
display: add AUTO/MANUAL/TUNING screens and auto-clear messages
fsm_tuning: edit on temp copy, validate R = G + Y on save
docs: add lcd_screens and integration notes
```
