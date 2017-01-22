/**
 * Copyright (c) 2012-2013 Quantenna Communications, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/
#ifndef AUTOCONF_INCLUDED
#include <linux/config.h>
#endif
#include <linux/version.h>

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/elf.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <asm/hardware.h>

#include "qdrv_features.h"
#include "qdrv_debug.h"
#include "qdrv_mac.h"
#include "qdrv_soc.h"
#include "qdrv_muc.h"
#include "qdrv_hal.h"
#include "qdrv_wlan.h"

#include <qtn/txbf_mbox.h>


#define QDRV_MU_PROC_FILENAME	"qdrv_mu"

static struct qdrv_cb *qdrv_mu_qcb = NULL;

static const char* status_str[] = {"Enabled", "Disabled", "Freezed", "Not used"};

enum mu_grp_status {
	MU_GRP_STR_EN	= 0,
	MU_GRP_STR_DIS	= 1,
	MU_GRP_STR_FRZ	= 2,
	MU_GRP_STR_NUS	= 3,
};

static int qdrv_mu_stat_rd(char *page, char **start, off_t offset,
		int count, int *eof, void *data)
{
	struct qdrv_wlan *qw = qdrv_mu_qcb->macs[0].data;
	bool is_station = false;

	if (qw == NULL) {
		return -EFAULT;
	}

	struct ieee80211vap* vap = TAILQ_FIRST(&qw->ic.ic_vaps);
	if (vap->iv_opmode == IEEE80211_M_STA) {
		is_station = true;
	}
	struct ieee80211com* ic = &qw->ic;

	if (ic == NULL) {
		return -EFAULT;
	}

	char *p = page;
	struct qtn_mu_grp_args mu_grp_tbl[IEEE80211_MU_GRP_NUM_MAX];

	ieee80211_get_mu_grp(ic, &mu_grp_tbl[0]);

	int i, is_mu = 0;
	for (i = IEEE80211_VHT_GRP_1ST_BIT_OFFSET; i <= IEEE80211_VHT_GRP_MAX_BIT_OFFSET; i++) {
		if (mu_grp_tbl[i].grp_id == i) {
			is_mu = 1;
			int j;

			if (!is_station)
			{
				enum mu_grp_status idx = MU_GRP_STR_DIS;

				if (DSP_PARAM_GET(debug_flag) & MU_QMAT_FREEZE) {
					idx = MU_GRP_STR_FRZ;
				} else if (mu_grp_tbl[i].qmat_installed == MU_QMAT_ENABLED) {
					idx = MU_GRP_STR_EN;
				} else if (mu_grp_tbl[i].qmat_installed == MU_QMAT_NOT_USED) {
					idx = MU_GRP_STR_NUS;
				}

				p += sprintf(p, "GRP ID: %d update cnt %d", i, mu_grp_tbl[i].upd_cnt);
				p += sprintf(p, " %s\n", status_str[idx]);
				p += sprintf(p, "Rank: %d\n", mu_grp_tbl[i].rank);

				for (j = 0; j < ARRAY_SIZE(mu_grp_tbl[i].aid); j++) {
					if ( mu_grp_tbl[i].aid[j] != 0) {
						p += sprintf(p, "AID%d: 0x%04x ", j, mu_grp_tbl[i].aid[j]);
					}
				}

				p += sprintf(p, "\n");

				for (j = 0; j < ARRAY_SIZE(mu_grp_tbl[i].ncidx); j++) {
					if ( mu_grp_tbl[i].ncidx[j] != 0) {
						p += sprintf(p, "IDX%d:   %4d ", j, mu_grp_tbl[i].ncidx[j]);
					}
				}

				p += sprintf(p, "\n");

				if(mu_grp_tbl[i].qmat_installed == MU_QMAT_ENABLED ||
					mu_grp_tbl[i].qmat_installed == MU_QMAT_FREEZED) {
					p += sprintf(p, "u0_1ss_u1_1ss: 0x%x\n", mu_grp_tbl[i].u0_1ss_u1_1ss);
					p += sprintf(p, "u0_2ss_u1_1ss: 0x%x\n", mu_grp_tbl[i].u0_2ss_u1_1ss);
					p += sprintf(p, "u0_3ss_u1_1ss: 0x%x\n", mu_grp_tbl[i].u0_3ss_u1_1ss);
					p += sprintf(p, "u0_1ss_u1_2ss: 0x%x\n", mu_grp_tbl[i].u0_1ss_u1_2ss);
					p += sprintf(p, "u0_1ss_u1_3ss: 0x%x\n", mu_grp_tbl[i].u0_1ss_u1_3ss);
					p += sprintf(p, "u0_2ss_u1_2ss: 0x%x\n", mu_grp_tbl[i].u0_2ss_u1_2ss);
				}
			}
			else
			{
				p += sprintf(p, "AP GRP ID: %d update cnt %d\n", i, mu_grp_tbl[i].upd_cnt);
				for (j = 0; j < ARRAY_SIZE(mu_grp_tbl[i].aid); j++) {
					if ( mu_grp_tbl[i].aid[j] != 0) {
						p += sprintf(p, "User pos = %d with AID = 0x%04x\n", j, mu_grp_tbl[i].aid[j]);
					}
				}
				for (j = 0; j < ARRAY_SIZE(mu_grp_tbl[i].ncidx); j++) {
					if ( mu_grp_tbl[i].ncidx[j] != 0) {
						p += sprintf(p, "Local node index (Idx) = %d\n", mu_grp_tbl[i].ncidx[j]);
					}
				}
			}
		}
	}

	if (!is_mu) {
		p += sprintf(p, "No MU groups found\n");
	}

	return p - page;
}

int qdrv_mu_stat_init(struct qdrv_cb *qcb)
{
	printk("qdrv_mu_stat_init\n");

	if (qcb == NULL) {
		printk("qdrv_mu_stat_init: NULL qcb\n");
		return -EFAULT;
	}

	qdrv_mu_qcb = qcb;

	if (!create_proc_read_entry(QDRV_MU_PROC_FILENAME, 0,
				NULL, qdrv_mu_stat_rd, NULL)) {
		return -EEXIST;
	}

	return 0;
}

int qdrv_mu_stat_exit(struct qdrv_cb *qcb)
{
	if (qdrv_mu_qcb != NULL) {
		remove_proc_entry(QDRV_MU_PROC_FILENAME, 0);
	}

	return 0;
}

