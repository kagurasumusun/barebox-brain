/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "boot.h"
#include "board.h"

int bootcfg_verify_bytes(const u8 *p)
{
    u16 s, ns;
    if (memcmp(p, BOOTCFG_MAGIC, 30) != 0)
        return -1;
    s = (u16)p[0x2c] | ((u16)p[0x2d] << 8);
    ns = (u16)p[0x2e] | ((u16)p[0x2f] << 8);
    /* EBOOT / on-disk DevInfo: sum16(bytes[0..0x2F]) includes the
     * checksum fields. With nsum = ~sum this equals sum(0..0x2B)+0x1FE. */
    if (s != sum16(p, 0x30))
        return -2;
    if (ns != (u16)((0xffffu - s) & 0xffffu))
        return -3;
    return 0;
}

int bootcfg_parse(const u8 sec[512], struct boot_config *out)
{
    memset(out, 0, sizeof(*out));
    if (bootcfg_verify_bytes(sec) != 0)
        return -1;
    memcpy(out->magic, sec, 32);
    out->flags    = (u32)sec[0x20] | ((u32)sec[0x21] << 8) |
                    ((u32)sec[0x22] << 16) | ((u32)sec[0x23] << 24);
    out->devflags = (u32)sec[0x24] | ((u32)sec[0x25] << 8) |
                    ((u32)sec[0x26] << 16) | ((u32)sec[0x27] << 24);
    out->model    = (u32)sec[0x28] | ((u32)sec[0x29] << 8) |
                    ((u32)sec[0x2a] << 16) | ((u32)sec[0x2b] << 24);
    out->sum16    = (u16)sec[0x2c] | ((u16)sec[0x2d] << 8);
    out->nsum16   = (u16)sec[0x2e] | ((u16)sec[0x2f] << 8);
    out->valid    = 1;
    return 0;
}

int bootcfg_build(u32 flags, u32 devflags, u32 model, u8 sec[512])
{
    u16 s, ns;
    memset(sec, 0, 512);
    memcpy(sec, BOOTCFG_MAGIC, 30);
    sec[0x20] = (u8)flags;
    sec[0x21] = (u8)(flags >> 8);
    sec[0x22] = (u8)(flags >> 16);
    sec[0x23] = (u8)(flags >> 24);
    sec[0x24] = (u8)devflags;
    sec[0x25] = (u8)(devflags >> 8);
    sec[0x26] = (u8)(devflags >> 16);
    sec[0x27] = (u8)(devflags >> 24);
    sec[0x28] = (u8)model;
    sec[0x29] = (u8)(model >> 8);
    sec[0x2a] = (u8)(model >> 16);
    sec[0x2b] = (u8)(model >> 24);
    s = (u16)((sum16(sec, 0x2c) + 0x1feu) & 0xffffu);
    ns = (u16)((0xffffu - s) & 0xffffu);
    sec[0x2c] = (u8)s;
    sec[0x2d] = (u8)(s >> 8);
    sec[0x2e] = (u8)ns;
    sec[0x2f] = (u8)(ns >> 8);
    return 0;
}

int devinfo_parse(const u8 sec[512], struct dev_info *out)
{
    u16 s, ns;
    memset(out, 0, sizeof(*out));
    if (memcmp(sec, DEVINFO_MAGIC, 26) != 0)
        return -1;
    s = (u16)sec[0x2c] | ((u16)sec[0x2d] << 8);
    ns = (u16)sec[0x2e] | ((u16)sec[0x2f] << 8);
    if (s != sum16(sec, 0x30))
        return -2;
    if (ns != (u16)((0xffffu - s) & 0xffffu))
        return -3;
    memcpy(out->magic, sec, 26);
    memcpy(out->model, sec + 0x20, 8);
    out->version = (u32)sec[0x28] | ((u32)sec[0x29] << 8) |
                   ((u32)sec[0x2a] << 16) | ((u32)sec[0x2b] << 24);
    out->sum16 = s;
    out->nsum16 = ns;
    out->valid = 1;
    return 0;
}

static u32 r32le(const u8 *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

int b000ff_parse(const u8 *buf, u32 len, struct b000ff_hdr *hdr, const u8 **payload)
{
    if (len < 27)
        return -1;
    if (memcmp(buf, B000FF_MAGIC, 7) != 0)
        return -2;
    memcpy(hdr->magic, buf, 7);
    hdr->load_addr   = r32le(buf + 7);
    hdr->image_size  = r32le(buf + 11);
    hdr->load_addr2  = r32le(buf + 15);
    hdr->image_size2 = r32le(buf + 19);
    hdr->checksum    = r32le(buf + 23);
    if (hdr->load_addr != hdr->load_addr2 || hdr->image_size != hdr->image_size2)
        return -3;
    if (hdr->image_size > len - 27)
        return -4;
    if (payload)
        *payload = buf + 27;
    return 0;
}

int b000ff_verify(const struct b000ff_hdr *hdr, const u8 *payload)
{
    if (sum32(payload, hdr->image_size) != hdr->checksum)
        return -1;
    return 0;
}

int b000ff_build(u32 load, const u8 *payload, u32 size, u8 *out, u32 *out_len)
{
    u32 csum = sum32(payload, size);
    memcpy(out, B000FF_MAGIC, 7);
    out[7]  = (u8)load; out[8]  = (u8)(load >> 8);
    out[9]  = (u8)(load >> 16); out[10] = (u8)(load >> 24);
    out[11] = (u8)size; out[12] = (u8)(size >> 8);
    out[13] = (u8)(size >> 16); out[14] = (u8)(size >> 24);
    memcpy(out + 15, out + 7, 8);
    out[23] = (u8)csum; out[24] = (u8)(csum >> 8);
    out[25] = (u8)(csum >> 16); out[26] = (u8)(csum >> 24);
    memcpy(out + 27, payload, size);
    *out_len = 27 + size;
    return 0;
}

int ecec_probe(const u8 *buf, u32 len, u32 *load_hint)
{
    u32 sig;
    if (len < 0x4c)
        return -1;
    sig = r32le(buf + 0x40);
    if (sig != ECEC_MAGIC)
        return -2;
    if (load_hint)
        *load_hint = r32le(buf + 0x44);
    return 0;
}

u32 ecec_image_size(const u8 *buf, u32 len, u32 max_size)
{
    u32 hinted, last, i;

    if (ecec_probe(buf, len, NULL) != 0)
        return 0;
    hinted = r32le(buf + 0x48);
    if (hinted < 0x1000u || hinted > max_size)
        hinted = 0x100000u;
    if (hinted > len)
        hinted = len;
    last = hinted;
    for (i = hinted; i < len && i < max_size; i++) {
        if (buf[i])
            last = i + 1;
    }
    return last;
}

int nk_patch_fmd(u8 *image, u32 len)
{
    static const u32 start_def[6] = {
        0x00027800u, 0x00047800u, 0x00092000u,
        0x00047800u, 0x000f6800u, 0x00027000u
    };
    static const u32 size_def[6] = {
        0x00020000u, 0x00000000u, 0x00064800u,
        0x0004a800u, 0xffffffffu, 0x00000000u
    };
    u32 off;

    if (len < 48)
        return -1;
    /* MAIN NK on this eMMC keeps the table at 0x95b03c; repaired image
     * already has EBOOT's runtime patch applied. */
    if (len > 0x95b03cu + 48u && r32le(image + 0x95b03c) == start_def[0]) {
        if (r32le(image + 0x95b03c + 8) == 0x0009200fu &&
            r32le(image + 0x95b03c + 16) == 0x0018c809u)
            return (int)0x95b03cu;
        off = 0x95b03cu;
    } else {
        off = 0;
    }
    for (; off + 48 <= len; off += 4) {
        int i, ok = 1;
        for (i = 0; i < 6; i++) {
            if (r32le(image + off + i * 4) != start_def[i]) {
                ok = 0;
                break;
            }
        }
        if (!ok)
            continue;
        for (i = 0; i < 6; i++) {
            if (r32le(image + off + 0x18 + i * 4) != size_def[i]) {
                ok = 0;
                break;
            }
        }
        if (!ok)
            continue;
        image[off + 8]  = 0x0f;
        image[off + 9]  = 0x20;
        image[off + 10] = 0x09;
        image[off + 11] = 0x00;
        image[off + 16] = 0x09;
        image[off + 17] = 0xc8;
        image[off + 18] = 0x8c;
        image[off + 19] = 0x01;
        image[off + 0x18 + 16] = 0x40;
        image[off + 0x18 + 17] = 0x17;
        image[off + 0x18 + 18] = 0xac;
        image[off + 0x18 + 19] = 0x00;
        return (int)off;
    }
    return -1;
}

int chargeinfo_parse(const u8 sec[512], struct charge_info *out)
{
    memset(out, 0, sizeof(*out));
    if (memcmp(sec, CHARGE_MAGIC, 30) != 0)
        return -1;
    memcpy(out->magic, sec, 32);
    out->start_count[0]      = r32le(sec + 0x20);
    out->complete_count[0]   = r32le(sec + 0x24);
    out->temp_error_count[0] = r32le(sec + 0x28);
    out->total_time[0]       = r32le(sec + 0x2c);
    out->start_count[1]      = r32le(sec + 0x30);
    out->complete_count[1]   = r32le(sec + 0x34);
    out->temp_error_count[1] = r32le(sec + 0x38);
    out->total_time[1]       = r32le(sec + 0x3c);
    out->valid = 1;
    return 0;
}

int bootstatus_parse(const u8 sec[512], struct boot_status *out)
{
    memset(out, 0, sizeof(*out));
    if (memcmp(sec, BOOTSTATUS_MAGIC, 30) != 0)
        return -1;
    memcpy(out->magic, sec, 32);
    out->status = r32le(sec + 0x20);
    out->valid = 1;
    return 0;
}

int factory_setting_ok(const u8 sec[512])
{
    return sec[0] == 0x80;
}

int packed70_setting_ok(const u8 sec[512])
{
    return sec[0] == '7' && sec[1] == '0';
}
