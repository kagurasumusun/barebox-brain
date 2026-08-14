/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "lcd.h"
#include "board.h"

/* EBOOT Display first / second image: black Sharp wordmark. */
void splash_draw(void)
{
    u16 bg = RGB565(0, 0, 0);
    u16 fg = RGB565(240, 240, 240);
    u16 dim = RGB565(160, 160, 168);
    int cx = LCD_WIDTH / 2;

    lcd_fill(bg);
    lcd_text_scaled(cx - 5 * 16, 180, "SHARP", fg, bg, 4);
    lcd_hline(cx - 160, 250, 320, RGB565(48, 48, 52));
    lcd_text(cx - 8 * 12, 268, "E-DICTIONARY", dim, bg);
    lcd_flush();
}

void splash_second(const char *model)
{
    u16 bg = RGB565(0, 0, 0);
    u16 fg = RGB565(220, 220, 228);
    int cx = LCD_WIDTH / 2;
    const char *m = model ? model : BOARD_NAME;

    lcd_rect(0, 320, LCD_WIDTH, 40, bg);
    lcd_text(cx - (int)strlen(m) * 4, 328, m, fg, bg);
    lcd_flush();
}

void splash_banner(const char *line2)
{
    u16 bg = RGB565(0, 0, 0);
    u16 fg = RGB565(220, 220, 228);
    lcd_rect(0, LCD_HEIGHT - 24, LCD_WIDTH, 24, RGB565(8, 8, 10));
    lcd_text(8, LCD_HEIGHT - 18, line2 ? line2 : "", fg, bg);
    lcd_flush();
}
