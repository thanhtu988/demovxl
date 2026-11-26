/* main.h GIA cho test tren PC: chi khai bao phan HAL ma lcd.c / dht20.c dung. */
#ifndef PC_MAIN_H_
#define PC_MAIN_H_
#include <stdint.h>

typedef enum { HAL_OK = 0, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef struct { int dummy; } I2C_HandleTypeDef;

HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *h, uint16_t addr, uint8_t *d,
                                          uint16_t n, uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef *h, uint16_t addr, uint8_t *d,
                                         uint16_t n, uint32_t timeout);
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *h, uint16_t addr, uint32_t trials,
                                        uint32_t timeout);
void     HAL_Delay(uint32_t ms);
uint32_t HAL_GetTick(void);
#endif
