/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "ident.h"
#include "board.h"

struct model_row {
    const char *key;      /* dashless, upper */
    const char *internal; /* ED-SH6 */
    const char *retail;   /* PW-SH6 */
    const char *gen;
};

/*
 * ResetKit / lilo map, gen3_5 .. gen3_7. Only rows we have a source
 * for; unknown DevInfo models stay UNKNOWN rather than being forced
 * to PW-AJ2.
 */
static const struct model_row models[] = {
    { "EDAJ1", "ED-AJ1", "PW-AJ1", "gen3_5" },
    { "EDAA1", "ED-AA1", "PW-AA1", "gen3_5" },
    { "EDSH5", "ED-SH5", "PW-SH5", "gen3_5" },
    { "EDSJ5", "ED-SJ5", "PW-SJ5", "gen3_5" },
    { "EDSB5", "ED-SB5", "PW-SB5", "gen3_5" },
    { "EDSA5", "ED-SA5", "PW-SA5", "gen3_5" },
    { "EDH78", "ED-H78", "PW-H78", "gen3_5" },
    { "EDSH6", "ED-SH6", "PW-SH6", "gen3_6" },
    { "EDAA2", "ED-AA2", "PW-AA2", "gen3_6" },
    { "EDAJ2", "ED-AJ2", "PW-AJ2", "gen3_6" },
    { "EDSS6", "ED-SS6", "PW-SS6", "gen3_6" },
    { "EDH80", "ED-H80", "PW-H80", "gen3_6" },
    { "EDSB6", "ED-SB6", "PW-SB6", "gen3_6" },
    { "EDSH7", "ED-SH7", "PW-SH7", "gen3_7" },
    { "EDSS7", "ED-SS7", "PW-SS7", "gen3_7" },
    { "EDSB7", "ED-SB7", "PW-SB7", "gen3_7" },
    { "EDH81", "ED-H81", "PW-H81", "gen3_7" },
    { "EDH91", "ED-H91", "PW-H91", "gen3_7" },
    { "EDSR3", "ED-SR3", "PW-SR3", "gen3_7" },
};

static void trim_copy(char *dst, u32 dstsz, const char *src, u32 n)
{
    u32 i = 0, j = 0;
    while (i < n && src[i] == ' ')
        i++;
    while (i < n && src[i] && src[i] != ' ' && j + 1 < dstsz)
        dst[j++] = src[i++];
    dst[j] = 0;
}

static void make_key(const char *in, char *out, u32 outsz)
{
    u32 j = 0;
    while (*in && j + 1 < outsz) {
        char c = *in++;
        if (c == '-' || c == '_')
            continue;
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 'a' + 'A');
        out[j++] = c;
    }
    out[j] = 0;
}

void ident_clear(struct device_ident *id)
{
    memset(id, 0, sizeof(*id));
    strncpy(id->retail, "UNKNOWN", sizeof(id->retail) - 1);
    strncpy(id->internal, "UNKNOWN", sizeof(id->internal) - 1);
    strncpy(id->gen, "-", sizeof(id->gen) - 1);
    strncpy(id->cfg_name, SD_BOOTCFG_NAME, sizeof(id->cfg_name) - 1);
    strncpy(id->exe_name, SD_BOOTEXE_NAME, sizeof(id->exe_name) - 1);
    strncpy(id->dev_name, SD_DEVINFO_NAME, sizeof(id->dev_name) - 1);
    strncpy(id->banner, "SHARP   UNKNOWN", sizeof(id->banner) - 1);
}

void ident_format(struct device_ident *id)
{
    const struct model_row *row = NULL;
    u32 i;
    char ver[12];

    make_key(id->raw_model[0] ? id->raw_model : id->internal, id->key,
             sizeof(id->key));

    for (i = 0; i < ARRAY_SIZE(models); i++) {
        if (strcmp(id->key, models[i].key) == 0) {
            row = &models[i];
            break;
        }
    }

    if (row) {
        strncpy(id->internal, row->internal, sizeof(id->internal) - 1);
        strncpy(id->retail, row->retail, sizeof(id->retail) - 1);
        strncpy(id->gen, row->gen, sizeof(id->gen) - 1);
        id->known = 1;
    } else if (id->raw_model[0]) {
        strncpy(id->internal, id->raw_model, sizeof(id->internal) - 1);
        strncpy(id->retail, "UNKNOWN", sizeof(id->retail) - 1);
        strncpy(id->gen, "-", sizeof(id->gen) - 1);
        id->known = 0;
    }

    /* Same 8.3 rule for CFG / EXE / DEV. DEVINFO.BIN does not fit 8.3
     * (EDSH6DEVINFO = 12); FAT 8.3 name is EDSH6DEV.BIN. */
    if (id->key[0] && strlen(id->key) <= 8) {
        snprint(id->cfg_name, sizeof(id->cfg_name), "%sCFG.BIN", id->key);
        snprint(id->exe_name, sizeof(id->exe_name), "%sEXE.BIN", id->key);
        snprint(id->dev_name, sizeof(id->dev_name), "%sDEV.BIN", id->key);
    } else {
        strncpy(id->cfg_name, SD_BOOTCFG_NAME, sizeof(id->cfg_name) - 1);
        strncpy(id->exe_name, SD_BOOTEXE_NAME, sizeof(id->exe_name) - 1);
        strncpy(id->dev_name, SD_DEVINFO_NAME, sizeof(id->dev_name) - 1);
    }

    if (id->version)
        snprint(ver, sizeof(ver), "V%u", id->version);
    else
        strncpy(ver, "V???", sizeof(ver) - 1);

    snprint(id->banner, sizeof(id->banner), "SHARP   %s  %s",
            id->internal, ver);
}

void ident_from_devinfo(struct device_ident *id, const struct dev_info *di)
{
    ident_clear(id);
    if (!di || !di->valid)
        return;
    trim_copy(id->raw_model, sizeof(id->raw_model), di->model, 8);
    id->version = di->version;
    id->from_devinfo = 1;
    ident_format(id);
}

const char *ident_display(const struct device_ident *id)
{
    return id->retail[0] ? id->retail : "UNKNOWN";
}

#ifndef HOST_BUILD

/* Same formula as barebox include/mach/mxs/imx28.h::imx28_get_memsize(). */
#define DRAM_CTL(n)             (0x800e0000u + 4u * (n))
#define DRAM_CTL29_CS0_EN       (1u << 24)
#define DRAM_CTL29_CS1_EN       (1u << 25)

static u32 ident_dram_from_ctl(void)
{
    u32 ctl29 = readl(DRAM_CTL(29));
    u32 ctl31 = readl(DRAM_CTL(31));
    int columns = 12 - (int)((ctl29 >> 16) & 7u);
    int rows    = 15 - (int)((ctl29 >> 8) & 7u);
    int banks   = (ctl31 & (1u << 16)) ? 8 : 4;
    int cs0     = (ctl29 & DRAM_CTL29_CS0_EN) ? 1 : 0;
    int cs1     = (ctl29 & DRAM_CTL29_CS1_EN) ? 1 : 0;
    int cs      = cs0 + cs1;
    u32 bytes;

    if (columns < 8 || columns > 12 || rows < 10 || rows > 16 || cs == 0)
        return 0;
    bytes = (1u << (unsigned)columns) * (1u << (unsigned)rows) *
            (u32)banks * (u32)cs;
    /* Controller reports chips, not bytes-per-chip width. 16-bit parts
     * on this SoC yield the physical DRAM window. Reject nonsense. */
    if (bytes < (16u << 20) || bytes > (512u << 20))
        return 0;
    return bytes;
}

/* When DRAM_CTL is unprogrammed (qemu-brain, some XLDR skips), walk
 * 16..256 MiB by writing a token at the last page of each size and
 * checking it is not an alias of a smaller window. */
static u32 ident_dram_walk(void)
{
    static const u32 sizes[] = {
        16u << 20, 32u << 20, 64u << 20, 128u << 20
    };
    u32 base = DRAM_PHYS_BASE;
    u32 last = 0;
    u32 i, j;

    for (i = 0; i < ARRAY_SIZE(sizes); i++) {
        u32 sz = sizes[i];
        u32 addr = base + sz - 0x1000u;
        volatile u32 *p;
        u32 old, token, aliased = 0;

        /* Never touch past the physical window: an unmapped poke
         * prefetch-aborts on this SoC / qemu-brain (strict-hw). */
        if (sz > DRAM_PHYS_SIZE || addr >= base + DRAM_PHYS_SIZE)
            break;
        /* Do not poke this image or the framebuffer. */
        if (addr >= 0x40200000u && addr < 0x40300000u)
            continue;
        if (addr >= 0x46000000u && addr < 0x46200000u)
            continue;
        p = (volatile u32 *)addr;
        token = 0xB2010000u + i;
        old = *p;
        *p = token;
        if (*p != token) {
            *p = old;
            break;
        }
        for (j = 0; j < i; j++) {
            u32 a2 = base + sizes[j] - 0x1000u;
            if (*(volatile u32 *)a2 == token) {
                aliased = 1;
                break;
            }
        }
        *p = old;
        if (aliased)
            break;
        last = sz;
    }
    return last;
}

u32 ident_dram_bytes(void)
{
    u32 n = ident_dram_from_ctl();
    if (n)
        return n;
    return ident_dram_walk();
}

u32 ident_edna2_doorbell(void)
{
    return readl(EDNA2_MAILBOX_PHYS + EDNA2_DOORBELL_OFF);
}

u32 ident_i2c0_ctrl(void)
{
    return readl(IMX_I2C0_BASE);
}

#else
u32 ident_dram_bytes(void) { return 0; }
u32 ident_edna2_doorbell(void) { return 0; }
u32 ident_i2c0_ctrl(void) { return 0; }
#endif
