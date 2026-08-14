/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Host-side tests for the protocol parsers. No hardware.
 */
#include "board.h"
#include "boot.h"
#include "fat.h"
#include "config.h"
#include "ident.h"
#include "bootmenu.h"
#include "policy.h"
#include "keyboard.h"
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
    /* First 48 bytes of eMMC LBA 4 (emmc-repaired3, measured). */
    static const u8 real[48] = {
        0x53,0x48,0x41,0x52,0x50,0x20,0x45,0x2d,0x44,0x49,0x43,0x54,0x49,0x4f,0x4e,0x41,
        0x52,0x59,0x20,0x44,0x45,0x56,0x20,0x49,0x4e,0x46,0x4f,0x00,0x00,0x00,0x00,0x00,
        0x45,0x44,0x53,0x48,0x36,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0xad,0x0a,0x52,0xf5
    };
    u8 sec[512];
    struct dev_info di;
    memset(sec, 0, 512);
    memcpy(sec, real, 48);
    EXPECT(sum16(sec, 0x30) == 0x0aad, "devinfo on-disk sum16 inclusive");
    EXPECT(devinfo_parse(sec, &di) == 0, "devinfo parse real LBA4");
    EXPECT(di.version == 4, "devinfo version 4");
    EXPECT(memcmp(di.model, "EDSH6", 5) == 0, "devinfo model EDSH6");
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

static void test_charge_and_fmd(void)
{
    u8 sec[512], img[64];
    struct charge_info ci;
    static const u32 start_def[6] = {
        0x00027800u, 0x00047800u, 0x00092000u,
        0x00047800u, 0x000f6800u, 0x00027000u
    };
    static const u32 size_def[6] = {
        0x00020000u, 0x00000000u, 0x00064800u,
        0x0004a800u, 0xffffffffu, 0x00000000u
    };
    int i;

    memset(sec, 0, 512);
    memcpy(sec, CHARGE_MAGIC, 30);
    sec[0x20] = 0xb0; sec[0x21] = 0x01;
    sec[0x24] = 0x04;
    sec[0x2c] = 0x7b; sec[0x2d] = 0xa4; sec[0x2e] = 0x01;
    sec[0x30] = 0x0a;
    sec[0x34] = 0x04;
    sec[0x3c] = 0x5c; sec[0x3d] = 0xda;
    EXPECT(chargeinfo_parse(sec, &ci) == 0, "chargeinfo parse");
    EXPECT(ci.start_count[0] == 432, "charge start[0]");
    EXPECT(ci.total_time[0] == 107643, "charge time[0]");
    EXPECT(ci.start_count[1] == 10, "charge start[1]");
    EXPECT(ci.total_time[1] == 55900, "charge time[1]");

    memset(sec, 0, 512);
    sec[0] = 0x80;
    EXPECT(factory_setting_ok(sec) == 1, "factory flag 0x80");
    factory_setting_build(sec);
    EXPECT(factory_setting_ok(sec) == 1, "factory build flag 0x80");
    {
        static const u8 lba17[12] = {
            0x37,0x30,0x00,0x00,0xff,0xff,0xff,0xff,0x63,0x04,0x9c,0xfb
        };
        static const u8 lba66[16] = {
            0x37,0x30,0x00,0x00,0xbc,0xd0,0x2b,0x05,
            0x30,0xee,0x00,0x00,0x41,0x03,0xbe,0xfc
        };
        struct packed70 rec;
        u8 built[16];
        u32 n = 0;
        u8 tiny[64];
        u8 big[4];
        EXPECT(packed70_parse(lba17, 12, &rec) == 0, "packed70 LBA17");
        EXPECT(rec.value == 0xffffffffu, "packed70 LBA17 value");
        EXPECT(packed70_build(0, built, &n) == 0 && n == 12, "packed70 build");
        EXPECT(packed70_parse(built, n, &rec) == 0, "packed70 build parses");
        EXPECT(packed70_parse(lba66, 16, &rec) == 0, "packed70 LBA66");
        EXPECT(rec.nbytes == 16, "packed70 LBA66 16 bytes");
        EXPECT(rec.value == 0x052bd0bcu, "packed70 LBA66 value");
        memset(tiny, 0, sizeof(tiny));
        tiny[3] = 0xea;
        EXPECT(boot_image_is_os(tiny, 64) == 0, "tiny ARM is not Card BOOT OS");
        memset(tiny, 0, sizeof(tiny));
        tiny[3] = 0xea;
        EXPECT(boot_image_is_os(tiny, CARD_BOOT_MIN_OS) == 1, "1MiB ARM is OS");
        EXPECT(boot_image_is_os(tiny, CARD_BOOT_MIN_OS - 1) == 0, "just-under 1MiB rejected");
        (void)big;
    }

    memset(img, 0, sizeof(img));
    for (i = 0; i < 6; i++) {
        img[i * 4 + 0] = (u8)start_def[i];
        img[i * 4 + 1] = (u8)(start_def[i] >> 8);
        img[i * 4 + 2] = (u8)(start_def[i] >> 16);
        img[i * 4 + 3] = (u8)(start_def[i] >> 24);
        img[0x18 + i * 4 + 0] = (u8)size_def[i];
        img[0x18 + i * 4 + 1] = (u8)(size_def[i] >> 8);
        img[0x18 + i * 4 + 2] = (u8)(size_def[i] >> 16);
        img[0x18 + i * 4 + 3] = (u8)(size_def[i] >> 24);
    }
    EXPECT(nk_patch_fmd(img, sizeof(img)) == 0, "FMD patch default table");
    EXPECT(img[8] == 0x0f && img[10] == 0x09, "FMD start[2]");
    EXPECT(img[16] == 0x09 && img[18] == 0x8c && img[19] == 0x01, "FMD start[4]");
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

static void test_bootmenu(void)
{
    u8 sec[512];
    struct boot_menu bm;
    bootmenu_build(BOOTMENU_FLAG_SHOW, 0xA5A5A5A5u, sec);
    EXPECT(bootmenu_verify_bytes(sec) == 0, "bootmenu self-checksum");
    EXPECT(bootmenu_parse(sec, &bm) == 0, "bootmenu parse");
    EXPECT(bm.flags == BOOTMENU_FLAG_SHOW, "bootmenu SHOW flag");
    EXPECT(bm.cookie == 0xA5A5A5A5u, "bootmenu cookie");
    sec[0x2e] ^= 1;
    EXPECT(bootmenu_verify_bytes(sec) != 0, "bootmenu rejects bad nsum");
    bootmenu_build(0, 0, sec);
    EXPECT(bootmenu_parse(sec, &bm) == 0, "bootmenu hide parses");
    EXPECT((bm.flags & BOOTMENU_FLAG_SHOW) == 0, "bootmenu hide has no SHOW");
}

static void test_ident(void)
{
    struct dev_info di;
    struct device_ident id;
    memset(&di, 0, sizeof(di));
    memcpy(di.model, "EDSH6", 5);
    di.version = 4;
    di.valid = 1;
    ident_from_devinfo(&id, &di);
    EXPECT(id.from_devinfo == 1, "ident from devinfo");
    EXPECT(id.known == 1, "ident EDSH6 known");
    EXPECT(strcmp(id.retail, "PW-SH6") == 0, "ident EDSH6 -> PW-SH6");
    EXPECT(strcmp(id.internal, "ED-SH6") == 0, "ident internal ED-SH6");
    EXPECT(strcmp(id.gen, "gen3_6") == 0, "ident gen3_6");
    EXPECT(strcmp(id.exe_name, "EDSH6EXE.BIN") == 0, "ident exe name");

    memcpy(di.model, "EDAJ2", 5);
    ident_from_devinfo(&id, &di);
    EXPECT(strcmp(id.retail, "PW-AJ2") == 0, "ident EDAJ2 -> PW-AJ2");

    memcpy(di.model, "EDZZ9", 5);
    ident_from_devinfo(&id, &di);
    EXPECT(id.known == 0, "ident unknown model not forced");
    EXPECT(strcmp(id.retail, "UNKNOWN") == 0, "ident unknown retail");

    ident_from_devinfo(&id, NULL);
    EXPECT(strcmp(id.retail, "UNKNOWN") == 0, "ident missing devinfo");
}

static void test_policy(void)
{
    struct boot_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    bb_config_defaults(&ctx.cfg);
    EXPECT(policy_want_menu(&ctx) == 0, "policy default silent");
    EXPECT(policy_silent_action(&ctx) == ACT_WINCE, "policy default NK");

    ctx.bm.valid = 1;
    ctx.bm.flags = BOOTMENU_FLAG_SHOW;
    EXPECT(policy_want_menu(&ctx) == 1, "policy BOOTMENU SHOW");
    ctx.bm.flags = 0;
    EXPECT(policy_want_menu(&ctx) == 0, "policy BOOTMENU hide");

    ctx.bm.valid = 0;
    ctx.bc.valid = 1;
    ctx.bc.flags = BOOTCFG_FLAG_ENGINEER;
    EXPECT(policy_want_menu(&ctx) == 0, "policy Engineer is not our UI");
    EXPECT(policy_silent_action(&ctx) == ACT_WINCE, "policy Engineer still NK");
    ctx.bc.flags = BOOTCFG_FLAG_DIAG;
    EXPECT(policy_want_menu(&ctx) == 0, "policy Diag alone is silent");
    EXPECT(policy_silent_action(&ctx) == ACT_DIAGOS, "policy DiagOS");

    ctx.bc.flags = 0;
    ctx.cfg.default_target = BB_DEFAULT_LINUX;
    EXPECT(policy_silent_action(&ctx) == ACT_WINCE, "policy linux default does not steal silent");
    ctx.cfg.default_target = BB_DEFAULT_WINCE;
    ctx.card_exe_os = 1;
    EXPECT(policy_silent_action(&ctx) == ACT_WINCE, "policy valid CFG ignores EXE");
    ctx.bc.valid = 0;
    EXPECT(policy_silent_action(&ctx) == ACT_SDEXE, "policy no CFG + real EXE");
    ctx.card_exe_os = 0;
    EXPECT(policy_silent_action(&ctx) == ACT_WINCE, "policy no CFG no EXE is NK");

    ctx.bc.valid = 1;
    ctx.key_held = 1;
    EXPECT(policy_want_menu(&ctx) == 1, "policy key held");
    ctx.key_held = 0;
    ctx.cfg.loaded = 1;
    ctx.cfg.menu_force = 1;
    EXPECT(policy_want_menu(&ctx) == 1, "policy cfg menu=on");

    ctx.cfg.menu_force = 0;
    ctx.bm.valid = 1;
    ctx.bm.flags = BOOTMENU_FLAG_SHELL;
    EXPECT(policy_want_shell(&ctx) == 1, "policy SHELL flag");
}

static void test_config_menu(void)
{
    struct bb_config c;
    const char *txt = "autoboot=0\ndefault=wince\nmenu=on\n";
    EXPECT(bb_config_parse(txt, (u32)strlen(txt), &c) == 0, "cfg menu parse");
    EXPECT(c.menu_force == 1, "cfg menu=on");
    EXPECT(c.autoboot == 0, "cfg autoboot 0");
}

static void test_keyboard_map(void)
{
    EXPECT(keyboard_lookup(3, 3) == BK_UP, "EDNA2 col3 row3 is Up");
    EXPECT(keyboard_lookup(3, 1) == BK_DOWN, "EDNA2 col3 row1 is Down");
    EXPECT(keyboard_lookup(3, 5) == BK_ENTER, "EDNA2 col3 row5 is Enter");
    EXPECT(keyboard_lookup(5, 3) == BK_ESC, "EDNA2 col5 row3 is Esc");
    EXPECT(keyboard_lookup(3, 2) == BK_LEFT, "EDNA2 col3 row2 is Left");
    EXPECT(keyboard_lookup(3, 4) == BK_RIGHT, "EDNA2 col3 row4 is Right");
    EXPECT(keyboard_edna2_sc(3, 3) == 0x26, "EDNA2 scancode Up cell");
    EXPECT(keyboard_lookup(4, 4) == BK_SHIFT, "shift cell");
    EXPECT(strcmp(brain_key_name(BK_UP), "Up") == 0, "key name Up");
}

int main(void)
{
    test_bootcfg();
    test_devinfo();
    test_charge_and_fmd();
    test_b000ff();
    test_ecec();
    test_mbr();
    test_crc_sum();
    test_fat16();
    test_config();
    test_bootmenu();
    test_ident();
    test_policy();
    test_config_menu();
    test_keyboard_map();
    printf("%s  fails=%d\n", fails ? "SOME TESTS FAILED" : "ALL HOST TESTS PASSED", fails);
    return fails ? 1 : 0;
}
