/*
 * dht20.c  (C) -- DHT20 non-blocking tren scheduler
 *
 * Trinh tu (datasheet Aosong DHT20, muc 7.4):
 *   1. Sau cap nguon cho >= 100 ms. Doc 1 byte trang thai; neu (st & 0x18) != 0x18
 *      thi khoi tao lai 3 thanh ghi 0x1B, 0x1C, 0x1E.          -> dht20_init()
 *   2. Gui 0xAC 0x33 0x00 (kich hoat do).                       -> dht20_trigger()
 *   3. Cho 80 ms (one-shot 8 tick, KHONG HAL_Delay).
 *   4. Doc 7 byte: [trang thai][RH 20 bit][T 20 bit][CRC].      -> dht20_read()
 *      Bit 7 trang thai = 1 nghia la con dang do -> doc lai sau 10 ms.
 *   5. CRC-8: da thuc 0x31 (x^8 + x^5 + x^4 + 1), gia tri dau 0xFF, tinh tren 6 byte dau.
 *
 * So voi code mau cua thay: bo HAL_Delay(80) (chan ca he thong 8 tick), them kiem tra
 * busy + CRC, lam tron khi doi don vi, va xu ly so am cua nhiet do.
 */
#include "main.h"
#include "dht20.h"
#include "i2c_bus.h"
#include "scheduler.h"

extern I2C_HandleTypeDef hi2c1;

#define DHT20_HAL_ADDR   ((uint16_t)(DHT20_ADDR << 1))

static int16_t        s_temp_x10   = 0;
static uint16_t       s_rh_x10     = 0;
static uint8_t        s_has_data   = 0;     /* da co it nhat 1 lan doc dung       */
static uint8_t        s_fail_cycle = 0;     /* so chu ky 2 s lien tiep that bai   */
static uint8_t        s_busy_retry = 0;
static uint8_t        s_pending    = 0;     /* dang cho ket qua (trigger -> read) */
static uint8_t        s_err_count  = 0;     /* loi I2C lien tiep                  */
static dht20_status_t s_status     = DHT20_ST_NO_DATA;

/* ---------------- ham thuan ---------------- */

uint8_t dht20_crc8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0xFFU;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8U; b++) {
            crc = (crc & 0x80U) ? (uint8_t)((crc << 1) ^ 0x31U) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

void dht20_convert(const uint8_t raw[7], int16_t *temp_x10, uint16_t *rh_x10)
{
    uint32_t s_rh = ((uint32_t)raw[1] << 12) | ((uint32_t)raw[2] << 4) | ((uint32_t)raw[3] >> 4);
    uint32_t s_t  = (((uint32_t)raw[3] & 0x0FU) << 16) | ((uint32_t)raw[4] << 8) | (uint32_t)raw[5];

    /* x10 va lam tron: (S * 1000 + 2^19) >> 20 ; S <= 2^20 - 1 nen khong tran uint32 */
    *rh_x10   = (uint16_t)((s_rh * 1000UL + (1UL << 19)) >> 20);
    *temp_x10 = (int16_t)((int32_t)((s_t * 2000UL + (1UL << 19)) >> 20) - 500);
}

/* ---------------- tang I2C ---------------- */

static void note_i2c_error(void)
{
    s_status = DHT20_ST_ERR_I2C;
    if (++s_err_count >= I2C_BUS_ERR_LIMIT) {
        s_err_count = 0;
        i2c_bus_recover();
    }
}

static void end_cycle_failed(void)
{
    s_pending = 0;
    if (s_fail_cycle < 255U) {
        s_fail_cycle++;
    }
}

static void reset_register(uint8_t reg)
{
    /* quy trinh cua nha san xuat (cung cach lam trong code mau cua thay) */
    uint8_t buf[3] = { reg, 0x00, 0x00 };
    uint8_t reply[3] = { 0 };
    (void)HAL_I2C_Master_Transmit(&hi2c1, DHT20_HAL_ADDR, buf, 3, I2C_BUS_TIMEOUT_MS);
    HAL_Delay(5);
    (void)HAL_I2C_Master_Receive(&hi2c1, DHT20_HAL_ADDR, reply, 3, I2C_BUS_TIMEOUT_MS);
    HAL_Delay(10);
    buf[0] = (uint8_t)(0xB0U | reg);
    buf[1] = reply[1];
    buf[2] = reply[2];
    (void)HAL_I2C_Master_Transmit(&hi2c1, DHT20_HAL_ADDR, buf, 3, I2C_BUS_TIMEOUT_MS);
}

/* ---------------- API ---------------- */

void dht20_init(void)
{
    uint8_t st = 0;

    if (HAL_GetTick() < 100U) {          /* >= 100 ms ke tu cap nguon */
        HAL_Delay(100U - HAL_GetTick());
    }
    if (HAL_I2C_Master_Receive(&hi2c1, DHT20_HAL_ADDR, &st, 1, I2C_BUS_TIMEOUT_MS) != HAL_OK) {
        s_status = DHT20_ST_ERR_I2C;
        return;                          /* trigger sau van thu lai moi 2 s */
    }
    if ((st & 0x18U) != 0x18U) {
        reset_register(0x1B);
        reset_register(0x1C);
        reset_register(0x1E);
    }
    HAL_Delay(10);
}

void dht20_trigger(void)
{
    static uint8_t cmd[3] = { 0xAC, 0x33, 0x00 };

    if (s_pending) {
        /* lan truoc chua xong (khong xay ra neu chu ky 2 s >> 80 ms) */
        return;
    }
    if (HAL_I2C_Master_Transmit(&hi2c1, DHT20_HAL_ADDR, cmd, 3, I2C_BUS_TIMEOUT_MS) != HAL_OK) {
        note_i2c_error();
        end_cycle_failed();
        return;
    }
    s_pending    = 1;
    s_busy_retry = 0;
    if (SCH_Add_Task(dht20_read, DHT20_MEASURE_TICKS, 0) == NO_TASK_ID) {   /* one-shot 80 ms */
        end_cycle_failed();               /* mang task day: bo chu ky nay, 2 s sau thu lai */
    }
}

void dht20_read(void)
{
    uint8_t raw[7];

    if (!s_pending) {
        return;
    }
    if (HAL_I2C_Master_Receive(&hi2c1, DHT20_HAL_ADDR, raw, 7, I2C_BUS_TIMEOUT_MS) != HAL_OK) {
        note_i2c_error();
        end_cycle_failed();
        return;
    }
    s_err_count = 0;

    if (raw[0] & 0x80U) {                 /* bit busy: chua do xong */
        if (s_busy_retry < DHT20_BUSY_RETRY_MAX) {
            s_busy_retry++;
            if (SCH_Add_Task(dht20_read, 1, 0) == NO_TASK_ID) {
                end_cycle_failed();
            }
            return;                       /* van pending */
        }
        s_status = DHT20_ST_ERR_BUSY;
        end_cycle_failed();
        return;
    }
    if (dht20_crc8(raw, 6) != raw[6]) {
        s_status = DHT20_ST_ERR_CRC;      /* bo so lieu nay, giu gia tri cu */
        end_cycle_failed();
        return;
    }

    dht20_convert(raw, &s_temp_x10, &s_rh_x10);
    s_has_data   = 1;
    s_fail_cycle = 0;
    s_pending    = 0;
    s_status     = DHT20_ST_OK;
}

uint8_t dht20_get(int16_t *temp_x10, uint16_t *rh_x10)
{
    if (!s_has_data || s_fail_cycle >= DHT20_STALE_LIMIT) {
        return 0;                         /* chua co, hoac qua cu (>= 6 s khong doc duoc) */
    }
    if (temp_x10 != 0) {
        *temp_x10 = s_temp_x10;
    }
    if (rh_x10 != 0) {
        *rh_x10 = s_rh_x10;
    }
    return 1;
}

dht20_status_t dht20_last_status(void)
{
    return s_status;
}
