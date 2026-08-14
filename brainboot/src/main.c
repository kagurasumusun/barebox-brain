/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "board.h"
#include "boot.h"
#include "ui.h"
#include "keyboard.h"
#include "config.h"
#include "policy.h"
#include "shell.h"
#include "ident.h"
#include "bootmenu.h"

extern int menu_run(struct boot_ctx *ctx);

#ifdef HOST_BUILD
int main(void) { return 0; }
#else

static int fat_try_find(struct fat_fs *sd, const char *name, struct fat_file *f)
{
    if (!sd || !sd->ready || !name || !name[0])
        return -1;
    return fat_find(sd, name, f);
}

static void load_bootcfg(struct boot_ctx *ctx)
{
    u8 sec[512];
    struct fat_file f;
    int from_sd = 0, ok = 0;

    memset(&ctx->bc, 0, sizeof(ctx->bc));

    if (fat_try_find(&ctx->sdfat, ctx->ident.cfg_name, &f) == 0 ||
        fat_try_find(&ctx->sdfat, SD_BOOTCFG_NAME, &f) == 0) {
        from_sd = 1;
        if (fat_read(&ctx->sdfat, &f, sec, 512) >= 48 &&
            bootcfg_parse(sec, &ctx->bc) == 0)
            ok = 1;
    }
    if (!ok && ctx->emmc.ready &&
        mmc_read(&ctx->emmc, EMMC_BOOTCFG_LBA, 1, sec) == 0) {
        from_sd = 0;
        if (bootcfg_parse(sec, &ctx->bc) == 0)
            ok = 1;
    }
    policy_announce_bootcfg(ctx, from_sd, ok);
}

static void load_devinfo(struct boot_ctx *ctx)
{
    u8 sec[512];
    memset(&ctx->di, 0, sizeof(ctx->di));
    if (ctx->emmc.ready && mmc_read(&ctx->emmc, EMMC_DEVINFO_LBA, 1, sec) == 0)
        devinfo_parse(sec, &ctx->di);
    ident_from_devinfo(&ctx->ident, &ctx->di);
    policy_announce_devinfo(ctx);
    printf("%s\n", ctx->ident.banner);
}

static void load_bootmenu(struct boot_ctx *ctx)
{
    memset(&ctx->bm, 0, sizeof(ctx->bm));
    if (bootmenu_load(&ctx->sdfat, &ctx->bm) == 0)
        printf("BOOTMENU.BIN flags=0x%x cookie=0x%x\n",
               ctx->bm.flags, ctx->bm.cookie);
}

static void ensure_lcd(void)
{
    if (!lcd_ready())
        lcd_init();
}

static int execute(struct boot_ctx *ctx, int act)
{
    const char *exe;

    policy_announce_action(act);
    switch (act) {
    case ACT_WINCE:
        if (lcd_ready())
            ui_message(ctx, "Boot WinCE", "Reading NK from eMMC 0x120000 ...");
        printf("Copying NK image to RAM 0xa0200000\n");
        if (boot_wince_from_emmc(&ctx->emmc, EMMC_NK_OFFSET, 0x01000000u))
            printf("WinCE boot failed\n");
        else
            printf("Jumping to image\n");
        break;
    case ACT_LINUX:
        if (lcd_ready())
            ui_message(ctx, "Boot Linux", "Loading zImage from SD ...");
        if (!ctx->sdfat.ready ||
            boot_linux_from_sd(&ctx->sdfat, ctx->cfg.zimage, ctx->cfg.dtb,
                               ctx->cfg.cmdline))
            printf("Linux boot failed\n");
        break;
    case ACT_DIAGOS:
        if (lcd_ready())
            ui_message(ctx, "Boot DiagOS", "Reading DiagOS 0x4120000 ...");
        printf("DIAG dwActualLength=[0xb9437c]\n");
        printf("Copying NK image to RAM 0xa0200000\n");
        if (boot_wince_from_emmc(&ctx->emmc, EMMC_DIAGOS_OFFSET, EMMC_DIAGOS_SIZE))
            printf("DiagOS boot failed\n");
        else
            printf("Jumping to image\n");
        break;
    case ACT_SDEXE:
        exe = ctx->ident.exe_name[0] ? ctx->ident.exe_name : SD_BOOTEXE_NAME;
        if (lcd_ready())
            ui_message(ctx, "Boot SD EXE", exe);
        if (!ctx->sdfat.ready) {
            printf("SD EXE boot failed\n");
            break;
        }
        if (boot_wince_from_sd(&ctx->sdfat, exe) != 0 &&
            (strcmp(exe, SD_BOOTEXE_NAME) == 0 ||
             boot_wince_from_sd(&ctx->sdfat, SD_BOOTEXE_NAME) != 0))
            printf("SD EXE boot failed\n");
        else
            printf("Jumping to image\n");
        break;
    case ACT_NK2:
        if (lcd_ready())
            ui_message(ctx, "Boot NK2", "Reading NK2 0x2120000 ...");
        printf("Copying NK image to RAM 0xa0200000\n");
        if (boot_wince_from_emmc(&ctx->emmc, EMMC_NK2_OFFSET, 0x01000000u))
            printf("NK2 boot failed\n");
        else
            printf("Jumping to image\n");
        break;
    case ACT_POWEROFF:
        if (lcd_ready())
            ui_message(ctx, "Power Off", "Asserting POWER_RESET.PWD");
        mdelay(200);
        board_poweroff();
        break;
    case ACT_REBOOT:
        board_reboot();
        break;
    case ACT_SHELL:
        return shell_run(ctx);
    default:
        printf("unknown action %d\n", act);
        break;
    }
    return ACT_NONE;
}

int main(void)
{
    struct boot_ctx ctx;
    int act;
    int k;

    memset(&ctx, 0, sizeof(ctx));
    ident_clear(&ctx.ident);
    uart_init();
    printf("\n\n======== brainboot v%s ========\n", BB_VERSION);

    board_early_init();
    ctx.cpu_hz = board_cpu_hz();
    ctx.ocotp_lock = board_ocotp_lock();
    ctx.rtc_seconds = board_rtc_seconds();
    ctx.power_sts = board_power_sts();
    ctx.ocram_ok = board_probe_ocram();
    ctx.dram_ok = board_probe_dram();
    ctx.dram_bytes = ident_dram_bytes();
    ctx.edna2_doorbell = ident_edna2_doorbell();
    ctx.i2c0_ctrl = ident_i2c0_ctrl();
    printf("chipid=0x%x cpu=%u Hz ocram=%d dram=%d dram_bytes=%u\n",
           board_chipid(), ctx.cpu_hz, ctx.ocram_ok, ctx.dram_ok,
           ctx.dram_bytes);
    printf("lock=0x%x pwr=0x%x rtc=%u edna2=0x%x i2c0=0x%x\n",
           ctx.ocotp_lock, ctx.power_sts, ctx.rtc_seconds,
           ctx.edna2_doorbell, ctx.i2c0_ctrl);

    keyboard_init();
    bb_config_defaults(&ctx.cfg);

    if (mmc_init(&ctx.emmc, MMC_PORT_EMMC) != 0)
        printf("eMMC init failed\n");
    if (mmc_init(&ctx.sd, MMC_PORT_SD) != 0)
        printf("SD init failed\n");

    if (ctx.sd.ready) {
        if (fat_mount(&ctx.sdfat, &ctx.sd, 0) != 0)
            printf("SD FAT mount failed\n");
        else
            printf("SD FAT%s mounted\n", ctx.sdfat.fat32 ? "32" : "16");
    }

    load_devinfo(&ctx);
    load_bootcfg(&ctx);
    load_bootmenu(&ctx);
    if (bb_config_load(&ctx.sdfat, &ctx.cfg) == 0)
        printf("loaded BRAINBOO.CFG autoboot=%u default=%s menu=%d\n",
               ctx.cfg.autoboot, bb_default_name(ctx.cfg.default_target),
               ctx.cfg.menu_force);

    k = keyboard_poll();
    if (k == KEY_ENTER || k == KEY_ESC)
        ctx.key_held = 1;
    if (uart_tstc()) {
        (void)uart_getc();
        ctx.key_held = 1;
    }

    if (policy_want_shell(&ctx) && !policy_want_menu(&ctx)) {
        printf("BOOTMENU.BIN: command line\n");
        act = shell_run(&ctx);
        if (act)
            execute(&ctx, act);
    } else if (policy_want_menu(&ctx)) {
        printf("BOOTMENU / Engineer / key: showing UI\n");
        ensure_lcd();
        act = menu_run(&ctx);
        while (act == ACT_SHELL)
            act = execute(&ctx, act);
        if (act)
            execute(&ctx, act);
    } else {
        act = policy_silent_action(&ctx);
        printf("silent boot action=%d\n", act);
        if (act)
            execute(&ctx, act);
    }

    printf("returned from boot path, hanging.\n");
    for (;;)
        ;
    return 0;
}
#endif
