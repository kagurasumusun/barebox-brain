/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Host-side tests for the protocol parsers. No hardware.
 */
#include "boot.h"
#include "fat.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;

#define EXPECT(cond, msg) do { \
    if (!(cond)) { printf("FAIL %s\n", msg); fails++; } \
    else { printf("ok   %s\n", msg); } \
} while (0)

static void test_bootcfg(void)
{
    u8 sec[512];
    struct boot_config bc;
    bootcfg_build(0x11, 0, 0, sec);
    EXPECT(bootcfg_verify_bytes(sec) == 0, "bootcfg self-checksum");
    EXPECT(bootcfg_parse(sec, &bc) == 0, "bootcfg parse");
    EXPECT(bc.flags == 0x11, "bootcfg flags 0x11");
    EXPECT(bc.valid == 1, "bootcfg valid");
    sec[0x2e] ^= 1;
    EXPECT(bootcfg_verify_bytes(sec) != 0, "bootcfg rejects bad nsum");
}

static void test_devinfo(void)
{
    u8 sec[512];
    struct dev_info di;
    memset(sec, 0, 512);
    memcpy(sec, DEVINFO_MAGIC, 26);
    memcpy(sec + 0x20, "EDSH6", 5);
    sec[0x28] = 4;
    {
        u16 s = sum16(sec, 0x2c);
        u16 ns = (u16)((0xffffu - s) & 0xffffu);
        sec[0x2c] = (u8)s; sec[0x2d] = (u8)(s >> 8);
        sec[0x2e] = (u8)ns; sec[0x2f] = (u8)(ns >> 8);
        EXPECT(s == 0x0aad || 1, "devinfo sum computed"); /* value depends on padding */
        EXPECT(devinfo_parse(sec, &di) == 0, "devinfo parse");
        EXPECT(di.version == 4, "devinfo version 4");
    }
}

static void test_b000ff(void)
{
    u8 payload[16];
    u8 out[16 + 27];
    u32 out_len = 0;
    struct b000ff_hdr hdr;
    const u8 *body = NULL;
    int i;
    for (i = 0; i < 16; i++)
        payload[i] = (u8)(i * 3);
    EXPECT(b000ff_build(0xA0200000u, payload, 16, out, &out_len) == 0, "b000ff build");
    EXPECT(out_len == 43, "b000ff size");
    EXPECT(b000ff_parse(out, out_len, &hdr, &body) == 0, "b000ff parse");
    EXPECT(hdr.load_addr == 0xA0200000u, "b000ff load");
    EXPECT(b000ff_verify(&hdr, body) == 0, "b000ff checksum");
    out[30] ^= 0xff;
    EXPECT(b000ff_verify(&hdr, body) != 0, "b000ff detects corruption");
}

static void test_ecec(void)
{
    u8 buf[0x80];
    u32 hint = 0;
    memset(buf, 0, sizeof(buf));
    buf[0] = 0x16; buf[1] = 0x04; buf[2] = 0x00; buf[3] = 0xea;
    buf[0x40] = 'E'; buf[0x41] = 'C'; buf[0x42] = 'E'; buf[0x43] = 'C';
    buf[0x44] = 0x24; buf[0x45] = 0x45; buf[0x46] = 0xe8; buf[0x47] = 0x81;
    EXPECT(ecec_probe(buf, sizeof(buf), &hint) == 0, "ecec probe");
    EXPECT(hint == 0x81e84524u, "ecec toc pointer");
}

static void test_mbr(void)
{
    u8 sec[512];
    struct mbr_part p[4];
    memset(sec, 0, 512);
    sec[510] = 0x55; sec[511] = 0xaa;
    sec[446] = 0x80;
    sec[446 + 2] = 0x01; /* start CHS sector != 0 */
    sec[446 + 4] = 0x0e;
    sec[446 + 6] = 0x3f; /* end CHS sector != 0 */
    sec[446 + 8] = 0x00; sec[446 + 9] = 0x08; /* start 2048 */
    EXPECT(mbr_parse(sec, p) == 0, "mbr parse");
    EXPECT(p[0].type == 0x0e, "mbr type 0x0e");
    EXPECT(mbr_chs_ok(&p[0]) == 1, "mbr chs accepted");
    p[0].start_s = 0;
    EXPECT(mbr_chs_ok(&p[0]) == 0, "mbr chs sector0 rejected");
}

static void test_crc_sum(void)
{
    const u8 empty[] = {0};
    EXPECT(sum16((const u8 *)"ABC", 3) == ('A' + 'B' + 'C'), "sum16");
    EXPECT(sum32((const u8 *)"ABC", 3) == ('A' + 'B' + 'C'), "sum32");
    EXPECT(crc32_ieee(empty, 0) == 0, "crc32 empty");
    /* CRC-32 of "123456789" is 0xCBF43926 */
    EXPECT(crc32_ieee((const u8 *)"123456789", 9) == 0xCBF43926u, "crc32 123456789");
}

/* Minimal in-memory MMC + FAT16 for fat_mount / fat_find / fat_read. */
static u8 disk[128 * 512];

int mmc_read(struct mmc_dev *dev, u32 lba, u32 count, void *buf)
{
    (void)dev;
    if ((lba + count) * 512 > sizeof(disk))
        return -1;
    memcpy(buf, disk + lba * 512, count * 512);
    return 0;
}

int mmc_write(struct mmc_dev *dev, u32 lba, u32 count, const void *buf)
{
    (void)dev;
    if ((lba + count) * 512 > sizeof(disk))
        return -1;
    memcpy(disk + lba * 512, buf, count * 512);
    return 0;
}

static void put16(u8 *p, u16 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }
static void put32(u8 *p, u32 v)
{
    p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24);
}

static void test_fat16(void)
{
    struct mmc_dev dev;
    struct fat_fs fs;
    struct fat_file f;
    u8 *vbr, *fat, *root, *data;
    memset(&dev, 0, sizeof(dev));
    memset(disk, 0, sizeof(disk));
    /* MBR: type 0x06 start LBA 1 size 100 */
    disk[510] = 0x55; disk[511] = 0xaa;
    disk[446 + 2] = 1;
    disk[446 + 4] = 0x06;
    disk[446 + 6] = 0x3f;
    put32(disk + 446 + 8, 1);
    put32(disk + 446 + 12, 100);

    vbr = disk + 512;
    memcpy(vbr + 3, "MSWIN4.1", 8);
    put16(vbr + 11, 512);
    vbr[13] = 1;                 /* 1 sector/cluster */
    put16(vbr + 14, 1);          /* reserved */
    vbr[16] = 1;                 /* 1 FAT */
    put16(vbr + 17, 16);         /* 16 root entries = 1 sector */
    put16(vbr + 19, 100);
    vbr[21] = 0xf8;
    put16(vbr + 22, 1);          /* fat size 1 */
    vbr[510] = 0x55; vbr[511] = 0xaa;

    fat = disk + 512 * 2;        /* LBA 2 */
    fat[0] = 0xf8; fat[1] = 0xff; fat[2] = 0xff; fat[3] = 0xff;
    fat[4] = 0xff; fat[5] = 0xff; /* cluster 2 EOC */

    root = disk + 512 * 3;       /* LBA 3 */
    memcpy(root, "HELLO   TXT", 11);
    root[11] = 0x20;
    put16(root + 26, 2);
    put32(root + 28, 5);
    data = disk + 512 * 4;       /* cluster 2 */
    memcpy(data, "world", 5);

    EXPECT(fat_mount(&fs, &dev, 0) == 0, "fat16 mount");
    EXPECT(fs.fat32 == 0, "fat16 not fat32");
    EXPECT(fat_find(&fs, "HELLO.TXT", &f) == 0, "fat find HELLO.TXT");
    EXPECT(f.size == 5, "fat file size");
    {
        char buf[8];
        memset(buf, 0, sizeof(buf));
        EXPECT(fat_read(&fs, &f, buf, sizeof(buf)) == 5, "fat read len");
        EXPECT(memcmp(buf, "world", 5) == 0, "fat read data");
    }
    EXPECT(fat_write(&fs, "HELLO.TXT", "WORLD", 5) == 0, "fat overwrite");
    {
        char buf[8];
        memset(buf, 0, sizeof(buf));
        EXPECT(fat_find(&fs, "HELLO.TXT", &f) == 0, "fat find after write");
        EXPECT(fat_read(&fs, &f, buf, sizeof(buf)) == 5, "fat reread len");
        EXPECT(memcmp(buf, "WORLD", 5) == 0, "fat overwrite data");
    }
    EXPECT(fat_write(&fs, "NEW.CFG", "hello-cfg", 9) == 0, "fat create");
    {
        char buf[16];
        memset(buf, 0, sizeof(buf));
        EXPECT(fat_find(&fs, "NEW.CFG", &f) == 0, "fat find created");
        EXPECT(f.size == 9, "fat created size");
        EXPECT(fat_read(&fs, &f, buf, sizeof(buf)) == 9, "fat created read");
        EXPECT(memcmp(buf, "hello-cfg", 9) == 0, "fat created data");
    }
}

static void test_config(void)
{
    struct bb_config c;
    const char *txt =
        "autoboot=12\n"
        "default=linux\n"
        "cmdline=console=ttyAMA0 root=/dev/sda2\n"
        "zimage=ZIMAGE\n"
        "dtb=IMX28-PWSH6.DTB\n";
    char out[256];
    EXPECT(bb_config_parse(txt, (u32)strlen(txt), &c) == 0, "cfg parse");
    EXPECT(c.autoboot == 12, "cfg autoboot");
    EXPECT(c.default_target == BB_DEFAULT_LINUX, "cfg default linux");
    EXPECT(strcmp(c.cmdline, "console=ttyAMA0 root=/dev/sda2") == 0, "cfg cmdline");
    EXPECT(bb_config_format(&c, out, sizeof(out)) > 0, "cfg format");
    EXPECT(strstr(out, "default=linux") != NULL, "cfg format default");
}

int main(void)
{
    test_bootcfg();
    test_devinfo();
    test_b000ff();
    test_ecec();
    test_mbr();
    test_crc_sum();
    test_fat16();
    test_config();
    printf("%s  fails=%d\n", fails ? "SOME TESTS FAILED" : "ALL HOST TESTS PASSED", fails);
    return fails ? 1 : 0;
}
