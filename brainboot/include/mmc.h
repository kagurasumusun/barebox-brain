/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef MMC_H
#define MMC_H

#include "brainboot.h"

#define MMC_BLOCK_SIZE  512u

enum mmc_port {
    MMC_PORT_EMMC = 0,  /* SSP0, 8-bit */
    MMC_PORT_SD   = 1,  /* SSP1, 4-bit */
};

struct mmc_cid {
    u8  mid;
    u16 oid;
    char pnm[7];
    u8  prv;
    u32 psn;
    u8  mdt_month;
    u16 mdt_year;
};

struct mmc_dev {
    enum mmc_port port;
    u32 base;
    u32 rca;
    u32 ocr;
    u32 capacity_lba;
    int is_sd;
    int is_hc;
    int bus_width;      /* 1, 4 or 8 */
    int ready;
    struct mmc_cid cid;
    u32 csd[4];
    u8  ext_csd_rev;
};

int  mmc_init(struct mmc_dev *dev, enum mmc_port port);
int  mmc_read(struct mmc_dev *dev, u32 lba, u32 count, void *buf);
int  mmc_write(struct mmc_dev *dev, u32 lba, u32 count, const void *buf);
int  mmc_read_extcsd(struct mmc_dev *dev, u8 ext[512]);
void mmc_print_info(const struct mmc_dev *dev);

#endif
