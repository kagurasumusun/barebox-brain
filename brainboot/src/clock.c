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
    writel_set(RTC_CTRL_WATCHDOGEN, IMX_RTC_BASE + HW_RTC_CTRL);
    writel(1, IMX_RTC_BASE + HW_RTC_WATCHDOG);
    for (;;)
        ;
}

void board_poweroff(void)
{
    /* i.MX28 HW_POWER_RESET.PWD with unlock key 0x3E77 */
    writel(POWER_RESET_UNLOCK | POWER_RESET_PWD, IMX_POWER_BASE + HW_POWER_RESET);
    for (;;)
        ;
}

u32 board_cpu_hz(void)
{
    u32 cpu = readl(IMX_CLKCTRL_BASE + HW_CLKCTRL_CPU);
    u32 div = cpu & 0x3fu;
    if (div == 0)
        div = 1;
    /* ref_cpu is PLL0 / FRAC. PLL0 is 480 MHz when locked. */
    return 480000000u / div;
}

u32 board_ocotp_lock(void)
{
    u32 ctrl = IMX_OCOTP_BASE + HW_OCOTP_CTRL;
    u32 g = 100000;
    writel_set(OCOTP_CTRL_RD_BANK_OPEN, ctrl);
    while ((readl(ctrl) & OCOTP_CTRL_BUSY) && --g)
        ;
    return readl(IMX_OCOTP_BASE + HW_OCOTP_LOCK);
}

u32 board_rtc_seconds(void)
{
    return readl(IMX_RTC_BASE + HW_RTC_SECONDS);
}

int board_probe_ocram(void)
{
    volatile u32 *p = (volatile u32 *)0x00018000u;
    u32 old = *p;
    *p = 0xA5A55A5Au;
    if (*p != 0xA5A55A5Au)
        return 0;
    *p = old;
    return 1;
}

int board_probe_dram(void)
{
    volatile u32 *p = (volatile u32 *)0x47E00000u;
    u32 old = *p;
    *p = 0x5A5AA5A5u;
    if (*p != 0x5A5AA5A5u)
        return 0;
    *p = old;
    return 1;
}

u32 board_power_sts(void)
{
    return readl(IMX_POWER_BASE + HW_POWER_STS);
}

#else
void clock_init(void) {}
u32 board_chipid(void) { return 0x2800u << 16; }
void board_reboot(void) {}
void board_poweroff(void) {}
u32 board_cpu_hz(void) { return 0; }
u32 board_ocotp_lock(void) { return 0; }
u32 board_rtc_seconds(void) { return 0; }
int board_probe_ocram(void) { return 0; }
int board_probe_dram(void) { return 0; }
u32 board_power_sts(void) { return 0; }
#endif
