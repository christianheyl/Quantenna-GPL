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

#ifndef _QDRV_HAL_H
#define _QDRV_HAL_H

/* FIXME copied from qh_reg.h - hw dependent */
#define HAL_REG_OFFSET(_i)              0xE5050000+(_i)*0x10000
#define HAL_REG(_reg)                   (HAL_REG_OFFSET(0) + (_reg))

#define HAL_REG_TSF_LO			0x3014
#define HAL_REG_TSF_HI			0x3018
#define HAL_REG_RX_CSR                  0x2000          /* Rx CSR and Rx filter */

void hal_reset(void);
void hal_enable_muc(u32 muc_start_addr);
void hal_disable_muc(void);
void hal_disable_mbx(void);
void hal_enable_mbx(void);
void hal_disable_dsp(void);
void hal_enable_dsp(void);
void hal_enable_gpio(void);
void hal_dsp_start(u32 dsp_start_addr);
void hal_disable_auc(void);
void hal_enable_auc(void);
int hal_range_check_sram_addr(void *addr);
void hal_rf_enable(void);
void hal_get_tsf(uint64_t *tsf);

#endif
