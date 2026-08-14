/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "mmc.h"
#include "board.h"

#ifndef HOST_BUILD

/* MMC/SD commands */
#define CMD0    0
#define CMD1    1
#define CMD2    2
#define CMD3    3
#define CMD7    7
#define CMD8    8
#define CMD9    9
#define CMD13   13
#define CMD16   16
#define CMD17   17
#define CMD12   12
#define CMD18   18
#define CMD55   55
#define ACMD41  41
#define CMD6    6
#define CMD8_MMC 8

#define RESP_NONE   0
#define RESP_R1     1
#define RESP_R2     2
#define RESP_R3     3
#define RESP_R6     6
#define RESP_R7     7

static void ssp_softreset(u32 base)
{
    u32 guard = 100000;
    writel_set(SSP_CTRL0_SFTRST, base + HW_SSP_CTRL0);
    while (!(readl(base + HW_SSP_CTRL0) & SSP_CTRL0_CLKGATE) && --guard)
        ;
    writel_clr(SSP_CTRL0_SFTRST | SSP_CTRL0_CLKGATE, base + HW_SSP_CTRL0);
    guard = 100000;
    while ((readl(base + HW_SSP_CTRL0) & (SSP_CTRL0_SFTRST | SSP_CTRL0_CLKGATE)) && --guard)
        ;
}

static void ssp_set_clock(u32 base, u32 ssp_clk_hz, u32 target_hz)
{
    u32 div, rate, clock;
    /* TIMING: clock = ssp_clk / (CLOCK_DIVIDE * (1 + CLOCK_RATE))
     * CLOCK_DIVIDE even, 2..254. */
    div = 2;
    rate = ssp_clk_hz / (div * target_hz);
    if (rate > 0)
        rate -= 1;
    if (rate > 255)
        rate = 255;
    clock = SSP_TIMING_TIMEOUT(0xffff) | SSP_TIMING_CLOCK_DIVIDE(div) |
            SSP_TIMING_CLOCK_RATE(rate);
    writel(clock, base + HW_SSP_TIMING);
}

static void ssp_set_bus_width(u32 base, int bits)
{
    u32 w = 0;
    if (bits == 4)
        w = 1;
    else if (bits == 8)
        w = 2;
    clrl(3u << 22, base + HW_SSP_CTRL0);
    setl(SSP_CTRL0_BUS_WIDTH(w), base + HW_SSP_CTRL0);
}

static int ssp_wait_not(u32 base, u32 mask, u32 usec)
{
    u32 t = usec;
    while (t--) {
        if (!(readl(base + HW_SSP_STATUS) & mask))
            return 0;
        udelay(1);
    }
    return -1;
}

static int ssp_cmd(struct mmc_dev *dev, u32 cmd, u32 arg, int restype, u32 *resp)
{
    u32 base = dev->base;
    u32 ctrl0, cmd0, st;
    u32 guard;

    if (ssp_wait_not(base, SSP_STATUS_CMD_BUSY | SSP_STATUS_BUSY, 100000))
        return -1;

    writel(arg, base + HW_SSP_CMD1);
    cmd0 = SSP_CMD0_CMD(cmd);
    if (cmd == CMD12 || cmd == 0)
        cmd0 |= SSP_CMD0_APPEND_8CYC;
    writel(cmd0, base + HW_SSP_CMD0);

    ctrl0 = SSP_CTRL0_ENABLE | SSP_CTRL0_RUN;
    if (restype != RESP_NONE)
        ctrl0 |= SSP_CTRL0_GET_RESP;
    if (restype == RESP_R2)
        ctrl0 |= SSP_CTRL0_LONG_RESP;
    writel(ctrl0, base + HW_SSP_CTRL0);

    guard = 200000;
    while ((readl(base + HW_SSP_CTRL0) & SSP_CTRL0_RUN) && --guard)
        ;
    if (!guard)
        return -2;

    st = readl(base + HW_SSP_STATUS);
    if (st & (SSP_STATUS_RESP_TIMEOUT | SSP_STATUS_TIMEOUT))
        return -3;
    if ((st & SSP_STATUS_RESP_ERR) && restype != RESP_R3 && restype != RESP_R7)
        return -4;

    if (resp) {
        if (restype == RESP_R2) {
            resp[0] = readl(base + HW_SSP_SDRESP0);
            resp[1] = readl(base + HW_SSP_SDRESP1);
            resp[2] = readl(base + HW_SSP_SDRESP2);
            resp[3] = readl(base + HW_SSP_SDRESP3);
        } else {
            resp[0] = readl(base + HW_SSP_SDRESP0);
        }
    }
    return 0;
}

static int ssp_read_blocks(struct mmc_dev *dev, u32 cmd, u32 arg,
                           u32 blocks, void *buf)
{
    u32 base = dev->base;
    u32 ctrl0, *p = buf;
    u32 words = blocks * (MMC_BLOCK_SIZE / 4);
    u32 got = 0;
    u32 guard;

    if (ssp_wait_not(base, SSP_STATUS_BUSY, 200000))
        return -1;

    writel(arg, base + HW_SSP_CMD1);
    writel(SSP_CMD0_CMD(cmd), base + HW_SSP_CMD0);
    writel(blocks * MMC_BLOCK_SIZE, base + HW_SSP_XFER_COUNT);
    writel((blocks << 4) | 9u, base + HW_SSP_BLOCK_SIZE); /* 2^9 = 512 */

    ctrl0 = SSP_CTRL0_ENABLE | SSP_CTRL0_GET_RESP | SSP_CTRL0_DATA_XFER |
            SSP_CTRL0_READ | SSP_CTRL0_WAIT_FOR_CMD | SSP_CTRL0_RUN;
    writel(ctrl0, base + HW_SSP_CTRL0);

    while (got < words) {
        guard = 200000;
        while ((readl(base + HW_SSP_STATUS) & SSP_STATUS_FIFO_EMPTY) && --guard) {
            if (readl(base + HW_SSP_STATUS) &
                (SSP_STATUS_TIMEOUT | SSP_STATUS_DATA_CRC_ERR | SSP_STATUS_RESP_TIMEOUT))
                return -2;
        }
        if (!guard)
            return -3;
        p[got++] = readl(base + HW_SSP_DATA);
    }

    guard = 200000;
    while ((readl(base + HW_SSP_CTRL0) & SSP_CTRL0_RUN) && --guard)
        ;
    if (readl(base + HW_SSP_STATUS) & (SSP_STATUS_DATA_CRC_ERR | SSP_STATUS_TIMEOUT))
        return -4;
    return 0;
}

static void parse_cid(struct mmc_dev *dev, const u32 *r)
{
    /* R2 CID is 128 bits in SDRESP3..0, MSB first. */
    u8 cid[16];
    int i;
    cid[0]  = (u8)(r[3] >> 16);
    cid[1]  = (u8)(r[3] >> 8);
    cid[2]  = (u8)(r[3]);
    cid[3]  = (u8)(r[2] >> 24);
    cid[4]  = (u8)(r[2] >> 16);
    cid[5]  = (u8)(r[2] >> 8);
    cid[6]  = (u8)(r[2]);
    cid[7]  = (u8)(r[1] >> 24);
    cid[8]  = (u8)(r[1] >> 16);
    cid[9]  = (u8)(r[1] >> 8);
    cid[10] = (u8)(r[1]);
    cid[11] = (u8)(r[0] >> 24);
    cid[12] = (u8)(r[0] >> 16);
    cid[13] = (u8)(r[0] >> 8);
    cid[14] = (u8)(r[0]);
    cid[15] = 0;
    (void)i;
    dev->cid.mid = cid[0];
    dev->cid.oid = ((u16)cid[1] << 8) | cid[2];
    dev->cid.pnm[0] = (char)cid[3];
    dev->cid.pnm[1] = (char)cid[4];
    dev->cid.pnm[2] = (char)cid[5];
    dev->cid.pnm[3] = (char)cid[6];
    dev->cid.pnm[4] = (char)cid[7];
    dev->cid.pnm[5] = (char)cid[8];
    dev->cid.pnm[6] = 0;
    dev->cid.prv = cid[9];
    dev->cid.psn = ((u32)cid[10] << 24) | ((u32)cid[11] << 16) |
                   ((u32)cid[12] << 8) | cid[13];
    dev->cid.mdt_year = 2000 + (cid[14] >> 4);
    dev->cid.mdt_month = cid[14] & 0x0f;
}

/* CSD[127:96] is in SDRESP3. */
static u32 csd3_hi(const u32 *csd)
{
    return csd[3];
}

static u32 csd_capacity(const u32 *csd, int is_sd_hc)
{
    u32 csd1 = csd[1], csd2 = csd[2];
    u32 structure = (csd3_hi(csd) >> 30) & 3u;
    if (is_sd_hc || structure == 1) {
        /* C_SIZE in bits 69:48 of CSD */
        u32 c_size = ((csd2 & 0x3fu) << 16) | ((csd1 >> 16) & 0xffffu);
        return (c_size + 1u) * 1024u;
    }
    {
        u32 read_bl_len = (csd2 >> 16) & 0xfu;
        u32 c_size = ((csd2 & 0x3ffu) << 2) | ((csd1 >> 30) & 3u);
        u32 c_mult = (csd1 >> 15) & 7u;
        u32 blocknr = (c_size + 1u) * (1u << (c_mult + 2u));
        u32 block_len = 1u << read_bl_len;
        return (blocknr * block_len) / 512u;
    }
}

static int mmc_go_idle(struct mmc_dev *dev)
{
    return ssp_cmd(dev, CMD0, 0, RESP_NONE, NULL);
}

static int sd_send_if_cond(struct mmc_dev *dev)
{
    u32 resp;
    int rc = ssp_cmd(dev, CMD8, 0x1aa, RESP_R7, &resp);
    if (rc)
        return rc;
    if ((resp & 0xfff) != 0x1aa)
        return -1;
    return 0;
}

static int sd_send_op_cond(struct mmc_dev *dev)
{
    int i;
    u32 resp;
    for (i = 0; i < 1000; i++) {
        if (ssp_cmd(dev, CMD55, 0, RESP_R1, &resp))
            return -1;
        if (ssp_cmd(dev, ACMD41, 0x40ff8000u, RESP_R3, &resp))
            return -1;
        if (resp & 0x80000000u) {
            dev->ocr = resp;
            dev->is_hc = (resp & 0x40000000u) ? 1 : 0;
            return 0;
        }
        mdelay(1);
    }
    return -1;
}

static int mmc_send_op_cond(struct mmc_dev *dev)
{
    int i;
    u32 resp;
    for (i = 0; i < 1000; i++) {
        if (ssp_cmd(dev, CMD1, 0x40ff8000u, RESP_R3, &resp))
            return -1;
        if (resp & 0x80000000u) {
            dev->ocr = resp;
            dev->is_hc = (resp & 0x40000000u) ? 1 : 0;
            return 0;
        }
        mdelay(1);
    }
    return -1;
}

int mmc_init(struct mmc_dev *dev, enum mmc_port port)
{
    u32 resp[4];
    int rc;

    memset(dev, 0, sizeof(*dev));
    dev->port = port;
    dev->base = (port == MMC_PORT_EMMC) ? IMX_SSP0_BASE : IMX_SSP1_BASE;
    dev->bus_width = 1;

    ssp_softreset(dev->base);
    /* SSP clock is ~96 MHz after clock_init. Identify at 400 kHz. */
    ssp_set_clock(dev->base, 96000000u, 400000u);
    writel(SSP_CTRL1_SSP_MODE(SSP_MODE_SD_MMC) |
           SSP_CTRL1_WORD_LENGTH(7) | SSP_CTRL1_POLARITY,
           dev->base + HW_SSP_CTRL1);
    ssp_set_bus_width(dev->base, 1);
    mdelay(1);

    mmc_go_idle(dev);
    mdelay(2);

    if (port == MMC_PORT_SD) {
        rc = sd_send_if_cond(dev);
        if (rc == 0) {
            dev->is_sd = 1;
            rc = sd_send_op_cond(dev);
        } else {
            /* maybe MMC on SSP1 */
            mmc_go_idle(dev);
            rc = mmc_send_op_cond(dev);
            dev->is_sd = 0;
        }
    } else {
        rc = mmc_send_op_cond(dev);
        dev->is_sd = 0;
        if (rc) {
            mmc_go_idle(dev);
            if (sd_send_if_cond(dev) == 0) {
                dev->is_sd = 1;
                rc = sd_send_op_cond(dev);
            }
        }
    }
    if (rc) {
        printf("mmc%u: op_cond failed (%d)\n", (unsigned)port, rc);
        return rc;
    }

    if (ssp_cmd(dev, CMD2, 0, RESP_R2, resp))
        return -10;
    parse_cid(dev, resp);

    if (ssp_cmd(dev, CMD3, dev->is_sd ? 0 : 0x10000, RESP_R6, resp))
        return -11;
    dev->rca = dev->is_sd ? (resp[0] >> 16) : 0x1;

    if (ssp_cmd(dev, CMD9, dev->rca << 16, RESP_R2, resp))
        return -12;
    memcpy(dev->csd, resp, 16);
    dev->capacity_lba = csd_capacity(dev->csd, dev->is_hc);

    if (ssp_cmd(dev, CMD7, dev->rca << 16, RESP_R1, resp))
        return -13;

    if (ssp_cmd(dev, CMD16, MMC_BLOCK_SIZE, RESP_R1, resp))
        return -14;

    /* Switch to 4-bit (SD) or 8-bit (eMMC). Failure is non-fatal. */
    if (dev->is_sd) {
        if (ssp_cmd(dev, CMD55, dev->rca << 16, RESP_R1, resp) == 0 &&
            ssp_cmd(dev, 6, 2, RESP_R1, resp) == 0) {
            ssp_set_bus_width(dev->base, 4);
            dev->bus_width = 4;
        }
    } else {
        /* SWITCH (CMD6): access=3 write byte, index=183 (BUS_WIDTH), value=2 (8-bit) */
        u32 arg = (3u << 24) | (183u << 16) | (2u << 8);
        if (ssp_cmd(dev, CMD6, arg, RESP_R1, resp) == 0) {
            mdelay(10);
            ssp_set_bus_width(dev->base, 8);
            dev->bus_width = 8;
        }
    }

    ssp_set_clock(dev->base, 96000000u, 24000000u);
    dev->ready = 1;
    if (!dev->is_sd && dev->capacity_lba < 0x10000u) {
        u8 ext[512];
        if (mmc_read_extcsd(dev, ext) == 0) {
            u32 sec = (u32)ext[212] | ((u32)ext[213] << 8) |
                      ((u32)ext[214] << 16) | ((u32)ext[215] << 24);
            if (sec > dev->capacity_lba) {
                dev->capacity_lba = sec;
                dev->is_hc = 1;
                printf("mmc%u: EXT_CSD SEC_COUNT=%u\n", (unsigned)port, sec);
            }
            ssp_cmd(dev, 12, 0, RESP_R1, resp);
            ssp_cmd(dev, CMD16, MMC_BLOCK_SIZE, RESP_R1, resp);
            /* Drain one sector; EXT_CSD can leave a stale FIFO word. */
            mmc_read(dev, 0, 1, ext);
        }
    }
    printf("mmc%u: %s %s rca=%x lba=%u width=%u\n",
           (unsigned)port, dev->is_sd ? "SD" : "MMC",
           dev->cid.pnm, dev->rca, dev->capacity_lba, (unsigned)dev->bus_width);
    return 0;
}

int mmc_read(struct mmc_dev *dev, u32 lba, u32 count, void *buf)
{
    u32 addr;
    u32 done = 0;
    u8 *p = buf;

    if (!dev->ready || count == 0)
        return -1;
    addr = dev->is_hc ? lba : lba * MMC_BLOCK_SIZE;

    while (done < count) {
        u32 n = count - done;
        int rc;
        if (n > 32)
            n = 32;
        if (n == 1)
            rc = ssp_read_blocks(dev, CMD17, addr, 1, p);
        else
            rc = ssp_read_blocks(dev, CMD18, addr, n, p);
        if (rc) {
            printf("mmc%u: read lba=%u n=%u err=%d\n",
                   (unsigned)dev->port, lba + done, n, rc);
            return rc;
        }
        if (n > 1)
            ssp_cmd(dev, 12, 0, RESP_R1, NULL); /* STOP */
        p += n * MMC_BLOCK_SIZE;
        done += n;
        addr += dev->is_hc ? n : n * MMC_BLOCK_SIZE;
    }
    return 0;
}

static int ssp_write_blocks(struct mmc_dev *dev, u32 cmd, u32 arg,
                            u32 blocks, const void *buf)
{
    u32 base = dev->base;
    const u32 *p = buf;
    u32 words = blocks * (MMC_BLOCK_SIZE / 4);
    u32 put = 0;
    u32 ctrl0, guard;

    if (ssp_wait_not(base, SSP_STATUS_BUSY, 200000))
        return -1;

    writel(arg, base + HW_SSP_CMD1);
    writel(SSP_CMD0_CMD(cmd), base + HW_SSP_CMD0);
    writel(blocks * MMC_BLOCK_SIZE, base + HW_SSP_XFER_COUNT);
    writel((blocks << 4) | 9u, base + HW_SSP_BLOCK_SIZE);

    ctrl0 = SSP_CTRL0_ENABLE | SSP_CTRL0_GET_RESP | SSP_CTRL0_DATA_XFER |
            SSP_CTRL0_WAIT_FOR_CMD | SSP_CTRL0_RUN;
    writel(ctrl0, base + HW_SSP_CTRL0);

    while (put < words) {
        guard = 200000;
        while ((readl(base + HW_SSP_STATUS) & SSP_STATUS_FIFO_FULL) && --guard) {
            if (readl(base + HW_SSP_STATUS) &
                (SSP_STATUS_TIMEOUT | SSP_STATUS_DATA_CRC_ERR | SSP_STATUS_RESP_TIMEOUT))
                return -2;
        }
        if (!guard)
            return -3;
        writel(p[put++], base + HW_SSP_DATA);
    }

    guard = 400000;
    while ((readl(base + HW_SSP_CTRL0) & SSP_CTRL0_RUN) && --guard)
        ;
    if (readl(base + HW_SSP_STATUS) & (SSP_STATUS_DATA_CRC_ERR | SSP_STATUS_TIMEOUT))
        return -4;
    return 0;
}

int mmc_write(struct mmc_dev *dev, u32 lba, u32 count, const void *buf)
{
    u32 addr;
    u32 done = 0;
    const u8 *p = buf;

    if (!dev->ready || count == 0)
        return -1;
    addr = dev->is_hc ? lba : lba * MMC_BLOCK_SIZE;

    while (done < count) {
        u32 n = count - done;
        int rc;
        if (n > 16)
            n = 16;
        if (n == 1)
            rc = ssp_write_blocks(dev, 24, addr, 1, p);
        else
            rc = ssp_write_blocks(dev, 25, addr, n, p);
        if (rc) {
            printf("mmc%u: write lba=%u n=%u err=%d\n",
                   (unsigned)dev->port, lba + done, n, rc);
            return rc;
        }
        if (n > 1)
            ssp_cmd(dev, 12, 0, RESP_R1, NULL);
        p += n * MMC_BLOCK_SIZE;
        done += n;
        addr += dev->is_hc ? n : n * MMC_BLOCK_SIZE;
    }
    return 0;
}

int mmc_read_extcsd(struct mmc_dev *dev, u8 ext[512])
{
    if (dev->is_sd)
        return -1;
    return ssp_read_blocks(dev, 8, 0, 1, ext);
}

#else /* HOST_BUILD: no hardware */

int mmc_init(struct mmc_dev *dev, enum mmc_port port)
{
    memset(dev, 0, sizeof(*dev));
    dev->port = port;
    return -1;
}
int mmc_read(struct mmc_dev *dev, u32 lba, u32 count, void *buf)
{
    (void)dev; (void)lba; (void)count; (void)buf;
    return -1;
}
int mmc_write(struct mmc_dev *dev, u32 lba, u32 count, const void *buf)
{
    (void)dev; (void)lba; (void)count; (void)buf;
    return -1;
}
int mmc_read_extcsd(struct mmc_dev *dev, u8 ext[512])
{
    (void)dev; (void)ext;
    return -1;
}
#endif

void mmc_print_info(const struct mmc_dev *dev)
{
    if (!dev->ready) {
        printf("mmc%u: not ready\n", (unsigned)dev->port);
        return;
    }
    printf("mmc%u %s name='%s' mid=0x%x serial=0x%x date=%u/%u\n",
           (unsigned)dev->port, dev->is_sd ? "SD" : "eMMC",
           dev->cid.pnm, dev->cid.mid, dev->cid.psn,
           (unsigned)dev->cid.mdt_month, (unsigned)dev->cid.mdt_year);
    printf("  ocr=0x%x hc=%d lba=%u (%u MiB) width=%u\n",
           dev->ocr, dev->is_hc, dev->capacity_lba,
           dev->capacity_lba / 2048u, (unsigned)dev->bus_width);
}
