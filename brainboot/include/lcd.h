/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef LCD_H
#define LCD_H

#include "brainboot.h"

#define RGB565(r, g, b) \
    ((u16)(((((r) & 0xf8) << 8) | (((g) & 0xfc) << 3) | (((b) & 0xf8) >> 3))))

void lcd_init(void);
void lcd_fill(u16 color);
void lcd_pixel(int x, int y, u16 color);
void lcd_rect(int x, int y, int w, int h, u16 color);
void lcd_hline(int x, int y, int w, u16 color);
void lcd_vline(int x, int y, int h, u16 color);
void lcd_char(int x, int y, char c, u16 fg, u16 bg);
void lcd_text(int x, int y, const char *s, u16 fg, u16 bg);
void lcd_flush(void);
int  lcd_ready(void);

void splash_draw(void);
void splash_banner(const char *line2);

#endif
