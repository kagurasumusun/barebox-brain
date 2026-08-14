/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "fat.h"
#include "board.h"

static u16 r16(const u8 *p) { return (u16)p[0] | ((u16)p[1] << 8); }
static u32 r32(const u8 *p) { return (u32)p[0] | ((u32)p[1] << 8) |
                                     ((u32)p[2] << 16) | ((u32)p[3] << 24); }

int mbr_parse(const u8 sector[512], struct mbr_part parts[4])
{
    int i;
    if (sector[510] != 0x55 || sector[511] != 0xaa)
        return -1;
    for (i = 0; i < 4; i++) {
        const u8 *e = sector + 446 + i * 16;
        parts[i].boot = e[0];
        parts[i].type = e[4];
        parts[i].start_s = e[2] & 0x3f;
        parts[i].end_s = e[6] & 0x3f;
        parts[i].start_lba = r32(e + 8);
        parts[i].size_lba = r32(e + 12);
    }
    return 0;
}

int mbr_chs_ok(const struct mbr_part *p)
{
    /* Stock EBOOT FATInitDisk rejects CHS sector-field == 0. */
    if (p->type == 0)
        return 1;
    return (p->start_s != 0 && p->end_s != 0);
}

static void fat83_from_dirent(const u8 *d, char out[13])
{
    int i, n = 0;
    for (i = 0; i < 8 && d[i] != ' '; i++)
        out[n++] = (char)d[i];
    if (d[8] != ' ') {
        out[n++] = '.';
        for (i = 8; i < 11 && d[i] != ' '; i++)
            out[n++] = (char)d[i];
    }
    out[n] = 0;
}

static void name83_normalize(const char *in, char out[13])
{
    int i = 0;
    while (*in && i < 12) {
        char c = *in++;
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');
        out[i++] = c;
    }
    out[i] = 0;
}

static u32 fat_next_cluster(struct fat_fs *fs, u32 clus)
{
    u8 sec[512];
    u32 off, lba, ent;

    if (fs->fat32) {
        off = clus * 4;
        lba = fs->fat_lba + off / 512;
        if (mmc_read(fs->dev, lba, 1, sec))
            return 0x0fffffff;
        ent = r32(sec + (off % 512)) & 0x0fffffffu;
        if (ent >= 0x0ffffff8u)
            return 0;
        return ent;
    } else {
        off = clus * 2;
        lba = fs->fat_lba + off / 512;
        if (mmc_read(fs->dev, lba, 1, sec))
            return 0xffff;
        ent = r16(sec + (off % 512));
        if (ent >= 0xfff8u)
            return 0;
        return ent;
    }
}

static u32 clus_to_lba(struct fat_fs *fs, u32 clus)
{
    return fs->data_lba + (clus - 2) * fs->sectors_per_cluster;
}

static int parse_bpb(struct fat_fs *fs, const u8 *s, u32 base_lba)
{
    u16 bps, resv, fatsz16, tot16, rootent;
    u8  spc, nfats, media;
    u32 fatsz32, tot32, fatsz, totsec, data_sec, countclus;

    if (s[510] != 0x55 || s[511] != 0xaa)
        return -1;
    bps = r16(s + 11);
    spc = s[13];
    resv = r16(s + 14);
    nfats = s[16];
    rootent = r16(s + 17);
    tot16 = r16(s + 19);
    media = s[21];
    fatsz16 = r16(s + 22);
    tot32 = r32(s + 32);
    fatsz32 = r32(s + 36);
    (void)media;
    if (bps != 512 || spc == 0 || nfats == 0)
        return -1;

    fatsz = fatsz16 ? fatsz16 : fatsz32;
    totsec = tot16 ? tot16 : tot32;
    fs->bytes_per_sector = bps;
    fs->sectors_per_cluster = spc;
    fs->fat_size = fatsz;
    fs->root_entries = rootent;
    fs->part_lba = base_lba;
    fs->fat_lba = base_lba + resv;
    fs->root_lba = fs->fat_lba + nfats * fatsz;
    fs->data_lba = fs->root_lba + ((rootent * 32) + 511) / 512;
    data_sec = totsec - (fs->data_lba - base_lba);
    countclus = data_sec / spc;
    fs->cluster_count = countclus;
    fs->fat32 = (countclus >= 65525) ? 1 : 0;
    if (fs->fat32) {
        fs->root_cluster = r32(s + 44);
        fs->data_lba = fs->fat_lba + nfats * fatsz;
    } else {
        fs->root_cluster = 0;
    }
    fs->ready = 1;
    return 0;
}

int fat_mount(struct fat_fs *fs, struct mmc_dev *dev, u32 hint_lba)
{
    u8 sec[512];
    struct mbr_part parts[4];
    int i;

    memset(fs, 0, sizeof(*fs));
    fs->dev = dev;

    if (mmc_read(dev, hint_lba, 1, sec))
        return -1;

    /* Direct VBR? */
    if (parse_bpb(fs, sec, hint_lba) == 0)
        return 0;

    if (mbr_parse(sec, parts) != 0)
        return -2;

    for (i = 0; i < 4; i++) {
        u8 type = parts[i].type;
        if (type == 0x01 || type == 0x04 || type == 0x06 ||
            type == 0x0b || type == 0x0c || type == 0x0e) {
            if (mmc_read(dev, hint_lba + parts[i].start_lba, 1, sec))
                continue;
            if (parse_bpb(fs, sec, hint_lba + parts[i].start_lba) == 0)
                return 0;
        }
    }
    return -3;
}

static int walk_dir_sector(struct fat_fs *fs, const u8 *sec,
                           const char *want, struct fat_file *out,
                           void (*cb)(const struct fat_file *f, void *ctx),
                           void *ctx)
{
    int i;
    (void)fs;
    for (i = 0; i < 16; i++) {
        const u8 *d = sec + i * 32;
        struct fat_file f;
        if (d[0] == 0)
            return 1; /* end */
        if (d[0] == 0xe5 || (d[11] & 0x08) || d[11] == 0x0f)
            continue;
        memset(&f, 0, sizeof(f));
        fat83_from_dirent(d, f.name);
        f.attr = d[11];
        f.first_cluster = r16(d + 26) | ((u32)r16(d + 20) << 16);
        f.size = r32(d + 28);
        if (cb)
            cb(&f, ctx);
        if (want && strcmp(f.name, want) == 0) {
            *out = f;
            return 2;
        }
    }
    return 0;
}

static int walk_root(struct fat_fs *fs, const char *want, struct fat_file *out,
                     void (*cb)(const struct fat_file *f, void *ctx), void *ctx)
{
    u8 sec[512];
    if (!fs->fat32) {
        u32 nsec = ((fs->root_entries * 32) + 511) / 512;
        u32 i;
        for (i = 0; i < nsec; i++) {
            int r;
            if (mmc_read(fs->dev, fs->root_lba + i, 1, sec))
                return -1;
            r = walk_dir_sector(fs, sec, want, out, cb, ctx);
            if (r)
                return (r == 2) ? 0 : ((r == 1) ? -2 : r);
        }
        return want ? -2 : 0;
    } else {
        u32 clus = fs->root_cluster;
        while (clus) {
            u32 lba = clus_to_lba(fs, clus);
            u32 s;
            for (s = 0; s < fs->sectors_per_cluster; s++) {
                int r;
                if (mmc_read(fs->dev, lba + s, 1, sec))
                    return -1;
                r = walk_dir_sector(fs, sec, want, out, cb, ctx);
                if (r == 2)
                    return 0;
                if (r == 1)
                    return want ? -2 : 0;
            }
            clus = fat_next_cluster(fs, clus);
        }
        return want ? -2 : 0;
    }
}

int fat_find(struct fat_fs *fs, const char *name83, struct fat_file *out)
{
    char want[13];
    if (!fs->ready)
        return -1;
    name83_normalize(name83, want);
    return walk_root(fs, want, out, NULL, NULL);
}

int fat_list(struct fat_fs *fs, void (*cb)(const struct fat_file *f, void *ctx), void *ctx)
{
    if (!fs->ready)
        return -1;
    return walk_root(fs, NULL, NULL, cb, ctx);
}

int fat_read(struct fat_fs *fs, const struct fat_file *f, void *buf, u32 maxlen)
{
    u32 remain = f->size < maxlen ? f->size : maxlen;
    u32 clus = f->first_cluster;
    u8 *p = buf;
    u32 got = 0;
    u32 bpc = fs->sectors_per_cluster * 512;
    u8 bounce[512];

    if (clus < 2 && f->size)
        return -1;
    while (remain && clus) {
        u32 lba = clus_to_lba(fs, clus);
        u32 off = 0;
        while (off < bpc && remain) {
            u32 chunk = remain < 512 ? remain : 512;
            if (mmc_read(fs->dev, lba + off / 512, 1, bounce))
                return -2;
            memcpy(p, bounce, chunk);
            p += chunk;
            got += chunk;
            remain -= chunk;
            off += 512;
        }
        clus = fat_next_cluster(fs, clus);
    }
    return (int)got;
}
