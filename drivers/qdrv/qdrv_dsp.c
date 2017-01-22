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

#ifndef AUTOCONF_INCLUDED
#include <linux/config.h>
#endif
#include <linux/version.h>

#include <linux/device.h>
#include <linux/firmware.h>
#include <asm/io.h>
#include "qdrv_features.h"
#include "qdrv_debug.h"
#include "qdrv_mac.h"
#include "qdrv_soc.h"
#include "qdrv_dsp.h"
#include "qdrv_hal.h"
#include "qdrv_fw.h"
#include <qtn/registers.h>

int qdrv_dsp_init(struct qdrv_cb *qcb)
{
	u32 dsp_start_addr = 0;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter");

	if (qdrv_fw_load_dsp(qcb->dev, qcb->dsp_firmware, &dsp_start_addr) < 0) {
		DBGPRINTF_E("dsp load firmware failed\n");
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return(-1);
	}

	DBGPRINTF(DBG_LL_INFO, QDRV_LF_DSP, "Firmware start address is %x\n", dsp_start_addr);

	hal_dsp_start(dsp_start_addr);
	hal_enable_dsp();

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return(0);
}

int qdrv_dsp_exit(struct qdrv_cb *qcb)
{
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter");

	hal_disable_dsp();

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return(0);
}
