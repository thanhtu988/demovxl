#ifndef FAKE_HW_H_
#define FAKE_HW_H_
#include <stdint.h>
extern uint32_t g_fake_ms;
extern uint32_t fake_i2c_max_tx_bytes, fake_i2c_total_bytes;
extern int fake_lcd_present, fake_recover_calls, fake_lcd_display_on;
extern int fake_dht_present, fake_dht_corrupt_crc, fake_dht_measure_ms, fake_dht_trigger_count;
void fake_hw_reset(void);
void fake_lcd_read_row(int row, char out[17]);
void fake_dht_set(double t_c, double rh);
#endif
