/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "board.h"
#include "boot.h"
#include "ui.h"
#include "keyboard.h"
#include "config.h"

extern int menu_run(struct boot_ctx *ctx);

#ifdef HOST_BUILD
int main(void) { return 0; }
#else

static void load_bootcfg(struct boot_ctx *ctx)
{
    u8 sec[512];
    struct fat_file f;

    memset(&ctx->bc, 0, sizeof(ctx->bc));
    if (ctx->sdfat.ready && fat_find(&ctx->sdfat, SD_BOOTCFG_NAME, &f) == 0) {
        if (fat_read(&ctx->sdfat, &f, sec, 512) >= 48 &&
            bootcfg_parse(sec, &ctx->bc) == 0) {
            printf("BootConfig from SD flags=0x%x\n", ctx->bc.flags);
            return;
        }
        printf("SD BootConfig present but invalid\n");
    }
    if (ctx->emmc.ready && mmc_read(&ctx->emmc, EMMC_BOOTCFG_LBA, 1, sec) == 0) {
        if (bootcfg_parse(sec, &ctx->bc) == 0) {
            printf("BootConfig from eMMC sec2 flags=0x%x\n", ctx->bc.flags);
            return;
        }
    }
    printf("BootConfig: none\n");
}

static void load_devinfo(struct boot_ctx *ctx)
{
    u8 sec[512];
    memset(&ctx->di, 0, sizeof(ctx->di));
    if (ctx->emmc.ready && mmc_read(&ctx->emmc, EMMC_DEVINFO_LBA, 1, sec) == 0) {
        if (devinfo_parse(sec, &ctx->di) == 0) {
            printf("DevInfo OK model='%s' ver=%u\n", ctx->di.model, ctx->di.version);
            return;
        }
    }
    printf("DevInfo: missing or bad checksum\n");
}

int main(void)
{
    struct boot_ctx ctx;
    int act;

    memset(&ctx, 0, sizeof(ctx));
    uart_init();
    printf("\n\n======== brainboot %s v%s ========\n", BOARD_NAME, BB_VERSION);
    printf("internal %s / %s\n", BOARD_INTERNAL_AA, BOARD_INTERNAL_SH);

    board_early_init();
    ctx.cpu_hz = board_cpu_hz();
    ctx.ocotp_lock = board_ocotp_lock();
    ctx.rtc_seconds = board_rtc_seconds();
    ctx.power_sts = board_power_sts();
    ctx.ocram_ok = board_probe_ocram();
    ctx.dram_ok = board_probe_dram();
    printf("chipid=0x%x cpu=%u Hz ocram=%d dram=%d lock=0x%x pwr=0x%x\n",
           board_chipid(), ctx.cpu_hz, ctx.ocram_ok, ctx.dram_ok,
           ctx.ocotp_lock, ctx.power_sts);

    keyboard_init();
    lcd_init();
    bb_config_defaults(&ctx.cfg);
    ui_message(&ctx, "brainboot", "Probing eMMC / SD ...");

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
    if (bb_config_load(&ctx.sdfat, &ctx.cfg) == 0)
        printf("loaded BRAINBOO.CFG autoboot=%u default=%s\n",
               ctx.cfg.autoboot, bb_default_name(ctx.cfg.default_target));

    act = menu_run(&ctx);

    switch (act) {
    case 1:
        ui_message(&ctx, "Boot WinCE", "Reading NK from eMMC 0x120000 ...");
        if (boot_wince_from_emmc(&ctx.emmc, EMMC_NK_OFFSET, 0x01000000u))
            printf("WinCE boot failed\n");
        break;
    case 2:
        ui_message(&ctx, "Boot Linux", "Loading zImage from SD ...");
        if (!ctx.sdfat.ready ||
            boot_linux_from_sd(&ctx.sdfat, ctx.cfg.zimage, ctx.cfg.dtb, ctx.cfg.cmdline))
            printf("Linux boot failed\n");
        break;
    case 3:
        ui_message(&ctx, "Boot DiagOS", "Reading DiagOS 0x4120000 ...");
        if (boot_wince_from_emmc(&ctx.emmc, EMMC_DIAGOS_OFFSET, EMMC_DIAGOS_SIZE))
            printf("DiagOS boot failed\n");
        break;
    case 4:
        ui_message(&ctx, "Boot SD EXE", "Loading EDSH6EXE.BIN ...");
        if (!ctx.sdfat.ready || boot_wince_from_sd(&ctx.sdfat, SD_BOOTEXE_NAME))
            printf("SD EXE boot failed\n");
        break;
    case 5:
        ui_message(&ctx, "Boot NK2", "Reading NK2 0x2120000 ...");
        if (boot_wince_from_emmc(&ctx.emmc, EMMC_NK2_OFFSET, 0x01000000u))
            printf("NK2 boot failed\n");
        break;
    case 6:
        ui_message(&ctx, "Power Off", "Asserting POWER_RESET.PWD");
        mdelay(200);
        board_poweroff();
        break;
    case 7:
        board_reboot();
        break;
    default:
        printf("unknown action %d\n", act);
        break;
    }

    ui_message(&ctx, "Boot returned", "Image jump came back. Hanging.");
    printf("returned from boot path, hanging.\n");
    for (;;)
        ;
    return 0;
}
#endif
