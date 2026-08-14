/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "lcd.h"
#include "board.h"
#include "gpio.h"

extern const u8 font8x8[96][8];

#ifndef HOST_BUILD

static int g_lcd_ok;
static u16 *fb = (u16 *)FRAMEBUFFER_PHYS;

/* ILI9805 / MIPI-DBI command stream used by gen3_6 (PW-SH6 / ED-AA2). */
struct lcd_op {
    u8 payload;
    u8 is_data;
    u16 delay_ms;
};

static const struct lcd_op ili9805_init[] = {
    { 0xff, 0, 0 }, { 0xff, 1, 0 }, { 0x98, 1, 0 }, { 0x05, 1, 0 },
    { 0xfd, 0, 0 }, { 0x03, 1, 0 }, { 0x13, 1, 0 }, { 0x44, 1, 0 }, { 0x00, 1, 0 },
    { 0xb8, 0, 0 }, { 0x72, 1, 0 },
    { 0x3a, 0, 0 }, { 0x55, 1, 0 }, /* 16 bpp */
    { 0xb1, 0, 0 }, { 0x00, 1, 0 }, { 0x12, 1, 0 }, { 0x14, 1, 0 },
    { 0xb4, 0, 0 }, { 0x02, 1, 0 },
    { 0xc1, 0, 0 }, { 0x13, 1, 0 }, { 0x28, 1, 0 }, { 0x08, 1, 0 }, { 0x26, 1, 0 },
    { 0xc7, 0, 0 }, { 0x90, 1, 0 },
};

static int lcdif_wait_run_clear(void)
{
    u32 g = 0x10000;
    while ((readl(IMX_LCDIF_BASE + HW_LCDIF_CTRL) & LCDIF_CTRL_RUN) && --g)
        ;
    return g ? 0 : -1;
}

static int lcd_write_byte(u32 payload, int data)
{
    u32 lcd = IMX_LCDIF_BASE;
    if (lcdif_wait_run_clear())
        return -1;
    writel(LCDIF_TRANSFER_COUNT(1, 1), lcd + HW_LCDIF_TRANSFER_COUNT);
    writel_clr(LCDIF_CTRL_DATA_SELECT | LCDIF_CTRL_RUN, lcd + HW_LCDIF_CTRL);
    if (data)
        writel_set(LCDIF_CTRL_DATA_SELECT, lcd + HW_LCDIF_CTRL);
    writel_set(LCDIF_CTRL_RUN, lcd + HW_LCDIF_CTRL);
    {
        u32 g = 0x10000;
        while ((readl(lcd + HW_LCDIF_STAT) & (1u << 29)) && --g)
            ;
        if (!g)
            return -1;
    }
    writel(payload, lcd + HW_LCDIF_DATA);
    return lcdif_wait_run_clear();
}

static void lcd_cmd(u8 c) { lcd_write_byte(c, 0); }
static void lcd_dat(u8 d) { lcd_write_byte(d, 1); }

static void pwm_backlight_on(void)
{
    u32 pwm = IMX_PWM_BASE;
    writel_clr(PWM_CTRL_SFTRST | PWM_CTRL_CLKGATE, pwm + HW_PWM_CTRL);
    writel((0x005a << 16) | 0x0000, pwm + HW_PWM_ACTIVE0);
    writel((0x00f0 << 16) | 0x0000, pwm + HW_PWM_ACTIVE1);
    /* period, active-high */
    writel((1u << 20) | (2u << 18) | (3u << 16) | 0x01f3, pwm + HW_PWM_PERIOD0);
    writel((0u << 20) | (3u << 18) | (3u << 16) | 0x07cf, pwm + HW_PWM_PERIOD1);
    writel_set(PWM_CTRL_PWM0_ENABLE | PWM_CTRL_PWM1_ENABLE, pwm + HW_PWM_CTRL);
    gpio_direction_output(GPIO_BL_ENABLE, 1);
}

void lcd_init(void)
{
    u32 lcd = IMX_LCDIF_BASE;
    unsigned i;
    u8 mac;

    gpio_direction_output(GPIO_LCD_EN0, 1);
    gpio_direction_output(GPIO_LCD_EN1, 1);
    gpio_direction_output(GPIO_LCD_EN2, 1);
    mdelay(20);

    /* Soft reset LCDIF */
    writel_set(LCDIF_CTRL_SFTRST, lcd + HW_LCDIF_CTRL);
    mdelay(1);
    writel_clr(LCDIF_CTRL_SFTRST | LCDIF_CTRL_CLKGATE, lcd + HW_LCDIF_CTRL);

    /* 0xF = all four bytes valid → two packed RGB565 pixels per word. */
    writel(LCDIF_CTRL1_BYTE_PACKING_FORMAT(0xf) | LCDIF_CTRL1_RESET,
           lcd + HW_LCDIF_CTRL1);

    /* System 8080 mode */
    writel_clr(LCDIF_CTRL_MASTER | LCDIF_CTRL_DOTCLK_MODE | LCDIF_CTRL_BYPASS_COUNT,
               lcd + HW_LCDIF_CTRL);
    writel_set(LCDIF_CTRL_VSYNC_MODE, lcd + HW_LCDIF_CTRL);
    writel_set(LCDIF_VDCTRL3_VSYNC_ONLY, lcd + HW_LCDIF_VDCTRL3);
    writel(LCDIF_TIMING_CMD_HOLD(1) | LCDIF_TIMING_CMD_SETUP(1) |
           LCDIF_TIMING_DATA_HOLD(1) | LCDIF_TIMING_DATA_SETUP(1),
           lcd + HW_LCDIF_TIMING);

    for (i = 0; i < ARRAY_SIZE(ili9805_init); i++) {
        lcd_write_byte(ili9805_init[i].payload, ili9805_init[i].is_data);
        if (ili9805_init[i].delay_ms)
            mdelay(ili9805_init[i].delay_ms);
    }

    /* Memory Access Control: transpose + flip_y_gs (pwsh6 dts) */
    mac = (1u << 5) | (1u << 0);
    lcd_cmd(0x36);
    lcd_dat(mac);

    lcd_cmd(0x11); /* Sleep Out */
    mdelay(120);
    lcd_cmd(0x29); /* Display On */
    mdelay(20);

    /* Column / page window: after transpose, width=854, height=480 */
    lcd_cmd(0x2a);
    lcd_dat(0x00); lcd_dat(0x00);
    lcd_dat((u8)((LCD_WIDTH - 1) >> 8));
    lcd_dat((u8)((LCD_WIDTH - 1) & 0xff));
    lcd_cmd(0x2b);
    lcd_dat(0x00); lcd_dat(0x00);
    lcd_dat((u8)((LCD_HEIGHT - 1) >> 8));
    lcd_dat((u8)((LCD_HEIGHT - 1) & 0xff));
    lcd_cmd(0x2c);

    /* Switch LCDIF to master DMA-from-FB for subsequent flushes. */
    /* WORD_LENGTH=0 → 16 bpp. Do not set DATA_FORMAT_16: qemu-brain
     * treats that bit as ARGB1555, not RGB565. */
    writel_set(LCDIF_CTRL_MASTER | LCDIF_CTRL_DATA_SELECT, lcd + HW_LCDIF_CTRL);
    writel_clr(LCDIF_CTRL_DATA_FORMAT_16_BIT | LCDIF_CTRL_DATA_FORMAT_18_BIT |
               LCDIF_CTRL_DATA_FORMAT_24_BIT, lcd + HW_LCDIF_CTRL);
    writel(FRAMEBUFFER_PHYS, lcd + HW_LCDIF_CUR_BUF);
    writel(FRAMEBUFFER_PHYS, lcd + HW_LCDIF_NEXT_BUF);
    writel(LCDIF_TRANSFER_COUNT(LCD_WIDTH, LCD_HEIGHT), lcd + HW_LCDIF_TRANSFER_COUNT);

    pwm_backlight_on();
    lcd_fill(RGB565(5, 7, 12));
    lcd_flush();
    g_lcd_ok = 1;
}

void lcd_flush(void)
{
    u32 lcd = IMX_LCDIF_BASE;
    writel(FRAMEBUFFER_PHYS, lcd + HW_LCDIF_NEXT_BUF);
    writel(LCDIF_TRANSFER_COUNT(LCD_WIDTH, LCD_HEIGHT), lcd + HW_LCDIF_TRANSFER_COUNT);
    writel_set(LCDIF_CTRL_RUN, lcd + HW_LCDIF_CTRL);
    lcdif_wait_run_clear();
}

int lcd_ready(void) { return g_lcd_ok; }

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
    int r = 6;
    int yy;
    for (yy = 0; yy < h; yy++) {
        int inset = 0;
        if (yy < r)
            inset = r - 1 - yy;
        else if (yy >= h - r)
            inset = yy - (h - r);
        if (inset < 0)
            inset = 0;
        if (inset > r)
            inset = r;
        lcd_hline(x + inset, y + yy, w - 2 * inset, fill);
    }
    lcd_frame(x + 2, y + 2, w - 4, h - 4, border);
}

#else
static int g_lcd_ok;
void lcd_init(void) { g_lcd_ok = 0; }
void lcd_fill(u16 color) { (void)color; }
void lcd_pixel(int x, int y, u16 color) { (void)x;(void)y;(void)color; }
void lcd_rect(int x, int y, int w, int h, u16 color) { (void)x;(void)y;(void)w;(void)h;(void)color; }
void lcd_hline(int x, int y, int w, u16 color) { (void)x;(void)y;(void)w;(void)color; }
void lcd_vline(int x, int y, int h, u16 color) { (void)x;(void)y;(void)h;(void)color; }
void lcd_char(int x, int y, char c, u16 fg, u16 bg) { (void)x;(void)y;(void)c;(void)fg;(void)bg; }
void lcd_text(int x, int y, const char *s, u16 fg, u16 bg) { (void)x;(void)y;(void)s;(void)fg;(void)bg; }
void lcd_char_scaled(int x, int y, char c, u16 fg, u16 bg, int scale) { (void)x;(void)y;(void)c;(void)fg;(void)bg;(void)scale; }
void lcd_text_scaled(int x, int y, const char *s, u16 fg, u16 bg, int scale) { (void)x;(void)y;(void)s;(void)fg;(void)bg;(void)scale; }
void lcd_frame(int x, int y, int w, int h, u16 color) { (void)x;(void)y;(void)w;(void)h;(void)color; }
void lcd_rrect(int x, int y, int w, int h, u16 fill, u16 border) { (void)x;(void)y;(void)w;(void)h;(void)fill;(void)border; }
void lcd_flush(void) {}
int lcd_ready(void) { return 0; }
#endif
