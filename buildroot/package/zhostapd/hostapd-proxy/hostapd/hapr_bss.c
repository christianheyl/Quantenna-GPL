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

#include "hapr_bss.h"
#include "hapr_client.h"
#include "hapr_eloop.h"
#include "hapr_log.h"
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <linux/rtnetlink.h>
#include <net80211/ieee80211.h>
#include <net80211/ieee80211_crypto.h>
#include <net80211/ieee80211_ioctl.h>
#include "wireless.h"

#ifdef IEEE80211_IOCTL_FILTERFRAME
#include <netpacket/packet.h>

#ifndef ETH_P_80211_RAW
#define ETH_P_80211_RAW 0x0019
#endif
#endif /* IEEE80211_IOCTL_FILTERFRAME */

#ifdef IEEE80211_IOCTL_SETWMMPARAMS
/* Assume this is built against madwifi-ng */
#define MADWIFI_NG
#endif /* IEEE80211_IOCTL_SETWMMPARAMS */

/* Size of the Event prefix (including padding and alignement junk) */
#define IW_EV_LCP_LEN	(sizeof(struct iw_event) - sizeof(union iwreq_data))

/* iw_point events are special. First, the payload (extra data) come at
 * the end of the event, so they are bigger than IW_EV_POINT_LEN. Second,
 * we omit the pointer, so start at an offset. */
#define IW_EV_POINT_OFF (((char *) &(((struct iw_point *) NULL)->length)) - \
			  (char *) NULL)
#define IW_EV_POINT_LEN	(IW_EV_LCP_LEN + sizeof(struct iw_point) - \
			 IW_EV_POINT_OFF)

#define IWEVCUSTOM	0x8C02		/* Driver specific ascii string */
#define IWEVREGISTERED	0x8C03		/* Discovered a new node (AP mode) */
#define IWEVEXPIRED	0x8C04		/* Expired a node (AP mode) */
#define IWEVMICHAELMICFAILURE 0x8C06	/* Michael MIC failure
					 * (struct iw_michaelmicfailure)
					 */

extern struct hapr_eloop *main_eloop;

static void handle_read(void *ctx, const uint8_t *src_addr, const uint8_t *buf, size_t len)
{
	struct hapr_bss *bss = ctx;
	qlink_server_recv_packet(bss->client->qs, bss->qid, ETH_P_PAE, src_addr, buf, len);
}

#ifdef IEEE80211_IOCTL_FILTERFRAME
static void raw_receive(void *ctx, const uint8_t *src_addr, const uint8_t *buf, size_t len)
{
	struct hapr_bss *bss = ctx;
	qlink_server_recv_packet(bss->client->qs, bss->qid, ETH_P_80211_RAW, src_addr, buf, len);
}
#endif /* IEEE80211_IOCTL_FILTERFRAME */

static void wireless_event_wireless(struct hapr_bss *bss, char *data, int len)
{
	struct iw_event iwe_buf, *iwe = &iwe_buf;
	char *pos, *end, *custom, *buf;

	pos = data;
	end = data + len;

	while (pos + IW_EV_LCP_LEN <= end) {
		/* Event data may be unaligned, so make a local, aligned copy
		 * before processing. */
		memcpy(&iwe_buf, pos, IW_EV_LCP_LEN);
		HAPR_LOG(DEBUG, "Wireless event: cmd=0x%x len=%d",
			   iwe->cmd, iwe->len);
		if (iwe->len <= IW_EV_LCP_LEN)
			return;

		custom = pos + IW_EV_POINT_LEN;
		if (bss->we_version > 18 &&
		    (iwe->cmd == IWEVMICHAELMICFAILURE ||
		     iwe->cmd == IWEVCUSTOM)) {
			/* WE-19 removed the pointer from struct iw_point */
			char *dpos = (char *) &iwe_buf.u.data.length;
			int dlen = dpos - (char *) &iwe_buf;
			memcpy(dpos, pos + IW_EV_LCP_LEN,
			       sizeof(struct iw_event) - dlen);
		} else {
			memcpy(&iwe_buf, pos, sizeof(struct iw_event));
			custom += IW_EV_POINT_OFF;
		}

		switch (iwe->cmd) {
		case IWEVEXPIRED:
			qlink_server_recv_iwevent(bss->client->qs, bss->qid, iwe->cmd,
				iwe->u.addr.sa_data, ETH_ALEN);
			break;
		case IWEVREGISTERED:
			qlink_server_recv_iwevent(bss->client->qs, bss->qid, iwe->cmd,
				iwe->u.addr.sa_data, ETH_ALEN);
			break;
		case IWEVCUSTOM:
			if (custom + iwe->u.data.length > end)
				return;
			buf = malloc(iwe->u.data.length + 1);
			if (buf == NULL)
				return;		/* XXX */
			memcpy(buf, custom, iwe->u.data.length);
			buf[iwe->u.data.length] = '\0';
			qlink_server_recv_iwevent(bss->client->qs, bss->qid, iwe->cmd, buf,
				strlen(buf) + 1);
			free(buf);
			break;
		}

		pos += iwe->len;
	}
}

static void wireless_event_rtm_newlink(void *ctx, struct ifinfomsg *ifi, uint8_t *buf, size_t len)
{
	struct hapr_bss *bss = ctx;
	int attrlen, rta_len;
	struct rtattr *attr;

	if (ifi->ifi_index != bss->ifindex)
		return;

	attrlen = len;
	attr = (struct rtattr *)buf;

	rta_len = RTA_ALIGN(sizeof(struct rtattr));
	while (RTA_OK(attr, attrlen)) {
		if (attr->rta_type == IFLA_WIRELESS) {
			wireless_event_wireless(bss, ((char *)attr) + rta_len,
				attr->rta_len - rta_len);
		}
		attr = RTA_NEXT(attr, attrlen);
	}
}

static void hapr_bss_start(void *user_data)
{
	struct hapr_bss *bss = user_data;

	hapr_l2_packet_register(bss->sock_xmit, main_eloop);
	if (bss->sock_raw) {
		hapr_l2_packet_register(bss->sock_raw, main_eloop);
	}
	hapr_netlink_register(bss->netlink, main_eloop);

	if (!bss->is_vap)
		hapr_bss_set80211param(bss, IEEE80211_PARAM_HOSTAP_STARTED, 1);
}

static void hapr_bss_start_bridge_eapol(void *user_data)
{
	struct hapr_bss *bss = user_data;

	hapr_l2_packet_register(bss->sock_recv, main_eloop);
}

static void hapr_bss_stop(void *user_data)
{
	struct hapr_bss *bss = user_data;

	if (!bss->is_vap)
		hapr_bss_set80211param(bss, IEEE80211_PARAM_HOSTAP_STARTED, 0);

	hapr_netlink_unregister(bss->netlink, main_eloop);
	hapr_netlink_destroy(bss->netlink);

	if (bss->sock_raw) {
		hapr_l2_packet_unregister(bss->sock_raw, main_eloop);
		hapr_l2_packet_destroy(bss->sock_raw);
	}

	hapr_l2_packet_unregister(bss->sock_xmit, main_eloop);
	hapr_l2_packet_destroy(bss->sock_xmit);

	if (bss->sock_xmit != bss->sock_recv) {
		hapr_l2_packet_unregister(bss->sock_recv, main_eloop);
		hapr_l2_packet_destroy(bss->sock_recv);
	}

	close(bss->ioctl_sock);
	free(bss);

	HAPR_LOG(TRACE, "bss %p destroyed", bss);
}

static int receive_pkt(struct hapr_bss *bss)
{
	int ret = 0;
#ifdef IEEE80211_IOCTL_FILTERFRAME
	struct ieee80211req_set_filter filt;

	HAPR_LOG(TRACE, "enter");

	filt.app_filterype = 0;
	if ((bss->client->flags & QLINK_FLAG_WPS) != 0) {
		filt.app_filterype = IEEE80211_FILTER_TYPE_PROBE_REQ;
	}
	if ((bss->client->flags & QLINK_FLAG_HS20) != 0) {
		filt.app_filterype |= IEEE80211_FILTER_TYPE_ACTION;
	}

	ret = hapr_bss_set80211priv(bss, IEEE80211_IOCTL_FILTERFRAME, &filt,
			   sizeof(struct ieee80211req_set_filter));
	if (ret)
		return ret;

	bss->sock_raw = hapr_l2_packet_create(bss->brname, NULL, ETH_P_80211_RAW,
				       raw_receive, bss, 1);
	if (bss->sock_raw == NULL)
		return -1;
#endif /* IEEE80211_IOCTL_FILTERFRAME */
	return ret;
}

static int wireless_event_init(struct hapr_bss *bss)
{
	struct hapr_netlink_config *cfg;

	cfg = malloc(sizeof(*cfg));
	if (cfg == NULL) {
		HAPR_LOG(ERROR, "cannot allocate netlink cfg, no mem");
		return -1;
	}
	memset(cfg, 0, sizeof(*cfg));
	cfg->ctx = bss;
	cfg->newlink_cb = wireless_event_rtm_newlink;
	bss->netlink = hapr_netlink_create(cfg);
	if (bss->netlink == NULL) {
		free(cfg);
		return -1;
	}

	return 0;
}

static int init_bss_bridge(struct hapr_bss *bss, const char *ifname)
{
	char in_br[IFNAMSIZ + 1];
	const char* brname = bss->brname;
	int add_bridge_required = 1;

	if (brname[0] == 0) {
		add_bridge_required = 0;
	}

	if (hapr_linux_br_get(in_br, ifname) == 0) {
		/* it is in a bridge already */
		if (strcmp(in_br, brname) == 0) {
			add_bridge_required = 0;
		} else {
			/* but not the desired bridge; remove */
			HAPR_LOG(DEBUG, "Removing interface %s from bridge %s", ifname, in_br);
			if (hapr_linux_br_del_if(bss->ioctl_sock, in_br, ifname) < 0) {
				HAPR_LOG(ERROR, "Failed to "
					"remove interface %s from bridge %s: %s",
					ifname, brname, strerror(errno));
				return -1;
			}
		}
	}

	if (add_bridge_required) {
		HAPR_LOG(DEBUG, "Adding interface %s into bridge %s", ifname, brname);
		if (hapr_linux_br_add_if(bss->ioctl_sock, brname, ifname) < 0) {
			HAPR_LOG(ERROR, "Failed to add interface %s "
					"into bridge %s: %s", ifname, brname, strerror(errno));
			return -1;
		}
		bss->added_if_into_bridge = 1;
	}

	return 0;
}

static int deinit_bss_bridge(struct hapr_bss *bss, const char *ifname)
{
	const char *brname = bss->brname;

	if (bss->added_if_into_bridge) {
		if (hapr_linux_br_del_if(bss->ioctl_sock, brname, ifname) < 0) {
			HAPR_LOG(ERROR, "Failed to remove interface %s from bridge %s: %s",
				ifname, brname, strerror(errno));
			return -1;
		}
		bss->added_if_into_bridge = 0;
	}

	return 0;
}

static int get_we_version(struct hapr_bss *bss, int *we_version)
{
	struct iw_range *range;
	struct iwreq iwr;
	int minlen;
	size_t buflen;

	*we_version = 0;

	/*
	 * Use larger buffer than struct iw_range in order to allow the
	 * structure to grow in the future.
	 */
	buflen = sizeof(struct iw_range) + 500;
	range = malloc(buflen);
	if (range == NULL) {
		HAPR_LOG(ERROR, "cannot allocate iw_range, no mem");
		return -1;
	}
	memset(range, 0, buflen);

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, bss->ifname, sizeof(iwr.ifr_name) - 1);
	iwr.u.data.pointer = (caddr_t)range;
	iwr.u.data.length = buflen;

	minlen = ((char *)&range->enc_capa) - (char *)range + sizeof(range->enc_capa);

	if (ioctl(bss->ioctl_sock, SIOCGIWRANGE, &iwr) < 0) {
		HAPR_LOG(ERROR, "ioctl[SIOCGIWRANGE]");
		free(range);
		return -1;
	} else if (iwr.u.data.length >= minlen &&
		   range->we_version_compiled >= 18) {
		HAPR_LOG(DEBUG, "SIOCGIWRANGE: WE(compiled)=%d "
			   "WE(source)=%d enc_capa=0x%x",
			   range->we_version_compiled,
			   range->we_version_source,
			   range->enc_capa);
		*we_version = range->we_version_compiled;
	}

	free(range);
	return 0;
}

struct hapr_bss *hapr_bss_create(struct hapr_client *client, uint32_t qid, const char *ifname,
	const char *brname, int is_vap)
{
	struct hapr_bss *bss;
	struct ifreq ifr;

	bss = malloc(sizeof(*bss));

	if (bss == NULL) {
		HAPR_LOG(ERROR, "cannot allocate bss, no mem");
		goto fail_alloc;
	}

	memset(bss, 0, sizeof(*bss));

	bss->is_vap = is_vap;
	bss->client = client;
	bss->qid = qid;
	strncpy(bss->ifname, ifname, sizeof(bss->ifname) - 1);
	strncpy(bss->brname, brname, sizeof(bss->brname) - 1);

	bss->ioctl_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (bss->ioctl_sock < 0) {
		HAPR_LOG(ERROR, "socket[PF_INET,SOCK_DGRAM]");
		goto fail_ioctl_sock;
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, bss->ifname, sizeof(ifr.ifr_name) - 1);
	if (ioctl(bss->ioctl_sock, SIOCGIFINDEX, &ifr) != 0) {
		HAPR_LOG(ERROR, "ioctl(SIOCGIFINDEX)");
		goto fail_ioctl;
	}
	bss->ifindex = ifr.ifr_ifindex;

	if (get_we_version(bss, &bss->we_version) != 0)
		goto fail_ioctl;

	bss->sock_xmit = hapr_l2_packet_create(bss->ifname, NULL, ETH_P_PAE,
		handle_read, bss, 1);
	if (bss->sock_xmit == NULL)
		goto fail_ioctl;

	if (hapr_l2_packet_get_own_addr(bss->sock_xmit, bss->own_addr) != 0)
		goto fail_ioctl;

	bss->sock_recv = bss->sock_xmit;

	/* mark down during setup */
	hapr_linux_set_iface_flags(bss->ioctl_sock, bss->ifname, 0);
	hapr_bss_set_privacy(bss, 0); /* default to no privacy */

	receive_pkt(bss);

	if (wireless_event_init(bss))
		goto fail_wireless_event;

	if (init_bss_bridge(bss, ifname))
		goto fail_bridge;

	hapr_list_add_tail(&client->bss_list, &bss->list);

	HAPR_LOG(TRACE, "new bss %p, %d, %s, %s", bss, qid, ifname, brname);

	hapr_eloop_register_timeout(main_eloop, &hapr_bss_start, bss, 0);

	return bss;

fail_bridge:
	hapr_netlink_destroy(bss->netlink);
fail_wireless_event:
	if (bss->sock_raw)
		hapr_l2_packet_destroy(bss->sock_raw);
	hapr_l2_packet_destroy(bss->sock_xmit);
fail_ioctl:
	close(bss->ioctl_sock);
fail_ioctl_sock:
	free(bss);
fail_alloc:

	return NULL;
}

void hapr_bss_destroy(struct hapr_bss *bss)
{
	deinit_bss_bridge(bss, bss->ifname);

	hapr_bss_set_privacy(bss, 0);

	hapr_linux_set_iface_flags(bss->ioctl_sock, bss->ifname, 0);

	hapr_list_remove(&bss->list);

	if (bss->is_vap)
		hapr_qdrv_vap_stop(bss->ifname);

	hapr_eloop_register_timeout(main_eloop, &hapr_bss_stop, bss, 0);
}

int hapr_bss_set80211param(struct hapr_bss *bss, int op, int arg)
{
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, bss->ifname, sizeof(iwr.ifr_name) - 1);
	iwr.u.mode = op;
	memcpy(iwr.u.name + sizeof(__u32), &arg, sizeof(arg));

	if (ioctl(bss->ioctl_sock, IEEE80211_IOCTL_SETPARAM, &iwr) < 0) {
		HAPR_LOG(ERROR, "failed to set parameter (op %d arg %d): %s", op, arg,
			strerror(errno));
		return -1;
	}

	return 0;
}

int hapr_bss_set80211priv(struct hapr_bss *bss, int op, void *data, int len)
{
	struct iwreq iwr;
	int do_inline = len < IFNAMSIZ;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, bss->ifname, sizeof(iwr.ifr_name) - 1);
#ifdef IEEE80211_IOCTL_FILTERFRAME
	/* FILTERFRAME must be NOT inline, regardless of size. */
	if (op == IEEE80211_IOCTL_FILTERFRAME)
		do_inline = 0;
#endif /* IEEE80211_IOCTL_FILTERFRAME */
	if ((op == IEEE80211_IOCTL_SET_APPIEBUF) ||
	    (op == IEEE80211_IOCTL_POSTEVENT) ||
	    (op == IEEE80211_IOCTL_TXEAPOL))
		do_inline = 0;
	if (do_inline) {
		/*
		 * Argument data fits inline; put it there.
		 */
		memcpy(iwr.u.name, data, len);
	} else {
		/*
		 * Argument data too big for inline transfer; setup a
		 * parameter block instead; the kernel will transfer
		 * the data for the driver.
		 */
		iwr.u.data.pointer = data;
		iwr.u.data.length = len;
	}

	if (ioctl(bss->ioctl_sock, op, &iwr) < 0) {
#ifdef MADWIFI_NG
		int first = IEEE80211_IOCTL_SETPARAM;
		static const char *opnames[] = {
			"ioctl[IEEE80211_IOCTL_SETPARAM]",
			"ioctl[IEEE80211_IOCTL_GETPARAM]",
			"ioctl[IEEE80211_IOCTL_SETMODE]",
			"ioctl[IEEE80211_IOCTL_GETMODE]",
			"ioctl[IEEE80211_IOCTL_SETWMMPARAMS]",
			"ioctl[IEEE80211_IOCTL_GETWMMPARAMS]",
			"ioctl[IEEE80211_IOCTL_SETCHANLIST]",
			"ioctl[IEEE80211_IOCTL_GETCHANLIST]",
			"ioctl[IEEE80211_IOCTL_CHANSWITCH]",
			"ioctl[IEEE80211_IOCTL_GET_APPIEBUF]",
			"ioctl[IEEE80211_IOCTL_SET_APPIEBUF]",
			"ioctl[IEEE80211_IOCTL_GETSCANRESULTS]",
			"ioctl[IEEE80211_IOCTL_FILTERFRAME]",
			"ioctl[IEEE80211_IOCTL_GETCHANINFO]",
			"ioctl[IEEE80211_IOCTL_SETOPTIE]",
			"ioctl[IEEE80211_IOCTL_GETOPTIE]",
			"ioctl[IEEE80211_IOCTL_SETMLME]",
			"ioctl[IEEE80211_IOCTL_RADAR]",
			"ioctl[IEEE80211_IOCTL_SETKEY]",
			"ioctl[IEEE80211_IOCTL_POSTEVENT]",
			"ioctl[IEEE80211_IOCTL_DELKEY]",
			"ioctl[IEEE80211_IOCTL_TXEAPOL]",
			"ioctl[IEEE80211_IOCTL_ADDMAC]",
			NULL,
			"ioctl[IEEE80211_IOCTL_DELMAC]",
			NULL,
			"ioctl[IEEE80211_IOCTL_WDSMAC]",
			NULL,
			"ioctl[IEEE80211_IOCTL_WDSDELMAC]",
			NULL,
			"ioctl[IEEE80211_IOCTL_KICKMAC]",
		};
#else /* MADWIFI_NG */
		int first = IEEE80211_IOCTL_SETPARAM;
		static const char *opnames[] = {
			"ioctl[IEEE80211_IOCTL_SETPARAM]",
			"ioctl[IEEE80211_IOCTL_GETPARAM]",
			"ioctl[IEEE80211_IOCTL_SETKEY]",
			"ioctl[SIOCIWFIRSTPRIV+3]",
			"ioctl[IEEE80211_IOCTL_DELKEY]",
			"ioctl[SIOCIWFIRSTPRIV+5]",
			"ioctl[IEEE80211_IOCTL_SETMLME]",
			"ioctl[SIOCIWFIRSTPRIV+7]",
			"ioctl[IEEE80211_IOCTL_SETOPTIE]",
			"ioctl[IEEE80211_IOCTL_GETOPTIE]",
			"ioctl[IEEE80211_IOCTL_ADDMAC]",
			"ioctl[SIOCIWFIRSTPRIV+11]",
			"ioctl[IEEE80211_IOCTL_DELMAC]",
			"ioctl[SIOCIWFIRSTPRIV+13]",
			"ioctl[IEEE80211_IOCTL_CHANLIST]",
			"ioctl[SIOCIWFIRSTPRIV+15]",
			"ioctl[IEEE80211_IOCTL_GETRSN]",
			"ioctl[SIOCIWFIRSTPRIV+17]",
			"ioctl[IEEE80211_IOCTL_GETKEY]",
		};
#endif /* MADWIFI_NG */
		int idx = op - first;
		const char *ioctl_name = "ioctl[unknown???]";
		if (first <= op &&
		    idx < (int)(sizeof(opnames)/sizeof(opnames[0])) &&
		    opnames[idx]) {
			ioctl_name = opnames[idx];
		}
		HAPR_LOG(DEBUG, "%s(0x%X) returned error %d: %s\n", ioctl_name, op, errno, strerror(errno));
		return -1;
	}
	return 0;
}

int hapr_bss_set_privacy(struct hapr_bss *bss, int enabled)
{
	HAPR_LOG(DEBUG, "enabled = %d", enabled);

	return hapr_bss_set80211param(bss, IEEE80211_PARAM_PRIVACY, enabled);
}

int hapr_bss_handle_bridge_eapol(struct hapr_bss *bss)
{
	char brname[IFNAMSIZ];

	if (bss->brname[0]) {
		HAPR_LOG(DEBUG, "Configure bridge %s for EAPOL traffic", bss->brname);
		bss->sock_recv = hapr_l2_packet_create(bss->brname, NULL,
			ETH_P_PAE, handle_read, bss, 1);
		if (bss->sock_recv == NULL)
			return -1;
		hapr_eloop_register_timeout(main_eloop, &hapr_bss_start_bridge_eapol, bss, 0);
	} else if (hapr_linux_br_get(brname, bss->ifname) == 0) {
		HAPR_LOG(DEBUG, "Interface in bridge %s; configure for EAPOL receive", brname);
		bss->sock_recv = hapr_l2_packet_create(brname, NULL,
			ETH_P_PAE, handle_read, bss, 1);
		if (bss->sock_recv == NULL)
			return -1;
		hapr_eloop_register_timeout(main_eloop, &hapr_bss_start_bridge_eapol, bss, 0);
	} else {
		bss->sock_recv = bss->sock_xmit;
	}

	return 0;
}

int hapr_bss_get_extended_capa(struct hapr_bss *bss, uint8_t *extended_capa_len,
	uint8_t **extended_capa, uint8_t **extended_capa_mask)
{
	struct iwreq iwr;
	uint8_t buf[MADWIFI_CMD_BUF_SIZE];
	uint8_t *pos = buf;
	uint8_t *end;
	uint32_t data_len;

	*extended_capa_len = 0;
	*extended_capa = NULL;
	*extended_capa_mask = NULL;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, bss->ifname, sizeof(iwr.ifr_name) - 1);

	iwr.u.data.flags = SIOCDEV_SUBIO_GET_DRIVER_CAPABILITY;
	iwr.u.data.pointer = &buf;
	iwr.u.data.length = sizeof(buf);

	if (ioctl(bss->ioctl_sock, IEEE80211_IOCTL_EXT, &iwr) < 0) {
		HAPR_LOG(ERROR, "Failed to get ext capability from driver");
		return -1;
	}

	data_len = (uint32_t)*pos;
	end = pos + data_len;
	pos += sizeof(uint32_t);

	while (pos < end) {
		switch (*pos) {
		case IEEE80211_ELEMID_EXTCAP:
			pos++;
			*extended_capa_len = *pos++;
			*extended_capa = malloc(*extended_capa_len);

			if (!*extended_capa) {
				HAPR_LOG(ERROR, "cannot allocate extended_capa, no mem");
				*extended_capa_len = 0;
				return -1;
			}

			memcpy(*extended_capa, pos, *extended_capa_len);
			pos += *extended_capa_len;

			*extended_capa_mask = malloc(*extended_capa_len);

			if (!*extended_capa_mask) {
				*extended_capa_len = 0;
				free(*extended_capa);
				*extended_capa = NULL;
				HAPR_LOG(ERROR, "cannot allocate extended_capa_mask, no mem");
				return -1;
			}

			memcpy(*extended_capa_mask, pos, *extended_capa_len);
			pos += *extended_capa_len;

			break;
		default:
			HAPR_LOG(DEBUG, "Not handling other data %d\n", *pos);
			pos = end; /* Exit here */
			break;
		}
	}

	return 0;
}

int hapr_bss_set_ssid(struct hapr_bss *bss, const uint8_t *buf, int len)
{
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, bss->ifname, sizeof(iwr.ifr_name) - 1);
	iwr.u.essid.flags = 1; /* SSID active */
	iwr.u.essid.pointer = (caddr_t) buf;
	iwr.u.essid.length = len + 1;

	if (ioctl(bss->ioctl_sock, SIOCSIWESSID, &iwr) < 0) {
		HAPR_LOG(ERROR, "ioctl[SIOCSIWESSID]");
		return -1;
	}
	return 0;
}

int hapr_bss_commit(struct hapr_bss *bss)
{
	return hapr_linux_set_iface_flags(bss->ioctl_sock, bss->ifname, 1);
}

int hapr_bss_set_interworking(struct hapr_bss *bss, uint8_t eid, int interworking,
	uint8_t access_network_type, const uint8_t *hessid)
{
	struct iwreq iwr;
	struct app_ie ie;

	memset(&ie, 0, sizeof(struct app_ie));

	ie.id = eid;
	ie.u.interw.interworking = interworking;
	ie.len++;

	if (interworking) {
		ie.u.interw.an_type = access_network_type;
		ie.len++;

		if (hessid) {
			memcpy(ie.u.interw.hessid, hessid, ETH_ALEN);
			ie.len += ETH_ALEN;
		}
	}

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, bss->ifname, sizeof(iwr.ifr_name) - 1);
	iwr.u.data.flags = SIOCDEV_SUBIO_SET_AP_INFO;
	iwr.u.data.pointer = &ie;
	iwr.u.data.length = ie.len + 1 + 2;	/* IE data len + IE ID + IE len */

	if (ioctl(bss->ioctl_sock, IEEE80211_IOCTL_EXT, &iwr) < 0) {
		HAPR_LOG(ERROR, "Failed to set interworking info ioctl");
		return -1;
	}

	return 0;
}

int hapr_bss_brcm(struct hapr_bss *bss, uint8_t *data, uint32_t len)
{
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, bss->ifname, sizeof(iwr.ifr_name) - 1);
	iwr.u.data.flags = SIOCDEV_SUBIO_SET_BRCM_IOCTL;
	iwr.u.data.pointer = data;
	iwr.u.data.length = len;

	if (ioctl(bss->ioctl_sock, IEEE80211_IOCTL_EXT, &iwr) < 0) {
		HAPR_LOG(ERROR, "Failed to do brcm info ioctl");
		return -1;
	}

	return 0;
}

int hapr_bss_check_if(struct hapr_bss *bss)
{
	if (!hapr_linux_iface_up(bss->ioctl_sock, bss->ifname)) {
		HAPR_LOG(DEBUG, "Interface is not up");
		return -1;
	}

	return 0;
}

int hapr_bss_set_channel(struct hapr_bss *bss, int channel)
{
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, bss->ifname, sizeof(iwr.ifr_name) - 1);
	iwr.u.freq.m = channel;
	iwr.u.freq.e = 0;

	if (ioctl(bss->ioctl_sock, SIOCSIWFREQ, &iwr) < 0) {
		HAPR_LOG(ERROR, "Failed to do channel set ioctl");
		return -1;
	}

	return 0;
}

int hapr_bss_get_ssid(struct hapr_bss *bss, uint8_t *buf, int *len)
{
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, bss->ifname, sizeof(iwr.ifr_name) - 1);
	iwr.u.essid.pointer = (caddr_t)buf;
	iwr.u.essid.length = *len;

	if (ioctl(bss->ioctl_sock, SIOCGIWESSID, &iwr) < 0) {
		HAPR_LOG(ERROR, "Failed to do ssid get ioctl");
		return -1;
	}

	*len = iwr.u.essid.length;

	HAPR_LOG(TRACE, "len = %d", *len);

	return 0;
}

int hapr_bss_send_action(struct hapr_bss *bss, const void *data, int len)
{
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, bss->ifname, sizeof(iwr.ifr_name) - 1);
	iwr.u.data.flags = SIOCDEV_SUBIO_SEND_ACTION_FRAME;
	iwr.u.data.pointer = (void *)data;
	iwr.u.data.length = len;

	if (ioctl(bss->ioctl_sock, IEEE80211_IOCTL_EXT, &iwr) < 0) {
		HAPR_LOG(ERROR, "Failed to send action frame ioctl");
		return -1;
	}

	return 0;
}

int hapr_bss_set_qos_map(struct hapr_bss *bss, uint8_t *data)
{
	struct iwreq iwr;

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, bss->ifname, sizeof(iwr.ifr_name) - 1);
	iwr.u.data.flags = SIOCDEV_SUBIO_SET_DSCP2TID_MAP;
	iwr.u.data.pointer = data;
	iwr.u.data.length = IP_DSCP_NUM;

	if (ioctl(bss->ioctl_sock, IEEE80211_IOCTL_EXT, &iwr) < 0) {
		HAPR_LOG(ERROR, "Failed to do SIOCDEV_SUBIO_SET_DSCP2TID_MAP ioctl");
		return -1;
	}

	return 0;
}
