/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "board.h"

#ifndef HOST_BUILD

static u32 muxsel_addr(u32 bank, u32 pin)
{
    /* 16 pins per MUXSEL register, 2 bits each, 0x10 stride. */
    u32 reg = (bank * 2u) + (pin / 16u);
    return IMX_PINCTRL_BASE + HW_PINCTRL_MUXSEL0 + reg * 0x10u;
}

void gpio_set_mux_gpio(u32 gpio)
{
    u32 bank = GPIO_BANK(gpio);
    u32 pin = GPIO_PIN(gpio);
    u32 addr = muxsel_addr(bank, pin);
    u32 shift = (pin % 16u) * 2u;
    u32 v = readl(addr);
    v &= ~(3u << shift);
    v |= (3u << shift); /* 0b11 = GPIO */
    writel(v, addr);
}

void gpio_direction_output(u32 gpio, int value)
{
    u32 bank = GPIO_BANK(gpio);
    u32 pin = GPIO_PIN(gpio);
    u32 bit = 1u << pin;
    u32 doe = IMX_PINCTRL_BASE + HW_PINCTRL_DOE0 + bank * 0x10u;
    u32 dout = IMX_PINCTRL_BASE + HW_PINCTRL_DOUT0 + bank * 0x10u;

    gpio_set_mux_gpio(gpio);
    if (value)
        writel_set(bit, dout);
    else
        writel_clr(bit, dout);
    writel_set(bit, doe);
}

void gpio_direction_input(u32 gpio)
{
    u32 bank = GPIO_BANK(gpio);
    u32 pin = GPIO_PIN(gpio);
    u32 bit = 1u << pin;
    u32 doe = IMX_PINCTRL_BASE + HW_PINCTRL_DOE0 + bank * 0x10u;
    gpio_set_mux_gpio(gpio);
    writel_clr(bit, doe);
}

void gpio_set_value(u32 gpio, int value)
{
    u32 bank = GPIO_BANK(gpio);
    u32 pin = GPIO_PIN(gpio);
    u32 bit = 1u << pin;
    u32 dout = IMX_PINCTRL_BASE + HW_PINCTRL_DOUT0 + bank * 0x10u;
    if (value)
        writel_set(bit, dout);
    else
        writel_clr(bit, dout);
}

int gpio_get_value(u32 gpio)
{
    u32 bank = GPIO_BANK(gpio);
    u32 pin = GPIO_PIN(gpio);
    u32 din = IMX_PINCTRL_BASE + HW_PINCTRL_DIN0 + bank * 0x10u;
    return (readl(din) >> pin) & 1u;
}

#else
void gpio_set_mux_gpio(u32 gpio) { (void)gpio; }
void gpio_direction_output(u32 gpio, int value) { (void)gpio; (void)value; }
void gpio_direction_input(u32 gpio) { (void)gpio; }
void gpio_set_value(u32 gpio, int value) { (void)gpio; (void)value; }
int gpio_get_value(u32 gpio) { (void)gpio; return 0; }
#endif
