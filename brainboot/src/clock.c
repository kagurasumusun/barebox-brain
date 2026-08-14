/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "board.h"

#ifndef HOST_BUILD

static void wait_busy(u32 addr, u32 bit)
{
    u32 guard = 1000000;
    while ((readl(addr) & bit) && --guard)
        ;
}

void clock_init(void)
{
    u32 ccm = IMX_CLKCTRL_BASE;

    /*
     * Boot ROM / first-stage already brought PLL0 and DRAM up.
     * Un-bypass SSP0/SSP1 so they are sourced from the ref_io path,
     * program 96 MHz-class SSP clocks, ungated LCD/PWM XTAL.
     */
    writel_clr(CLKCTRL_SSP_CLKGATE, ccm + HW_CLKCTRL_SSP0);
    writel_clr(CLKCTRL_SSP_CLKGATE, ccm + HW_CLKCTRL_SSP1);

    /* divider: ref_io (~480 MHz) / 5 = 96 MHz. BIT[29]=BUSY */
    writel((readl(ccm + HW_CLKCTRL_SSP0) & 0xfffffe00u) | 5u, ccm + HW_CLKCTRL_SSP0);
    writel((readl(ccm + HW_CLKCTRL_SSP1) & 0xfffffe00u) | 5u, ccm + HW_CLKCTRL_SSP1);
    wait_busy(ccm + HW_CLKCTRL_SSP0, CLKCTRL_SSP_BUSY);
    wait_busy(ccm + HW_CLKCTRL_SSP1, CLKCTRL_SSP_BUSY);

    writel_clr(CLKCTRL_CLKSEQ_BYPASS_SSP0 | CLKCTRL_CLKSEQ_BYPASS_SSP1,
               ccm + HW_CLKCTRL_CLKSEQ);

    /* PWM 24 MHz from XTAL */
    writel_clr(CLKCTRL_XTAL_PWM_CLK24M_GATE, ccm + HW_CLKCTRL_XTAL);

    /* LCDIF clock ungated */
    writel_clr(1u << 31, ccm + HW_CLKCTRL_LCD);
}

u32 board_chipid(void)
{
    return readl(IMX_DIGCTL_BASE + HW_DIGCTL_CHIPID);
}

void board_reboot(void)
{
    /* Enable RTC watchdog with a short timeout. */
    writel_set(RTC_CTRL_WATCHDOGEN, IMX_RTC_BASE + HW_RTC_CTRL);
    writel(1, IMX_RTC_BASE + HW_RTC_WATCHDOG);
    for (;;)
        ;
}

#else
void clock_init(void) {}
u32 board_chipid(void) { return 0x2800u << 16; }
void board_reboot(void) {}
#endif
