/*
 * lcd.h  (C) -- LCD 16x2 HD44780 qua module I2C PCF8574, giao tiep 4-bit
 *
 * Thiet ke "frame buffer + flush co gioi han":
 *   - lcd_print_line() chi ghi vao bo dem RAM (vai us), KHONG gui I2C.
 *   - lcd_flush() la task 10 ms: so bo dem voi noi dung dang hien tren LCD,
 *     chi gui cac ky tu khac nhau, toi da LCD_FLUSH_MAX_CHARS ky tu moi lan
 *     (~3.4 ms o 100 kHz) => khong task nao chiem qua 1 tick 10 ms.
 *   - Khong bao gio lcd_clear() dinh ky (lenh clear mat 1.6 ms va gay nhap nhay).
 *
 * Ban do bit PCF8574 -> LCD: P0 = RS, P1 = RW, P2 = EN, P3 = den nen, P4..P7 = D4..D7
 */
#ifndef INC_LCD_H_
#define INC_LCD_H_

#include <stdint.h>

#define LCD_COLS              16U
#define LCD_ROWS              2U
#define LCD_FLUSH_MAX_CHARS   8U     /* ky tu toi da moi lan lcd_flush()            */
#define LCD_ADDR_DEFAULT      0x27U  /* dia chi 7-bit (0x4E khi da dich trai 1 bit)  */

void    lcd_init(void);                              /* BLOCKING, goi truoc khi scheduler chay */
void    lcd_print_line(uint8_t row, const char *s);  /* dem du 16 ky tu, ghi de (vao bo dem)   */
void    lcd_flush(void);                             /* task 10 ms                             */
void    lcd_flush_all(void);                         /* BLOCKING, day het bo dem (chi luc init) */
uint8_t lcd_get_address(void);                       /* dia chi 7-bit tim duoc, 0 = khong co LCD */
uint8_t lcd_is_synced(void);                         /* 1 = LCD da hien dung noi dung bo dem    */

#endif /* INC_LCD_H_ */
