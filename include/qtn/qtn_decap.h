#ifndef __QTN_DECAP_H__
#define __QTN_DECAP_H__

#include <net80211/ieee80211.h>
#include <net80211/if_ethersubr.h>
#include <net80211/if_llc.h>

#include <qtn/qtn_net_packet.h>
#include <qtn/qtn_vlan.h>

/*
 * Length of received frame that requires dcache invalidate on receive.
 * The amount that must be read is:
 * - VLAN encap case: MAX_VLANS * (LLC + 2b) + LLC
 * - 802.11 MPDU, no amsdu: LLC + max l3 depth
 * - 802.11 AMSDU: msdu header + LLC + max l3 depth
 *
 * The max of these three is the VLAN case. There is also an assumption
 * here that if VLANs are processed, there is no need to process L3 header
 */
#define QTN_RX_LLC_DCACHE_INV_LEN	(((LLC_SNAPFRAMELEN + 2) * QTN_MAX_VLANS) + LLC_SNAPFRAMELEN)
#define QTN_RX_MPDU_DCACHE_INV_LEN	(QTN_RX_LLC_DCACHE_INV_LEN + sizeof(struct ieee80211_qosframe_addr4))
#define QTN_RX_MSDU_DCACHE_INV_LEN	(QTN_RX_LLC_DCACHE_INV_LEN + sizeof(struct ether_header))

struct qtn_rx_decap_info {
	void		*start;
	uint16_t	len;
	struct ether_header eh;			/* the eth header to be written to the packet */
	uint32_t	vlanh[QTN_MAX_VLANS];	/* space for vlan headers (must be after eh) */
	const void	*l3hdr;			/* pointer to layer 3 header in the payload */
	uint16_t	l3_ether_type;		/* l3 header type (may not match eh.ether_type for 802.3 */
	int8_t		tid;
	int8_t		nvlans;
	uint16_t	vlanid;			/* to which VLAN te msdu belongs */
	uint8_t		first_msdu	:1,	/* first msdu in an amsdu */
			last_msdu	:1,	/* last msdu in an amsdu */
			decapped	:1,	/* start is decapped eh, not wireless header */
			check_3addr_br	:1;	/* requires 3 address bridge dest mac set */
};

static __inline__ uint16_t
qtn_rx_decap_newhdr_size(const struct qtn_rx_decap_info *const di)
{
	return sizeof(struct ether_header) + (sizeof(struct qtn_8021q) * di->nvlans);
}

static __inline__ const struct qtn_8021q *
qtn_rx_decap_vlan(const struct qtn_rx_decap_info *const di, int8_t index)
{
	const struct qtn_8021q *v = (const void *) &di->eh.ether_type;
	return &v[index];
}

static __inline__ uint16_t qtn_rx_decap_header_size(const struct ieee80211_qosframe_addr4 *const wh)
{
	uint16_t size;
	const uint8_t dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;

	size = sizeof(struct ieee80211_frame);

	if (dir == IEEE80211_FC1_DIR_DSTODS)
		size += IEEE80211_ADDR_LEN;
	if (IEEE80211_QOS_HAS_SEQ(wh)) {
		size += sizeof(uint16_t);
		if ((wh->i_fc[1] & IEEE80211_FC1_ORDER) == IEEE80211_FC1_ORDER)
			/* Frame has HT control field in the header */
			size += sizeof(uint32_t);
	}

	return size;
}

#define LLC_ENCAP_RFC1042	0x0
#define LLC_ENCAP_BRIDGE_TUNNEL	0xF8

/*
 * Remove the LLC/SNAP header (if present) and replace with an Ethernet header
 *
 * See IEEE 802.1H for LLC/SNAP encapsulation/decapsulation.
 *   Ethernet-II SNAP header (RFC1042 for most Ethertypes)
 *   Bridge-Tunnel header (for Ethertypes ETH_P_AARP and ETH_P_IPX
 *   No encapsulation header if Ethertype < 0x600 (=length)
 */
static __inline__ void *
qtn_rx_decap_set_eth_hdr(struct qtn_rx_decap_info *di, const uint8_t *llc, const uint16_t llclen,
				uint16_t pvid, struct qtn_vlan_info *vlan_info, uint8_t vlan_enabled,
				void *token, void **rate_train)
{
	uint16_t *newhdrp = &di->eh.ether_type;
	int8_t llc_l3_gap = 0;
	uint16_t ether_type_l3;

	uint8_t last_byte = llc[5];
	uint16_t ether_type_eh;
	bool is_llc_snap_e;
	uint8_t vlan_hdr = 0;
	int tagrx;

	ether_type_l3 = (llc[6] << 0) | (llc[7] << 8);
	ether_type_eh = ether_type_l3;

	di->nvlans = 0;
	di->vlanid = 0;

	if (ether_type_l3 == htons(ETHERTYPE_8021Q)) {
		pvid = ((llc[9] << 0) | (llc[8] << 8)) & QVLAN_MASK_VID;
	}

	/*
	 * For EAPOL and VLAN frames we do not want to add 802.1Q header.
	 * Otherwise, the frame won't go through a driver.
	 */
	if (vlan_enabled) {
		di->vlanid = pvid;
		tagrx = qtn_vlan_get_tagrx(vlan_info->vlan_tagrx_bitmap, pvid);

		if (tagrx == QVLAN_TAGRX_TAG) {
			if (ether_type_l3 != htons(ETHERTYPE_8021Q) &&
					ether_type_l3 != htons(ETHERTYPE_PAE)) {
				*newhdrp++ = htons(ETHERTYPE_8021Q);
				*newhdrp++ = htons(pvid);
				di->nvlans++;
			}
		} else if (tagrx == QVLAN_TAGRX_STRIP) {
			if (ether_type_l3 == htons(ETHERTYPE_8021Q)) {
				ether_type_l3 = ((llc[10] << 0) | (llc[11] << 8));
				ether_type_eh = ether_type_l3;
				vlan_hdr = 4;
			}
		}
	}

	/*
	* Common part of the header - RFC1042 (final byte is 0x0) or
	* bridge tunnel encapsulation (final byte is 0xF8)
	*/
	is_llc_snap_e = llc[0] == LLC_SNAP_LSAP && llc[1] == LLC_SNAP_LSAP &&
		llc[2] == LLC_UI && llc[3] == 0x0 && llc[4] == 0x0;

	if (likely(is_llc_snap_e &&
				((last_byte == LLC_ENCAP_BRIDGE_TUNNEL) ||
				 (last_byte == LLC_ENCAP_RFC1042 &&
				  ether_type_eh != htons(ETHERTYPE_AARP) &&
				  ether_type_eh != htons(ETHERTYPE_IPX))))) {
		if (last_byte == LLC_ENCAP_RFC1042 && ether_type_eh == htons(ETHERTYPE_802A)) {
			struct oui_extended_ethertype *pe = (struct oui_extended_ethertype *)&llc[8];
			if (pe->oui[0] == (QTN_OUI & 0xff) &&
					pe->oui[1] == ((QTN_OUI >> 8) & 0xff) &&
					pe->oui[2] == ((QTN_OUI >> 16) & 0xff) &&
					pe->type == ntohs(QTN_OUIE_TYPE_TRAINING)) {
				/* Pass back pointer to start of training data */
				if (rate_train)
					*rate_train = (pe + 1);
				return NULL;
			}
		}

		llc += (LLC_SNAPFRAMELEN + vlan_hdr);
		*newhdrp++ = ether_type_eh;
	} else {
		ether_type_eh = htons(llclen);
		*newhdrp++ = ether_type_eh;
		llc_l3_gap = LLC_SNAPFRAMELEN;
	}

	di->l3hdr = llc + llc_l3_gap;
	di->l3_ether_type = ether_type_l3;
	di->start = (void *) (llc - qtn_rx_decap_newhdr_size(di));

	return di->start;
}


typedef int (*decap_handler_t)(struct qtn_rx_decap_info *, void *);

#define QTN_RX_DECAP_AMSDU	(0)
#define QTN_RX_DECAP_MPDU	(-1)
#define QTN_RX_DECAP_TRAINING	(-2)
#define QTN_RX_DECAP_NOT_DATA	(-3)
#define QTN_RX_DECAP_RUNT	(-4)
#define QTN_RX_DECAP_ABORTED	(-5)
#define QTN_RX_DECAP_ERROR(x)	((x) <= QTN_RX_DECAP_NOT_DATA)

#ifndef QTN_RX_DECAP_FNQUAL
#ifdef __KERNEL__
#define QTN_RX_DECAP_FNQUAL	static __sram_text
#define	qtn_rx_decap_inv_dcache_safe(a,b)
#else
#define QTN_RX_DECAP_FNQUAL	static __inline__
#define	qtn_rx_decap_inv_dcache_safe	invalidate_dcache_range_safe
#endif
#endif

QTN_RX_DECAP_FNQUAL int qtn_rx_decap(const struct ieee80211_qosframe_addr4 *const wh_copy,
		const void *const rxdata, const uint16_t rxlen,
		uint16_t pvid, struct qtn_vlan_info *vlan_info, uint8_t vlan_enabled,
		decap_handler_t handler, void *token, void **rate_train)
{
	const uint8_t *llc;
	const uint8_t type = wh_copy->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	const uint8_t subtype = wh_copy->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
	const uint8_t dir = wh_copy->i_fc[1] & IEEE80211_FC1_DIR_MASK;
	uint8_t qosctrl0 = 0;
	int8_t tid;
	bool is_amsdu = false;
	size_t header_size;
	int msdu;
	struct qtn_rx_decap_info __di[2];
	int dii	= 0;
	uint8_t *decap_start;

	/* only attempt to decap data frames */
	if (unlikely(type != IEEE80211_FC0_TYPE_DATA ||
				!(subtype == IEEE80211_FC0_SUBTYPE_DATA ||
				  subtype == IEEE80211_FC0_SUBTYPE_QOS))) {
		return QTN_RX_DECAP_NOT_DATA;
	}

	/* find qos ctrl field */
	if (IEEE80211_QOS_HAS_SEQ(wh_copy)){
		if (IEEE80211_IS_4ADDRESS(wh_copy)) {
			qosctrl0 = ((struct ieee80211_qosframe_addr4 *) wh_copy)->i_qos[0];
		} else {
			qosctrl0 = ((struct ieee80211_qosframe *) wh_copy)->i_qos[0];
		}
		tid = qosctrl0 & IEEE80211_QOS_TID;
		if (qosctrl0 & IEEE80211_QOS_A_MSDU_PRESENT) {
			is_amsdu = true;
		}
	} else {
		tid = WME_TID_NONQOS;
	}

	header_size = qtn_rx_decap_header_size(wh_copy);

	if (unlikely(header_size >= rxlen)) {
		return QTN_RX_DECAP_RUNT;
	}

	if (!is_amsdu) {
		const uint8_t *wh_eth_src;
		const uint8_t *wh_eth_dest;
		struct qtn_rx_decap_info *di = &__di[dii];

		switch (dir) {
		case IEEE80211_FC1_DIR_DSTODS:
			wh_eth_dest = wh_copy->i_addr3;
			wh_eth_src = wh_copy->i_addr4;
			if (IEEE80211_ADDR_EQ(wh_copy->i_addr1, wh_copy->i_addr3))
				di->check_3addr_br = 1;
			break;
		case IEEE80211_FC1_DIR_TODS:
			wh_eth_dest = wh_copy->i_addr3;
			wh_eth_src = wh_copy->i_addr2;
			break;
		case IEEE80211_FC1_DIR_NODS:
			wh_eth_dest = wh_copy->i_addr1;
			wh_eth_src = wh_copy->i_addr2;
			break;
		case IEEE80211_FC1_DIR_FROMDS:
			wh_eth_src = wh_copy->i_addr3;
			wh_eth_dest = wh_copy->i_addr1;
			di->check_3addr_br = 1;
			break;
		default:
			return QTN_RX_DECAP_ABORTED;
		}

		IEEE80211_ADDR_COPY(di->eh.ether_dhost, wh_eth_dest);
		IEEE80211_ADDR_COPY(di->eh.ether_shost, wh_eth_src);
		llc = ((uint8_t *) rxdata) + header_size;
		decap_start = qtn_rx_decap_set_eth_hdr(di, llc, rxlen - header_size,
							pvid, vlan_info, vlan_enabled, token, rate_train);
		if (unlikely(!decap_start)) {
			return QTN_RX_DECAP_TRAINING;
		}

		di->len = (((uint8_t *) rxdata) + rxlen) - decap_start;
		di->tid = tid;
		di->first_msdu = 1;
		di->last_msdu = 1;
		di->decapped = 1;

		if (handler(di, token)) {
			return QTN_RX_DECAP_ABORTED;
		}

		return QTN_RX_DECAP_MPDU;
	} else {
		/* amsdu */
		struct ether_header *msdu_header;
		struct ether_header *next_msdu_header;
		struct qtn_rx_decap_info *prev_di = NULL;
		uint16_t msdu_len;
		uint16_t subframe_len;
		uint16_t subframe_padding;
		uint16_t total_decapped_len = header_size;

		MUC_UPDATE_STATS(uc_rx_stats.rx_amsdu, 1);
		next_msdu_header = (struct ether_header *)(((uint8_t *)rxdata) + header_size);
		for (msdu = 0; total_decapped_len < rxlen; msdu++) {
			struct qtn_rx_decap_info *di = &__di[dii];

			msdu_header = next_msdu_header;
			llc = (uint8_t *)(msdu_header + 1);
			qtn_rx_decap_inv_dcache_safe(msdu_header, QTN_RX_MSDU_DCACHE_INV_LEN);
			msdu_len = ntohs(msdu_header->ether_type);
			subframe_len = sizeof(*msdu_header) + msdu_len;
			if (subframe_len < sizeof(*msdu_header) ||
					subframe_len > (rxlen - total_decapped_len) ||
					subframe_len > (ETHER_JUMBO_MAX_LEN + LLC_SNAPFRAMELEN)) {
				break;
			}
			subframe_padding = ((subframe_len + 0x3) & ~0x3) - subframe_len;
			next_msdu_header = (struct ether_header *)(llc + msdu_len + subframe_padding);
			/* decapped length includes subframe padding */
			total_decapped_len = ((uint8_t *)next_msdu_header) - ((uint8_t *)rxdata);

			decap_start = qtn_rx_decap_set_eth_hdr(di, llc, msdu_len, pvid, vlan_info, vlan_enabled,
								token, rate_train);
			if (unlikely(!decap_start)) {
				return QTN_RX_DECAP_TRAINING;
			}

			if (prev_di) {
				if (handler(prev_di, token)) {
					return QTN_RX_DECAP_ABORTED;
				}
			}

			IEEE80211_ADDR_COPY(di->eh.ether_dhost, msdu_header->ether_dhost);
			IEEE80211_ADDR_COPY(di->eh.ether_shost, msdu_header->ether_shost);
			di->len = ((uint8_t *)next_msdu_header - decap_start) - subframe_padding;
			di->tid = tid;
			di->first_msdu = (prev_di == NULL);
			di->last_msdu = 0;
			di->decapped = 1;
			di->check_3addr_br = 0;
			prev_di = di;
			dii = !dii;
		}
		if (prev_di) {
			prev_di->last_msdu = 1;
			if (handler(prev_di, token)) {
				return QTN_RX_DECAP_ABORTED;
			}
		} else {
			return QTN_RX_DECAP_ABORTED;
		}

		return QTN_RX_DECAP_AMSDU;
	}
}

#endif	// __QTN_DECAP_H__

