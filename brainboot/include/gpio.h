/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef GPIO_H
#define GPIO_H

#include "brainboot.h"

void gpio_set_mux_gpio(u32 gpio);
void gpio_direction_output(u32 gpio, int value);
void gpio_direction_input(u32 gpio);
void gpio_set_value(u32 gpio, int value);
int  gpio_get_value(u32 gpio);

#endif
