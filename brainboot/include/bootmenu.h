/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef BOOTMENU_H
#define BOOTMENU_H

#include "brainboot.h"
#include "fat.h"

/*
 * BOOTMENU.BIN — 512-byte record, same ones-complement checksum as
 * BootConfig / DevInfo.
 *
 * No BOOTMENU.BIN (or bootmenu.bin) exists in the kagurasumusun trees
 * or in EBL.bin. This layout is defined here so the UI can be gated
 * on a flag + checksum the same way stock EBOOT gates DiagBoot. Magic
 * is parallel to the other SHARP E-DICTIONARY records.
 *
 *   0x00  "SHARP E-DICTIONARY BOOT MENU" (28 bytes) + 4 zero pad
 *   0x20  flags
 *   0x24  reserved
 *   0x28  cookie (optional extra check value; 0 = unused)
 *   0x2C  sum16(bytes[0..0x2B])
 *   0x2E  nsum16 = (0xFFFF - sum16) & 0xFFFF
 */

#define BOOTMENU_MAGIC      "SHARP E-DICTIONARY BOOT MENU"
#define BOOTMENU_MAGIC_LEN  28
#define BOOTMENU_NAME       "BOOTMENU.BIN"

#define BOOTMENU_FLAG_SHOW      0x01u  /* draw the graphical boot UI */
#define BOOTMENU_FLAG_VERBOSE   0x02u  /* extra serial */
#define BOOTMENU_FLAG_SHELL     0x04u  /* drop to command line, no UI */

struct boot_menu {
    char magic[32];
    u32  flags;
    u32  reserved;
    u32  cookie;
    u16  sum16;
    u16  nsum16;
    int  valid;
};

int  bootmenu_verify_bytes(const u8 *p);
int  bootmenu_parse(const u8 sec[512], struct boot_menu *out);
int  bootmenu_build(u32 flags, u32 cookie, u8 sec[512]);
int  bootmenu_load(struct fat_fs *sd, struct boot_menu *out);

#endif
