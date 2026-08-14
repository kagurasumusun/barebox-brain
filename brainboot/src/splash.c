/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "lcd.h"
#include "board.h"

#define ABS_LOCAL(v) ((v) < 0 ? -(v) : (v))

static void hexagon(int cx, int cy, int r, u16 color)
{
    /* Flat-top hexagon outline */
    int dx = r;
    int dy = r * 866 / 1000; /* ~sin(60) */
    int x[6], y[6], i, t;
    x[0] = cx - dx / 2; y[0] = cy - dy;
    x[1] = cx + dx / 2; y[1] = cy - dy;
    x[2] = cx + dx;     y[2] = cy;
    x[3] = cx + dx / 2; y[3] = cy + dy;
    x[4] = cx - dx / 2; y[4] = cy + dy;
    x[5] = cx - dx;     y[5] = cy;
    for (i = 0; i < 6; i++) {
        int x0 = x[i], y0 = y[i];
        int x1 = x[(i + 1) % 6], y1 = y[(i + 1) % 6];
        int steps = MAX(ABS_LOCAL(x1 - x0), ABS_LOCAL(y1 - y0));
        for (t = 0; t <= steps; t++) {
            int px = x0 + (x1 - x0) * t / (steps ? steps : 1);
            int py = y0 + (y1 - y0) * t / (steps ? steps : 1);
            lcd_pixel(px, py, color);
            lcd_pixel(px + 1, py, color);
        }
    }
}

void splash_draw(void)
{
    int x, y;
    u16 bg = RGB565(5, 8, 14);
    u16 c1 = RGB565(62, 224, 255);
    u16 c2 = RGB565(107, 92, 255);
    u16 dim = RGB565(20, 32, 48);
    int cx = LCD_WIDTH / 2;
    int cy = LCD_HEIGHT / 2 - 20;

    lcd_fill(bg);

    /* faint scanlines */
    for (y = 0; y < LCD_HEIGHT; y += 3)
        lcd_hline(0, y, LCD_WIDTH, dim);

    /* radiating ticks */
    for (x = 0; x < 24; x++) {
        int a = x * 15;
        /* simple 15-degree steps using integer approx */
        int cs[] = {1000,966,866,707,500,259,0,-259,-500,-707,-866,-966,
                    -1000,-966,-866,-707,-500,-259,0,259,500,707,866,966};
        int sn[] = {0,259,500,707,866,966,1000,966,866,707,500,259,
                    0,-259,-500,-707,-866,-966,-1000,-966,-866,-707,-500,-259};
        int x0 = cx + cs[x] * 40 / 1000;
        int y0 = cy + sn[x] * 40 / 1000;
        int x1 = cx + cs[x] * 90 / 1000;
        int y1 = cy + sn[x] * 90 / 1000;
        int t, steps;
        (void)a;
        steps = MAX(ABS_LOCAL(x1 - x0), ABS_LOCAL(y1 - y0));
        for (t = 0; t <= steps; t++) {
            int px = x0 + (x1 - x0) * t / (steps ? steps : 1);
            int py = y0 + (y1 - y0) * t / (steps ? steps : 1);
            lcd_pixel(px, py, (x & 1) ? c2 : c1);
        }
    }

    hexagon(cx, cy, 70, c1);
    hexagon(cx, cy, 48, c2);
    hexagon(cx, cy, 26, c1);
    lcd_rect(cx - 4, cy - 4, 8, 8, c1);

    lcd_text(cx - 8 * 4, cy + 110, "BRAINBOOT", c1, bg);
    lcd_text(cx - 8 * 8, cy + 124, "PW-AJ2  /  ED-AA2", RGB565(180, 200, 220), bg);
    lcd_text(cx - 8 * 10, cy + 140, "i.MX28  128MiB  eMMC", RGB565(90, 110, 140), bg);
    lcd_flush();
}

void splash_banner(const char *line2)
{
    u16 bg = RGB565(5, 8, 14);
    u16 fg = RGB565(62, 224, 255);
    lcd_rect(0, LCD_HEIGHT - 24, LCD_WIDTH, 24, RGB565(8, 12, 20));
    lcd_text(8, LCD_HEIGHT - 18, line2 ? line2 : "", fg, bg);
    lcd_flush();
}
