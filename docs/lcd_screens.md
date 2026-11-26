# Màn hình LCD 16×2 (phần C)

LCD 16×2 HD44780 nối qua module I2C PCF8574, địa chỉ mặc định 0x27 (`lcd_init()` tự dò 0x27, 0x3F, 0x20–0x26, 0x39–0x3E; bỏ 0x38 vì trùng DHT20).

**Quy tắc:** dòng 1 luôn hiện trạng thái đèn 2 tuyến, để lúc demo nhìn LCD là biết đèn đang ra sao. Dòng 2 hiện nội dung phụ, và bị thông báo tạm (2 s) đè lên khi có.

## 1. Bảng màn hình

| Chế độ | Dòng 1 | Dòng 2 (trang 0) | Dòng 2 (trang 1 – nút USER) |
|---|---|---|---|
| INIT (2 s) | `STARTING...` | `T:28.5C H:65.2%` | – |
| AUTO | `AUTO 1:R07 2:G05` | `T:28.5C H:65.2%` | `SET R05 G03 Y02` |
| MANUAL | `MAN R1 - G2` | `T:28.5C H:65.2%` | `SET R05 G03 Y02` |
| TUNING | `TUNE >RED  : 05s` | `R05 G03 Y02 *` | – |
| Lỗi an toàn (mọi chế độ) | `SAFETY ERR` | giữ nguyên | – |

Ký hiệu đèn: `R` Đỏ, `Y` Vàng, `G` Xanh, `-` Tắt. Trong TUNING, dấu `*` nghĩa là bản tạm đang khác giờ đang áp dụng (chưa Lưu).

Khi chưa có số liệu, hoặc 3 lần đọc liên tiếp (6 s) bị lỗi: `T:--.-C H:--.-%`. Nhiệt độ âm hiện `T:-5.2C`.

## 2. Thông báo tạm (đè dòng 2 trong 2 s, tự xoá bằng one-shot `msg_clear`)

| Sự kiện (TUNING, nút USER = Lưu) | Dòng 2 | `g_time_*` |
|---|---|---|
| Hợp lệ: TĐ = TX + TV, TX ≥ 1, TV ≥ 1 | `SAVED` | cập nhật |
| TĐ ≠ TX + TV | `ERR: R!=G+Y` | giữ nguyên |
| TX = 0 hoặc TV = 0 | `ERR: G,Y >= 1s` | giữ nguyên |

Nếu có thông báo mới trong lúc thông báo cũ còn hiện, one-shot cũ bị `SCH_Delete_Task` trước rồi mới thêm one-shot mới. Nhờ vậy thông báo mới luôn hiện đủ 2 s.

## 3. Đếm ngược ở AUTO (mục 2.3 của kế hoạch)

`phase = ceil(timer_remaining_ms(TMR_PHASE) / 1000)`

| Tuyến này | Tuyến kia | Số hiện |
|---|---|---|
| Xanh hoặc Vàng | Đỏ | `phase` |
| Đỏ | Xanh | `phase + T_V` |
| Đỏ | Vàng hoặc Đỏ | `phase` |

Ví dụ: đang ở R1G2, timer pha còn 2,3 s, TV = 2 thì LCD hiện `AUTO 1:R05 2:G03`.

## 4. Thời gian ghi I2C (lý do có task `lcd_flush`)

| Thao tác | Byte I2C | Thời gian @100 kHz |
|---|---|---|
| 1 ký tự (2 nibble × 2 byte) | 4 | ≈ 0,36 ms |
| Ghi cả 1 dòng theo cách cũ (16 lần gửi riêng) | 64 + 16 địa chỉ | ≈ 7–8 ms |
| Ghi cả màn hình theo cách cũ | ≈ 160 | ≈ 15 ms (> 1 tick) |
| 1 lần `lcd_flush()` (con trỏ + tối đa 8 ký tự, 1 giao dịch) | ≤ 36 | ≈ 3,4 ms |
| Đếm ngược đổi 2 chữ số | 12 | ≈ 1,2 ms |

`lcd_print_line()` chỉ ghi vào bộ đệm RAM. Task `lcd_flush` (mỗi 10 ms) so bộ đệm với nội dung đang hiện và chỉ gửi những ký tự thay đổi. Đổi cả màn hình mất 4 lần flush (40 ms, mắt không thấy), nhưng không task nào chạy quá 1 tick.
