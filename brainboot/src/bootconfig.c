/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "boot.h"
#include "board.h"

int bootcfg_describe_flags(u32 flags, char *out, u32 outsz)
{
    char buf[96];
    u32 n = 0;
    u32 unk = flags & ~BOOTCFG_FLAG_KNOWN;

    buf[0] = 0;
    if (flags & BOOTCFG_FLAG_DIAG)
        n += snprint(buf + n, sizeof(buf) - n, "DiagBoot ");
    if (flags & BOOTCFG_FLAG_CARDBOOT)
        n += snprint(buf + n, sizeof(buf) - n, "CardBoot ");
    if (flags & BOOTCFG_FLAG_ENGINEER)
        n += snprint(buf + n, sizeof(buf) - n, "Engineer ");
    if (flags & BOOTCFG_FLAG_DEVMODE)
        n += snprint(buf + n, sizeof(buf) - n, "DevMode ");
    if (unk)
        n += snprint(buf + n, sizeof(buf) - n, "unk=0x%x ", unk);
    if (n == 0)
        snprint(buf, sizeof(buf), "(none)");
    return snprint(out, outsz, "%s", buf);
}

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

int devinfo_build(const char *model, u32 version, u8 sec[512])
{
    u32 i;

    memset(sec, 0, 512);
    memcpy(sec, DEVINFO_MAGIC, 26);
    if (model) {
        for (i = 0; i < 8 && model[i]; i++)
            sec[0x20 + i] = (u8)model[i];
    }
    sec[0x28] = (u8)version;
    sec[0x29] = (u8)(version >> 8);
    sec[0x2a] = (u8)(version >> 16);
    sec[0x2b] = (u8)(version >> 24);
    {
        u16 s = (u16)((sum16(sec, 0x2c) + 0x1feu) & 0xffffu);
        u16 ns = (u16)((0xffffu - s) & 0xffffu);
        sec[0x2c] = (u8)s;
        sec[0x2d] = (u8)(s >> 8);
        sec[0x2e] = (u8)ns;
        sec[0x2f] = (u8)(ns >> 8);
    }
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

static void w16le(u8 *p, u16 v)
{
    p[0] = (u8)v;
    p[1] = (u8)(v >> 8);
}

static void w32le(u8 *p, u32 v)
{
    p[0] = (u8)v;
    p[1] = (u8)(v >> 8);
    p[2] = (u8)(v >> 16);
    p[3] = (u8)(v >> 24);
}

int chargeinfo_build(const struct charge_info *in, u8 sec[512])
{
    memset(sec, 0, 512);
    memcpy(sec, CHARGE_MAGIC, 30);
    w32le(sec + 0x20, in->start_count[0]);
    w32le(sec + 0x24, in->complete_count[0]);
    w32le(sec + 0x28, in->temp_error_count[0]);
    w32le(sec + 0x2c, in->total_time[0]);
    w32le(sec + 0x30, in->start_count[1]);
    w32le(sec + 0x34, in->complete_count[1]);
    w32le(sec + 0x38, in->temp_error_count[1]);
    w32le(sec + 0x3c, in->total_time[1]);
    return 0;
}

int bootstatus_build(u32 status, u8 sec[512])
{
    memset(sec, 0, 512);
    memcpy(sec, BOOTSTATUS_MAGIC, 30);
    w32le(sec + 0x20, status);
    return 0;
}

/*
 * Sharp packed "70" record (LBA 17/18/65 = 12 bytes, LBA 66 = 16 bytes):
 *   +0  u32 magic 0x00003037
 *   +4  payload (4 or 8 bytes)
 *   +8/+12  sum16(bytes before checksum) / ones-complement
 */
int packed70_parse(const u8 *p, u32 len, struct packed70 *out)
{
    u32 try_n;

    memset(out, 0, sizeof(*out));
    if (len < 12 || r32le(p) != PACKED70_MAGIC)
        return -1;
    for (try_n = 12; try_n <= 16 && try_n <= len; try_n += 4) {
        u16 s = (u16)p[try_n - 4] | ((u16)p[try_n - 3] << 8);
        u16 ns = (u16)p[try_n - 2] | ((u16)p[try_n - 1] << 8);
        if (s != sum16(p, try_n - 4))
            continue;
        if (ns != (u16)((0xffffu - s) & 0xffffu))
            continue;
        out->magic = PACKED70_MAGIC;
        out->value = r32le(p + 4);
        out->value2 = (try_n == 16) ? r32le(p + 8) : 0;
        out->nbytes = try_n;
        out->sum16 = s;
        out->nsum16 = ns;
        out->valid = 1;
        return 0;
    }
    return -2;
}

int packed70_build(u32 value, u8 *p, u32 *nbytes)
{
    u16 s, ns;

    w32le(p, PACKED70_MAGIC);
    w32le(p + 4, value);
    s = sum16(p, 8);
    ns = (u16)((0xffffu - s) & 0xffffu);
    w16le(p + 8, s);
    w16le(p + 10, ns);
    if (nbytes)
        *nbytes = 12;
    return 0;
}

int packed70_setting_ok(const u8 sec[512])
{
    struct packed70 rec;
    return packed70_parse(sec, 512, &rec) == 0;
}

int factory_setting_ok(const u8 sec[512])
{
    /* REGIONS.md: first byte 0x80 is the factory-valid flag Diag writes. */
    return sec[0] == FACTORY_FLAG_VALID;
}

int factory_setting_parse(const u8 sec[512], struct packed70 *inner)
{
    memset(inner, 0, sizeof(*inner));
    if (!factory_setting_ok(sec))
        return -1;
    if (r32le(sec + 4) == PACKED70_MAGIC)
        packed70_parse(sec + 4, 508, inner);
    return 0;
}

int factory_setting_build(u8 sec[512])
{
    memset(sec, 0, 512);
    sec[0] = FACTORY_FLAG_VALID;
    sec[1] = 0x01; /* same extra byte as the repaired eMMC */
    return 0;
}

int boot_image_is_os(const u8 *buf, u32 len)
{
    struct b000ff_hdr hdr;
    const u8 *pay = NULL;

    if (len < CARD_BOOT_MIN_OS)
        return 0;
    if (ecec_probe(buf, len, NULL) == 0)
        return 1;
    if (b000ff_parse(buf, len, &hdr, &pay) == 0 &&
        hdr.image_size >= CARD_BOOT_MIN_OS)
        return 1;
    if (len > 16 && (buf[3] == 0xea || buf[3] == 0xe5))
        return 1;
    return 0;
}
