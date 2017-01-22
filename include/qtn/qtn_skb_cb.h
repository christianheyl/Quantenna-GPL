/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __QTN_SKB_CB_H
#define __QTN_SKB_CB_H

struct qtn_skb_cb {
	uint8_t		encap;		/* Encapsulation type */
	uint8_t		ip_protocol;	/* IP protocol */
	uint16_t	ether_type;	/* Ethernet Type */
	void		*ni;		/* Node structure pointer */
	uint32_t	flags;
#define M_LINK0			0x0001	/* frame needs WEP encryption */
#define M_FF			0x0002	/* fast frame */
#define M_PWR_SAV		0x0004	/* bypass powersave handling */
#define M_UAPSD			0x0008	/* frame flagged for u-apsd handling */
#define M_RAW			0x0010
#define M_CLASSIFY		0x0020	/* Packet has been classified */
#define M_VSP_CHK		0x0040	/* VSP check done */
#define M_VSP_NOT_FOUND		0x0080	/* VSP stream not found - usually first pkts from Eth */
#define M_VSP_RX_BSS		0x0100	/* VSP stream originating from the BSS */
#define M_RATE_TRAINING		0x0200	/* Empty data frame used to do rate training */
#define M_NO_AMSDU		0x0400	/* AMSDU is prohibited for this frame */
#define M_ENQUEUED_SCH		0x0800	/* Enqueued in qdrv_sch */
#define M_ENQUEUED_MUC		0x1000	/* Enqueued to MuC */
#define	M_TX_DONE_IMM_INT	0x2000	/* Immediately interrupt lhost when tx done */
#define M_VLAN_TAGGED		0x4000	/* skb belongs to some VLAN */
#define M_ORIG_OUTSIDE		0x8000	/* skb is not from local protocol stack */
#define M_ORIG_BR		0x10000	/* skb is sent from bridge interfaces */
#define M_NO_L2_LRN		0x20000	/* MAC learning disabled */
};

#define QTN_SKB_ENCAP_ETH		0
#define QTN_SKB_ENCAP_80211_MGMT	1
#define QTN_SKB_ENCAP_80211_DATA	2
#define QTN_SKB_ENCAP(_skb)		((_skb)->qtn_cb.encap)
#define QTN_SKB_ENCAP_IS_80211(_skb)	((_skb)->qtn_cb.encap > 0)
#define QTN_SKB_ENCAP_IS_80211_MGMT(_skb) \
					((_skb)->qtn_cb.encap == QTN_SKB_ENCAP_80211_MGMT)

#define QTN_SKB_CB_NI(_skb)		((_skb)->qtn_cb.ni)
#define QTN_SKB_CB_ETHERTYPE(_skb)	((_skb)->qtn_cb.ether_type)
#define QTN_SKB_CB_IPPROTO(_skb)	((_skb)->qtn_cb.ip_protocol)

#define M_FLAG_SET(_skb, _flag)		((_skb)->qtn_cb.flags |= _flag)
#define M_FLAG_CLR(_skb, _flag)		((_skb)->qtn_cb.flags &= ~_flag)
#define M_FLAG_GET(_skb, _flag)		((_skb)->qtn_cb.flags & _flag)
#define M_FLAG_ISSET(_skb, _flag)	(!!((_skb)->qtn_cb.flags & _flag))
#define M_FLAG_KEEP_ONLY(_skb, _flag)	((_skb)->qtn_cb.flags &= _flag)

#define M_PWR_SAV_SET(skb)		M_FLAG_SET((skb), M_PWR_SAV)
#define M_PWR_SAV_CLR(skb)		M_FLAG_CLR((skb), M_PWR_SAV)
#define M_PWR_SAV_GET(skb)		M_FLAG_GET((skb), M_PWR_SAV)

#endif /* #ifndef __QTN_SKB_CB_H */
