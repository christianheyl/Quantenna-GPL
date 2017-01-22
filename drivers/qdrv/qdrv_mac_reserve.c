/**
  Copyright (c) 2008 - 2015 Quantenna Communications Inc
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

#include "qdrv_mac.h"
#include "qdrv_wlan.h"
#include "qdrv_mac_reserve.h"
#include "qtn/topaz_fwt_sw.h"

#include <qtn/topaz_tqe.h>

#define QDRV_MAC_RESERVE_MAX		6

struct qdrv_mac_reserve_ent_s {
	uint8_t addr[ETH_ALEN];
	uint8_t mask[ETH_ALEN];
};

struct qdrv_mac_reserve_s {
	struct qdrv_wlan *qw;
	uint32_t max;
	struct qdrv_mac_reserve_ent_s entry[QDRV_MAC_RESERVE_MAX];
};

static struct qdrv_mac_reserve_s qdrv_mac_reserve;

/* specialised version of compare_ether_addr() */
static inline unsigned qdrv_mac_reserve_compare_ether_addr_masked(const void *addr1,
						const void *mask, const void *addr2)
{
	const uint16_t *a = addr1;
	const uint16_t *m = mask;
	const uint16_t *b = addr2;

	return ((a[0] ^ (b[0] & m[0])) | (a[1] ^ (b[1] & m[1])) | (a[2] ^ (b[2] & m[2]))) != 0;
}

int __sram_text qdrv_mac_reserved(const uint8_t *addr)
{
	struct qdrv_mac_reserve_ent_s *res;
	int i;

	for (i = 0; i < qdrv_mac_reserve.max; i++) {
		res = &qdrv_mac_reserve.entry[i];
		if (!qdrv_mac_reserve_compare_ether_addr_masked(res->addr, res->mask, addr)) {
			RXSTAT(qdrv_mac_reserve.qw, rx_mac_reserved);
			return 1;
		}
	}

	return 0;
}
EXPORT_SYMBOL(qdrv_mac_reserved);

void qdrv_mac_reserve_clear(void)
{
	local_bh_disable();

	tqe_register_mac_reserved_cbk(NULL);
	qdrv_mac_reserve.max = 0;
	memset(&qdrv_mac_reserve.entry, 0, sizeof(qdrv_mac_reserve.entry));

	local_bh_enable();

	printk("%s: mac reservation table cleared\n", __func__);
}

/*
 * Reserve a MAC address for use by non-WiFi interfaces or clear all reserved MAC addresses.
 */
int qdrv_mac_reserve_set(const uint8_t *addr, const uint8_t *mask)
{
	int i;

	if (qdrv_mac_reserve.max > (ARRAY_SIZE(qdrv_mac_reserve.entry) - 1)) {
		printk("%s: mac address reservation for %pM failed - table is full\n", __func__,
			addr);
		return -1;
	}

	if (IEEE80211_ADDR_NULL(addr) || IEEE80211_IS_MULTICAST(addr)) {
		printk("%s: invalid mac address %pM\n", __func__, addr);
		return -1;
	}

	if (IEEE80211_ADDR_NULL(mask)) {
		printk("%s: invalid mask address %pM\n", __func__, mask);
		return -1;
	}

	local_bh_disable();

	for (i = 0; i < ETH_ALEN; i++) {
		qdrv_mac_reserve.entry[qdrv_mac_reserve.max].addr[i] = (addr[i] & mask[i]);
		qdrv_mac_reserve.entry[qdrv_mac_reserve.max].mask[i] = mask[i];
	}

	qdrv_mac_reserve.max++;
	if (qdrv_mac_reserve.max == 1)
		tqe_register_mac_reserved_cbk(qdrv_mac_reserved);

	/* clear the FWT table */
	fwt_sw_reset();

	local_bh_enable();

	printk("%s: mac address %pM/%pM reserved\n", __func__, addr, mask);

	return 0;
}

/*
 * Get the list of reserved MAC addresses.
 */
void qdrv_mac_reserve_show(struct seq_file *s, void *data, u32 num)
{
	struct qdrv_mac_reserve_ent_s *res;
	int i;

	if (strcmp(data, "full") == 0) {
		seq_printf(s, "%u\n",
			(qdrv_mac_reserve.max > ARRAY_SIZE(qdrv_mac_reserve.entry)));
		return;
	}

	seq_printf(s, "MAC address       Mask\n");
	for (i = 0; i < qdrv_mac_reserve.max; i++) {
		res = &qdrv_mac_reserve.entry[i];
		seq_printf(s, "%pM %pM\n", res->addr, res->mask);
	}
}

void qdrv_mac_reserve_init(struct qdrv_wlan *qw)
{
	qdrv_mac_reserve_clear();
	qdrv_mac_reserve.qw = qw;
}

