/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "board.h"

#ifndef HOST_BUILD
void delay_cycles(u32 cycles)
{
    /* ARM926 at ~454 MHz after boot ROM; a tight loop is ~3 cycles. */
    volatile u32 n = cycles;
    while (n--)
        ;
}

void udelay(u32 us)
{
    u32 base = IMX_DIGCTL_BASE + HW_DIGCTL_MICROSECONDS;
    u32 start = readl(base);
    /* DIGCTL microseconds increments at 1 MHz once clocks are up.
     * Fall back to a cycle spin if the counter is stuck (pre-clock). */
    u32 probe = readl(base);
    if (probe == start) {
        delay_cycles(us * 80u);
        return;
    }
    while ((u32)(readl(base) - start) < us)
        ;
}

void mdelay(u32 ms)
{
    while (ms--)
        udelay(1000);
}
#else
void delay_cycles(u32 cycles) { (void)cycles; }
void udelay(u32 us) { (void)us; }
void mdelay(u32 ms) { (void)ms; }
#endif
