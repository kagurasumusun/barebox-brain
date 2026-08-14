/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Host render of splash_draw / splash_second to a PPM for screen verification.
 */
#ifndef HOST_BUILD
#define HOST_BUILD
#endif
#include "lcd.h"
#include "board.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern const u8 font8x8[96][8];

static u16 fb[LCD_WIDTH * LCD_HEIGHT];
static int g_ok;

void lcd_init(void) { memset(fb, 0, sizeof(fb)); g_ok = 1; }
void lcd_flush(void) {}
int  lcd_ready(void) { return g_ok; }
void lcd_fill(u16 color)
{
    u32 i, n = (u32)LCD_WIDTH * LCD_HEIGHT;
    for (i = 0; i < n; i++)
        fb[i] = color;
}
void lcd_pixel(int x, int y, u16 color)
{
    if ((unsigned)x >= LCD_WIDTH || (unsigned)y >= LCD_HEIGHT)
        return;
    fb[y * LCD_WIDTH + x] = color;
}
void lcd_hline(int x, int y, int w, u16 color)
{
    int i;
    for (i = 0; i < w; i++)
        lcd_pixel(x + i, y, color);
}
void lcd_vline(int x, int y, int h, u16 color)
{
    int i;
    for (i = 0; i < h; i++)
        lcd_pixel(x, y + i, color);
}
void lcd_rect(int x, int y, int w, int h, u16 color)
{
    int j;
    for (j = 0; j < h; j++)
        lcd_hline(x, y + j, w, color);
}
void lcd_char(int x, int y, char c, u16 fg, u16 bg)
{
    int gx, gy;
    const u8 *g;
    if ((u8)c < 0x20 || (u8)c > 0x7f)
        c = '?';
    g = font8x8[(u8)c - 0x20];
    for (gy = 0; gy < 8; gy++) {
        u8 bits = g[gy];
        for (gx = 0; gx < 8; gx++)
            lcd_pixel(x + gx, y + gy, (bits & (0x80 >> gx)) ? fg : bg);
    }
}
void lcd_text(int x, int y, const char *s, u16 fg, u16 bg)
{
    while (*s) {
        lcd_char(x, y, *s++, fg, bg);
        x += 8;
    }
}
void lcd_char_scaled(int x, int y, char c, u16 fg, u16 bg, int scale)
{
    int gx, gy, sx, sy;
    const u8 *g;
    if (scale < 1)
        scale = 1;
    if ((u8)c < 0x20 || (u8)c > 0x7f)
        c = '?';
    g = font8x8[(u8)c - 0x20];
    for (gy = 0; gy < 8; gy++) {
        u8 bits = g[gy];
        for (gx = 0; gx < 8; gx++) {
            u16 col = (bits & (0x80 >> gx)) ? fg : bg;
            for (sy = 0; sy < scale; sy++)
                for (sx = 0; sx < scale; sx++)
                    lcd_pixel(x + gx * scale + sx, y + gy * scale + sy, col);
        }
    }
}
void lcd_text_scaled(int x, int y, const char *s, u16 fg, u16 bg, int scale)
{
    while (*s) {
        lcd_char_scaled(x, y, *s++, fg, bg, scale);
        x += 8 * scale;
    }
}
void lcd_frame(int x, int y, int w, int h, u16 color)
{
    lcd_hline(x, y, w, color);
    lcd_hline(x, y + h - 1, w, color);
    lcd_vline(x, y, h, color);
    lcd_vline(x + w - 1, y, h, color);
}
void lcd_rrect(int x, int y, int w, int h, u16 fill, u16 border)
{
    (void)x;(void)y;(void)w;(void)h;(void)fill;(void)border;
}

extern void splash_draw(void);
extern void splash_second(const char *model);

static void rgb565_to_rgb(u16 p, u8 *r, u8 *g, u8 *b)
{
    *r = (u8)(((p >> 11) & 0x1f) * 255 / 31);
    *g = (u8)(((p >> 5) & 0x3f) * 255 / 63);
    *b = (u8)((p & 0x1f) * 255 / 31);
}

static int write_ppm(const char *path)
{
    FILE *f = fopen(path, "wb");
    u32 i, n = (u32)LCD_WIDTH * LCD_HEIGHT;
    if (!f)
        return -1;
    fprintf(f, "P6\n%d %d\n255\n", LCD_WIDTH, LCD_HEIGHT);
    for (i = 0; i < n; i++) {
        u8 rgb[3];
        rgb565_to_rgb(fb[i], &rgb[0], &rgb[1], &rgb[2]);
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    return 0;
}

int main(int argc, char **argv)
{
    u32 i, lit = 0, n = (u32)LCD_WIDTH * LCD_HEIGHT;
    const char *out = argc > 1 ? argv[1] : "build/splash.ppm";

    lcd_init();
    splash_draw();
    splash_second("PW-SH6");
    for (i = 0; i < n; i++)
        if (fb[i] != 0)
            lit++;
    if (lit < 200) {
        fprintf(stderr, "splash too dark: lit=%u\n", lit);
        return 1;
    }
    /* SHARP wordmark sits around y=180, x center. */
    {
        int x, y, hits = 0;
        for (y = 180; y < 180 + 32; y++)
            for (x = LCD_WIDTH / 2 - 80; x < LCD_WIDTH / 2 + 80; x++)
                if (fb[y * LCD_WIDTH + x] > 0x7bef)
                    hits++;
        if (hits < 80) {
            fprintf(stderr, "SHARP wordmark missing: hits=%d\n", hits);
            return 2;
        }
        printf("splash ok  lit=%u  wordmark_hits=%d  %dx%d\n",
               lit, hits, LCD_WIDTH, LCD_HEIGHT);
    }
    if (write_ppm(out)) {
        perror(out);
        return 3;
    }
    printf("wrote %s\n", out);
    return 0;
}
