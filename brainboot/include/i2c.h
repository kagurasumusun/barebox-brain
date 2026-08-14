/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef I2C_H
#define I2C_H

#include "brainboot.h"

/* i.MX28 I2C0 @ 0x80058000, PIO mode used by the WinCE BSP / EBL. */
#define I2C_SGTL5000_ADDR   0x0Au
#define I2C_KBD_ADDR        0x28u

int  i2c_init(void);
int  i2c_probe(u8 addr7);
int  i2c_write(u8 addr7, const u8 *data, u32 n);
int  i2c_read(u8 addr7, u8 *data, u32 n);
int  i2c_write_read(u8 addr7, const u8 *w, u32 wn, u8 *r, u32 rn);

/* sgtl5000: chip id at reg 0x0000, expected 0xa0xx */
int  codec_read16(u16 reg, u16 *val);
int  codec_probe(u16 *chipid);

#endif
