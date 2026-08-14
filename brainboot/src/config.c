/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "config.h"
#include "board.h"

void bb_config_defaults(struct bb_config *c)
{
    memset(c, 0, sizeof(*c));
    c->autoboot = AUTOBOOT_SECONDS;
    c->default_target = BB_DEFAULT_WINCE;
    strncpy(c->cmdline,
            "console=ttyAMA0,115200 console=tty1 root=/dev/mmcblk1p2 rw rootwait",
            sizeof(c->cmdline) - 1);
    strncpy(c->zimage, SD_ZIMAGE_NAME, sizeof(c->zimage) - 1);
    c->zimage[sizeof(c->zimage) - 1] = 0;
    strncpy(c->dtb, SD_DTB_NAME, sizeof(c->dtb) - 1);
    c->dtb[sizeof(c->dtb) - 1] = 0;
    c->loaded = 0;
}

const char *bb_default_name(u32 t)
{
    switch (t) {
    case BB_DEFAULT_LINUX: return "linux";
    case BB_DEFAULT_DIAG:  return "diag";
    case BB_DEFAULT_SDEXE: return "sdexe";
    case BB_DEFAULT_NONE:  return "none";
    default:               return "wince";
    }
}

static u32 parse_target(const char *s)
{
    if (strncmp(s, "linux", 5) == 0) return BB_DEFAULT_LINUX;
    if (strncmp(s, "diag", 4) == 0)  return BB_DEFAULT_DIAG;
    if (strncmp(s, "sdexe", 5) == 0) return BB_DEFAULT_SDEXE;
    if (strncmp(s, "none", 4) == 0)  return BB_DEFAULT_NONE;
    return BB_DEFAULT_WINCE;
}

static int is_ws(char c) { return c == ' ' || c == '\t' || c == '\r'; }

int bb_config_parse(const char *text, u32 len, struct bb_config *c)
{
    u32 i = 0;
    bb_config_defaults(c);
    while (i < len) {
        char line[180];
        u32 n = 0;
        char *eq;
        while (i < len && text[i] != '\n' && n + 1 < sizeof(line))
            line[n++] = text[i++];
        if (i < len && text[i] == '\n')
            i++;
        line[n] = 0;
        while (n && (line[n - 1] == '\r' || is_ws(line[n - 1])))
            line[--n] = 0;
        if (line[0] == 0 || line[0] == '#' || line[0] == ';')
            continue;
        eq = strchr(line, '=');
        if (!eq)
            continue;
        *eq++ = 0;
        if (strcmp(line, "autoboot") == 0) {
            u32 v = 0;
            while (*eq >= '0' && *eq <= '9')
                v = v * 10 + (u32)(*eq++ - '0');
            if (v > 60)
                v = 60;
            c->autoboot = v;
        } else if (strcmp(line, "default") == 0) {
            c->default_target = parse_target(eq);
        } else if (strcmp(line, "cmdline") == 0) {
            strncpy(c->cmdline, eq, sizeof(c->cmdline) - 1);
        } else if (strcmp(line, "zimage") == 0) {
            strncpy(c->zimage, eq, sizeof(c->zimage) - 1);
        } else if (strcmp(line, "dtb") == 0) {
            strncpy(c->dtb, eq, sizeof(c->dtb) - 1);
        }
    }
    c->loaded = 1;
    return 0;
}

int bb_config_format(const struct bb_config *c, char *out, u32 outsz)
{
    return snprint(out, outsz,
                   "# brainboot cfg\nautoboot=%u\ndefault=%s\ncmdline=%s\nzimage=%s\ndtb=%s\n",
                   c->autoboot, bb_default_name(c->default_target),
                   c->cmdline, c->zimage, c->dtb);
}

int bb_config_load(struct fat_fs *sd, struct bb_config *c)
{
    struct fat_file f;
    char buf[512];
    int n;

    bb_config_defaults(c);
    if (!sd || !sd->ready)
        return -1;
    if (fat_find(sd, SD_BBCONF_NAME, &f))
        return -2;
    if (f.size >= sizeof(buf))
        f.size = sizeof(buf) - 1;
    n = fat_read(sd, &f, buf, f.size);
    if (n < 0)
        return -3;
    buf[n] = 0;
    return bb_config_parse(buf, (u32)n, c);
}

int bb_config_save(struct fat_fs *sd, const struct bb_config *c)
{
    char buf[512];
    int n;
    if (!sd || !sd->ready)
        return -1;
    memset(buf, 0, sizeof(buf));
    n = bb_config_format(c, buf, sizeof(buf));
    if (n <= 0)
        return -2;
    return fat_write(sd, SD_BBCONF_NAME, buf, (u32)n);
}
