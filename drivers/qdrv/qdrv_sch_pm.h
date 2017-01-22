/**
  Copyright (c) 2008 - 2013 Quantenna Communications Inc
  All Rights Reserved

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

 **/
#ifndef __QDRV_SCH_PM_H
#define __QDRV_SCH_PM_H
#include <linux/module.h>
#include <linux/types.h>
#include <linux/skbuff.h>

#include <asm/atomic.h>

#include "qdrv_sch_const.h"

#define	QDRV_SCH_PM	0

#if QDRV_SCH_PM
void qdrv_sch_pm_init(void);
void qdrv_sch_pm_exit(void);

static inline __sram_text void qdrv_sch_enqueue_pm(void)
{
	extern atomic_t qdrv_sch_pm_enqueued_counter;
	atomic_inc(&qdrv_sch_pm_enqueued_counter);
}
#endif	// QDRV_SCH_PM

#endif // __QDRV_SCH_PM_H

