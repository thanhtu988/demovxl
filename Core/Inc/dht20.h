/*
 * dht20.h  (C) -- cam bien nhiet do / do am DHT20 (I2C, dia chi 0x38)
 *
 * Doc KHONG chan (non-blocking), tach 2 pha de khong task nao chay qua 1 tick:
 *   dht20_trigger()  (task 2 s)   : gui lenh do 0xAC 0x33 0x00, roi tu them
 *                                   one-shot dht20_read() sau 8 tick = 80 ms
 *   dht20_read()     (one-shot)   : doc 7 byte, kiem tra bit busy + CRC-8,
 *                                   doi ra so nguyen x10 (285 = 28.5 do C)
 *
 * Cong thuc (datasheet DHT20):
 *   RH = S_RH / 2^20 * 100 %          T = S_T / 2^20 * 200 - 50  (do C)
 */
#ifndef INC_DHT20_H_
#define INC_DHT20_H_

#include <stdint.h>

#define DHT20_ADDR              0x38U  /* 7-bit */
#define DHT20_MEASURE_TICKS     8U     /* 80 ms: thoi gian do theo datasheet         */
#define DHT20_BUSY_RETRY_MAX    3U     /* con ban thi doc lai sau 1 tick, toi da 3 lan */
#define DHT20_STALE_LIMIT       3U     /* 3 chu ky lien tiep loi => so lieu khong hop le */

typedef enum {
    DHT20_ST_NO_DATA = 0,   /* chua doc duoc lan nao                     */
    DHT20_ST_OK,            /* lan doc gan nhat thanh cong               */
    DHT20_ST_ERR_I2C,       /* khong ACK / timeout                       */
    DHT20_ST_ERR_BUSY,      /* het 80 ms + 3 lan doc lai van con ban     */
    DHT20_ST_ERR_CRC        /* sai CRC => bo so lieu, giu gia tri cu     */
} dht20_status_t;

void    dht20_init(void);                                 /* BLOCKING, truoc scheduler */
void    dht20_trigger(void);                              /* task 2 s                  */
void    dht20_read(void);                                 /* one-shot do trigger them  */
uint8_t dht20_get(int16_t *temp_x10, uint16_t *rh_x10);   /* 0 neu chua co so lieu     */
dht20_status_t dht20_last_status(void);

/* ham thuan (khong dung phan cung) -- tach ra de test tren PC */
uint8_t dht20_crc8(const uint8_t *data, uint8_t len);
void    dht20_convert(const uint8_t raw[7], int16_t *temp_x10, uint16_t *rh_x10);

#endif /* INC_DHT20_H_ */
