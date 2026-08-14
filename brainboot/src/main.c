/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "board.h"
#include "boot.h"
#include "mmc.h"
#include "fat.h"
#include "lcd.h"
#include "keyboard.h"
#include "gpio.h"

extern int menu_run(struct mmc_dev *emmc, struct mmc_dev *sd,
                    struct fat_fs *sdfat,
                    struct boot_config *bc, struct dev_info *di);

#ifdef HOST_BUILD
int main(void) { return 0; }
#else

static void load_bootcfg(struct mmc_dev *emmc, struct mmc_dev *sd,
                         struct fat_fs *sdfat, struct boot_config *bc)
{
    u8 sec[512];
    struct fat_file f;

    memset(bc, 0, sizeof(*bc));
    /* SD file wins if present (stock EBOOT order). */
    if (sdfat->ready && fat_find(sdfat, SD_BOOTCFG_NAME, &f) == 0) {
        if (fat_read(sdfat, &f, sec, 512) >= 48 && bootcfg_parse(sec, bc) == 0) {
            printf("BootConfig from SD %s flags=0x%x\n", SD_BOOTCFG_NAME, bc->flags);
            return;
        }
        printf("SD BootConfig present but invalid\n");
    }
    if (emmc->ready && mmc_read(emmc, EMMC_BOOTCFG_LBA, 1, sec) == 0) {
        if (bootcfg_parse(sec, bc) == 0) {
            printf("BootConfig from eMMC sec2 flags=0x%x\n", bc->flags);
            return;
        }
    }
    printf("BootConfig: none\n");
}

static void load_devinfo(struct mmc_dev *emmc, struct dev_info *di)
{
    u8 sec[512];
    memset(di, 0, sizeof(*di));
    if (emmc->ready && mmc_read(emmc, EMMC_DEVINFO_LBA, 1, sec) == 0) {
        if (devinfo_parse(sec, di) == 0) {
            printf("DevInfo OK model='%s' ver=%u\n", di->model, di->version);
            return;
        }
    }
    printf("DevInfo: missing or bad checksum\n");
}

int main(void)
{
    struct mmc_dev emmc, sd;
    struct fat_fs sdfat;
    struct boot_config bc;
    struct dev_info di;
    int act;

    uart_init();
    printf("\n\n======== brainboot " BOARD_NAME " ========\n");
    printf("internal %s / %s  %s\n", BOARD_INTERNAL_AA, BOARD_INTERNAL_SH, BOARD_VERSION);
    printf("linked @ 0x%x  dram 0x%x+0x%x\n",
           TEXT_BASE, DRAM_PHYS_BASE, DRAM_PHYS_SIZE);

    board_early_init();
    printf("chipid=0x%x\n", board_chipid());

    keyboard_init();
    lcd_init();
    splash_draw();
    splash_banner("initialising storage...");

    if (mmc_init(&emmc, MMC_PORT_EMMC) != 0)
        printf("eMMC init failed — WinCE from eMMC unavailable\n");
    if (mmc_init(&sd, MMC_PORT_SD) != 0)
        printf("SD init failed — SD boot unavailable\n");

    memset(&sdfat, 0, sizeof(sdfat));
    if (sd.ready) {
        if (fat_mount(&sdfat, &sd, 0) != 0)
            printf("SD FAT mount failed\n");
        else
            printf("SD FAT%s mounted lba=%u spc=%u\n",
                   sdfat.fat32 ? "32" : "16", sdfat.part_lba,
                   sdfat.sectors_per_cluster);
    }

    load_devinfo(&emmc, &di);
    load_bootcfg(&emmc, &sd, &sdfat, &bc);

    splash_banner("ready — choose OS");
    act = menu_run(&emmc, &sd, &sdfat, &bc, &di);

    switch (act) {
    case 1: /* ACT_WINCE */
        splash_banner("booting WinCE NK...");
        if (boot_wince_from_emmc(&emmc, EMMC_NK_OFFSET, 0x01000000u))
            printf("WinCE boot failed\n");
        break;
    case 2: /* ACT_DIAGOS */
        splash_banner("booting DiagOS...");
        if (boot_wince_from_emmc(&emmc, EMMC_DIAGOS_OFFSET, EMMC_DIAGOS_SIZE))
            printf("DiagOS boot failed\n");
        break;
    case 3: /* ACT_SD_EXE */
        splash_banner("booting SD EXE...");
        if (!sdfat.ready || boot_wince_from_sd(&sdfat, SD_BOOTEXE_NAME))
            printf("SD EXE boot failed\n");
        break;
    case 4: /* ACT_LINUX */
        splash_banner("booting Linux...");
        if (!sdfat.ready || boot_linux_from_sd(&sdfat, SD_ZIMAGE_NAME, SD_DTB_NAME))
            printf("Linux boot failed\n");
        break;
    case 7: /* ACT_REBOOT */
        splash_banner("reboot");
        board_reboot();
        break;
    default:
        printf("unknown action %d\n", act);
        break;
    }

    printf("returned from boot path, hanging.\n");
    for (;;)
        ;
    return 0;
}
#endif
