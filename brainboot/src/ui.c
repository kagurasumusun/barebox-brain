/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ui.h"
#include "board.h"

void ui_clear(void)
{
    lcd_fill(COL_BG);
}

void ui_status_row(int x, int y, const char *label, int st, const char *tag)
{
    u16 tc = COL_GREEN;
    if (st == ST_FAIL) tc = COL_RED;
    else if (st == ST_SKIP) tc = COL_AMBER;
    else if (st == ST_INFO) tc = COL_DIM;
    lcd_text(x, y, label, COL_FG, COL_BG);
    lcd_text(x + 240, y, tag ? tag : "", tc, COL_BG);
}

void ui_kv(int x, int y, const char *k, const char *v)
{
    lcd_text(x, y, k, COL_DIM, COL_BG);
    lcd_text(x + 136, y, v ? v : "", COL_FG, COL_BG);
}

void ui_menu_item(int x, int y, int w, const char *label, int selected)
{
    u16 bg = selected ? COL_BLUE : COL_BOX;
    u16 fg = selected ? COL_HI_FG : COL_FG;
    lcd_rect(x, y, w, 28, bg);
    if (selected)
        lcd_text(x + 12, y + 10, ">", fg, bg);
    lcd_text(x + 28, y + 10, label, fg, bg);
}

void ui_keycap(int x, int y, const char *cap)
{
    int w = (int)strlen(cap) * 8 + 10;
    lcd_frame(x, y, w, 14, COL_ACCENT);
    lcd_text(x + 5, y + 3, cap, COL_ACCENT, COL_BG);
}

void ui_footer(void)
{
    lcd_hline(20, 448, LCD_WIDTH - 40, COL_LINE);
    ui_keycap(24, 458, "UP");
    ui_keycap(56, 458, "DN");
    lcd_text(92, 461, "Select", COL_DIM, COL_BG);
    ui_keycap(160, 458, "ENTER");
    lcd_text(218, 461, "Confirm", COL_DIM, COL_BG);
    ui_keycap(292, 458, "ESC");
    lcd_text(332, 461, "Back", COL_DIM, COL_BG);
    lcd_text(520, 461, "brainboot  Open Today. Extend Forever.", COL_DIM, COL_BG);
}

static void fmt_storage(const struct boot_ctx *ctx, char *out, u32 n)
{
    if (ctx->emmc.ready) {
        u32 mib = ctx->emmc.capacity_lba / 2048u;
        snprint(out, n, "eMMC (%u MiB)", mib);
    } else {
        strncpy(out, "eMMC (not ready)", n - 1);
        out[n - 1] = 0;
    }
}

void ui_chrome(const struct boot_ctx *ctx, const char *hint_right)
{
    char line[48];
    char stor[32];

    ui_clear();
    lcd_text(24, 14, "SHARP", COL_FG, COL_BG);
    lcd_text(80, 14, "BRAIN", COL_ACCENT, COL_BG);
    lcd_text(128, 14, ident_display(&ctx->ident), COL_FG, COL_BG);
    lcd_text(620, 10, "Embedded Bootloader  v" BB_VERSION, COL_DIM, COL_BG);
    lcd_text(620, 22, "Open Today. Extend Forever.", COL_DIM, COL_BG);
    lcd_hline(20, 36, LCD_WIDTH - 40, COL_LINE);

    lcd_text_scaled(24, 48, "BRAINBOOT", COL_FG, COL_BG, 3);
    {
        char sub[64];
        snprint(sub, sizeof(sub), "Embedded Bootloader for SHARP BRAIN %s",
                ident_display(&ctx->ident));
        lcd_text(28, 80, sub, COL_ACCENT, COL_BG);
    }
    lcd_text(28, 92, "Open Today. Extend Forever.", COL_DIM, COL_BG);

    lcd_text(620, 52, "CPU", COL_DIM, COL_BG);
    lcd_text(668, 52, ": i.MX28 (ARM926EJ-S)", COL_FG, COL_BG);
    lcd_text(620, 66, "RAM", COL_DIM, COL_BG);
    if (ctx->dram_bytes)
        snprint(line, sizeof(line), ": %u MB", ctx->dram_bytes >> 20);
    else
        strncpy(line, ": unknown", sizeof(line) - 1);
    lcd_text(668, 66, line, COL_FG, COL_BG);
    lcd_text(620, 80, "BOOT", COL_DIM, COL_BG);
    fmt_storage(ctx, stor, sizeof(stor));
    snprint(line, sizeof(line), ": %s", stor);
    lcd_text(668, 80, line, COL_FG, COL_BG);
    lcd_text(620, 94, "CHIP", COL_DIM, COL_BG);
    snprint(line, sizeof(line), ": %x", board_chipid());
    lcd_text(668, 94, line, COL_FG, COL_BG);

    lcd_hline(20, 108, LCD_WIDTH - 40, COL_LINE);
    if (hint_right)
        lcd_text(470, 400, hint_right, COL_DIM, COL_BG);
    ui_footer();
}

void ui_home(const struct boot_ctx *ctx, const char **items, int n,
             int sel, int remain, const char *desc)
{
    int i;
    char line[40];
    const char *mode = "Normal";

    ui_chrome(ctx, NULL);

    lcd_rrect(20, 118, 430, 310, COL_BOX, COL_LINE);
    for (i = 0; i < n; i++)
        ui_menu_item(32, 132 + i * 36, 406, items[i], i == sel);

    lcd_text(470, 122, "SYSTEM STATUS", COL_ACCENT, COL_BG);
    ui_status_row(470, 142, "OCRAM probe", ctx->ocram_ok ? ST_OK : ST_FAIL,
                  ctx->ocram_ok ? "[ OK ]" : "[FAIL]");
    ui_status_row(470, 158, "DRAM  probe", ctx->dram_ok ? ST_OK : ST_FAIL,
                  ctx->dram_ok ? "[ OK ]" : "[FAIL]");
    ui_status_row(470, 174, "eMMC interface", ctx->emmc.ready ? ST_OK : ST_FAIL,
                  ctx->emmc.ready ? "[ OK ]" : "[FAIL]");
    ui_status_row(470, 190, "SD / FAT", ctx->sdfat.ready ? ST_OK : ST_SKIP,
                  ctx->sdfat.ready ? "[ OK ]" : "[ -- ]");
    ui_status_row(470, 206, "Boot configuration", ctx->bc.valid ? ST_OK : ST_SKIP,
                  ctx->bc.valid ? "[ OK ]" : "[ -- ]");
    ui_status_row(470, 222, "POWER_STS readable", ctx->power_sts ? ST_OK : ST_INFO,
                  ctx->power_sts ? "[ OK ]" : "[ ?? ]");

    lcd_hline(470, 242, 360, COL_LINE);
    lcd_text(470, 254, "DEVICE INFO", COL_ACCENT, COL_BG);
    {
        char model[40], intern[40];
        snprint(model, sizeof(model), "SHARP BRAIN %s", ident_display(&ctx->ident));
        if (ctx->ident.known)
            snprint(intern, sizeof(intern), "%s / %s",
                    ctx->ident.internal, ctx->ident.gen);
        else
            strncpy(intern, ctx->ident.internal, sizeof(intern) - 1);
        ui_kv(470, 274, "Model", model);
        ui_kv(470, 290, "Internal", intern);
    }
    if (ctx->bc.valid && (ctx->bc.flags & BOOTCFG_FLAG_DIAG))
        mode = "Diag";
    else if (ctx->cfg.default_target == BB_DEFAULT_LINUX)
        mode = "Linux default";
    ui_kv(470, 306, "Boot mode", mode);
    snprint(line, sizeof(line), "LOCK=%x", ctx->ocotp_lock);
    ui_kv(470, 322, "OCOTP", line);
    snprint(line, sizeof(line), "%u MHz (CLKCTRL)", ctx->cpu_hz / 1000000u);
    ui_kv(470, 338, "CPU clk", line);
    ui_kv(470, 354, "Version", "v" BB_VERSION);

    if (remain >= 0) {
        snprint(line, sizeof(line), "Autoboot in %u s", (unsigned)remain);
        lcd_text(470, 380, line, COL_AMBER, COL_BG);
    }
    if (desc)
        lcd_text(470, 400, desc, COL_DIM, COL_BG);
    lcd_flush();
}

void ui_panel(const struct boot_ctx *ctx, const char *title,
              const char **items, int n, int sel, const char *body)
{
    int i;
    ui_chrome(ctx, NULL);
    lcd_text(24, 118, title, COL_ACCENT, COL_BG);
    lcd_rrect(20, 134, 430, 300, COL_BOX, COL_LINE);
    for (i = 0; i < n; i++)
        ui_menu_item(32, 148 + i * 32, 406, items[i], i == sel);
    if (body) {
        int y = 134;
        const char *p = body;
        char line[44];
        int li;
        while (*p && y < 420) {
            li = 0;
            while (*p && *p != '\n' && li < 43)
                line[li++] = *p++;
            line[li] = 0;
            if (*p == '\n')
                p++;
            lcd_text(470, y, line, COL_FG, COL_BG);
            y += 12;
        }
    }
    lcd_flush();
}

void ui_message(const struct boot_ctx *ctx, const char *title, const char *body)
{
    ui_chrome(ctx, NULL);
    lcd_text(24, 122, title, COL_ACCENT, COL_BG);
    if (body) {
        int y = 148;
        const char *p = body;
        char line[96];
        int li;
        while (*p && y < 430) {
            li = 0;
            while (*p && *p != '\n' && li < 95)
                line[li++] = *p++;
            line[li] = 0;
            if (*p == '\n')
                p++;
            lcd_text(24, y, line, COL_FG, COL_BG);
            y += 12;
        }
    }
    lcd_flush();
}
