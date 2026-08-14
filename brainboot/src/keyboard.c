/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "board.h"
#include "gpio.h"
#include "keyboard.h"

/*
 * keybd_EDNA2.dll 7x7 table (qemu-brain brain_kbd.c). Index [col][row].
 * Each byte is the BSP's own scancode, not a Linux keycode.
 */
static const u8 edna2_sc[KBD_COLS][KBD_ROWS] = {
    { 0x16, 0x08, 0x19, 0x25, 0x03, 0x04, 0x00 },
    { 0x0d, 0x0b, 0x21, 0x27, 0x2a, 0x06, 0x05 },
    { 0x02, 0x0a, 0x15, 0x28, 0x31, 0x18, 0x00 },
    { 0x0e, 0x09, 0x2e, 0x26, 0x2b, 0x11, 0x1d },
    { 0x2c, 0x07, 0x14, 0x29, 0x2d, 0x01, 0x00 },
    { 0x10, 0x0f, 0x22, 0x24, 0x1a, 0x17, 0x1e },
    { 0x1b, 0x0c, 0x12, 0x2f, 0x30, 0x13, 0x1f },
};

/*
 * Semantic map: same electrical intersections as imx28-pwsh6.dts
 * after swapping the DTS in/out banks to EDNA2 col/row.
 * DTS <out in KEY>  =>  col = in (GPIO4 index, skip pin 5), row = out.
 */
static const enum brain_key semantic[KBD_COLS][KBD_ROWS] = {
    /* col0 GPIO4_0 : DTS in=0 */
    { BK_V, BK_M, BK_B, BK_N, BK_BS, BK_P, BK_MINUS },
    /* col1 GPIO4_1 : DTS in=1 */
    { BK_H, BK_K, BK_J, BK_I, BK_G, BK_O, BK_L },
    /* col2 GPIO4_2 : DTS in=2 */
    { BK_Y, BK_HISTORY, BK_U, BK_MYDICT, BK_T, BK_ICHIRAN, BK_R },
    /* col3 GPIO4_3 : DTS in=3  (arrows + enter + space) */
    { BK_NONE, BK_DOWN, BK_LEFT, BK_UP, BK_RIGHT, BK_ENTER, BK_SPACE },
    /* col4 GPIO4_4 : DTS in=4 */
    { BK_C, BK_PGUP, BK_SYMBOL, BK_NONE, BK_SHIFT, BK_Z, BK_ONSEI },
    /* col5 GPIO4_6 : DTS in=6 */
    { BK_HOME, BK_E, BK_NONE, BK_ESC, BK_NONE, BK_KOKUGO, BK_EIWA },
    /* col6 GPIO4_7 : DTS has no in=7 entries */
    { BK_NONE, BK_NONE, BK_NONE, BK_NONE, BK_NONE, BK_NONE, BK_NONE },
};

static const u32 col_gpio[KBD_COLS] = {
    GPIO(4, 0), GPIO(4, 1), GPIO(4, 2), GPIO(4, 3),
    GPIO(4, 4), GPIO(4, 6), GPIO(4, 7),
};

static const u32 row_gpio[KBD_ROWS] = {
    GPIO(2, 16), GPIO(2, 17), GPIO(2, 18), GPIO(2, 19),
    GPIO(2, 20), GPIO(2, 21), GPIO(4, 8),
};

static const char *const key_names[] = {
    [BK_NONE] = "-",
    [BK_V] = "V", [BK_H] = "H", [BK_Y] = "Y", [BK_BACK] = "Back",
    [BK_C] = "C", [BK_D] = "D", [BK_HOME] = "Home",
    [BK_M] = "M", [BK_K] = "K", [BK_HISTORY] = "Hist", [BK_DOWN] = "Down",
    [BK_PGUP] = "PgUp", [BK_F] = "F", [BK_E] = "E",
    [BK_B] = "B", [BK_J] = "J", [BK_U] = "U", [BK_LEFT] = "Left",
    [BK_SYMBOL] = "Sym", [BK_X] = "X",
    [BK_N] = "N", [BK_I] = "I", [BK_MYDICT] = "MyDic", [BK_UP] = "Up",
    [BK_PGDN] = "PgDn", [BK_Q] = "Q", [BK_ESC] = "Home/Esc",
    [BK_BS] = "BS", [BK_G] = "G", [BK_T] = "T", [BK_RIGHT] = "Right",
    [BK_SHIFT] = "Shift", [BK_A] = "A",
    [BK_P] = "P", [BK_O] = "O", [BK_ICHIRAN] = "List", [BK_ENTER] = "Enter",
    [BK_Z] = "Z", [BK_W] = "W", [BK_KOKUGO] = "Kokugo",
    [BK_MINUS] = "-", [BK_L] = "L", [BK_R] = "R", [BK_SPACE] = "Space",
    [BK_ONSEI] = "Onsei", [BK_S] = "S", [BK_EIWA] = "Eiwa",
};

const char *brain_key_name(enum brain_key k)
{
    if ((unsigned)k >= ARRAY_SIZE(key_names) || !key_names[k])
        return "?";
    return key_names[k];
}

enum brain_key keyboard_lookup(int col, int row)
{
    if (col < 0 || col >= KBD_COLS || row < 0 || row >= KBD_ROWS)
        return BK_NONE;
    return semantic[col][row];
}

u8 keyboard_edna2_sc(int col, int row)
{
    if (col < 0 || col >= KBD_COLS || row < 0 || row >= KBD_ROWS)
        return 0;
    return edna2_sc[col][row];
}

#ifndef HOST_BUILD

static void pull_set_row(u32 gpio)
{
    u32 bank = GPIO_BANK(gpio);
    u32 pin = GPIO_PIN(gpio);
    u32 addr = IMX_PINCTRL_BASE + HW_PINCTRL_PULL0 + bank * 0x10u;
    writel_set(1u << pin, addr);
}

void keyboard_init(void)
{
    int i;
    /* Rows: GPIO input, pull-up (idle high). */
    for (i = 0; i < KBD_ROWS; i++) {
        gpio_direction_input(row_gpio[i]);
        pull_set_row(row_gpio[i]);
    }
    /* Columns: GPIO output, idle HIGH (not selected). */
    for (i = 0; i < KBD_COLS; i++)
        gpio_direction_output(col_gpio[i], 1);
}

void keyboard_scan(u8 bits[KBD_COLS])
{
    int c, r, i;

    memset(bits, 0, KBD_COLS);
    for (c = 0; c < KBD_COLS; c++) {
        for (i = 0; i < KBD_COLS; i++)
            gpio_set_value(col_gpio[i], 1);
        gpio_set_value(col_gpio[c], 0);
        udelay(30);
        for (r = 0; r < KBD_ROWS; r++) {
            /* LOW = pressed (EDNA2 / qemu-brain). */
            if (!gpio_get_value(row_gpio[r]))
                bits[c] |= (u8)(1u << r);
        }
        gpio_set_value(col_gpio[c], 1);
    }
}

#else
void pull_set_row(u32 gpio) { (void)gpio; }
void keyboard_init(void) {}
void keyboard_scan(u8 bits[KBD_COLS]) { memset(bits, 0, KBD_COLS); }
#endif

int keyboard_first(struct kbd_event *ev)
{
    u8 bits[KBD_COLS];
    int c, r;

    memset(ev, 0, sizeof(*ev));
    keyboard_scan(bits);
    ev->shift = 0;
    ev->symbol = 0;
    for (c = 0; c < KBD_COLS; c++) {
        for (r = 0; r < KBD_ROWS; r++) {
            if (!(bits[c] & (1u << r)))
                continue;
            if (semantic[c][r] == BK_SHIFT)
                ev->shift = 1;
            else if (semantic[c][r] == BK_SYMBOL)
                ev->symbol = 1;
        }
    }
    for (c = 0; c < KBD_COLS; c++) {
        for (r = 0; r < KBD_ROWS; r++) {
            enum brain_key k;
            if (!(bits[c] & (1u << r)))
                continue;
            k = semantic[c][r];
            if (k == BK_NONE || k == BK_SHIFT || k == BK_SYMBOL)
                continue;
            ev->col = c;
            ev->row = r;
            ev->edna2_sc = edna2_sc[c][r];
            ev->key = k;
            return 1;
        }
    }
    return 0;
}

static int brain_to_menu(enum brain_key k)
{
    switch (k) {
    case BK_UP:    return KEY_UP;
    case BK_DOWN:  return KEY_DOWN;
    case BK_ENTER: return KEY_ENTER;
    case BK_ESC:
    case BK_HOME:  return KEY_ESC;
    case BK_LEFT:  return KEY_LEFT;
    case BK_RIGHT: return KEY_RIGHT;
    default:       return KEY_NONE;
    }
}

int keyboard_poll(void)
{
    struct kbd_event ev;
    if (keyboard_first(&ev))
        return brain_to_menu(ev.key);
    return KEY_NONE;
}

int keyboard_get_timeout(u32 ms)
{
    while (ms--) {
        int k = keyboard_poll();
        if (k)
            return k;
#ifndef HOST_BUILD
        if (uart_tstc()) {
            int c = uart_getc();
            if (c == 'w' || c == 'W' || c == 'k' || c == 0x10) return KEY_UP;
            if (c == 's' || c == 'S' || c == 'j' || c == 0x0e) return KEY_DOWN;
            if (c == '\r' || c == '\n' || c == ' ') return KEY_ENTER;
            if (c == 0x1b || c == 'q' || c == 'Q') return KEY_ESC;
            if (c == 'a' || c == 'A' || c == 'h') return KEY_LEFT;
            if (c == 'd' || c == 'D' || c == 'l') return KEY_RIGHT;
            if (c == 'c' || c == 'C' || c == '`')
                return KEY_SHELL;
            if (c >= '1' && c <= '9')
                return 100 + (c - '0');
        }
        mdelay(1);
#endif
    }
    return KEY_NONE;
}
