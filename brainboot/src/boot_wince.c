/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "boot.h"
#include "board.h"

#ifndef HOST_BUILD
extern void copy_and_jump(u32 src, u32 dst, u32 size, u32 entry);
#endif

/*
 * Stock firmware copies the NK image to VA 0xA0200000 = PA 0x40200000
 * and branches there. We are ourselves linked at 0x40200000 when started
 * as a B000FF payload, so the image is first staged at LINUX_ZIMAGE_PHYS
 * (0x42000000) and a trampoline in high DRAM overwrites us and jumps.
 */

static u32 detect_payload(const u8 *buf, u32 len, const u8 **body, u32 *body_len)
{
    struct b000ff_hdr hdr;
    const u8 *pay = NULL;
    u32 hint;

    if (b000ff_parse(buf, len, &hdr, &pay) == 0) {
        if (b000ff_verify(&hdr, pay) != 0) {
            printf("B000FF checksum mismatch (hdr=%x calc=%x)\n",
                   hdr.checksum, sum32(pay, hdr.image_size));
            return 0;
        }
        *body = pay;
        *body_len = hdr.image_size;
        printf("B000FF payload %u bytes load=0x%x\n", hdr.image_size, hdr.load_addr);
        return hdr.load_addr ? hdr.load_addr : NK_LOAD_PHYS;
    }
    if (ecec_probe(buf, len, &hint) == 0) {
        u32 actual = ecec_image_size(buf, len, len);
        *body = buf;
        *body_len = actual ? actual : len;
        printf("ECEC image %u bytes toc=0x%x\n", *body_len, hint);
        return NK_LOAD_PHYS;
    }
    /* Raw image: accept if it starts with an ARM branch (ea......) */
    if (len > 16 && (buf[3] == 0xea || buf[3] == 0xe5)) {
        *body = buf;
        *body_len = len;
        printf("raw ARM image %u bytes\n", len);
        return NK_LOAD_PHYS;
    }
    printf("unrecognised WinCE image (len=%u)\n", len);
    return 0;
}

#ifdef HOST_BUILD
int boot_wince_from_mem(const u8 *buf, u32 len)
{
    const u8 *body;
    u32 blen, dest;
    dest = detect_payload(buf, len, &body, &blen);
    return dest ? 0 : -1;
}
int boot_wince_from_emmc(struct mmc_dev *emmc, u32 offset, u32 size_hint)
{
    (void)emmc; (void)offset; (void)size_hint;
    return -1;
}
int boot_wince_from_sd(struct fat_fs *sd, const char *name)
{
    (void)sd; (void)name;
    return -1;
}
#else

#define STAGE_PHYS   0x42000000u
#define TRAMP_PHYS   0x47F00000u
#define MAX_NK_SIZE  0x02000000u  /* 32 MiB */

int boot_wince_from_mem(const u8 *buf, u32 len)
{
    const u8 *body;
    u32 blen, dest;

    dest = detect_payload(buf, len, &body, &blen);
    if (!dest)
        return -1;
    if (blen == 0 || blen > MAX_NK_SIZE) {
        printf("NK size %u out of range\n", blen);
        return -2;
    }
    if ((u32)(uintptr_t)body != STAGE_PHYS) {
        printf("relocating %u bytes to stage 0x%x\n", blen, STAGE_PHYS);
        memcpy((void *)STAGE_PHYS, body, blen);
        body = (const u8 *)STAGE_PHYS;
    }
    {
        int patched = nk_patch_fmd((u8 *)body, blen);
        if (patched >= 0)
            printf("FMD region table at 0x%x\n", (u32)patched);
    }
    printf("OEMLaunch called PhysAddress 0x%x.\n", dest);
    printf("Download successful!  Jumping to image at 0x0 (physical 0x%x)...\n",
           dest);
    printf("WinCE jump dest=0x%x size=%u\n", dest, blen);
    copy_and_jump(STAGE_PHYS, dest, ALIGN_UP(blen, 4), dest);
    return -3; /* not reached */
}

int boot_wince_from_emmc(struct mmc_dev *emmc, u32 offset, u32 size_hint)
{
    u32 lba = offset / 512;
    u32 nsec;
    u8 *stage = (u8 *)STAGE_PHYS;
    u8 head[512];
    u32 size = size_hint;

    if (!emmc->ready)
        return -1;
    if (mmc_read(emmc, lba, 1, head))
        return -2;

    if (size == 0) {
        size = 0x01000000u;
        if (ecec_probe(head, 512, NULL) == 0)
            printf("ECEC header at eMMC 0x%x\n", offset);
    }
    if (ecec_probe(head, 512, NULL) != 0 &&
        (head[3] == 0xea || head[3] == 0xe5)) {
        int i, empty = 1;
        for (i = 16; i < 512; i++) {
            if (head[i]) {
                empty = 0;
                break;
            }
        }
        if (empty)
            size = 512;
    }
    if (emmc->capacity_lba && lba < emmc->capacity_lba) {
        u32 maxb = (emmc->capacity_lba - lba) * 512u;
        if (size > maxb)
            size = maxb;
    }
    nsec = (size + 511) / 512;
    printf("INFO: Reading NK dwActualLength=[0x%x]\n", size);
    printf("reading NK %u sectors from eMMC LBA %u ...\n", nsec, lba);
    if (mmc_read(emmc, lba, nsec, stage))
        return -3;
    printf("Copy of NK completed 100%%\n");
    return boot_wince_from_mem(stage, size);
}

int boot_wince_from_sd(struct fat_fs *sd, const char *name)
{
    struct fat_file f;
    u8 *stage = (u8 *)STAGE_PHYS;
    int n;

    if (fat_find(sd, name, &f)) {
        printf("SD: %s not found\n", name);
        return -1;
    }
    if (f.size > MAX_NK_SIZE) {
        printf("SD: %s too large (%u)\n", name, f.size);
        return -2;
    }
    printf("SD: loading %s (%u bytes)\n", name, f.size);
    n = fat_read(sd, &f, stage, f.size);
    if (n < 0 || (u32)n != f.size) {
        printf("SD: short read %d\n", n);
        return -3;
    }
    return boot_wince_from_mem(stage, f.size);
}

#endif
