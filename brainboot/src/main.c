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
#include "ebl.h"
#include "eboot.h"

extern int menu_run(struct boot_ctx *ctx);

#ifdef HOST_BUILD
int main(void) { return 0; }
#else

static void execute(struct boot_ctx *ctx, int act)
{
    const char *exe;

    policy_announce_action(ctx, act);
    switch (act) {
    case ACT_WINCE:
        if (lcd_ready())
            ui_message(ctx, "Boot WinCE", "Reading NK from eMMC 0x120000 ...");
        if (boot_wince_from_emmc(&ctx->emmc, EMMC_NK_OFFSET, 0))
            printf("ERROR: Failed to load OS image from NAND.\n");
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
        printf("INFO: DIAG dwActualLength = [0x%x]\n", EMMC_DIAGOS_SIZE);
        if (boot_wince_from_emmc(&ctx->emmc, EMMC_DIAGOS_OFFSET, EMMC_DIAGOS_SIZE))
            printf("ERROR: Failed to load OS image from NAND.\n");
        break;
    case ACT_SDEXE:
        exe = ctx->ident.exe_name[0] ? ctx->ident.exe_name : SD_BOOTEXE_NAME;
        if (lcd_ready())
            ui_message(ctx, "Boot SD EXE", exe);
        if (!ctx->sdfat.ready) {
            printf("ERROR: Failed to load OS image from SD/MMC.\n");
            break;
        }
        printf("INFO: Downloading NK RAM image.\n");
        if (boot_wince_from_sd(&ctx->sdfat, exe) != 0 &&
            (strcmp(exe, SD_BOOTEXE_NAME) == 0 ||
             boot_wince_from_sd(&ctx->sdfat, SD_BOOTEXE_NAME) != 0))
            printf("ERROR: Failed to load OS image from SD/MMC.\n");
        break;
    case ACT_NK2:
        if (lcd_ready())
            ui_message(ctx, "Boot NK2", "Reading NK2 0x2120000 ...");
        if (boot_wince_from_emmc(&ctx->emmc, EMMC_NK2_OFFSET, 0))
            printf("ERROR: Failed to load OS image from NAND.\n");
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
        (void)shell_run(ctx);
        break;
    default:
        printf("unknown action %d\n", act);
        break;
    }
}

int main(void)
{
    struct boot_ctx ctx;
    struct ebl_state es;
    struct resume_info resume;
    struct ebl_power pwr;
    int act;
    int k;

    memset(&ctx, 0, sizeof(ctx));
    ident_clear(&ctx.ident);
    uart_init();
    printf("\nMicrosoft Windows CE Bootloader Common Library Version 1.4 Built Aug  6 2019 14:34:18\n");
    printf("======== brainboot v%s ========\n", BB_VERSION);

    board_early_init();
    ebl_oem_init(&es);
    ebl_print_state(&es);
    ctx.rtc_seconds = es.rtc.seconds;
    ctx.power_sts = es.pwr.sts;
    ctx.edna2_doorbell = es.edna2_doorbell;
    ctx.i2c0_ctrl = es.i2c0_ctrl;
    ctx.cpu_hz = board_cpu_hz();
    ctx.ocotp_lock = board_ocotp_lock();
    ctx.ocram_ok = board_probe_ocram();
    ctx.dram_ok = board_probe_dram();
    ctx.dram_bytes = ident_dram_bytes();

    ebl_power_read(&pwr);
    if (!pwr.vbus)
        printf("USB is not detected\n");
    else
        printf("POARIC USB Connect\n");

    keyboard_init();
    k = keyboard_poll();
    printf("CheckWakeupKey() = %x\n", k);
    if (k == KEY_ENTER || k == KEY_ESC)
        ctx.key_held = 1;

    ctx.resume = eboot_check_resume(&resume);
    if (ctx.resume)
        printf("***************CheckResumeInfo()=TRUE\n");
    else
        printf("Not Resume !!!\n");

    printf("Init DRIVER_GLOBAL_WORK size=496\n");
    bb_config_defaults(&ctx.cfg);

    printf("OEMPlatformInit\n");
    if (mmc_init(&ctx.emmc, MMC_PORT_EMMC) != 0)
        printf("WARNING: OEMPlatformInit: Failed to initialize SDHC device.\n");
    if (mmc_init(&ctx.sd, MMC_PORT_SD) != 0)
        printf("SDInterface_Init 1 Failed\n");
    eboot_announce_mmc(&ctx);

    if (ctx.sd.ready) {
        if (fat_mount(&ctx.sdfat, &ctx.sd, 0) != 0)
            printf("SDMMCDownload : FATInitDisk Fail\n");
        else
            printf("SD FAT%s mounted\n", ctx.sdfat.fat32 ? "32" : "16");
    }

    eboot_load_records(&ctx);
    eboot_probe_card_exe(&ctx);
    eboot_display_init(&ctx);

    printf("Batt Detect\n");
    printf("Cold Boot OS\n");
    eboot_beep();
    ebl_power_read(&pwr);
    printf("Battery Voltage = %u\n", pwr.batt_mv);
    printf("System ready!\n");

    memset(&ctx.bm, 0, sizeof(ctx.bm));
    if (bootmenu_load(&ctx.sdfat, &ctx.bm) == 0)
        printf("BOOTMENU.BIN flags=0x%x cookie=0x%x\n",
               ctx.bm.flags, ctx.bm.cookie);
    if (bb_config_load(&ctx.sdfat, &ctx.cfg) == 0)
        printf("loaded BRAINBOO.CFG autoboot=%u default=%s menu=%d\n",
               ctx.cfg.autoboot, bb_default_name(ctx.cfg.default_target),
               ctx.cfg.menu_force);

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
        printf("BOOTMENU.BIN / menu=on / key: showing UI\n");
        act = menu_run(&ctx);
        while (act == ACT_SHELL) {
            execute(&ctx, act);
            act = menu_run(&ctx);
        }
        if (act)
            execute(&ctx, act);
    } else {
        act = policy_silent_action(&ctx);
        if (act)
            execute(&ctx, act);
    }

    printf("SpinForever...\n");
    for (;;)
        ;
    return 0;
}
#endif
