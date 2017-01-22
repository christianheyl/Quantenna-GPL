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

#ifndef _QDRV_COMM_H
#define _QDRV_COMM_H

#include <linux/skbuff.h>
#include <linux/netdevice.h>

#include <net80211/if_media.h>
#include <net80211/ieee80211_var.h>

#include <qtn/lhost_muc_comm.h>

#include "qdrv_soc.h"

#define QNET_MAXBUF_SIZE 4400
int qdrv_comm_init(struct qdrv_cb *qcb);
int qdrv_comm_exit(struct qdrv_cb *qcb);

#endif

