/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef BOOT_H
#define BOOT_H

#include "mmc.h"
#include "fat.h"

#define BOOTCFG_MAGIC   "SHARP E-DICTIONARY BOOT CONFIG"
#define DEVINFO_MAGIC   "SHARP E-DICTIONARY DEV INFO"

struct boot_config {
    char magic[32];
    u32  flags;
    u32  devflags;
    u32  model;
    u16  sum16;
    u16  nsum16;
    int  valid;
};

struct dev_info {
    char magic[26];
    char pad0[6];
    char model[8];
    u32  version;
    u16  sum16;
    u16  nsum16;
    int  valid;
};

/* B000FF on-disk header (27 bytes) followed by payload. */
struct b000ff_hdr {
    char magic[7];
    u32  load_addr;
    u32  image_size;
    u32  load_addr2;
    u32  image_size2;
    u32  checksum;
} __attribute__((packed));

int  bootcfg_parse(const u8 sec[512], struct boot_config *out);
int  bootcfg_build(u32 flags, u32 devflags, u32 model, u8 sec[512]);
int  bootcfg_verify_bytes(const u8 *p);

int  devinfo_parse(const u8 sec[512], struct dev_info *out);

int  b000ff_parse(const u8 *buf, u32 len, struct b000ff_hdr *hdr, const u8 **payload);
int  b000ff_verify(const struct b000ff_hdr *hdr, const u8 *payload);
int  b000ff_build(u32 load, const u8 *payload, u32 size, u8 *out, u32 *out_len);

int  ecec_probe(const u8 *buf, u32 len, u32 *load_hint);

int  boot_wince_from_emmc(struct mmc_dev *emmc, u32 offset, u32 size_hint);
int  boot_wince_from_mem(const u8 *buf, u32 len);
int  boot_wince_from_sd(struct fat_fs *sd, const char *name);

int  boot_linux_from_sd(struct fat_fs *sd, const char *zname, const char *dtbname);
int  linux_prepare_atags(u32 atag_phys, const char *cmdline, u32 mem_base, u32 mem_size);
int  zimage_probe(const u8 *buf, u32 len);
int  dtb_probe(const u8 *buf, u32 len);

void jump_os(u32 r0, u32 r1, u32 r2, u32 entry);

#endif
