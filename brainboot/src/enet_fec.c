/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "xport.h"
#include "board.h"

#ifndef HOST_BUILD

static int fec_mdio_read(u32 fec, int phy, int reg)
{
    u32 v, g = 200;
    writel(FEC_IEVENT_MII, fec + FEC_IEVENT);
    v = FEC_MII_DATA_ST | FEC_MII_DATA_OP_RD | FEC_MII_DATA_TA |
        ((u32)phy << FEC_MII_DATA_PA_SHIFT) |
        ((u32)reg << FEC_MII_DATA_RA_SHIFT);
    writel(v, fec + FEC_MII_DATA);
    while (g--) {
        if (readl(fec + FEC_IEVENT) & FEC_IEVENT_MII)
            break;
        udelay(10);
    }
    if (!(readl(fec + FEC_IEVENT) & FEC_IEVENT_MII))
        return -1;
    writel(FEC_IEVENT_MII, fec + FEC_IEVENT);
    return (int)(readl(fec + FEC_MII_DATA) & 0xffffu);
}

static int fec_scan_phy(u32 fec)
{
    int a, id1;
    for (a = 0; a < 32; a++) {
        id1 = fec_mdio_read(fec, a, 2);
        if (id1 < 0)
            return -1;
        if (id1 != 0x0000 && id1 != 0xffff)
            return a;
    }
    return -1;
}

int enet_probe(struct xport_status *st)
{
    u32 ccm = IMX_CLKCTRL_BASE;
    u32 fec = IMX_ENET0_BASE;
    u32 ecr, g;

    st->enet_clk = 0;
    st->fec_present = 0;
    st->phy_addr = -1;

    writel_clr(CLKCTRL_ENET_CLKGATE, ccm + HW_CLKCTRL_ENET);
    st->enet_clk = (readl(ccm + HW_CLKCTRL_ENET) & CLKCTRL_ENET_CLKGATE) ? 0 : 1;

    ecr = readl(fec + FEC_ECNTRL);
    writel(ecr | FEC_ECNTRL_RESET, fec + FEC_ECNTRL);
    g = 10000;
    while ((readl(fec + FEC_ECNTRL) & FEC_ECNTRL_RESET) && --g)
        udelay(10);
    if (readl(fec + FEC_ECNTRL) & FEC_ECNTRL_RESET)
        return -1;
    st->fec_present = 1;

    /* MDIO clock: IPG/ (2*(speed+1)).  Use a conservative divider. */
    writel(0x1a, fec + FEC_MII_SPEED);
    st->phy_addr = fec_scan_phy(fec);
    return (st->phy_addr >= 0) ? 0 : -2;
}

#else
int enet_probe(struct xport_status *st)
{
    st->enet_clk = 0;
    st->fec_present = 0;
    st->phy_addr = -1;
    return -1;
}
#endif
