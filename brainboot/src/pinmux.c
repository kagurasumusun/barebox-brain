/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "board.h"
#include "gpio.h"

#ifndef HOST_BUILD

/*
 * i.MX28 PINCTRL MUXSEL: 2 bits per pin, 16 pins per register.
 * 0 = main function 0, 1 = alt1, 2 = alt2, 3 = GPIO.
 *
 * Values follow the public PW-SH6 iomux (same gen3_6 platform as ED-AA2).
 */

static void mux_set(u32 bank, u32 pin, u32 func)
{
    u32 reg = (bank * 2u) + (pin / 16u);
    u32 addr = IMX_PINCTRL_BASE + HW_PINCTRL_MUXSEL0 + reg * 0x10u;
    u32 shift = (pin % 16u) * 2u;
    u32 v = readl(addr);
    v &= ~(3u << shift);
    v |= (func & 3u) << shift;
    writel(v, addr);
}

static void pull_set(u32 bank, u32 pin, int enable)
{
    u32 addr = IMX_PINCTRL_BASE + HW_PINCTRL_PULL0 + bank * 0x10u;
    u32 bit = 1u << pin;
    if (enable)
        writel_set(bit, addr);
    else
        writel_clr(bit, addr);
}

void pinmux_init(void)
{
    int i;

    /* SSP0 eMMC: DATA0-7, CMD = func0, pull-up. SCK func0 no pull. */
    for (i = 0; i <= 7; i++) {
        mux_set(2, i, 0);          /* SSP0_DATA0..7 are bank2 pin0..7 */
        pull_set(2, i, 1);
    }
    mux_set(2, 8, 0);              /* SSP0_CMD */
    pull_set(2, 8, 1);
    mux_set(2, 10, 0);             /* SSP0_SCK */
    pull_set(2, 10, 0);
    mux_set(2, 9, 0);              /* SSP0_DETECT */
    pull_set(2, 9, 0);

    /* SSP1 SD: GPMI_D00..D03 = SSP1_D0..D3 (bank0 pin0..3 alt1=1) */
    for (i = 0; i <= 3; i++) {
        mux_set(0, i, 1);
        pull_set(0, i, 1);
    }
    mux_set(0, 21, 1);             /* GPMI_RDY1 = SSP1_CMD */
    pull_set(0, 21, 1);
    mux_set(0, 24, 1);             /* GPMI_WRN = SSP1_SCK */
    pull_set(0, 24, 0);
    mux_set(0, 20, 1);             /* GPMI_RDY0 = SSP1_DETECT */
    pull_set(0, 20, 0);

    /* LCD data D00-D15: bank1 pin0-15 func0, 1.8V, no pull */
    for (i = 0; i < 16; i++) {
        mux_set(1, i, 0);
        pull_set(1, i, 0);
    }
    mux_set(1, 19, 0);             /* LCD_RD_E */
    mux_set(1, 20, 0);             /* LCD_WR_RWN */
    mux_set(1, 21, 0);             /* LCD_RS */
    mux_set(1, 22, 0);             /* LCD_CS */
    mux_set(1, 18, 0);             /* LCD_RESET / VSYNC */

    /* Debug UART: AUART0/DUART pins are typically bank3. Leave as Boot ROM
     * configured them; we only talk to the already-routed UARTDBG. */

    /* Power enables. GPIO2.21 is a keyboard ROW in keybd_EDNA2.dll —
     * do not steal it as SD_POWER (u-boot does; that breaks the matrix). */
    gpio_direction_output(GPIO_EMMC_POWER, 1);
    gpio_direction_output(GPIO_LCD_EN0, 1);
    gpio_direction_output(GPIO_LCD_EN1, 1);
    gpio_direction_output(GPIO_LCD_EN2, 1);
    gpio_direction_output(GPIO_BL_ENABLE, 1);
}

void board_early_init(void)
{
    clock_init();
    pinmux_init();
}

#else
void pinmux_init(void) {}
void clock_init(void);
void board_early_init(void) {}
#endif
