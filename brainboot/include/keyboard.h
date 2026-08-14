/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "brainboot.h"

#define KEY_NONE    0
#define KEY_UP      1
#define KEY_DOWN    2
#define KEY_ENTER   3
#define KEY_ESC     4
#define KEY_LEFT    5
#define KEY_RIGHT   6
#define KEY_SHELL   7

void keyboard_init(void);
int  keyboard_poll(void);
int  keyboard_get_timeout(u32 ms);

#endif
