/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef IDENT_H
#define IDENT_H

#include "boot.h"
#include "mmc.h"

/*
 * Runtime device identity. Nothing here is compiled in as "this is a
 * PW-AJ2". The retail name is looked up from DevInfo.model (and, if
 * that record is missing, left as UNKNOWN). The table is the ResetKit
 * / lilo gen map used by this platform family.
 */

struct device_ident {
    char raw_model[12];     /* bytes from DevInfo, trimmed */
    char internal[12];      /* normalised "ED-SH6" */
    char key[12];           /* dashless "EDSH6" */
    char retail[16];        /* "PW-SH6" or "UNKNOWN" */
    char gen[12];           /* "gen3_6" or "-" */
    char banner[48];        /* EBL-style identity line */
    char cfg_name[16];      /* EDSH6CFG.BIN  (8.3) */
    char exe_name[16];      /* EDSH6EXE.BIN  (8.3) */
    char dev_name[16];      /* EDSH6DEV.BIN  (8.3 of DEVINFO) */
    u32  version;
    int  known;
    int  from_devinfo;
};

void ident_clear(struct device_ident *id);
void ident_from_devinfo(struct device_ident *id, const struct dev_info *di);
void ident_format(struct device_ident *id);
const char *ident_display(const struct device_ident *id);

/* Probe DRAM controller the same way barebox's imx28_get_memsize() does. */
u32 ident_dram_bytes(void);

/* EDNA2 mailbox page used by this SoC / qemu-brain (PA 0x400EA000). */
#define EDNA2_MAILBOX_PHYS   0x400EA000u
#define EDNA2_DOORBELL_OFF   0x3cu
#define EDNA2_STATUS_OFF     0x30u

u32 ident_edna2_doorbell(void);
u32 ident_i2c0_ctrl(void);

#endif
