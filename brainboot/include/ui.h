/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef UI_H
#define UI_H

#include "lcd.h"
#include "mmc.h"
#include "fat.h"
#include "boot.h"
#include "config.h"

#define COL_BG      RGB565(0, 0, 0)
#define COL_FG      RGB565(228, 228, 232)
#define COL_DIM     RGB565(130, 132, 140)
#define COL_BLUE    RGB565(37, 99, 220)
#define COL_ACCENT  RGB565(80, 160, 255)
#define COL_GREEN   RGB565(34, 197, 94)
#define COL_RED     RGB565(239, 68, 68)
#define COL_AMBER   RGB565(245, 158, 11)
#define COL_LINE    RGB565(42, 42, 48)
#define COL_BOX     RGB565(8, 8, 10)
#define COL_HI_FG   RGB565(255, 255, 255)

#define ST_OK    0
#define ST_FAIL  1
#define ST_SKIP  2
#define ST_INFO  3

struct boot_ctx {
    struct mmc_dev emmc;
    struct mmc_dev sd;
    struct fat_fs sdfat;
    struct boot_config bc;
    struct dev_info di;
    struct bb_config cfg;
    int ocram_ok;
    int dram_ok;
    u32 cpu_hz;
    u32 ocotp_lock;
    u32 rtc_seconds;
    u32 power_sts;
};

void ui_clear(void);
void ui_chrome(const struct boot_ctx *ctx, const char *hint_right);
void ui_status_row(int x, int y, const char *label, int st, const char *tag);
void ui_kv(int x, int y, const char *k, const char *v);
void ui_menu_item(int x, int y, int w, const char *label, int selected);
void ui_keycap(int x, int y, const char *cap);
void ui_footer(void);
void ui_home(const struct boot_ctx *ctx, const char **items, int n,
             int sel, int remain, const char *desc);
void ui_panel(const struct boot_ctx *ctx, const char *title,
              const char **items, int n, int sel, const char *body);
void ui_message(const struct boot_ctx *ctx, const char *title, const char *body);

#endif
