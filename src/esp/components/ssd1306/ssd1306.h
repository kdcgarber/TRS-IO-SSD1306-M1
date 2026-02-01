// Simple header for SSD1306 component
#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>

#define I2C_MASTER_FREQ_HZ         100000
#define I2C_MASTER_NUM             I2C_NUM_0

// SDA on GPIO 4, SCL on GPIO 5
#define I2C_MASTER_SDA_IO          4   // ---led
#define I2C_MASTER_SCL_IO          5   // ---led

#define OLED_ADDR                  0x3C
#define SCREEN_WIDTH               128
#define SCREEN_HEIGHT              32
#define FRAMEBUFFER_SIZE           (SCREEN_WIDTH * SCREEN_HEIGHT / 8)

extern uint8_t framebuffer[FRAMEBUFFER_SIZE];

// Initialization and low-level
void i2c_master_init(void);
void ssd1306_send_command(uint8_t cmd);
void ssd1306_init(void);

// Framebuffer helpers
void clear_framebuffer(void);
void ssd1306_clear_display(void);
void draw_char_to_buffer(char c, int x, int page);
void draw_text(const char* text, int x, int page);
void ssd1306_send_framebuffer(void);
void ssd1306_start_vertical_scroll(void);

#endif // SSD1306_H
