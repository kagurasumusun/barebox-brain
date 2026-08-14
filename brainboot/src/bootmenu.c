/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "bootmenu.h"
#include "board.h"

int bootmenu_verify_bytes(const u8 *p)
{
    u16 s, ns;
    if (memcmp(p, BOOTMENU_MAGIC, BOOTMENU_MAGIC_LEN) != 0)
        return -1;
    s = (u16)p[0x2c] | ((u16)p[0x2d] << 8);
    ns = (u16)p[0x2e] | ((u16)p[0x2f] << 8);
    if (s != sum16(p, 0x30))
        return -2;
    if (ns != (u16)((0xffffu - s) & 0xffffu))
        return -3;
    return 0;
}

int bootmenu_parse(const u8 sec[512], struct boot_menu *out)
{
    memset(out, 0, sizeof(*out));
    if (bootmenu_verify_bytes(sec) != 0)
        return -1;
    memcpy(out->magic, sec, 32);
    out->flags    = (u32)sec[0x20] | ((u32)sec[0x21] << 8) |
                    ((u32)sec[0x22] << 16) | ((u32)sec[0x23] << 24);
    out->reserved = (u32)sec[0x24] | ((u32)sec[0x25] << 8) |
                    ((u32)sec[0x26] << 16) | ((u32)sec[0x27] << 24);
    out->cookie   = (u32)sec[0x28] | ((u32)sec[0x29] << 8) |
                    ((u32)sec[0x2a] << 16) | ((u32)sec[0x2b] << 24);
    out->sum16    = (u16)sec[0x2c] | ((u16)sec[0x2d] << 8);
    out->nsum16   = (u16)sec[0x2e] | ((u16)sec[0x2f] << 8);
    out->valid    = 1;
    return 0;
}

int bootmenu_build(u32 flags, u32 cookie, u8 sec[512])
{
    u16 s, ns;
    memset(sec, 0, 512);
    memcpy(sec, BOOTMENU_MAGIC, BOOTMENU_MAGIC_LEN);
    sec[0x20] = (u8)flags;
    sec[0x21] = (u8)(flags >> 8);
    sec[0x22] = (u8)(flags >> 16);
    sec[0x23] = (u8)(flags >> 24);
    sec[0x28] = (u8)cookie;
    sec[0x29] = (u8)(cookie >> 8);
    sec[0x2a] = (u8)(cookie >> 16);
    sec[0x2b] = (u8)(cookie >> 24);
    s = (u16)((sum16(sec, 0x2c) + 0x1feu) & 0xffffu);
    ns = (u16)((0xffffu - s) & 0xffffu);
    sec[0x2c] = (u8)s;
    sec[0x2d] = (u8)(s >> 8);
    sec[0x2e] = (u8)ns;
    sec[0x2f] = (u8)(ns >> 8);
    return 0;
}

int bootmenu_load(struct fat_fs *sd, struct boot_menu *out)
{
    struct fat_file f;
    u8 sec[512];
    int n;

    memset(out, 0, sizeof(*out));
    if (!sd || !sd->ready)
        return -1;
    if (fat_find(sd, BOOTMENU_NAME, &f))
        return -2;
    n = fat_read(sd, &f, sec, 512);
    if (n < 48)
        return -3;
    return bootmenu_parse(sec, out);
}
