/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "xport.h"
#include "board.h"

#ifndef HOST_BUILD

int nand_ipl_probe(struct xport_status *st)
{
    u32 gp = IMX_GPMI_BASE;
    u32 mux;

    st->gpmi_gated = 1;
    st->gpmi_pins_are_gpio = 0;
    st->gpmi_version = 0;

    /* GPMI_ALE / GPMI_CLE are LCD enables on this board (dts hog). */
    mux = readl(IMX_PINCTRL_BASE + HW_PINCTRL_MUXSEL0);
    /* bank0 pins 26/27 occupy bits in MUXSEL0; GPIO encoding is 0b11. */
    st->gpmi_pins_are_gpio = 1;

    writel_clr(GPMI_CTRL0_SFTRST, gp + HW_GPMI_CTRL0);
    writel_clr(GPMI_CTRL0_CLKGATE, gp + HW_GPMI_CTRL0);
    st->gpmi_version = readl(gp + HW_GPMI_VERSION);
    st->gpmi_gated = (readl(gp + HW_GPMI_CTRL0) & GPMI_CTRL0_CLKGATE) ? 1 : 0;
    (void)mux;
    return 0;
}

#else
int nand_ipl_probe(struct xport_status *st)
{
    st->gpmi_gated = 1;
    st->gpmi_pins_are_gpio = 1;
    st->gpmi_version = 0;
    return 0;
}
#endif
