/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "brainboot.h"

/*
 * Wiring recovered from keybd_EDNA2.dll (qemu-brain hw/input/brain_kbd.c):
 *   columns (driven LOW): GPIO4 pins 0,1,2,3,4,6,7
 *   rows    (read,  LOW = pressed, idle pulled high):
 *            GPIO2 pins 16..21  (rows 0..5)
 *            GPIO4 pin  8       (row 6)
 *
 * Linux imx28-pwsh6.dts has the banks swapped and ACTIVE_HIGH. That
 * mapping does not match the WinCE BSP or qemu-brain. We scan like EDNA2.
 */

#define KEY_NONE    0
#define KEY_UP      1
#define KEY_DOWN    2
#define KEY_ENTER   3
#define KEY_ESC     4
#define KEY_LEFT    5
#define KEY_RIGHT   6
#define KEY_SHELL   7

#define KBD_COLS    7
#define KBD_ROWS    7

/* Brain keys (not Linux KEY_* numbers). 0 = empty cell. */
enum brain_key {
    BK_NONE = 0,
    BK_V, BK_H, BK_Y, BK_BACK, BK_C, BK_D, BK_HOME,
    BK_M, BK_K, BK_HISTORY, BK_DOWN, BK_PGUP, BK_F, BK_E,
    BK_B, BK_J, BK_U, BK_LEFT, BK_SYMBOL, BK_X,
    BK_N, BK_I, BK_MYDICT, BK_UP, BK_PGDN, BK_Q, BK_ESC,
    BK_BS, BK_G, BK_T, BK_RIGHT, BK_SHIFT, BK_A,
    BK_P, BK_O, BK_ICHIRAN, BK_ENTER, BK_Z, BK_W, BK_KOKUGO,
    BK_MINUS, BK_L, BK_R, BK_SPACE, BK_ONSEI, BK_S, BK_EIWA,
};

struct kbd_event {
    int  col;
    int  row;
    u8   edna2_sc;          /* byte from keybd_EDNA2.dll table */
    enum brain_key key;
    int  shift;
    int  symbol;
};

void keyboard_init(void);
/* Full 7x7 scan. bits[col] bit row set if pressed. */
void keyboard_scan(u8 bits[KBD_COLS]);
int  keyboard_first(struct kbd_event *ev);
int  keyboard_poll(void);
int  keyboard_get_timeout(u32 ms);
const char *brain_key_name(enum brain_key k);
enum brain_key keyboard_lookup(int col, int row);
u8   keyboard_edna2_sc(int col, int row);

#endif
