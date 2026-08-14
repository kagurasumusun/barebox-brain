/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef BOARD_H
#define BOARD_H

#include "imx28.h"

/*
 * SHARP Brain PW-AJ2 / ED-AA2 / ED-SH6 (gen3_6, i.MX28, 128 MiB DRAM).
 *
 * Memory map used by the stock CE firmware on this SoC:
 *   VA 0x80000000 -> PA 0x40000000, 128 MiB  (cached DRAM)
 *   VA 0x88000000 -> PA 0x00000000,   1 MiB  (OCRAM)
 *   VA 0x90000000 -> PA 0x80000000,   1 MiB  (SoC regs)
 *   VA 0x90100000 -> PA 0x60000000,   1 MiB
 *
 * WinCE NK is copied to VA 0xA0200000 = PA 0x40200000 (uncached alias of
 * the same DRAM). This loader is linked at that address so the stock
 * first-stage can start it as a B000FF / NK image.
 */

#define BOARD_NAME              "PW-AJ2"
#define BOARD_INTERNAL_AA       "ED-AA2"
#define BOARD_INTERNAL_SH       "ED-SH6"
#define BOARD_VERSION           "V030"
#define BOARD_BANNER            "SHARP   ED-AA2  V030            ED-SH6"

#define TEXT_BASE               0x40200000u
#define STACK_SIZE              0x00010000u

#define NK_LOAD_PHYS            0x40200000u
#define NK_LOAD_VA_UNCACHED     0xA0200000u
#define LINUX_ZIMAGE_PHYS       0x42000000u
#define LINUX_DTB_PHYS          0x41000000u
#define LINUX_ATAG_PHYS         0x40000100u
#define SCRATCH_PHYS            0x45000000u
#define FRAMEBUFFER_PHYS        0x46000000u

/* eMMC / SD layout used by this device's stock firmware */
#define EMMC_DEVINFO_LBA        4u
#define EMMC_BOOTCFG_LBA        2u
#define EMMC_FACTORY_LBA        16u
#define EMMC_USER_LBA           17u
#define EMMC_DEVELOP_LBA        18u
#define EMMC_SYSTEM_LBA         64u
#define EMMC_CHARGE_LBA         71u
#define EMMC_BOOTSTATUS_LBA     8u
#define EMMC_NK_OFFSET          0x00120000u
#define EMMC_NK_SLOT            0x02000000u
#define EMMC_NK2_OFFSET         0x02120000u
#define EMMC_DIAGOS_OFFSET      0x04120000u
#define EMMC_DIAGOS_SIZE        0x00B9437Cu
#define EMMC_BOOT_PART_TYPE     0x53u

#define SD_BOOTCFG_NAME         "EDSH6CFG.BIN"
#define SD_DEVINFO_NAME         "EDSH6DEV.BIN"     /* 8.3 of EDSH6DEVINFO.BIN */
#define SD_DEVINFO_LONG         "EDSH6DEVINFO.BIN"
#define SD_BOOTEXE_NAME         "EDSH6EXE.BIN"
#define SD_ZIMAGE_NAME          "ZIMAGE"
#define SD_DTB_NAME             "IMX28-PWSH6.DTB"
#define SD_UIMAGE_NAME          "UIMAGE"
#define SD_BBCONF_NAME          "BRAINBOO.CFG"
#define SD_BOOTMENU_NAME        "BOOTMENU.BIN"

#define BB_VERSION              "1.7.0"

#define LCD_WIDTH               854
#define LCD_HEIGHT              480
#define LCD_BPP                 16

/* LCD enable GPIOs from imx28-pwsh6.dts */
#define GPIO_LCD_EN0            GPIO(0, 26)   /* GPMI_ALE */
#define GPIO_LCD_EN1            GPIO(0, 27)   /* GPMI_CLE */
#define GPIO_LCD_EN2            GPIO(4, 16)   /* ENET_CLK */
#define GPIO_SD_POWER           GPIO(2, 21)   /* SSP2_SS2, active low */
#define GPIO_EMMC_POWER         GPIO(3, 28)   /* PWM3 */
#define GPIO_BL_ENABLE          GPIO(3, 5)    /* AUART1_TX */

/* Keyboard matrix (imx28-pwsh6.dts) */
#define KBD_IN_BANKS            8
#define KBD_OUT_BANKS           7

/* BootConfig flags */
#define BOOTCFG_FLAG_DIAG       0x01u
#define BOOTCFG_FLAG_CARDBOOT   0x02u
#define BOOTCFG_FLAG_RESERVED2  0x04u  /* no verified meaning — do not invent */
#define BOOTCFG_FLAG_RESERVED3  0x08u  /* no verified meaning — do not invent */
#define BOOTCFG_FLAG_ENGINEER   0x10u
#define BOOTCFG_FLAG_DEVMODE    0x20u
#define BOOTCFG_FLAG_KNOWN      (BOOTCFG_FLAG_DIAG | BOOTCFG_FLAG_CARDBOOT | \
                                 BOOTCFG_FLAG_ENGINEER | BOOTCFG_FLAG_DEVMODE)

#define B000FF_MAGIC            "B000FF\n"
#define ECEC_MAGIC              0x43454345u
#define ZIMAGE_MAGIC            0x016F2818u
#define FDT_MAGIC               0xD00DFEEDu

#define MACH_TYPE_MX28EVK       2531u

#define AUTOBOOT_SECONDS        5

void board_early_init(void);
void board_reboot(void);
void board_poweroff(void);
void clock_init(void);
void eboot_setup_pixclock(void);
void pinmux_init(void);
u32  board_chipid(void);
u32  board_cpu_hz(void);
u32  board_ocotp_lock(void);
u32  board_rtc_seconds(void);
int  board_probe_ocram(void);
int  board_probe_dram(void);
u32  board_power_sts(void);

#endif
