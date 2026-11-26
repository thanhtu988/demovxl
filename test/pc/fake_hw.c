/*
 * fake_hw.c -- gia lap phan cung cho test PC:
 *   - HD44780 sau PCF8574: giai ma tung byte I2C (chot nibble o canh xuong EN),
 *     theo doi che do 8-bit/4-bit, DDRAM, con tro tu tang.
 *   - DHT20: lenh 0xAC 0x33 0x00, ban 80 ms, 7 byte co CRC; co the tiem loi.
 *   - Bus I2C: dem byte moi giao dich de uoc luong thoi gian o 100 kHz.
 *   - i2c_bus_recover gia (ban that dung thanh ghi STM32, chi build bang arm-gcc).
 */
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "fake_hw.h"

I2C_HandleTypeDef hi2c1;
uint32_t g_fake_ms = 0;

/* ---------------- bus ---------------- */
uint32_t fake_i2c_max_tx_bytes = 0;
uint32_t fake_i2c_total_bytes  = 0;
int      fake_lcd_present = 1;
int      fake_recover_calls = 0;

void HAL_Delay(uint32_t ms) { g_fake_ms += ms; }
uint32_t HAL_GetTick(void) { return g_fake_ms; }

void i2c_bus_recover(void) { fake_recover_calls++; }
uint32_t i2c_bus_recover_count(void) { return (uint32_t)fake_recover_calls; }

/* ---------------- HD44780 ---------------- */
static char     lcd_ddram[128];
static uint8_t  lcd_ac = 0;
static int      lcd_4bit = 0;
static int      lcd_have_hi = 0;
static uint8_t  lcd_hi = 0;
static uint8_t  lcd_prev = 0;
int             fake_lcd_display_on = 0;

static void lcd_exec(uint8_t rs, uint8_t v)
{
    if (rs) {
        lcd_ddram[lcd_ac & 0x7F] = (char)v;
        lcd_ac = (uint8_t)((lcd_ac + 1) & 0x7F);
        return;
    }
    if (v & 0x80) { lcd_ac = v & 0x7F; return; }
    if (v & 0x40) { return; }                       /* CGRAM: bo qua */
    if (v & 0x20) { lcd_4bit = (v & 0x10) ? 0 : 1; return; }
    if (v & 0x08) { fake_lcd_display_on = (v & 0x04) ? 1 : 0; return; }
    if (v & 0x04) { return; }                       /* entry mode */
    if (v & 0x02) { lcd_ac = 0; return; }
    if (v & 0x01) { memset(lcd_ddram, ' ', sizeof(lcd_ddram)); lcd_ac = 0; return; }
}

static void lcd_pcf_byte(uint8_t b)
{
    /* canh xuong EN (bit 2) => chot nibble D7..D4 = b[7:4] */
    if ((lcd_prev & 0x04) && !(b & 0x04)) {
        uint8_t nib = (uint8_t)(b >> 4);
        uint8_t rs  = (uint8_t)(b & 0x01);
        if (!lcd_4bit) {
            lcd_exec(rs, (uint8_t)(nib << 4));
            lcd_have_hi = 0;
        } else if (!lcd_have_hi) {
            lcd_hi = nib; lcd_have_hi = 1;
        } else {
            lcd_exec(rs, (uint8_t)((lcd_hi << 4) | nib));
            lcd_have_hi = 0;
        }
    }
    lcd_prev = b;
}

void fake_hw_reset(void)
{
    memset(lcd_ddram, '?', sizeof(lcd_ddram));   /* rac sau cap nguon: phai co lenh clear */
    lcd_ac = 0; lcd_4bit = 0; lcd_have_hi = 0; lcd_prev = 0; fake_lcd_display_on = 0;
    fake_i2c_max_tx_bytes = 0; fake_i2c_total_bytes = 0;
}

void fake_lcd_read_row(int row, char out[17])
{
    memcpy(out, &lcd_ddram[row ? 0x40 : 0x00], 16);
    out[16] = '\0';
}

/* ---------------- DHT20 ---------------- */
static uint8_t  dht_status = 0x18;
static uint32_t dht_trig_ms = 0;
static int      dht_measuring = 0;
static uint32_t dht_raw_t = 0, dht_raw_h = 0;
int             fake_dht_present = 1;
int             fake_dht_corrupt_crc = 0;
int             fake_dht_measure_ms = 80;
int             fake_dht_trigger_count = 0;

static uint8_t crc8(const uint8_t *d, int n)
{
    uint8_t c = 0xFF;
    for (int i = 0; i < n; i++) {
        c ^= d[i];
        for (int b = 0; b < 8; b++) c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x31) : (uint8_t)(c << 1);
    }
    return c;
}

void fake_dht_set(double t_c, double rh)
{
    dht_raw_t = (uint32_t)((t_c + 50.0) / 200.0 * 1048576.0 + 0.5);
    dht_raw_h = (uint32_t)(rh / 100.0 * 1048576.0 + 0.5);
    if (dht_raw_t > 0xFFFFF) dht_raw_t = 0xFFFFF;
    if (dht_raw_h > 0xFFFFF) dht_raw_h = 0xFFFFF;
}

/* ---------------- HAL I2C ---------------- */
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *h, uint16_t addr, uint8_t *d,
                                          uint16_t n, uint32_t timeout)
{
    (void)h; (void)timeout;
    uint8_t a = (uint8_t)(addr >> 1);
    if (n > fake_i2c_max_tx_bytes) fake_i2c_max_tx_bytes = n;
    fake_i2c_total_bytes += n;
    if (a == 0x27) {
        if (!fake_lcd_present) return HAL_ERROR;
        for (uint16_t i = 0; i < n; i++) lcd_pcf_byte(d[i]);
        return HAL_OK;
    }
    if (a == 0x38) {
        if (!fake_dht_present) return HAL_ERROR;
        if (n == 3 && d[0] == 0xAC && d[1] == 0x33 && d[2] == 0x00) {
            dht_measuring = 1;
            dht_trig_ms = g_fake_ms;
            fake_dht_trigger_count++;
        }
        return HAL_OK;
    }
    return HAL_ERROR;
}

HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef *h, uint16_t addr, uint8_t *d,
                                         uint16_t n, uint32_t timeout)
{
    (void)h; (void)timeout;
    uint8_t a = (uint8_t)(addr >> 1);
    if (a != 0x38 || !fake_dht_present) return HAL_ERROR;
    int busy = dht_measuring && (g_fake_ms - dht_trig_ms) < (uint32_t)fake_dht_measure_ms;
    uint8_t st = (uint8_t)(dht_status | (busy ? 0x80 : 0x00));
    uint8_t buf[7];
    buf[0] = st;
    buf[1] = (uint8_t)(dht_raw_h >> 12);
    buf[2] = (uint8_t)(dht_raw_h >> 4);
    buf[3] = (uint8_t)(((dht_raw_h & 0x0F) << 4) | ((dht_raw_t >> 16) & 0x0F));
    buf[4] = (uint8_t)(dht_raw_t >> 8);
    buf[5] = (uint8_t)(dht_raw_t);
    buf[6] = crc8(buf, 6);
    if (fake_dht_corrupt_crc) buf[6] ^= 0x5A;
    for (uint16_t i = 0; i < n && i < 7; i++) d[i] = buf[i];
    return HAL_OK;
}

HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *h, uint16_t addr, uint32_t trials,
                                        uint32_t timeout)
{
    (void)h; (void)trials; (void)timeout;
    uint8_t a = (uint8_t)(addr >> 1);
    if (a == 0x27 && fake_lcd_present) return HAL_OK;
    if (a == 0x38 && fake_dht_present) return HAL_OK;
    return HAL_ERROR;
}
