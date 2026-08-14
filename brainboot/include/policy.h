/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef POLICY_H
#define POLICY_H

#include "ui.h"

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
 * Two layers. They must not mix.
 *
 * Layer A — stock EBOOT (silent, always the default):
 *   SD EDSH6CFG.BIN  >  flash LBA2  >  no CFG
 *   DIAG (0x01)      → DiagOS
 *   no CFG           → Check Card BOOT; real OS EXE (≥1 MiB) only
 *   else             → NK
 *   Engineer (0x10) is DiagApp's flag after DiagOS. It is NOT our UI.
 *   DevMode  (0x20) is printed, not a boot target.
 *
 * Layer B — brainboot overlay (opt-in only):
 *   BOOTMENU.BIN SHOW  |  BRAINBOO.CFG menu=on  |  held key / UART
 *   BRAINBOO.CFG default=linux is the UI countdown default only.
 *   It never overrides a silent stock decision.
 */

int  policy_want_menu(const struct boot_ctx *ctx);
int  policy_want_shell(const struct boot_ctx *ctx);
int  policy_stock_action(const struct boot_ctx *ctx);
int  policy_silent_action(const struct boot_ctx *ctx);
void policy_announce_devinfo(const struct boot_ctx *ctx);
void policy_announce_bootcfg(const struct boot_ctx *ctx, int from_sd, int ok);
void policy_announce_action(const struct boot_ctx *ctx, int act);

#endif
