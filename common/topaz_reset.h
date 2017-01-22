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

/* This header file defines reset function to be used on Topaz */

#ifndef __TOPAZ_RESET_H
#define __TOPAZ_RESET_H

#include <include/qtn/mproc_sync_base.h>
#ifdef TOPAZ_AMBER_IP
#include <include/qtn/amber.h>
#endif

static void topaz_set_reset_vec(int enable, unsigned long reset)
{
#ifdef TOPAZ_AMBER_IP
	unsigned long flush_mask = 0;

	switch (reset) {
	case TOPAZ_SYS_CTL_RESET_AUC:
		flush_mask = TOPAZ_AMBER_BUS_FLUSH_AUC;
		break;
	case RUBY_SYS_CTL_RESET_DSP_ALL:
		flush_mask = TOPAZ_AMBER_BUS_FLUSH_DSP;
		break;
	case RUBY_SYS_CTL_RESET_MUC_ALL:
		flush_mask = TOPAZ_AMBER_BUS_FLUSH_MUC;
		break;
	case RUBY_SYS_CTL_RESET_ENET0:
		flush_mask = TOPAZ_AMBER_BUS_FLUSH_RGMII;
		break;
	case RUBY_SYS_CTL_RESET_IOSS:
		flush_mask = TOPAZ_AMBER_BUS_FLUSH_BRIDGE | TOPAZ_AMBER_BUS_FLUSH_DMA;
		break;
	case RUBY_SYS_CTL_RESET_MAC:
		flush_mask = TOPAZ_AMBER_BUS_FLUSH_WMAC;
		break;
	case RUBY_SYS_CTL_RESET_BB:
		flush_mask = 0;
		break;
	default:
		/* In the case we accidentally get here - request/release flush for everything to be safe */
		flush_mask = TOPAZ_AMBER_BUS_FLUSH_AUC |
			TOPAZ_AMBER_BUS_FLUSH_DSP |
			TOPAZ_AMBER_BUS_FLUSH_MUC |
			TOPAZ_AMBER_BUS_FLUSH_RGMII |
			TOPAZ_AMBER_BUS_FLUSH_BRIDGE |
			TOPAZ_AMBER_BUS_FLUSH_DMA |
			TOPAZ_AMBER_BUS_FLUSH_WMAC |
			TOPAZ_AMBER_BUS_FLUSH_LHOST;
		qtn_mproc_sync_log("%s:%u: error - invalid reset flag 0x%08x\n", __FILE__, __LINE__, reset);
		break;
	}

	if (!enable && flush_mask) {
		/* Need to request bus flush before switching off */
		amber_bus_flush_req(flush_mask);
	}
#endif

	qtn_mproc_sync_mem_write(RUBY_SYS_CTL_CPU_VEC_MASK, reset);
	qtn_mproc_sync_mem_write_wmb(RUBY_SYS_CTL_CPU_VEC, enable ? reset : 0);
	qtn_mproc_sync_mem_write_wmb(RUBY_SYS_CTL_CPU_VEC_MASK, 0);

#ifdef TOPAZ_AMBER_IP
	if (enable && flush_mask) {
		/* Need to release bus flush after switching on */
		amber_bus_flush_release(flush_mask);
	}
#endif

}
#endif // #ifndef __TOPAZ_RESET_H


