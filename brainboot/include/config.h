/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef BB_CONFIG_H
#define BB_CONFIG_H

#include "fat.h"

#define BB_DEFAULT_WINCE  0
#define BB_DEFAULT_LINUX  1
#define BB_DEFAULT_DIAG   2
#define BB_DEFAULT_SDEXE  3
#define BB_DEFAULT_NONE   4

struct bb_config {
    u32 autoboot;
    u32 default_target;
    char cmdline[160];
    char zimage[16];
    char dtb[16];
    int  loaded;
    int  menu_force;
};

void bb_config_defaults(struct bb_config *c);
int  bb_config_parse(const char *text, u32 len, struct bb_config *c);
int  bb_config_format(const struct bb_config *c, char *out, u32 outsz);
int  bb_config_load(struct fat_fs *sd, struct bb_config *c);
int  bb_config_save(struct fat_fs *sd, const struct bb_config *c);
const char *bb_default_name(u32 t);

#endif
