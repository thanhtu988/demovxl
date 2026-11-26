/*
 * i2c_bus.c  (C) -- quet bus va khoi phuc I2C1 bi ket BUSY tren STM32F103
 *
 * Hien tuong: neu MCU reset dung luc slave (DHT20) dang keo SDA xuong muc 0,
 * hoac bo loc analog cua I2C F1 bat nham canh, co BUSY trong I2C_SR2 bi khoa
 * o 1 va moi lenh HAL_I2C_* deu tra HAL_BUSY.
 *
 * Cach khoi phuc (theo errata ES096 cua STM32F10x + cach lam pho bien):
 *   1. HAL_I2C_DeInit() de tat ngoai vi.
 *   2. Cau hinh SCL, SDA thanh GPIO Output Open-Drain.
 *   3. Tao toi da 9 xung SCL de slave nha SDA, roi tao dieu kien STOP.
 *   4. Dat roi xoa bit SWRST trong I2C1->CR1 de xoa co BUSY.
 *   5. HAL_I2C_Init() lai (MspInit tra 2 chan ve Alternate Function).
 *
 * Chi duoc goi tu ngu canh main (task hoac truoc while(1)), khong goi trong ISR.
 * Khong dung HAL_Delay de goi duoc ca khi scheduler da chay.
 */
#include "main.h"
#include "i2c_bus.h"

extern I2C_HandleTypeDef hi2c1;   /* khai bao trong main.c do CubeMX sinh ra */

static uint32_t s_recover_count = 0;

/* tre ngan ~5 us o 64 MHz (vai us o 8 MHz van du cho I2C 100 kHz) */
static void bus_delay(void)
{
    for (volatile uint32_t i = 0; i < 80U; i++) {
        __NOP();
    }
}

uint8_t i2c_bus_scan(uint8_t *found, uint8_t max_found)
{
    uint8_t n = 0;
    for (uint8_t addr = 1; addr < 128U; addr++) {
        if (HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(addr << 1), 1, 2) == HAL_OK) {
            if (found != 0 && n < max_found) {
                found[n] = addr;
            }
            n++;
        }
    }
    return n;
}

void i2c_bus_recover(void)
{
    GPIO_InitTypeDef gpio = {0};
    uint16_t scl_pin, sda_pin;

    /* I2C1: mac dinh PB6 = SCL, PB7 = SDA; neu remap thi PB8 = SCL, PB9 = SDA */
    __HAL_RCC_AFIO_CLK_ENABLE();
    if ((AFIO->MAPR & AFIO_MAPR_I2C1_REMAP) != 0U) {
        scl_pin = GPIO_PIN_8;
        sda_pin = GPIO_PIN_9;
    } else {
        scl_pin = GPIO_PIN_6;
        sda_pin = GPIO_PIN_7;
    }

    HAL_I2C_DeInit(&hi2c1);

    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOB, scl_pin | sda_pin, GPIO_PIN_SET);
    gpio.Pin   = scl_pin | sda_pin;
    gpio.Mode  = GPIO_MODE_OUTPUT_OD;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio);
    bus_delay();

    /* toi da 9 xung clock cho den khi slave nha SDA */
    for (uint8_t i = 0; i < 9U; i++) {
        if (HAL_GPIO_ReadPin(GPIOB, sda_pin) == GPIO_PIN_SET) {
            break;
        }
        HAL_GPIO_WritePin(GPIOB, scl_pin, GPIO_PIN_RESET);
        bus_delay();
        HAL_GPIO_WritePin(GPIOB, scl_pin, GPIO_PIN_SET);
        bus_delay();
    }

    /* START roi STOP thu cong: SDA xuong khi SCL cao, roi SDA len khi SCL cao */
    HAL_GPIO_WritePin(GPIOB, sda_pin, GPIO_PIN_RESET);
    bus_delay();
    HAL_GPIO_WritePin(GPIOB, scl_pin, GPIO_PIN_RESET);
    bus_delay();
    HAL_GPIO_WritePin(GPIOB, scl_pin, GPIO_PIN_SET);
    bus_delay();
    HAL_GPIO_WritePin(GPIOB, sda_pin, GPIO_PIN_SET);
    bus_delay();

    /* xoa co BUSY bang software reset cua ngoai vi */
    __HAL_RCC_I2C1_CLK_ENABLE();
    hi2c1.Instance->CR1 |= I2C_CR1_SWRST;
    bus_delay();
    hi2c1.Instance->CR1 &= ~I2C_CR1_SWRST;

    HAL_I2C_Init(&hi2c1);   /* goi lai HAL_I2C_MspInit: chan ve AF Open-Drain */
    s_recover_count++;
}

uint32_t i2c_bus_recover_count(void)
{
    return s_recover_count;
}
