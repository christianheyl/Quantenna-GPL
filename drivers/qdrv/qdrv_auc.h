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

#ifndef _QDRV_AUC_H
#define _QDRV_AUC_H

void qdrv_auc_stats_setup(void);
qtn_shared_node_stats_t* qdrv_auc_get_node_stats(uint8_t node);
qtn_shared_vap_stats_t* qdrv_auc_get_vap_stats(uint8_t vapid);
int qdrv_auc_init(struct qdrv_cb *qcb);
int qdrv_auc_exit(struct qdrv_cb *qcb);

#endif
