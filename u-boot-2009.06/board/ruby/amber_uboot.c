/*
 * (C) Copyright 2015 Quantenna Communications Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
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

#include <common.h>
#include <asm/arch/platform.h>
#include <asm/io.h>
#include <amber.h>
#include "ruby.h"

int amber_trigger_wifi2soc_interrupt(unsigned long interrupt_code)
{
	unsigned long interrupt_errors_mask = readl(TOPAZ_AMBER_WIFI2SOC_MASK_REG);

	interrupt_code &= interrupt_errors_mask;

	if (interrupt_code == 0) {
		return 0;
	}


	while (readl(TOPAZ_AMBER_WIFI2SOC_ERROR_REG) & interrupt_errors_mask);

	writel(interrupt_code, TOPAZ_AMBER_WIFI2SOC_ERROR_REG);
	gpio_output(TOPAZ_AMBER_WIFI2SOC_INT_OUTPUT, 1);
	udelay(1);
	gpio_output(TOPAZ_AMBER_WIFI2SOC_INT_OUTPUT, 0);
	return 0;
}

