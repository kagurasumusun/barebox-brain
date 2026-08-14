/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef FAT_H
#define FAT_H

#include "mmc.h"

struct fat_fs {
    struct mmc_dev *dev;
    u32 part_lba;
    u32 fat_lba;
    u32 root_lba;
    u32 data_lba;
    u32 sectors_per_cluster;
    u32 bytes_per_sector;
    u32 root_entries;
    u32 fat_size;
    u32 cluster_count;
    u32 root_cluster;   /* FAT32 */
    u32 reserved;
    u8  nfats;
    int fat32;
    int ready;
};

struct fat_file {
    u32 first_cluster;
    u32 size;
    u8  attr;
    char name[13];
    u32 dir_lba;
    u32 dir_off;
};

int fat_mount(struct fat_fs *fs, struct mmc_dev *dev, u32 hint_lba);
int fat_find(struct fat_fs *fs, const char *name83, struct fat_file *out);
int fat_read(struct fat_fs *fs, const struct fat_file *f, void *buf, u32 maxlen);
int fat_write(struct fat_fs *fs, const char *name83, const void *buf, u32 len);
int fat_list(struct fat_fs *fs, void (*cb)(const struct fat_file *f, void *ctx), void *ctx);

/* MBR helpers used by both FAT and the boot menu */
struct mbr_part {
    u8  boot;
    u8  type;
    u32 start_lba;
    u32 size_lba;
    u8  start_s; /* CHS sector field (low 6 bits) */
    u8  end_s;
};

int mbr_parse(const u8 sector[512], struct mbr_part parts[4]);
int mbr_chs_ok(const struct mbr_part *p);

#endif
