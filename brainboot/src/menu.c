/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ui.h"
#include "board.h"
#include "keyboard.h"
#include "boot.h"
#include "policy.h"
#include "shell.h"
#include "bootmenu.h"

static int wait_key(int aborted)
{
    return keyboard_get_timeout(aborted ? 60000 : 1000);
}

static int pick(int *sel, int n, int k)
{
    if (k == KEY_UP && *sel > 0)
        (*sel)--;
    else if (k == KEY_DOWN && *sel < n - 1)
        (*sel)++;
    else if (k >= 101 && k - 101 < n)
        *sel = k - 101;
    else if (k == KEY_ENTER)
        return 1;
    else if (k == KEY_ESC)
        return -1;
    return 0;
}

static void memtest_run(struct boot_ctx *ctx)
{
    volatile u32 *p = (volatile u32 *)0x43000000u;
    u32 n = 0x02000000u / 4u;
    u32 i, errs = 0;
    char body[256];

    ui_message(ctx, "DRAM walking-bit", "Testing 0x43000000-0x44FFFFFF ...");
    printf("memtest: walking 1s (%u words, stride 17)\n", n);
    for (i = 0; i < n; i += 17)
        p[i] = 1u << (i & 31);
    for (i = 0; i < n; i += 17) {
        u32 pat = 1u << (i & 31);
        if (p[i] != pat)
            errs++;
    }
    snprint(body, sizeof(body),
            "Window : 0x43000000-0x44FFFFFF\nStride : 17 words\nErrors : %u\n\nENTER to return",
            errs);
    ui_message(ctx, errs ? "DRAM test FAILED" : "DRAM test passed", body);
    printf("memtest: %u errors\n", errs);
    while (keyboard_get_timeout(60000) != KEY_ENTER &&
           keyboard_get_timeout(0) != KEY_ESC)
        ;
}

static void show_storage(struct boot_ctx *ctx)
{
    char body[512];
    char a[80], b[80];
    a[0] = b[0] = 0;
    if (ctx->emmc.ready)
        snprint(a, sizeof(a), "eMMC %s mid=%x lba=%u %ubit",
                ctx->emmc.cid.pnm, ctx->emmc.cid.mid,
                ctx->emmc.capacity_lba, (unsigned)ctx->emmc.bus_width);
    else
        strncpy(a, "eMMC not ready", sizeof(a) - 1);
    if (ctx->sd.ready)
        snprint(b, sizeof(b), "SD   %s mid=%x lba=%u %ubit",
                ctx->sd.cid.pnm, ctx->sd.cid.mid,
                ctx->sd.capacity_lba, (unsigned)ctx->sd.bus_width);
    else
        strncpy(b, "SD not ready", sizeof(b) - 1);
    snprint(body, sizeof(body),
            "%s\n%s\nFAT %s part_lba=%u\nDevInfo %s ver=%u\nBootConfig flags=0x%x\nOCOTP_LOCK=0x%x\nPOWER_STS=0x%x\nCPU %u Hz\n\nENTER to return",
            a, b,
            ctx->sdfat.ready ? (ctx->sdfat.fat32 ? "32" : "16") : "none",
            ctx->sdfat.part_lba,
            ctx->di.valid ? ctx->di.model : "-",
            ctx->di.valid ? ctx->di.version : 0,
            ctx->bc.valid ? ctx->bc.flags : 0,
            ctx->ocotp_lock, ctx->power_sts, ctx->cpu_hz);
    ui_message(ctx, "Storage / SoC", body);
    while (keyboard_get_timeout(60000) != KEY_ENTER)
        if (keyboard_get_timeout(0) == KEY_ESC)
            break;
}

struct file_acc {
    struct fat_file f[16];
    int n;
};

static void file_cb(const struct fat_file *f, void *ctx)
{
    struct file_acc *a = ctx;
    if (a->n < 16)
        a->f[a->n++] = *f;
}

static int file_browser(struct boot_ctx *ctx)
{
    struct file_acc acc;
    const char *names[16];
    char lines[16][20];
    int sel = 0, i;
    acc.n = 0;
    if (!ctx->sdfat.ready) {
        ui_message(ctx, "File browser", "SD FAT not mounted.\nENTER to return");
        keyboard_get_timeout(60000);
        return ACT_NONE;
    }
    fat_list(&ctx->sdfat, file_cb, &acc);
    if (acc.n == 0) {
        ui_message(ctx, "File browser", "Root is empty.\nENTER to return");
        keyboard_get_timeout(60000);
        return ACT_NONE;
    }
    for (i = 0; i < acc.n; i++) {
        strncpy(lines[i], acc.f[i].name, 19);
        names[i] = lines[i];
    }
    for (;;) {
        char info[80];
        snprint(info, sizeof(info), "%s  %u bytes  ENTER boots if image",
                acc.f[sel].name, acc.f[sel].size);
        ui_panel(ctx, "SD root", names, acc.n, sel, info);
        {
            int k = keyboard_get_timeout(60000);
            int r = pick(&sel, acc.n, k);
            if (r < 0)
                return ACT_NONE;
            if (r > 0)
                return boot_wince_from_sd(&ctx->sdfat, acc.f[sel].name) ?
                       ACT_NONE : ACT_NONE;
        }
    }
}

static void kbd_test(struct boot_ctx *ctx)
{
    char body[400];
    for (;;) {
        struct kbd_event ev;
        u8 bits[KBD_COLS];
        int c, r, n = 0;
        memset(&ev, 0, sizeof(ev));
        keyboard_scan(bits);
        n += snprint(body + n, sizeof(body) - (u32)n, "EDNA2 7x7\n");
        for (c = 0; c < KBD_COLS && n + 24 < (int)sizeof(body); c++) {
            n += snprint(body + n, sizeof(body) - (u32)n, "%d ", c);
            for (r = 0; r < KBD_ROWS; r++)
                n += snprint(body + n, sizeof(body) - (u32)n,
                             "%c", (bits[c] & (1u << r)) ? '#' : '.');
            n += snprint(body + n, sizeof(body) - (u32)n, "\n");
        }
        if (keyboard_first(&ev))
            n += snprint(body + n, sizeof(body) - (u32)n, "%s sc=%x\n",
                         brain_key_name(ev.key), ev.edna2_sc);
        ui_message(ctx, "Keyboard matrix", body);
        if (ev.key == BK_ESC || ev.key == BK_HOME)
            return;
        if (uart_tstc() && uart_getc() == 0x1b)
            return;
        mdelay(80);
    }
}

static int diagnostics(struct boot_ctx *ctx)
{
    const char *items[] = {
        "DRAM walking-bit test",
        "Storage / SoC registers",
        "SD file list / boot file",
        "Keyboard matrix test",
        "barebox command line",
        "Back",
    };
    int sel = 0;
    for (;;) {
        ui_panel(ctx, "Diagnostics", items, 6, sel,
                 "Probes only. No eMMC writes.");
        {
            int k = keyboard_get_timeout(60000);
            int r = pick(&sel, 6, k);
            if (r < 0 || (r > 0 && sel == 5))
                return ACT_NONE;
            if (r > 0) {
                if (sel == 0) memtest_run(ctx);
                else if (sel == 1) show_storage(ctx);
                else if (sel == 2) file_browser(ctx);
                else if (sel == 3) kbd_test(ctx);
                else if (sel == 4) return ACT_SHELL;
            }
        }
    }
}

static int bootcfg_editor(struct boot_ctx *ctx)
{
    const char *items[12];
    char row[8][40];
    int sel = 0, n;

    for (;;) {
        n = 0;
        snprint(row[n], 40, "DiagBoot     %s",
                (ctx->bc.flags & BOOTCFG_FLAG_DIAG) ? "ON" : "off");
        items[n] = row[n]; n++;
        snprint(row[n], 40, "CardBoot     %s",
                (ctx->bc.flags & BOOTCFG_FLAG_CARDBOOT) ? "ON" : "off");
        items[n] = row[n]; n++;
        snprint(row[n], 40, "Engineer     %s",
                (ctx->bc.flags & BOOTCFG_FLAG_ENGINEER) ? "ON" : "off");
        items[n] = row[n]; n++;
        snprint(row[n], 40, "DevMode      %s",
                (ctx->bc.flags & BOOTCFG_FLAG_DEVMODE) ? "ON" : "off");
        items[n] = row[n]; n++;
        snprint(row[n], 40, "Autoboot     %u s", ctx->cfg.autoboot);
        items[n] = row[n]; n++;
        snprint(row[n], 40, "Default      %s", bb_default_name(ctx->cfg.default_target));
        items[n] = row[n]; n++;
        items[n++] = "Save BRAINBOO.CFG to SD";
        items[n++] = "Write EDSH6CFG.BIN to SD";
        items[n++] = "Write BOOTMENU.BIN (show)";
        items[n++] = "Write BOOTMENU.BIN (hide)";
        items[n++] = "Back";

        ui_panel(ctx, "Boot Configuration", items, n, sel,
                 "ENTER toggles / saves.\nLeft/Right change numbers.\nStock checksum rule used.");
        {
            int k = keyboard_get_timeout(60000);
            int r = pick(&sel, n, k);
            if (r < 0 || (r > 0 && sel == n - 1))
                return ACT_NONE;
            if (k == KEY_LEFT || k == KEY_RIGHT) {
                if (sel == 4) {
                    if (k == KEY_LEFT && ctx->cfg.autoboot)
                        ctx->cfg.autoboot--;
                    if (k == KEY_RIGHT && ctx->cfg.autoboot < 30)
                        ctx->cfg.autoboot++;
                } else if (sel == 5) {
                    if (k == KEY_RIGHT)
                        ctx->cfg.default_target = (ctx->cfg.default_target + 1) % 5;
                    else if (ctx->cfg.default_target)
                        ctx->cfg.default_target--;
                    else
                        ctx->cfg.default_target = 4;
                }
            }
            if (r > 0) {
                if (sel == 0) ctx->bc.flags ^= BOOTCFG_FLAG_DIAG;
                else if (sel == 1) ctx->bc.flags ^= BOOTCFG_FLAG_CARDBOOT;
                else if (sel == 2) ctx->bc.flags ^= BOOTCFG_FLAG_ENGINEER;
                else if (sel == 3) ctx->bc.flags ^= BOOTCFG_FLAG_DEVMODE;
                else if (sel == 6) {
                    int rc = bb_config_save(&ctx->sdfat, &ctx->cfg);
                    ui_message(ctx, "Save BRAINBOO.CFG",
                               rc ? "FAILED (SD/FAT write)" : "Wrote BRAINBOO.CFG");
                    printf("save cfg rc=%d\n", rc);
                    keyboard_get_timeout(4000);
                } else if (sel == 7) {
                    u8 sec[512];
                    int rc;
                    bootcfg_build(ctx->bc.flags, ctx->bc.devflags, ctx->bc.model, sec);
                    rc = ctx->sdfat.ready ?
                         fat_write(&ctx->sdfat, SD_BOOTCFG_NAME, sec, 512) : -1;
                    ui_message(ctx, "Write EDSH6CFG.BIN",
                               rc ? "FAILED" : "Wrote EDSH6CFG.BIN (ones-complement sum)");
                    printf("save edsh6cfg rc=%d flags=0x%x\n", rc, ctx->bc.flags);
                    keyboard_get_timeout(4000);
                } else if (sel == 8 || sel == 9) {
                    u8 sec[512];
                    int rc;
                    u32 fl = (sel == 8) ? BOOTMENU_FLAG_SHOW : 0;
                    bootmenu_build(fl, 0, sec);
                    rc = ctx->sdfat.ready ?
                         fat_write(&ctx->sdfat, BOOTMENU_NAME, sec, 512) : -1;
                    if (rc == 0)
                        bootmenu_parse(sec, &ctx->bm);
                    ui_message(ctx, "Write BOOTMENU.BIN",
                               rc ? "FAILED" :
                               (fl ? "Wrote BOOTMENU.BIN SHOW" : "Wrote BOOTMENU.BIN hide"));
                    printf("save bootmenu rc=%d flags=0x%x\n", rc, fl);
                    keyboard_get_timeout(4000);
                }
            }
        }
    }
}

static int recovery(struct boot_ctx *ctx)
{
    const char *items[] = {
        "Boot DiagOS (eMMC 0x4120000)",
        "Boot EDSH6EXE.BIN from SD",
        "Boot NK2 copy (eMMC 0x2120000)",
        "Browse SD and boot a file",
        "Back",
    };
    int sel = 0;
    for (;;) {
        ui_panel(ctx, "Recovery Mode", items, 5, sel,
                 "Does not touch the SB partition.");
        {
            int k = keyboard_get_timeout(60000);
            int r = pick(&sel, 5, k);
            if (r < 0 || (r > 0 && sel == 4))
                return ACT_NONE;
            if (r > 0) {
                if (sel == 0) return ACT_DIAGOS;
                if (sel == 1) return ACT_SDEXE;
                if (sel == 2) return ACT_NK2;
                if (sel == 3) file_browser(ctx);
            }
        }
    }
}

static const char *home_items[] = {
    "Boot WinCE (Internal eMMC)",
    "Boot Linux (SD Card)",
    "Diagnostics",
    "Boot Configuration",
    "Recovery Mode",
    "Power Off",
};

static const char *home_desc[] = {
    "Start Windows CE from internal eMMC.",
    "Load zImage + DTB from microSD.",
    "Memory, storage, keyboard probes.",
    "Flags, autoboot, save config.",
    "DiagOS / SD EXE / NK2 / files.",
    "SoC power-down (PWD).",
};

int menu_run(struct boot_ctx *ctx)
{
    int sel = 0;
    int remain = (int)ctx->cfg.autoboot;
    int aborted = 0;
    int def = ACT_WINCE;

    switch (ctx->cfg.default_target) {
    case BB_DEFAULT_LINUX: def = ACT_LINUX; sel = 1; break;
    case BB_DEFAULT_DIAG:  def = ACT_DIAGOS; break;
    case BB_DEFAULT_SDEXE: def = ACT_SDEXE; break;
    case BB_DEFAULT_NONE:  remain = -1; aborted = 1; break;
    default: def = ACT_WINCE; sel = 0; break;
    }
    if (ctx->bc.valid && (ctx->bc.flags & BOOTCFG_FLAG_DIAG)) {
        def = ACT_DIAGOS;
        sel = 0;
    }

    printf("menu: autoboot=%d default=%s\n", remain, bb_default_name(ctx->cfg.default_target));

    for (;;) {
        ui_home(ctx, home_items, 6, sel, aborted ? -1 : remain, home_desc[sel]);
        {
            int k = wait_key(aborted);
            int r;
            if (!aborted && k == KEY_NONE) {
                remain--;
                if (remain < 0)
                    return def;
                continue;
            }
            aborted = 1;
            remain = -1;
            r = pick(&sel, 6, k);
            if (r > 0) {
                int act = ACT_NONE;
                if (sel == 0) return ACT_WINCE;
                if (sel == 1) return ACT_LINUX;
                if (sel == 2) act = diagnostics(ctx);
                else if (sel == 3) act = bootcfg_editor(ctx);
                else if (sel == 4) act = recovery(ctx);
                else if (sel == 5) return ACT_POWEROFF;
                if (act)
                    return act;
            }
        }
    }
}
