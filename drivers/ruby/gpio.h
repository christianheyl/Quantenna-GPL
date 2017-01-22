/*
 * (C) Copyright 2010 Quantenna Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */


#ifndef __BOARD_RUBY_GPIO_H
#define __BOARD_RUBY_GPIO_H

#include <linux/types.h>
#define ARCH_NR_GPIOS	(22)

void gpio_config(uint32_t pin, uint32_t cfg);
void gpio_uart0_config(void);
void gpio_uart1_config(void);
void gpio_spi_flash_config(void);
int gpio_enable_pwm(uint32_t pin, uint32_t high_count, uint32_t low_count);
uint32_t gpio_disable_pwm(uint32_t pin);
void gpio_wowlan_output(uint32_t pin, uint32_t cfg);
#endif // #ifndef __BOARD_RUBY_GPIO_H

