#ifndef DISPLAY_SSD1306_H
#define DISPLAY_SSD1306_H

#include <stdint.h>
#include <stdbool.h>

#define SSD1306_WIDTH   128
#define SSD1306_HEIGHT  64

extern uint8_t ssd1306_addr;

bool ssd1306_init(void);
void ssd1306_clear(void);
void ssd1306_draw_pixel(uint8_t x, uint8_t y, bool on);
void ssd1306_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, bool fill, bool on);
void ssd1306_draw_char(uint8_t x, uint8_t y, char c);
void ssd1306_draw_string(uint8_t x, uint8_t y, const char *str);
bool ssd1306_update(void);

extern volatile int last_i2c_error;
extern volatile int last_i2c_cmd;

#endif // DISPLAY_SSD1306_H
