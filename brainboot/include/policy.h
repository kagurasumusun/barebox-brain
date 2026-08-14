/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef POLICY_H
#define POLICY_H

#include "ui.h"

/* Same numeric actions as menu.c */
#define ACT_WINCE     1
#define ACT_LINUX     2
#define ACT_DIAGOS    3
#define ACT_SDEXE     4
#define ACT_NK2       5
#define ACT_POWEROFF  6
#define ACT_REBOOT    7
#define ACT_SHELL     8
#define ACT_NONE      0

/*
 * Stock EBOOT decision (BOOT.md), plus the BOOTMENU.BIN gate.
 *
 * Menu is shown only when at least one of:
 *   - BOOTMENU.BIN is present, checksums, and FLAG_SHOW is set
 *   - BootConfig Engineer (0x10) is set
 *   - a menu key is held (Enter / Esc / UART char)
 *   - BRAINBOO.CFG has menu=on
 *
 * Otherwise the path is silent: DiagBoot → DiagOS, else NK, with the
 * documented serial strings. Linux is silent only if BRAINBOO.CFG
 * default=linux (not a stock EBOOT path).
 */

int  policy_want_menu(const struct boot_ctx *ctx);
int  policy_want_shell(const struct boot_ctx *ctx);
int  policy_silent_action(const struct boot_ctx *ctx);
void policy_announce_devinfo(const struct boot_ctx *ctx);
void policy_announce_bootcfg(const struct boot_ctx *ctx, int from_sd, int ok);
void policy_announce_action(int act);

#endif
