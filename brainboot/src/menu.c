/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "board.h"
#include "boot.h"
#include "mmc.h"
#include "fat.h"
#include "lcd.h"
#include "keyboard.h"

struct menu_item {
    const char *key;
    const char *title;
    int id;
};

enum {
    ACT_WINCE = 1,
    ACT_DIAGOS,
    ACT_SD_EXE,
    ACT_LINUX,
    ACT_INFO,
    ACT_MEMTEST,
    ACT_REBOOT,
    ACT_SHELL,
};

static const struct menu_item items[] = {
    { "1", "WinCE  (eMMC NK @ 0x120000)",     ACT_WINCE },
    { "2", "DiagOS (eMMC @ 0x4120000)",       ACT_DIAGOS },
    { "3", "SD Card EXE (EDSH6EXE.BIN)",      ACT_SD_EXE },
    { "4", "Linux  (SD zImage + DTB)",        ACT_LINUX },
    { "5", "Hardware / DevInfo / BootConfig", ACT_INFO },
    { "6", "DRAM walking-bit test (safe window)", ACT_MEMTEST },
    { "7", "Reboot",                          ACT_REBOOT },
};

static void draw_menu(int sel, int remain)
{
    unsigned i;
    u16 bg = RGB565(5, 8, 14);
    u16 fg = RGB565(220, 230, 240);
    u16 hi = RGB565(5, 8, 14);
    u16 hb = RGB565(62, 224, 255);
    char line[48];

    lcd_rect(20, 40, LCD_WIDTH - 40, LCD_HEIGHT - 80, RGB565(8, 12, 22));
    lcd_text(32, 52, "BRAINBOOT  " BOARD_NAME, hb, RGB565(8, 12, 22));
    lcd_text(32, 66, "Linux / WinCE loader", RGB565(120, 140, 160), RGB565(8, 12, 22));

    for (i = 0; i < ARRAY_SIZE(items); i++) {
        int y = 96 + (int)i * 18;
        if ((int)i == sel) {
            lcd_rect(28, y - 2, LCD_WIDTH - 56, 16, hb);
            lcd_text(40, y, items[i].title, hi, hb);
        } else {
            lcd_rect(28, y - 2, LCD_WIDTH - 56, 16, RGB565(8, 12, 22));
            lcd_text(40, y, items[i].title, fg, RGB565(8, 12, 22));
        }
    }
    if (remain >= 0) {
        /* simple number print */
        line[0] = 'A'; line[1] = 'u'; line[2] = 't'; line[3] = 'o';
        line[4] = ' '; line[5] = 'i'; line[6] = 'n'; line[7] = ' ';
        line[8] = (char)('0' + remain);
        line[9] = 's'; line[10] = 0;
        lcd_text(32, LCD_HEIGHT - 48, line, RGB565(180, 160, 80), bg);
    } else {
        lcd_rect(32, LCD_HEIGHT - 48, 200, 10, RGB565(8, 12, 22));
    }
    lcd_text(32, LCD_HEIGHT - 34, "Enter/1-7  Up/Down  Q=abort timer",
             RGB565(90, 110, 130), bg);
    lcd_flush();
}

static void memtest_run(void)
{
    /* Safe window: 0x43000000 .. 0x44FFFFFF (32 MiB), away from us and FB. */
    volatile u32 *p = (volatile u32 *)0x43000000u;
    u32 n = 0x02000000u / 4u;
    u32 i, errs = 0;
    printf("memtest: walking 1s on 0x43000000-0x44ffffff (%u words)\n", n);
    splash_banner("DRAM test running...");
    for (i = 0; i < n; i += 17) { /* stride to finish in reasonable time */
        u32 pat = 1u << (i & 31);
        p[i] = pat;
    }
    for (i = 0; i < n; i += 17) {
        u32 pat = 1u << (i & 31);
        if (p[i] != pat) {
            if (errs < 8)
                printf("  mismatch @%x wrote %x read %x\n",
                       0x43000000u + i * 4, pat, p[i]);
            errs++;
        }
    }
    printf("memtest: %u errors in sampled words\n", errs);
}

static void show_info(struct mmc_dev *emmc, struct mmc_dev *sd,
                      const struct boot_config *bc, const struct dev_info *di)
{
    printf("---- board ----\n");
    printf("  %s  internal %s / %s  fw %s\n",
           BOARD_NAME, BOARD_INTERNAL_AA, BOARD_INTERNAL_SH, BOARD_VERSION);
    printf("  chipid=0x%x  dram=128MiB @ 0x40000000\n", board_chipid());
    printf("---- eMMC ----\n");
    mmc_print_info(emmc);
    printf("---- SD ----\n");
    mmc_print_info(sd);
    printf("---- DevInfo ----\n");
    if (di->valid)
        printf("  model='%s' ver=%u sum=%x\n", di->model, di->version, di->sum16);
    else
        printf("  invalid / missing\n");
    printf("---- BootConfig ----\n");
    if (bc->valid)
        printf("  flags=0x%x devflags=0x%x model=%u\n",
               bc->flags, bc->devflags, bc->model);
    else
        printf("  invalid / missing\n");
}

int menu_run(struct mmc_dev *emmc, struct mmc_dev *sd,
             struct fat_fs *sdfat,
             struct boot_config *bc, struct dev_info *di)
{
    int sel = 0;
    int remain = AUTOBOOT_SECONDS;
    int aborted = 0;
    int default_act = ACT_WINCE;

    if (bc->valid && (bc->flags & BOOTCFG_FLAG_DIAG))
        default_act = ACT_DIAGOS;
    /* pick default row */
    {
        unsigned i;
        for (i = 0; i < ARRAY_SIZE(items); i++)
            if (items[i].id == default_act)
                sel = (int)i;
    }

    printf("\nbrainboot " BOARD_NAME " — keys: 1-7, w/s, Enter\n");
    draw_menu(sel, remain);

    while (1) {
        int k = keyboard_get_timeout(aborted ? 60000 : 1000);
        if (!aborted && k == KEY_NONE) {
            remain--;
            if (remain < 0)
                return default_act;
            draw_menu(sel, remain);
            printf("autoboot in %d\n", remain);
            continue;
        }
        aborted = 1;
        remain = -1;
        if (k >= 101 && k <= 107)
            return items[k - 101].id;
        if (k == KEY_UP) {
            if (sel > 0) sel--;
            draw_menu(sel, remain);
        } else if (k == KEY_DOWN) {
            if (sel < (int)ARRAY_SIZE(items) - 1) sel++;
            draw_menu(sel, remain);
        } else if (k == KEY_ENTER) {
            int act = items[sel].id;
            if (act == ACT_INFO) {
                show_info(emmc, sd, bc, di);
                continue;
            }
            if (act == ACT_MEMTEST) {
                memtest_run();
                continue;
            }
            return act;
        } else if (k == KEY_ESC) {
            continue;
        }
    }
}
