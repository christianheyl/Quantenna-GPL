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
#ifndef _QDRV_TXBF_H_
#define _QDRV_TXBF_H_

#include <qtn/txbf_common.h>
#include <common/queue.h>

/* Number of NDPs that can be in process at a given time
 * DSP and ARM inclusive
 */
#define NUM_TXBF_PKTS (1)

#define TXBF_BUFF_SIZE (sizeof(u32) * 4 * 4 * 64 * 2)

struct txbf_state
{
	struct tasklet_struct txbf_dsp_mbox_task;
	volatile u8 send_txbf_netdebug; 
	volatile u8 st_mat_calc_chan_inv ; 
	volatile u8 st_mat_calc_two_streams; 
	volatile u8 st_mat_apply_per_ant_scaling;
	volatile u8 st_mat_apply_stream_mixing;
	volatile s8 st_mat_reg_scale_fac;
	unsigned stvec_install_success;
	unsigned stvec_install_fail;	
	unsigned stvec_overwrite;	
	unsigned svd_comp_bypass;
	unsigned stmat_install_bypass;
	unsigned cmp_act_frms_sent;
	unsigned uncmp_act_frms_sent;
	unsigned cmp_act_frms_rxd;
	unsigned uncmp_act_frms_rxd;
	unsigned qmat_bandwidth;
	unsigned qmat_offset;
	unsigned bf_ver;
	uint8_t	bf_tone_grp;
	void *owner;
};

int qdrv_txbf_init(struct qdrv_wlan *qw);
int qdrv_txbf_exit(struct qdrv_wlan *qw);
int qdrv_txbf_config_set(struct qdrv_wlan *qw, u32 value);
int qdrv_txbf_config_get(struct qdrv_wlan *qw, u32 *value);

#endif
