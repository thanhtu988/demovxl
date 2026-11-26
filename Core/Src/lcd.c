/*
 * lcd.c  (C) -- LCD 16x2 HD44780 qua PCF8574 (I2C1), che do 4-bit
 *
 * Moi nibble gui 2 byte qua PCF8574: (data | EN=1) roi (data | EN=0); canh xuong
 * cua EN chot du lieu. Mot ky tu = 2 nibble = 4 byte I2C.
 *
 * Thoi gian (I2C 100 kHz, 1 byte ~ 9 bit = 90 us):
 *   1 ky tu        : 4 byte              ~ 0.36 ms
 *   1 lan lcd_flush: 1 lenh dat con tro + toi da 8 ky tu = 36 byte trong 1 giao dich
 *                    ~ (1 + 36) * 90 us  ~ 3.4 ms  < 1 tick 10 ms
 *   ghi ca man hinh (32 ky tu) bang ban cu: ~ 12 ms > 1 tick  => ly do chia nho.
 *
 * Khong co HAL_Delay trong lcd_flush(); chi lcd_init()/lcd_flush_all() la blocking
 * va chi duoc goi truoc khi scheduler chay.
 */
#include <string.h>
#include "main.h"
#include "lcd.h"
#include "i2c_bus.h"

extern I2C_HandleTypeDef hi2c1;

#define PCF_RS   0x01U
#define PCF_EN   0x04U
#define PCF_BL   0x08U

#define LCD_CMD_CLEAR        0x01U
#define LCD_CMD_ENTRY_INC    0x06U
#define LCD_CMD_DISPLAY_ON   0x0CU
#define LCD_CMD_DISPLAY_OFF  0x08U
#define LCD_CMD_FUNC_4BIT_2L 0x28U
#define LCD_CMD_SET_DDRAM    0x80U

static const uint8_t k_row_addr[LCD_ROWS] = { 0x00U, 0x40U };

static uint8_t s_addr = 0;                         /* dia chi 7-bit, 0 = khong co LCD */
static char    s_want[LCD_ROWS][LCD_COLS];         /* noi dung muon hien thi           */
static char    s_shown[LCD_ROWS][LCD_COLS];        /* noi dung dang nam tren LCD       */
static uint8_t s_err_count = 0;

/* ---------------- tang thap: dong goi byte cho PCF8574 ---------------- */

/* them 1 byte (2 nibble = 4 byte PCF8574) vao buf, tra ve so byte da them */
static uint8_t pack_byte(uint8_t *buf, uint8_t value, uint8_t rs)
{
    uint8_t hi = (uint8_t)(value & 0xF0U);
    uint8_t lo = (uint8_t)((value << 4) & 0xF0U);
    uint8_t ctl = (uint8_t)(PCF_BL | (rs ? PCF_RS : 0U));
    buf[0] = (uint8_t)(hi | ctl | PCF_EN);
    buf[1] = (uint8_t)(hi | ctl);
    buf[2] = (uint8_t)(lo | ctl | PCF_EN);
    buf[3] = (uint8_t)(lo | ctl);
    return 4U;
}

static uint8_t lcd_tx(uint8_t *buf, uint16_t len)
{
    if (s_addr == 0U) {
        return 0U;
    }
    if (HAL_I2C_Master_Transmit(&hi2c1, (uint16_t)(s_addr << 1), buf, len,
                                I2C_BUS_TIMEOUT_MS + 5U) == HAL_OK) {
        s_err_count = 0;
        return 1U;
    }
    if (++s_err_count >= I2C_BUS_ERR_LIMIT) {
        s_err_count = 0;
        i2c_bus_recover();
    }
    return 0U;
}

/* gui 1 nibble cao (dung trong chuoi khoi dong o che do 8-bit) */
static void lcd_send_nibble(uint8_t nibble_hi)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)((nibble_hi & 0xF0U) | PCF_BL | PCF_EN);
    buf[1] = (uint8_t)((nibble_hi & 0xF0U) | PCF_BL);
    (void)lcd_tx(buf, 2);
}

static void lcd_send_cmd(uint8_t cmd)
{
    uint8_t buf[4];
    (void)lcd_tx(buf, pack_byte(buf, cmd, 0U));
}

/* ---------------- khoi tao ---------------- */

static uint8_t lcd_detect_address(void)
{
    /* PCF8574 : 0x20..0x27 (module pho bien 0x27)
     * PCF8574A: 0x38..0x3F (0x3F) -- bo qua 0x38 vi trung dia chi DHT20 */
    static const uint8_t k_candidates[] = {
        0x27, 0x3F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
        0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E
    };
    for (uint8_t i = 0; i < sizeof(k_candidates); i++) {
        if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(k_candidates[i] << 1), 2, 5) == HAL_OK) {
            return k_candidates[i];
        }
    }
    return 0U;
}

void lcd_init(void)
{
    s_addr = lcd_detect_address();

    memset(s_want, ' ', sizeof(s_want));
    memset(s_shown, ' ', sizeof(s_shown));
    if (s_addr == 0U) {
        return;   /* khong co LCD: cac ham khac van chay, chi khong gui gi */
    }

    /* chuoi khoi dong 4-bit theo datasheet HD44780 (Figure 24) */
    HAL_Delay(50);              /* > 40 ms sau khi VCC on dinh */
    lcd_send_nibble(0x30);
    HAL_Delay(5);
    lcd_send_nibble(0x30);
    HAL_Delay(1);
    lcd_send_nibble(0x30);
    HAL_Delay(1);
    lcd_send_nibble(0x20);      /* chuyen sang 4-bit */
    HAL_Delay(1);

    lcd_send_cmd(LCD_CMD_FUNC_4BIT_2L);
    lcd_send_cmd(LCD_CMD_DISPLAY_OFF);
    lcd_send_cmd(LCD_CMD_CLEAR);
    HAL_Delay(2);               /* lenh clear can 1.52 ms */
    lcd_send_cmd(LCD_CMD_ENTRY_INC);
    lcd_send_cmd(LCD_CMD_DISPLAY_ON);
}

/* ---------------- API ---------------- */

void lcd_print_line(uint8_t row, const char *s)
{
    if (row >= LCD_ROWS) {
        return;
    }
    uint8_t i = 0;
    if (s != 0) {
        for (; i < LCD_COLS && s[i] != '\0'; i++) {
            s_want[row][i] = s[i];
        }
    }
    for (; i < LCD_COLS; i++) {
        s_want[row][i] = ' ';    /* dem du 16 ky tu => ghi de het noi dung cu */
    }
}

void lcd_flush(void)
{
    if (s_addr == 0U) {
        return;
    }
    for (uint8_t row = 0; row < LCD_ROWS; row++) {
        /* tim ky tu khac dau tien tren dong */
        uint8_t first = 0;
        while (first < LCD_COLS && s_want[row][first] == s_shown[row][first]) {
            first++;
        }
        if (first == LCD_COLS) {
            continue;            /* dong nay khong doi */
        }
        /* tim ky tu khac cuoi cung trong cua so LCD_FLUSH_MAX_CHARS */
        uint8_t limit = (uint8_t)(first + LCD_FLUSH_MAX_CHARS);
        if (limit > LCD_COLS) {
            limit = LCD_COLS;
        }
        uint8_t last = first;
        for (uint8_t c = first; c < limit; c++) {
            if (s_want[row][c] != s_shown[row][c]) {
                last = c;
            }
        }

        /* 1 giao dich I2C: dat con tro + chuoi ky tu [first..last] */
        uint8_t buf[4U * (1U + LCD_FLUSH_MAX_CHARS)];
        uint8_t len = pack_byte(buf, (uint8_t)(LCD_CMD_SET_DDRAM | (k_row_addr[row] + first)), 0U);
        for (uint8_t c = first; c <= last; c++) {
            len = (uint8_t)(len + pack_byte(&buf[len], (uint8_t)s_want[row][c], 1U));
        }
        if (lcd_tx(buf, len)) {
            memcpy(&s_shown[row][first], &s_want[row][first], (size_t)(last - first + 1U));
        }
        return;                  /* moi lan goi chi xu ly 1 doan => thoi gian bi chan tren */
    }
}

void lcd_flush_all(void)
{
    /* toi da 2 dong x 2 doan = 4 lan; them du phong neu I2C loi */
    for (uint8_t i = 0; i < 8U && !lcd_is_synced(); i++) {
        lcd_flush();
    }
}

uint8_t lcd_get_address(void)
{
    return s_addr;
}

uint8_t lcd_is_synced(void)
{
    return (uint8_t)(memcmp(s_want, s_shown, sizeof(s_want)) == 0);
}
