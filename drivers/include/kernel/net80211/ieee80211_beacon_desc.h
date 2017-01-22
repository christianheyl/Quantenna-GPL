/**
  Copyright (c) 2015 Quantenna Communications Inc
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

#ifndef IEEE80211_BEACON_DESC_H_
#define IEEE80211_BEACON_DESC_H_
#include "qtn/beacon_ioctl.h"

struct ieee80211_beacon_param_t {
	struct beacon_shared_ie_t  *head;
	struct beacon_shared_ie_t  *tail;
	uint8_t buf[BEACON_PARAM_SIZE];		/* liner buffer for the ie list */
	uint32_t curr;			/* current offset of using buffer */
	uint16_t size;				/* allocated buffer size */
};

#define IEEE80211_ELEMID_VENDOR_WME		(IEEE80211_ELEMID_VENDOR << 8 | 0x0)
#define IEEE80211_ELEMID_VENDOR_WPA		(IEEE80211_ELEMID_VENDOR << 8 | 0x1)
#define IEEE80211_ELEMID_VENDOR_ATH		(IEEE80211_ELEMID_VENDOR << 8 | 0x2)
#define IEEE80211_ELEMID_VENDOR_QTN		(IEEE80211_ELEMID_VENDOR << 8 | 0x3)
#define IEEE80211_ELEMID_VENDOR_EXT_ROLE	(IEEE80211_ELEMID_VENDOR << 8 | 0x4)
#define IEEE80211_ELEMID_VENDOR_EXT_BSSID	(IEEE80211_ELEMID_VENDOR << 8 | 0x5)
#define IEEE80211_ELEMID_VENDOR_EXT_STATE	(IEEE80211_ELEMID_VENDOR << 8 | 0x6)
#define IEEE80211_ELEMID_VENDOR_QTN_WME		(IEEE80211_ELEMID_VENDOR << 8 | 0x7)
#define IEEE80211_ELEMID_VENDOR_EPIGRAM		(IEEE80211_ELEMID_VENDOR << 8 | 0x8)
#define IEEE80211_ELEMID_VENDOR_APP		(IEEE80211_ELEMID_VENDOR << 8 | 0x9)

int ieee80211_beacon_create_param(struct ieee80211vap *vap);
void ieee80211_beacon_flush_param(struct ieee80211_beacon_param_t *param);
void ieee80211_beacon_destroy_param(struct ieee80211vap *vap);
uint8_t *ieee80211_add_beacon_desc_header(struct ieee80211_node *ni, uint8_t *frm);
uint8_t *ieee80211_add_beacon_desc_mandatory_fields(struct ieee80211_node *ni, uint8_t *frm,
		struct ieee80211_beacon_offsets *bo);
uint8_t *ieee80211_add_beacon_desc_ie(struct ieee80211_node *ni, uint16_t ext_ie_id, uint8_t *frm);
void ieee80211_dump_beacon_desc_ie(struct ieee80211_beacon_param_t *param);

#endif
