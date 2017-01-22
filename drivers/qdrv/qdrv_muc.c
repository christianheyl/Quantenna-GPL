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
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/elf.h>
#include <asm/io.h>
#include <asm/hardware.h>
#include "qdrv_features.h"
#include "qdrv_debug.h"
#include "qdrv_mac.h"
#include "qdrv_soc.h"
#include "qdrv_muc.h"
#include "qdrv_hal.h"
#include "qdrv_wlan.h"
#include "qdrv_fw.h"
#include <asm/board/board_config.h>
#include <asm/board/gpio.h>

static int
adjust_muc_firmware_path( char *muc_firmware, size_t max_firmware_len )
{
	int      retval = 0;
	enum {
		SPI_shift_count = 7,
		RFIC_prj_shift_count = 5, 
           	RFIC_prj_mask = 0x07,
		RFIC3_value = 0,
		RFIC4_value = 1
	};

	const char      *default_suffix =".bin";
	const char      *RFIC3_suffix =".bin";
	const char      *RFIC4_suffix =".RFIC4.bin";

#ifdef FIXME_NOW
	u32		*RFIC_ver_addr = ioremap_nocache(RFIC_VERSION, 4);
	u32		 RFIC_ver_val = *RFIC_ver_addr;
	u32		 RFIC_prj_val = *RFIC_ver_addr;
#else
	u32 RFIC_prj_val = RFIC3_value;
#endif
	char		*tmpaddr = strstr( muc_firmware, default_suffix );
	unsigned int	 cur_firmware_len  = 0;

	if (tmpaddr != NULL)
	  *tmpaddr = '\0';

	cur_firmware_len = strnlen( muc_firmware, max_firmware_len );
	if (cur_firmware_len + strlen( RFIC4_suffix ) >= max_firmware_len)
	  return( -1 );

#ifdef FIXME_NOW
	RFIC_ver_val = RFIC_ver_val >> SPI_shift_count;
	RFIC_prj_val = ((RFIC_ver_val >> RFIC_prj_shift_count) & RFIC_prj_mask);
#endif

	switch (RFIC_prj_val) {
	case RFIC3_value:
		strcat( muc_firmware, RFIC3_suffix );
		break;
#ifdef FIXME_NOW
	case RFIC4_value:
		strcat( muc_firmware, RFIC4_suffix );
		break;

	default:
		retval = -1;
		break;
#endif
	}

	return( retval );
}

int qdrv_muc_init(struct qdrv_cb *qcb)
{
	u32 muc_start_addr = 0;
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	if (get_bootcfg_scancnt() == 0) {
#ifndef TOPAZ_AMBER_IP
		gpio_config(RUBY_GPIO_LNA_TOGGLE, RUBY_GPIO_ALT_OUTPUT);
#else
		/*
		 * In Amber GPIO pins are not shared. No need to set up alternate function.
		 */
#endif
	}

	if (adjust_muc_firmware_path( qcb->muc_firmware, sizeof( qcb->muc_firmware )) < 0) {
		DBGPRINTF_E( "Adjusting firmware path failed\n");
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return( -1 );
	}

	if (qdrv_fw_load_muc(qcb->dev, qcb->muc_firmware, &muc_start_addr) < 0) {
		DBGPRINTF_E( "Failed to load firmware\n");
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return(-1);
	}

#ifdef FIXME_NOW
	hal_rf_enable();
#endif

	hal_enable_muc(muc_start_addr);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return(0);
}

int qdrv_muc_exit(struct qdrv_cb *qcb)
{
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	hal_disable_muc();

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return(0);
}
