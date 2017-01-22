/*
 * (C) Copyright 2011 Quantenna Communications Inc.
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

#ifndef __RUBY_VERSION_H
#define __RUBY_VERSION_H

#ifndef __ASSEMBLY__

#include "mproc_sync_base.h"

RUBY_INLINE int get_hardware_revision(void)
{
	volatile struct shared_params* sp = qtn_mproc_sync_shared_params_get();
	if (sp) {
		return sp->hardware_revision;
	} else {
		return HARDWARE_REVISION_UNKNOWN;
	}
}

#ifdef __KERNEL__
RUBY_INLINE int _read_hardware_revision(void)
{
	int ret = HARDWARE_REVISION_UNKNOWN;
	uint32_t board_rev = readl(RUBY_SYS_CTL_CSR);

	if ((board_rev & CHIP_ID_MASK) == CHIP_ID_RUBY) {
		uint32_t spare1 = readl(RUBY_QT3_BB_TD_SPARE_1);
		if ((spare1 & CHIP_REV_ID_MASK)  == REV_ID_RUBY_A) {
			ret = HARDWARE_REVISION_RUBY_A;
		} else if ((spare1 & CHIP_REV_ID_MASK) == REV_ID_RUBY_B) {
			ret = HARDWARE_REVISION_RUBY_B;
		} else if ((spare1 & CHIP_REV_ID_MASK) == REV_ID_RUBY_D){
			ret = HARDWARE_REVISION_RUBY_D;
		}
	} else if ((board_rev & CHIP_ID_MASK) == CHIP_ID_TOPAZ) {
		switch (board_rev & CHIP_REV_ID_MASK) {
			case REV_ID_TOPAZ_A:
				ret = HARDWARE_REVISION_TOPAZ_A;
				break;
			case REV_ID_TOPAZ_B:
				ret = HARDWARE_REVISION_TOPAZ_B;
				break;
			case REV_ID_TOPAZ_A2:
				ret = HARDWARE_REVISION_TOPAZ_A2;
				break;
		}
	}
	return ret;
}
#endif //__KERNEL__

#endif	// __ASSEMBLY__
#endif	// __RUBY_VERSION_H

