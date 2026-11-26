/*
 * i2c_bus.h  (C) -- tien ich dung chung cho bus I2C1 (DHT20 + LCD)
 *
 * - i2c_bus_scan(): quet dia chi 7-bit 0x01..0x7F (dung cho kiem thu phan cung, Buoc 3.4)
 * - i2c_bus_recover(): thoat loi "I2C ket BUSY" da biet cua STM32F1
 *   (HAL tra HAL_BUSY mai sau khi reset giua chung mot giao dich).
 *
 * Chan SCL/SDA duoc doc tu AFIO->MAPR (I2C1 remap hay khong), nen khong
 * phu thuoc A cau hinh I2C1 o PB6/PB7 hay PB8/PB9 trong .ioc.
 */
#ifndef INC_I2C_BUS_H_
#define INC_I2C_BUS_H_

#include <stdint.h>

#define I2C_BUS_TIMEOUT_MS   5U   /* timeout moi giao dich HAL (ms)              */
#define I2C_BUS_ERR_LIMIT    5U   /* so loi lien tiep truoc khi tu khoi phuc bus */

uint8_t i2c_bus_scan(uint8_t *found, uint8_t max_found); /* tra ve so thiet bi tim thay */
void    i2c_bus_recover(void);                           /* goi sau MX_I2C1_Init()      */
uint32_t i2c_bus_recover_count(void);                    /* so lan da khoi phuc (debug) */

#endif /* INC_I2C_BUS_H_ */
