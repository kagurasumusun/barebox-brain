/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "boot.h"
#include "board.h"

#define ATAG_NONE       0x00000000u
#define ATAG_CORE       0x54410001u
#define ATAG_MEM        0x54410002u
#define ATAG_CMDLINE    0x54410009u

int zimage_probe(const u8 *buf, u32 len)
{
    u32 magic;
    /* ARM zImage: magic 0x016F2818 at offset 0x24. */
    if (len < 0x30)
        return -1;
    magic = (u32)buf[0x24] | ((u32)buf[0x25] << 8) |
            ((u32)buf[0x26] << 16) | ((u32)buf[0x27] << 24);
    if (magic != ZIMAGE_MAGIC)
        return -2;
    return 0;
}

int dtb_probe(const u8 *buf, u32 len)
{
    u32 magic;
    if (len < 8)
        return -1;
    magic = ((u32)buf[0] << 24) | ((u32)buf[1] << 16) |
            ((u32)buf[2] << 8) | buf[3];
    if (magic != FDT_MAGIC)
        return -2;
    return 0;
}

int linux_prepare_atags(u32 atag_phys, const char *cmdline, u32 mem_base, u32 mem_size)
{
    u32 *p = (u32 *)(uintptr_t)atag_phys;
    u32 clen, nwords;

    /* ATAG_CORE */
    *p++ = 5;
    *p++ = ATAG_CORE;
    *p++ = 0;
    *p++ = 0;
    *p++ = 0;

    /* ATAG_MEM */
    *p++ = 4;
    *p++ = ATAG_MEM;
    *p++ = mem_size;
    *p++ = mem_base;

    if (cmdline && cmdline[0]) {
        clen = (u32)strlen(cmdline) + 1;
        nwords = (clen + 3) / 4;
        *p++ = 2 + nwords;
        *p++ = ATAG_CMDLINE;
        memcpy(p, cmdline, clen);
        p += nwords;
    }

    *p++ = 0;
    *p++ = ATAG_NONE;
    return 0;
}

#ifdef HOST_BUILD
int boot_linux_from_sd(struct fat_fs *sd, const char *zname,
                       const char *dtbname, const char *cmdline)
{
    (void)sd; (void)zname; (void)dtbname; (void)cmdline;
    return -1;
}
#else

int boot_linux_from_sd(struct fat_fs *sd, const char *zname,
                       const char *dtbname, const char *cmdline)
{
    struct fat_file zf, df;
    u8 *zimg = (u8 *)LINUX_ZIMAGE_PHYS;
    u8 *dtb  = (u8 *)LINUX_DTB_PHYS;
    int n, have_dtb = 0;
    char ztry[3][16];
    int i, found = 0;

    strncpy(ztry[0], zname ? zname : "ZIMAGE", 15);
    strncpy(ztry[1], "ZIMAGE", 15);
    strncpy(ztry[2], "UIMAGE", 15);

    for (i = 0; i < 3; i++) {
        if (fat_find(sd, ztry[i], &zf) == 0) {
            found = 1;
            break;
        }
    }
    if (!found) {
        printf("Linux: no zImage/uImage on SD\n");
        return -1;
    }
    printf("Linux: loading %s (%u bytes) -> 0x%x\n",
           zf.name, zf.size, LINUX_ZIMAGE_PHYS);
    n = fat_read(sd, &zf, zimg, zf.size);
    if (n < 0 || (u32)n != zf.size)
        return -2;

    /* Skip uImage header (64 bytes) if present. */
    if (zf.size > 64 && zimg[0] == 0x27 && zimg[1] == 0x05 &&
        zimg[2] == 0x19 && zimg[3] == 0x56) {
        printf("uImage header detected, skipping 64 bytes\n");
        memmove(zimg, zimg + 64, zf.size - 64);
        zf.size -= 64;
    }
    if (zimage_probe(zimg, zf.size) != 0)
        printf("warning: zImage magic not found, booting anyway\n");

    if (dtbname && fat_find(sd, dtbname, &df) == 0) {
        n = fat_read(sd, &df, dtb, df.size);
        if (n > 0 && dtb_probe(dtb, (u32)n) == 0) {
            have_dtb = 1;
            printf("Linux: DTB %s (%d bytes) -> 0x%x\n", df.name, n, LINUX_DTB_PHYS);
        }
    } else if (fat_find(sd, SD_DTB_NAME, &df) == 0) {
        n = fat_read(sd, &df, dtb, df.size);
        if (n > 0 && dtb_probe(dtb, (u32)n) == 0) {
            have_dtb = 1;
            printf("Linux: DTB %s (%d bytes)\n", df.name, n);
        }
    }

    if (!cmdline || !cmdline[0])
        cmdline = "console=ttyAMA0,115200 console=tty1 root=/dev/mmcblk1p2 rw rootwait";
    linux_prepare_atags(LINUX_ATAG_PHYS, cmdline,
                        DRAM_PHYS_BASE, DRAM_PHYS_SIZE);

    printf("Linux: jumping to 0x%x (%s)\n",
           LINUX_ZIMAGE_PHYS, have_dtb ? "DTB" : "ATAG");
    jump_os(0,
            have_dtb ? 0xffffffffu : MACH_TYPE_MX28EVK,
            have_dtb ? LINUX_DTB_PHYS : LINUX_ATAG_PHYS,
            LINUX_ZIMAGE_PHYS);
    return -3;
}
#endif
