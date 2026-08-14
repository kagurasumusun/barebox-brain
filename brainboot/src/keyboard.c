/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "board.h"
#include "gpio.h"

/* Matrix from imx28-pwsh6.dts */
static const u32 kbd_in[KBD_IN_BANKS] = {
    GPIO(4, 0), GPIO(4, 1), GPIO(4, 2), GPIO(4, 3),
    GPIO(4, 4), GPIO(4, 5), GPIO(4, 6), GPIO(4, 7),
};
static const u32 kbd_out[KBD_OUT_BANKS] = {
    GPIO(2, 16), GPIO(2, 17), GPIO(2, 18), GPIO(2, 19),
    GPIO(2, 20), GPIO(2, 21), GPIO(4, 8),
};

/* Menu-relevant keys (out, in) */
#define KEY_NONE    0
#define KEY_UP      1
#define KEY_DOWN    2
#define KEY_ENTER   3
#define KEY_ESC     4
#define KEY_LEFT    5
#define KEY_RIGHT   6

void keyboard_init(void)
{
    int i;
    for (i = 0; i < KBD_IN_BANKS; i++)
        gpio_direction_input(kbd_in[i]);
    for (i = 0; i < KBD_OUT_BANKS; i++)
        gpio_direction_output(kbd_out[i], 0);
}

static int scan_cell(int out, int in)
{
    int i, v;
    for (i = 0; i < KBD_OUT_BANKS; i++)
        gpio_set_value(kbd_out[i], 0);
    gpio_set_value(kbd_out[out], 1);
    udelay(20);
    v = gpio_get_value(kbd_in[in]);
    gpio_set_value(kbd_out[out], 0);
    return v;
}

int keyboard_poll(void)
{
    /* Mapping from pwsh6.dts:
     * Up    = out 3, in 3
     * Down  = out 1, in 3
     * Enter = out 5, in 3
     * Esc   = out 3, in 6  (Home)
     * Left  = out 2, in 3
     * Right = out 4, in 3
     */
    if (scan_cell(3, 3)) return KEY_UP;
    if (scan_cell(1, 3)) return KEY_DOWN;
    if (scan_cell(5, 3)) return KEY_ENTER;
    if (scan_cell(3, 6)) return KEY_ESC;
    if (scan_cell(2, 3)) return KEY_LEFT;
    if (scan_cell(4, 3)) return KEY_RIGHT;
    return KEY_NONE;
}

int keyboard_get_timeout(u32 ms)
{
    while (ms--) {
        int k = keyboard_poll();
        if (k)
            return k;
        if (uart_tstc()) {
            int c = uart_getc();
            if (c == 'w' || c == 'W' || c == 'k' || c == 0x10) return KEY_UP;
            if (c == 's' || c == 'S' || c == 'j' || c == 0x0e) return KEY_DOWN;
            if (c == '\r' || c == '\n' || c == ' ') return KEY_ENTER;
            if (c == 0x1b || c == 'q' || c == 'Q') return KEY_ESC;
            if (c == 'a' || c == 'A' || c == 'h') return KEY_LEFT;
            if (c == 'd' || c == 'D' || c == 'l') return KEY_RIGHT;
            if (c >= '1' && c <= '9')
                return 100 + (c - '0'); /* direct index */
        }
        mdelay(1);
    }
    return KEY_NONE;
}
