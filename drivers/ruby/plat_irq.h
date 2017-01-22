/*
 * (C) Copyright 2010 Quantenna Communications Inc.
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

#ifndef __BOARD_RUBY_PLAT_IRQ_H
#define __BOARD_RUBY_PLAT_IRQ_H

#include "platform.h"

#define NR_IRQS 64

#define TIMER0_INT RUBY_IRQ_CPUTIMER0
#define TIMER1_INT RUBY_IRQ_CPUTIMER1

#define __ARCH_IRQ_EXIT_IRQS_DISABLED 1 /* tell Linux that we keep IRQs disabled while in interrupt */

#define __ARCH_USE_SOFTIRQ_DYNAMIC_MAX_RESTART /* balance how many times SoftIRQ be restarted before offload to softirqd */
//#define __ARCH_USE_SOFTIRQ_BALANCE /* more aggressively offload SoftIRQ processing to softirqd */

#endif // #ifndef __BOARD_RUBY_PLAT_IRQ_H

