/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "i2c.h"
#include "board.h"

#ifndef HOST_BUILD

static void i2c_softreset(void)
{
    u32 base = IMX_I2C0_BASE;
    u32 g = 100000;
    writel_set(I2C_CTRL0_SFTRST, base + HW_I2C_CTRL0);
    while (!(readl(base + HW_I2C_CTRL0) & I2C_CTRL0_CLKGATE) && --g)
        ;
    writel_clr(I2C_CTRL0_SFTRST | I2C_CTRL0_CLKGATE, base + HW_I2C_CTRL0);
}

static int i2c_wait_done(u32 usec)
{
    u32 base = IMX_I2C0_BASE;
    while (usec--) {
        u32 c0 = readl(base + HW_I2C_CTRL0);
        u32 c1 = readl(base + HW_I2C_CTRL1);
        if (!(c0 & I2C_CTRL0_RUN)) {
            if (c1 & I2C_CTRL1_NO_SLAVE_ACK)
                return -2;
            return 0;
        }
        udelay(1);
    }
    return -1;
}

int i2c_init(void)
{
    u32 base = IMX_I2C0_BASE;
    /* I2C0 SCL/SDA = bank3 pin24/25 func0 (MUXSEL7 bits 16 and 18). */
    {
        u32 mux = IMX_PINCTRL_BASE + HW_PINCTRL_MUXSEL0 + 7u * 0x10u;
        u32 v = readl(mux);
        v &= ~((3u << 16) | (3u << 18));
        writel(v, mux);
    }
    i2c_softreset();
    /* TIMING0/1 for ~100 kHz from 24 MHz XTAL (i.MX28 RM defaults). */
    writel(0x000f0007u, base + HW_I2C_TIMING0);
    writel(0x001f000fu, base + HW_I2C_TIMING1);
    writel(0x0015000du, base + HW_I2C_TIMING2);
    return 0;
}

int i2c_probe(u8 addr7)
{
    u32 base = IMX_I2C0_BASE;
    u32 ctrl;
    int rc;

    writel(I2C_CTRL1_CLR_GOT_A_NAK | I2C_CTRL1_NO_SLAVE_ACK |
           I2C_CTRL1_DATA_ENGINE_CMPLT, base + HW_I2C_CTRL1 + REG_CLR);
    ctrl = I2C_CTRL0_RUN | I2C_CTRL0_PIO_MODE | I2C_CTRL0_POST_SEND_STOP |
           I2C_CTRL0_PRE_SEND_START | I2C_CTRL0_MASTER_MODE | 1u;
    writel(ctrl, base + HW_I2C_CTRL0);
    writel((u32)addr7 << 1, base + HW_I2C_DATA);
    rc = i2c_wait_done(20000);
    if (rc)
        printf("I2C Timeout ERROR! addr=0x%x rc=%d  HW_I2C_STAT=0x%x  HW_I2C_CTRL0=0x%x\n",
               addr7, rc, readl(base + HW_I2C_STAT), readl(base + HW_I2C_CTRL0));
    return rc;
}

int i2c_write(u8 addr7, const u8 *data, u32 n)
{
    u32 base = IMX_I2C0_BASE;
    u32 i, ctrl;
    int rc;

    if (n == 0)
        return i2c_probe(addr7);
    writel(I2C_CTRL1_CLR_GOT_A_NAK | I2C_CTRL1_NO_SLAVE_ACK |
           I2C_CTRL1_DATA_ENGINE_CMPLT, base + HW_I2C_CTRL1 + REG_CLR);
    ctrl = I2C_CTRL0_RUN | I2C_CTRL0_PIO_MODE | I2C_CTRL0_POST_SEND_STOP |
           I2C_CTRL0_PRE_SEND_START | I2C_CTRL0_MASTER_MODE | (n + 1u);
    writel(ctrl, base + HW_I2C_CTRL0);
    writel((u32)addr7 << 1, base + HW_I2C_DATA);
    for (i = 0; i < n; i++)
        writel(data[i], base + HW_I2C_DATA);
    rc = i2c_wait_done(40000);
    if (rc)
        printf("I2C ERROR! 0x%x\n", readl(base + HW_I2C_STAT));
    return rc;
}

int i2c_read(u8 addr7, u8 *data, u32 n)
{
    u32 base = IMX_I2C0_BASE;
    u32 i, ctrl;
    int rc;

    if (n == 0)
        return -1;
    writel(I2C_CTRL1_CLR_GOT_A_NAK | I2C_CTRL1_NO_SLAVE_ACK |
           I2C_CTRL1_DATA_ENGINE_CMPLT, base + HW_I2C_CTRL1 + REG_CLR);
    /* Address phase (write). */
    ctrl = I2C_CTRL0_RUN | I2C_CTRL0_PIO_MODE | I2C_CTRL0_RETAIN_CLOCK |
           I2C_CTRL0_PRE_SEND_START | I2C_CTRL0_MASTER_MODE | 1u;
    writel(ctrl, base + HW_I2C_CTRL0);
    writel((u32)addr7 << 1, base + HW_I2C_DATA);
    rc = i2c_wait_done(20000);
    if (rc)
        return rc;
    /* Repeated start + read. */
    writel(I2C_CTRL1_DATA_ENGINE_CMPLT, base + HW_I2C_CTRL1 + REG_CLR);
    ctrl = I2C_CTRL0_RUN | I2C_CTRL0_PIO_MODE | I2C_CTRL0_POST_SEND_STOP |
           I2C_CTRL0_PRE_SEND_START | I2C_CTRL0_MASTER_MODE |
           I2C_CTRL0_DIRECTION | n;
    writel((u32)addr7 << 1 | 1u, base + HW_I2C_DATA);
    writel(ctrl, base + HW_I2C_CTRL0);
    rc = i2c_wait_done(40000);
    if (rc)
        return rc;
    for (i = 0; i < n; i++)
        data[i] = (u8)readl(base + HW_I2C_DATA);
    return 0;
}

int i2c_write_read(u8 addr7, const u8 *w, u32 wn, u8 *r, u32 rn)
{
    int rc = i2c_write(addr7, w, wn);
    if (rc)
        return rc;
    return i2c_read(addr7, r, rn);
}

int codec_read16(u16 reg, u16 *val)
{
    u8 wr[2] = { (u8)(reg >> 8), (u8)reg };
    u8 rd[2];
    int rc = i2c_write_read(I2C_SGTL5000_ADDR, wr, 2, rd, 2);
    if (rc)
        return rc;
    *val = ((u16)rd[0] << 8) | rd[1];
    return 0;
}

int codec_probe(u16 *chipid)
{
    u16 id = 0;
    int rc = codec_read16(0x0000, &id);
    if (chipid)
        *chipid = id;
    if (rc) {
        printf("CODEC_READ ERROR addr=0000\n");
        return rc;
    }
    /* sgtl5000 CHIP_ID partid is 0xA0 in the high byte. */
    if ((id & 0xff00u) != 0xa000u)
        return -3;
    return 0;
}

#else
int i2c_init(void) { return 0; }
int i2c_probe(u8 a) { (void)a; return -1; }
int i2c_write(u8 a, const u8 *d, u32 n) { (void)a;(void)d;(void)n; return -1; }
int i2c_read(u8 a, u8 *d, u32 n) { (void)a;(void)d;(void)n; return -1; }
int i2c_write_read(u8 a, const u8 *w, u32 wn, u8 *r, u32 rn)
{ (void)a;(void)w;(void)wn;(void)r;(void)rn; return -1; }
int codec_read16(u16 r, u16 *v) { (void)r; if (v) *v = 0; return -1; }
int codec_probe(u16 *id) { if (id) *id = 0; return -1; }
#endif
