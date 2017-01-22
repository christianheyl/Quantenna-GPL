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

#ifndef __QDRV_SCH_WMM_H
#define __QDRV_SCH_WMM_H

struct qdrv_sch_band_aifsn {
	int band_prio;
	int aifsn;
};

void qdrv_sch_set_remap_qos(u32 value);
u32 qdrv_sch_get_remap_qos(void);

#endif // __QDRV_SCH_WMM_H
