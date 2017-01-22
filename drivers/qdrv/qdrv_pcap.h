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

#ifndef _QDRV_PCAP_H
#define _QDRV_PCAP_H
#include <qtn/qtn_pcap_public.h>
#if defined(__KERNEL__)
struct qdrv_wlan;
#define QTN_GENPCAP_OP_START	0x1
#define QTN_GENPCAP_OP_STOP	0x2
#define QTN_GENPCAP_OP_FREE	0x3

#if QTN_GENPCAP
int qdrv_genpcap_set(struct qdrv_wlan *qw, int set, dma_addr_t *ctrl_dma);
void qdrv_genpcap_exit(struct qdrv_wlan *qw);
#else
static inline int qdrv_genpcap_set(struct qdrv_wlan *qw, int set, dma_addr_t *ctrl_dma)
{
	printk("%s: set QTN_GENPCAP to 1 and recompile\n", __FUNCTION__);
	*ctrl_dma = 0;
	return -1;
}
static inline void qdrv_genpcap_exit(struct qdrv_wlan *qw)
{
}
#endif	/* QTN_GENPCAP */
#endif	/* defined(__KERNEL__) */

#endif

