/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EBOOT_H
#define EBOOT_H

#include "ui.h"

/*
 * Stock EBOOT path from the type-0x53 SB LOAD at 0x40060000 (159744 B).
 * Record layouts and serial strings come from that binary + the real eMMC.
 */
#define RESUME_INFO_PHYS        0x400e8400u

struct resume_info {
    u32 uiChecksum;
    u32 uiChecksumRev;
    u32 uiStrSize;
};

void eboot_setup_pixclock(void);
int  eboot_check_resume(struct resume_info *out);
int  eboot_store_bootcfg(struct mmc_dev *emmc, u32 flags, u32 devflags, u32 model);
int  eboot_reset_default_bootcfg(struct mmc_dev *emmc);
void eboot_load_records(struct boot_ctx *ctx);
void eboot_display_init(struct boot_ctx *ctx);
void eboot_beep(void);
void eboot_announce_mmc(const struct boot_ctx *ctx);

#endif
