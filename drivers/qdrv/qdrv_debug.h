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

#ifndef _QDRV_DEBUG_H
#define _QDRV_DEBUG_H
#include <qtn/qtn_debug.h>
/* Global debug log messsage function mask definition of the moudle qdrv */
#define QDRV_LF_TRACE				DBG_LF_00
#define QDRV_LF_BRIDGE				DBG_LF_01
#define QDRV_LF_RADAR				DBG_LF_02
#define QDRV_LF_IGMP				DBG_LF_03
#define QDRV_LF_PKT_TX				DBG_LF_04
#define QDRV_LF_PKT_RX				DBG_LF_05
#define QDRV_LF_CALCMD				DBG_LF_06
#define QDRV_LF_HLINK				DBG_LF_07
#define QDRV_LF_TXBF				DBG_LF_08
#define QDRV_LF_WLAN				DBG_LF_09
#define QDRV_LF_VAP				DBG_LF_10
#define QDRV_LF_DUMP_RX_PKT			DBG_LF_11
#define QDRV_LF_DUMP_TX_PKT			DBG_LF_12
#define	QDRV_LF_DUMP_MGT			DBG_LF_13 /* management frames except for beacon and action */
#define	QDRV_LF_DUMP_BEACON			DBG_LF_14 /* beacon frame */
#define	QDRV_LF_DUMP_ACTION			DBG_LF_15 /* action frame */
#define	QDRV_LF_DUMP_DATA			DBG_LF_16 /* data frame */
#define QDRV_LF_DFS_QUICKTIMER			DBG_LF_17
#define QDRV_LF_DFS_DONTCAREDOTH		DBG_LF_18
#define QDRV_LF_DFS_TESTMODE			DBG_LF_19
#define QDRV_LF_DFS_DISALLOWRADARDETECT		DBG_LF_20
#define QDRV_LF_QCTRL				DBG_LF_21 /* qdrv control */
#define QDRV_LF_CMM				DBG_LF_22
#define QDRV_LF_DSP				DBG_LF_23
#define QDRV_LF_AUC				DBG_LF_24
#define QDRV_LF_VSP				DBG_LF_24
#define QDRV_LF_ALL				DBG_LF_ALL
#define DBG_LM					DBG_LM_QDRV

extern unsigned int g_dbg_dump_pkt_len;

#define	IS_BEACON(wh) \
    (((wh)->i_fc[0] & (IEEE80211_FC0_TYPE_MASK|IEEE80211_FC0_SUBTYPE_MASK)) == \
	 (IEEE80211_FC0_TYPE_MGT|IEEE80211_FC0_SUBTYPE_BEACON))

#define	IS_ACTION(wh) \
    (((wh)->i_fc[0] & (IEEE80211_FC0_TYPE_MASK|IEEE80211_FC0_SUBTYPE_MASK)) == \
	 (IEEE80211_FC0_TYPE_MGT|IEEE80211_FC0_SUBTYPE_ACTION))

#define	IS_MGT(wh) \
    (((wh)->i_fc[0] & (IEEE80211_FC0_TYPE_MASK)) == (IEEE80211_FC0_TYPE_MGT))

#define	IS_DATA(wh) \
    (((wh)->i_fc[0] & (IEEE80211_FC0_TYPE_MASK)) == (IEEE80211_FC0_TYPE_DATA))

#define	IFF_DUMPPKTS_RECV(wh, f)								\
	(((DBG_LOG_FUNC_TEST(QDRV_LF_PKT_RX)) &&						\
	((DBG_LOG_LEVEL >= (DBG_LL_TRIAL)) ||							\
	((!IS_BEACON(wh)) && (IS_MGT(wh)) && DBG_LOG_LEVEL >= (DBG_LL_INFO)))) ||		\
	((f & QDRV_LF_DUMP_RX_PKT) &&								\
	(((IS_MGT(wh)) &&									\
	(((f & QDRV_LF_DUMP_MGT) && !IS_BEACON(wh) && !IS_ACTION(wh))				\
	|| ((f & QDRV_LF_DUMP_BEACON) && (IS_BEACON(wh)))					\
	|| ((f & QDRV_LF_DUMP_ACTION) && (IS_ACTION(wh)))))					\
	|| ((f & QDRV_LF_DUMP_DATA) && IS_DATA(wh)))))

#define	IFF_DUMPPKTS_XMIT_MGT(wh, f)								\
	(((DBG_LOG_FUNC_TEST(QDRV_LF_PKT_TX)) &&						\
	(DBG_LOG_LEVEL >= (DBG_LL_INFO))) ||							\
	((f & QDRV_LF_DUMP_TX_PKT) &&								\
	(((f & QDRV_LF_DUMP_MGT) && !(IS_ACTION(wh)))						\
	|| ((f & QDRV_LF_DUMP_ACTION) && (IS_ACTION(wh))))))

#define	IFF_DUMPPKTS_XMIT_DATA(f)								\
	(((DBG_LOG_FUNC_TEST(QDRV_LF_PKT_TX)) &&						\
	(DBG_LOG_LEVEL >= (DBG_LL_INFO))) ||							\
	((f & QDRV_LF_DUMP_TX_PKT) &&								\
	(f & QDRV_LF_DUMP_DATA)))
#endif
