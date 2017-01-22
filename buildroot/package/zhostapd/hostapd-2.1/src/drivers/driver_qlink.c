/*
 * hostapd - driver interaction with qlink driver
 * Copyright (c) 2008 - 2015 Quantenna Communications Inc
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 *
 * This driver wrapper is only for hostapd AP mode functionality.
 */

#include "includes.h"
#include <sys/ioctl.h>
#include <sys/un.h>
#include <string.h>
#include <dirent.h>

#include "common.h"
#include "driver.h"
#include "driver_wext.h"
#include "eloop.h"
#include "common/ieee802_11_defs.h"
#include "wireless_copy.h"
#include "qlink/qlink_client.h"

#include "ap/hostapd.h"
#include "ap/ap_config.h"
#include "ap/ieee802_11_auth.h"
#include "ap/wps_hostapd.h"
#include "qtn_hapd/qtn_hapd_bss.h"
#include "qtn_hapd/qtn_hapd_pp.h"
#include "crypto/crypto.h"

#include <net80211/ieee80211.h>
#ifdef WME_NUM_AC
/* Assume this is built against BSD branch of madwifi driver. */
#define MADWIFI_BSD
#include <net80211/_ieee80211.h>
#endif /* WME_NUM_AC */
#include <net80211/ieee80211_crypto.h>
#include <net80211/ieee80211_ioctl.h>

#ifdef CONFIG_WPS
#ifdef IEEE80211_IOCTL_FILTERFRAME
#include <netpacket/packet.h>

#ifndef ETH_P_80211_RAW
#define ETH_P_80211_RAW 0x0019
#endif
#endif /* IEEE80211_IOCTL_FILTERFRAME */
#endif /* CONFIG_WPS */

#ifdef IEEE80211_IOCTL_SETWMMPARAMS
/* Assume this is built against madwifi-ng */
#define MADWIFI_NG
#endif /* IEEE80211_IOCTL_SETWMMPARAMS */

#include "l2_packet/l2_packet.h"

#define QLINK_HANDSHAKE_TIMEOUT 1000
#define QLINK_CONNECT_TIMEOUT 60000
#define QLINK_PING_TIMEOUT 1000
#define QLINK_MAX_MISSED_PINGS 15
#define QLINK_EAPOL_MAX_FRAME_SIZE 2047

#define QLINK_CMD_WDS_EXT_LEN		256
#define QLINK_WDS_KEY_LEN		32

#define QLINK_TEMPORARY_CONFIG_FILE "hostapd_qlink_tmp.conf"

/*
 * qlink hostapd driver is a client for hostapd-proxy(i.e. qlink server) that's running inside
 * qtn wifi device. This driver does the following:
 * + receives wireless events from the server, processes them and passes them up to hostapd
 * + receives EAPOL and 802.11 encapped frames from the server and passes them up to hostapd
 * + receives CLI commands from the server and passes them up to hostapd
 * + serves hostapd requests (i.e. wpa_driver_ops) by passing them down to the server
 * + server CLI responses by passing them down to the server
 *
 * All transport-related code is inside 'struct qlink_client', the code in this driver only handles
 * the logic, i.e. it doesn't have to worry about endianess or alignment of members in structures
 * except for the following cases:
 * + qlink_client_set_80211_priv calls, these are iwpriv ioctl commands, the structures passed must
 * be 4-byte aligned and have little-endian members.
 */

struct qlink_bss
{
	struct qlink_driver_data *drv;
	struct dl_list list;
	void *bss_ctx;
	char ifname[IFNAMSIZ];
	char brname[IFNAMSIZ];
	uint32_t qid;
	u8 acct_mac[ETH_ALEN];
	struct hostap_sta_driver_data acct_data;
};

struct qlink_ctrl_client
{
	struct qlink_driver_data *drv;
	struct dl_list list;
	int sock;
	char remote_addr[QLINK_CTRL_ADDR_BUF_SIZE];
	struct sockaddr_un local_addr;
};

struct qlink_driver_data
{
	struct hostapd_data *hapd;
	struct qlink_client *qc;
	int active;
	uint8_t guid[QLINK_GUID_BUF_SIZE];
	uint32_t server_ping_timeout_ms;
	uint32_t bss_next_qid;
	struct dl_list bss;

	int we_version;
	uint8_t *extended_capa;
	uint8_t *extended_capa_mask;
	uint8_t extended_capa_len;

	struct sockaddr_un ctrl_addr;
	struct dl_list ctrl_clients;
};

#ifndef CONFIG_NO_STDOUT_DEBUG
static const char *
ether_sprintf(const u8 *addr)
{
	static char buf[sizeof(MACSTR)];

	if (addr != NULL)
		snprintf(buf, sizeof(buf), MACSTR, MAC2STR(addr));
	else
		snprintf(buf, sizeof(buf), MACSTR, 0,0,0,0,0,0);
	return buf;
}
#endif /* CONFIG_NO_STDOUT_DEBUG */

static int eloop_register_timeout_ms(uint32_t timeout_ms,
	eloop_timeout_handler handler,
	void *eloop_data)
{
	return eloop_register_timeout(timeout_ms / 1000, (timeout_ms % 1000) * 1000, handler,
		eloop_data, NULL);
}

/*
 * ctrl iface related.
 * @{
 */

static void qlink_ctrl_client_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct qlink_ctrl_client *cc = eloop_ctx;
	char buf[QLINK_CTRL_MSG_BUF_SIZE];
	struct sockaddr_un from;
	socklen_t fromlen = sizeof(from);
	int ret;

	do {
		ret = recvfrom(sock, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&from, &fromlen);
	} while ((ret < 0) && (errno == EINTR));

	if (ret < 0) {
		wpa_printf(MSG_ERROR, "failed to recv from ctrl interface: %s", strerror(errno));
		return;
	}

	qlink_client_send_ctrl(cc->drv->qc, cc->remote_addr, buf, ret);
}

static struct qlink_ctrl_client *qlink_ctrl_client_get(struct qlink_driver_data *drv,
	const char *addr)
{
	static int counter = 0;
	struct qlink_ctrl_client *cc;
	int ret;

	dl_list_for_each(cc, &drv->ctrl_clients, struct qlink_ctrl_client, list) {
		if (strcmp(cc->remote_addr, addr) == 0) {
			return cc;
		}
	}

	cc = os_zalloc(sizeof(*cc));

	if (!cc) {
		wpa_printf(MSG_ERROR, "cannot allocate ctrl_client, no mem");
		goto fail_alloc;
	}

	cc->drv = drv;
	strncpy(cc->remote_addr, addr, sizeof(cc->remote_addr) - 1);

	cc->sock = socket(PF_UNIX, SOCK_DGRAM, 0);

	if (cc->sock < 0) {
		wpa_printf(MSG_ERROR, "cannot create unix socket");
		goto fail_sock;
	}

	cc->local_addr.sun_family = AF_UNIX;

	counter++;

	ret = os_snprintf(cc->local_addr.sun_path, sizeof(cc->local_addr.sun_path),
		"/tmp/hapd_ctrl_%d-%d", (int)getpid(), counter);
	if (ret < 0 || (size_t) ret >= sizeof(cc->local_addr.sun_path)) {
		wpa_printf(MSG_ERROR, "cannot form unix socket path");
		goto fail_path;
	}

	ret = bind(cc->sock, (struct sockaddr *)&cc->local_addr, sizeof(cc->local_addr));

	if ((ret < 0) && (errno == EADDRINUSE)) {
		/*
		 * getpid() returns unique identifier for this instance
		 * of hapd, so the existing socket file must have
		 * been left by unclean termination of an earlier run.
		 * Remove the file and try again.
		 */
		unlink(cc->local_addr.sun_path);
		ret = bind(cc->sock, (struct sockaddr *)&cc->local_addr, sizeof(cc->local_addr));
	}

	if (ret < 0) {
		wpa_printf(MSG_ERROR, "cannot bind to %s", cc->local_addr.sun_path);
		goto fail_path;
	}

	if (connect(cc->sock, (struct sockaddr *)&drv->ctrl_addr, sizeof(drv->ctrl_addr)) < 0) {
		wpa_printf(MSG_ERROR, "cannot connect to %s", drv->ctrl_addr.sun_path);
		goto fail_connect;
	}

	dl_list_add(&drv->ctrl_clients, &cc->list);

	eloop_register_read_sock(cc->sock, qlink_ctrl_client_receive, cc, NULL);

	wpa_printf(MSG_DEBUG, "new ctrl client %s", addr);

	return cc;

fail_connect:
	unlink(cc->local_addr.sun_path);
fail_path:
	close(cc->sock);
fail_sock:
	os_free(cc);
fail_alloc:

	return NULL;
}

static void qlink_ctrl_client_remove(struct qlink_ctrl_client *cc)
{
	wpa_printf(MSG_DEBUG, "ctrl client %s removed", cc->remote_addr);

	dl_list_del(&cc->list);
	eloop_unregister_read_sock(cc->sock);
	unlink(cc->local_addr.sun_path);
	close(cc->sock);
	os_free(cc);
}

static void qlink_ctrl_client_remove_by_addr(struct qlink_driver_data *drv, const char *addr)
{
	struct qlink_ctrl_client *cc;

	dl_list_for_each(cc, &drv->ctrl_clients, struct qlink_ctrl_client, list) {
		if (strcmp(cc->remote_addr, addr) == 0) {
			qlink_ctrl_client_remove(cc);
			break;
		}
	}
}

static void qlink_ctrl_client_remove_all(struct qlink_driver_data *drv)
{
	struct qlink_ctrl_client *cc, *tmp;

	dl_list_for_each_safe(cc, tmp, &drv->ctrl_clients, struct qlink_ctrl_client, list) {
		qlink_ctrl_client_remove(cc);
	}
}

static int qlink_ctrl_client_send(struct qlink_ctrl_client *cc, const char *data, int len)
{
	int ret;

	do {
		ret = send(cc->sock, data, len, 0);
	} while ((ret < 0) && (errno == EINTR));

	if (ret != len) {
		wpa_printf(MSG_ERROR, "failed to send to ctrl interface, len = %d, ret = %d",
			len, ret);
		return -1;
	}

	return 0;
}

/*
 * @}
 */

static struct qlink_bss *qlink_get_bss(struct qlink_driver_data *drv, uint32_t qid)
{
	struct qlink_bss *bss;

	dl_list_for_each(bss, &drv->bss, struct qlink_bss, list) {
		if (bss->qid == qid) {
			return bss;
		}
	}

	wpa_printf(MSG_ERROR, "bss %u not found", qid);

	return NULL;
}

static void qlink_client_ping_timeout(void *eloop_data, void *user_ctx);
static void qlink_server_ping_timeout(void *eloop_data, void *user_ctx);

static void qlink_client_ready_timeout(void *eloop_data, void *user_ctx)
{
	struct qlink_driver_data *drv = eloop_data;

	qlink_client_ready(drv->qc);
}

static void qlink_terminate(struct qlink_driver_data *drv)
{
	qlink_ctrl_client_remove_all(drv);

	eloop_cancel_timeout(&qlink_server_ping_timeout, drv, NULL);
	eloop_cancel_timeout(&qlink_client_ping_timeout, drv, NULL);

	qlink_client_set_active(drv->qc, 0);
	drv->active = 0;

	/*
	 * We've lost connection to the server, the best thing we can do is to exit hostapd.
	 * hostapd must be restarted by the system in order to establish new connection to the
	 * server again.
	 */
	eloop_terminate();
}

static void qlink_client_ping_timeout(void *eloop_data, void *user_ctx)
{
	struct qlink_driver_data *drv = eloop_data;

	qlink_client_ping(drv->qc);

	eloop_register_timeout_ms(QLINK_PING_TIMEOUT, &qlink_client_ping_timeout, drv);
}

static void qlink_server_ping_timeout(void *eloop_data, void *user_ctx)
{
	struct qlink_driver_data *drv = eloop_data;

	wpa_printf(MSG_ERROR, "qlink server ping timeout!");

	qlink_terminate(drv);
}

/*
 * wait until a handshake command is seen from server with proper protocol version and guid set.
 * returns:
 * < 0 on error
 * == 0 on success
 * > 0 on timeout
 */
static int qlink_wait_valid_handshake(struct qlink_driver_data *drv, const uint8_t *guid,
	uint32_t timeout_ms, uint32_t *server_ping_timeout_ms)
{
	int ret;
	struct timeval tv_start, tv_cur, tv;
	uint32_t passed_ms = 0, remain_ms;
	uint8_t server_guid[QLINK_GUID_BUF_SIZE];
	uint32_t server_version = 0;

	gettimeofday(&tv_start, NULL);

	while (passed_ms <= timeout_ms) {
		remain_ms = timeout_ms - passed_ms;

		ret = qlink_client_wait_handshake(drv->qc, remain_ms, &server_version,
			server_guid, server_ping_timeout_ms);

		if (ret < 0) {
			return -1;
		}

		if ((ret == 0) && (server_version == QLINK_VERSION) &&
			(os_memcmp(server_guid, guid, sizeof(server_guid)) == 0)) {
			return 0;
		}

		gettimeofday(&tv_cur, NULL);

		timersub(&tv_cur, &tv_start, &tv);

		passed_ms = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
	}

	return 1;
}

static void *qlink_init_bss(struct qlink_driver_data *drv, struct hostapd_data *hapd,
	const char *name, const char *brname, int is_vap, const uint8_t *bssid, uint8_t *own_addr)
{
	struct qlink_bss *bss;
	int ret;

	bss = os_zalloc(sizeof(struct qlink_bss));
	if (bss == NULL) {
		goto fail_alloc;
	}

	bss->drv = drv;
	bss->bss_ctx = hapd;
	strncpy(bss->ifname, name, sizeof(bss->ifname) - 1);
	strncpy(bss->brname, brname, sizeof(bss->brname) - 1);

	++drv->bss_next_qid;
	if (drv->bss_next_qid == 0)
		++drv->bss_next_qid;

	bss->qid = drv->bss_next_qid;

	if (is_vap)
		ret = qlink_client_vap_add(drv->qc, bss->qid, bssid, name, brname, own_addr);
	else
		ret = qlink_client_bss_add(drv->qc, bss->qid, name, brname, own_addr);

	if (ret != 0)
		goto fail_qlink;

	dl_list_add_tail(&drv->bss, &bss->list);

	return bss;

fail_qlink:
	os_free(bss);
fail_alloc:

	return NULL;
}

static void qlink_bss_remove(struct qlink_bss *bss)
{
	dl_list_del(&bss->list);
	qlink_client_bss_remove(bss->drv->qc, bss->qid);
	os_free(bss);
}

static int qlink_send_security_config(struct qlink_driver_data *drv, struct hostapd_iface *iface)
{
	int ret = -1;
	FILE *f;
	int len;
	char *config;

	f = fopen(iface->config_fname, "r");
	if (f == NULL) {
		wpa_printf(MSG_ERROR, "Could not open configuration file '%s' "
			   "for reading.", iface->config_fname);
		goto out_open;
	}

	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (len < 0) {
		wpa_printf(MSG_ERROR, "cannot get file '%s' size", iface->config_fname);
		goto out_alloc;
	}

	if (len > (QLINK_MSG_BUF_SIZE - 1)) {
		wpa_printf(MSG_ERROR, "file '%s' is too big", iface->config_fname);
		goto out_alloc;
	}

	config = os_malloc(len + 1);
	if (!config) {
		wpa_printf(MSG_ERROR, "cannot alloc config buffer, no mem");
		goto out_alloc;
	}

	if (fread(config, 1, len, f) != len) {
		wpa_printf(MSG_ERROR, "error reading config file");
		goto out_read;
	}

	config[len] = '\0';

	ret = qlink_client_update_security_config(drv->qc, config);

out_read:
	os_free(config);
out_alloc:
	fclose(f);
out_open:

	return ret;
}

static int qlink_write_security_config(struct hostapd_iface *iface, const char *security_config)
{
	char tmp_path[PATH_MAX * 2];
	char *config_file;
	FILE *f;
	int len;

	config_file = strrchr(iface->config_fname, '/');
	if (config_file) {
		memcpy(tmp_path, iface->config_fname, (config_file - iface->config_fname));
		tmp_path[config_file - iface->config_fname] = '\0';
		strcat(tmp_path, "/");
		strcat(tmp_path, QLINK_TEMPORARY_CONFIG_FILE);
	} else {
		strcpy(tmp_path, QLINK_TEMPORARY_CONFIG_FILE);
	}

	f = fopen(tmp_path, "w");
	if (f == NULL) {
		wpa_printf(MSG_ERROR, "Could not open temporary configuration file '%s' "
			   "for writing.", tmp_path);
		return -1;
	}

	len = strlen(security_config);

	if (fwrite(security_config, 1, len, f) != len) {
		wpa_printf(MSG_ERROR, "error writing tmp config file %s", tmp_path);
		fclose(f);
		unlink(tmp_path);
		return -1;
	}

	fclose(f);

	if (rename(tmp_path, iface->config_fname) != 0) {
		wpa_printf(MSG_ERROR, "cannot rename %s to %s", tmp_path, iface->config_fname);
		unlink(tmp_path);
		return -1;
	}

	return 0;
}

/*
 * Configure WPA parameters.
 */
static int qlink_configure_wpa(struct qlink_bss *bss, struct wpa_bss_params *params)
{
	int v;
	switch (params->wpa_group) {
	case WPA_CIPHER_CCMP:
		v = IEEE80211_CIPHER_AES_CCM;
		break;
	case WPA_CIPHER_TKIP:
		v = IEEE80211_CIPHER_TKIP;
		break;
	case WPA_CIPHER_WEP104:
		v = IEEE80211_CIPHER_WEP;
		break;
	case WPA_CIPHER_WEP40:
		v = IEEE80211_CIPHER_WEP;
		break;
	case WPA_CIPHER_NONE:
		v = IEEE80211_CIPHER_NONE;
		break;
	default:
		wpa_printf(MSG_ERROR, "Unknown group key cipher %u",
			   params->wpa_group);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "%s: group key cipher=%d", __func__, v);
	if (qlink_client_set_80211_param(bss->drv->qc, bss->qid, IEEE80211_PARAM_MCASTCIPHER, v)) {
		printf("Unable to set group key cipher to %u\n", v);
		return -1;
	}
	if (v == IEEE80211_CIPHER_WEP) {
		/* key length is done only for specific ciphers */
		v = (params->wpa_group == WPA_CIPHER_WEP104 ? 13 : 5);
		if (qlink_client_set_80211_param(bss->drv->qc, bss->qid,
			IEEE80211_PARAM_MCASTKEYLEN, v)) {
			printf("Unable to set group key length to %u\n", v);
			return -1;
		}
	}

	v = 0;
	if (params->wpa_pairwise & WPA_CIPHER_CCMP)
		v |= 1<<IEEE80211_CIPHER_AES_CCM;
	if (params->wpa_pairwise & WPA_CIPHER_TKIP)
		v |= 1<<IEEE80211_CIPHER_TKIP;
	if (params->wpa_pairwise & WPA_CIPHER_NONE)
		v |= 1<<IEEE80211_CIPHER_NONE;
	wpa_printf(MSG_DEBUG, "%s: pairwise key ciphers=0x%x", __func__, v);
	if (qlink_client_set_80211_param(bss->drv->qc, bss->qid,
		IEEE80211_PARAM_UCASTCIPHERS, v)) {
		printf("Unable to set pairwise key ciphers to 0x%x\n", v);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "%s: key management algorithms=0x%x",
		   __func__, params->wpa_key_mgmt);
	if (qlink_client_set_80211_param(bss->drv->qc, bss->qid, IEEE80211_PARAM_KEYMGTALGS,
			  params->wpa_key_mgmt)) {
		printf("Unable to set key management algorithms to 0x%x\n",
			params->wpa_key_mgmt);
		return -1;
	}

	v = 0;
	if (params->rsn_preauth)
		v |= BIT(0);
	wpa_printf(MSG_DEBUG, "%s: rsn capabilities=0x%x",
		   __func__, params->rsn_preauth);
	if (qlink_client_set_80211_param(bss->drv->qc, bss->qid, IEEE80211_PARAM_RSNCAPS, v)) {
		printf("Unable to set RSN capabilities to 0x%x\n", v);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "%s: enable WPA=0x%x", __func__, params->wpa);
	if (qlink_client_set_80211_param(bss->drv->qc, bss->qid, IEEE80211_PARAM_WPA,
		params->wpa)) {
		printf("Unable to set WPA to %u\n", params->wpa);
		return -1;
	}
	return 0;
}

static int qlink_del_key(void *priv, const u8 *addr, int key_idx)
{
	struct qlink_bss *bss = priv;
	struct ieee80211req_del_key wk;
	int ret;

	wpa_printf(MSG_DEBUG, "%s: addr=%s key_idx=%d",
		   __func__, ether_sprintf(addr), key_idx);

	memset(&wk, 0, sizeof(wk));
	if (addr != NULL) {
		memcpy(wk.idk_macaddr, addr, IEEE80211_ADDR_LEN);
		wk.idk_keyix = (u8) IEEE80211_KEYIX_NONE;
	} else {
		wk.idk_keyix = key_idx;
		memset(wk.idk_macaddr, 0xff, IEEE80211_ADDR_LEN);
	}

	ret = qlink_client_set_80211_priv(bss->drv->qc, bss->qid, IEEE80211_IOCTL_DELKEY,
		&wk, sizeof(wk));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to delete key (addr %s"
			   " key_idx %d)", __func__, ether_sprintf(addr),
			   key_idx);
	}

	return ret;
}

#ifdef CONFIG_WPS
/**
 * return 0 if Probe request packet is received and handled
 * return -1 if frame is not a probe request frame
 */
static int qlink_raw_recv_probe_req(void *ctx, const u8 *src_addr, const u8 *buf,
	size_t len)
{
	struct qlink_bss *bss = ctx;
	const struct ieee80211_mgmt *mgmt;
	u16 fc;
	union wpa_event_data event;

	/* Send Probe Request information to WPS processing */

	if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.probe_req))
		return -1;
	mgmt = (const struct ieee80211_mgmt *) buf;

	fc = le_to_host16(mgmt->frame_control);
	if (WLAN_FC_GET_TYPE(fc) != WLAN_FC_TYPE_MGMT ||
	    WLAN_FC_GET_STYPE(fc) != WLAN_FC_STYPE_PROBE_REQ)
		return -1;

	os_memset(&event, 0, sizeof(event));
	event.rx_probe_req.sa = mgmt->sa;
	event.rx_probe_req.da = mgmt->da;
	event.rx_probe_req.bssid = mgmt->bssid;
	event.rx_probe_req.ie = mgmt->u.probe_req.variable;
	event.rx_probe_req.ie_len =
		len - (IEEE80211_HDRLEN + sizeof(mgmt->u.probe_req));
	wpa_supplicant_event(bss->bss_ctx, EVENT_RX_PROBE_REQ, &event);

	return 0;
}
#endif /* CONFIG_WPS */

#ifdef CONFIG_HS20
static int qlink_raw_recv_hs20(void *ctx, const u8 *src_addr, const u8 *buf,
	size_t len)
{
	struct qlink_bss *bss = ctx;
	const struct ieee80211_mgmt *mgmt;
	union wpa_event_data event;
	u16 fc;

	/* Send the Action frame for HS20 processing */

	if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.action.category) +
			sizeof(mgmt->u.action.u.public_action))
		return -1;

	mgmt = (const struct ieee80211_mgmt *) buf;

	fc = le_to_host16(mgmt->frame_control);

	if (WLAN_FC_GET_TYPE(fc) != WLAN_FC_TYPE_MGMT ||
			WLAN_FC_GET_STYPE(fc) != WLAN_FC_STYPE_ACTION ||
			mgmt->u.action.category != WLAN_ACTION_PUBLIC)
		return -1;

	/*TODO We are handling Public Action Frames now, Dropping other types
	 * of action frames */
	mgmt = (const struct ieee80211_mgmt *) buf;
	wpa_printf(MSG_DEBUG, "%s:Received Public Action frame\n", __func__);

	os_memset(&event, 0, sizeof(event));
	event.rx_mgmt.frame = (const u8 *) mgmt;
	event.rx_mgmt.frame_len = len;
	wpa_supplicant_event(bss->bss_ctx, EVENT_RX_MGMT, &event);

	return 0;
}
#endif

static int qlink_sta_disassoc(void *priv, const u8 *own_addr, const u8 *addr,
	int reason_code);

static void qlink_new_sta(struct qlink_bss *bss, u8 addr[IEEE80211_ADDR_LEN])
{
	struct ieee80211req_wpaie ie;
	int ielen = 0;
	u8 *iebuf = NULL;
	int res;
	struct hostapd_data *hapd = bss->bss_ctx;

	if (qtn_hapd_acl_reject(hapd, addr)){
		/* This reason code is used only by the driver, for blacklisting */
		res = qlink_sta_disassoc(bss, hapd->own_addr, addr, WLAN_REASON_DENIED);
		if (res < 0) {
			wpa_printf(MSG_ERROR, "%s: Failed to disassociate STA (addr " MACSTR
				   " that is denied by MAC address ACLs)",
				   __func__, MAC2STR(addr));
		}
		return;
	}

	/*
	 * Fetch negotiated WPA/RSN parameters from the system.
	 */
	memset(&ie, 0, sizeof(ie));
	memcpy(ie.wpa_macaddr, addr, IEEE80211_ADDR_LEN);
	if (qlink_client_set_80211_priv(bss->drv->qc, bss->qid, IEEE80211_IOCTL_GETWPAIE,
		&ie, sizeof(ie))) {
		wpa_printf(MSG_DEBUG, "%s: Failed to get WPA/RSN IE",
			   __func__);
		goto no_ie;
	}

	/*
	 * Handling Pairing hash IE here.
	 */
	if (qtn_hapd_pairingie_handle(bss, hapd, addr, &ie) < 0) {
		qlink_sta_disassoc(bss, hapd->own_addr,
					addr, IEEE80211_REASON_IE_INVALID);
		return;
	}
	wpa_hexdump(MSG_MSGDUMP, "madwifi req WPA IE",
		    ie.wpa_ie, IEEE80211_MAX_OPT_IE);
	iebuf = ie.wpa_ie;
	/* madwifi seems to return some random data if WPA/RSN IE is not set.
	 * Assume the IE was not included if the IE type is unknown. */
	if (iebuf[0] != WLAN_EID_VENDOR_SPECIFIC)
		iebuf[1] = 0;
#ifdef MADWIFI_NG
	wpa_hexdump(MSG_MSGDUMP, "madwifi req RSN IE",
		    ie.rsn_ie, IEEE80211_MAX_OPT_IE);
	if (iebuf[1] == 0 && ie.rsn_ie[1] > 0) {
		/* madwifi-ng svn #1453 added rsn_ie. Use it, if wpa_ie was not
		 * set. This is needed for WPA2. */
		iebuf = ie.rsn_ie;
		if (iebuf[0] != WLAN_EID_RSN)
			iebuf[1] = 0;
	}
#endif /* MADWIFI_NG */

	ielen = iebuf[1];

	if (((ie.wps_ie[1] > 0) &&
	     (ie.wps_ie[0] == WLAN_EID_VENDOR_SPECIFIC))) {
		iebuf = ie.wps_ie;
		ielen = ie.wps_ie[1];
	}

#ifdef CONFIG_HS20
	wpa_hexdump(MSG_MSGDUMP, "qlink req OSEN IE",
		    ie.osen_ie, IEEE80211_MAX_OPT_IE);
	if (ielen == 0 && ie.osen_ie[1] > 0) {
		iebuf = ie.osen_ie;
		ielen = ie.osen_ie[1];
	}
#endif

	if (ielen == 0)
		iebuf = NULL;
	else
		ielen += 2;

no_ie:
	drv_event_assoc(bss->bss_ctx, addr, iebuf, ielen, 0);

	if (memcmp(addr, bss->acct_mac, ETH_ALEN) == 0) {
		/* Cached accounting data is not valid anymore. */
		memset(bss->acct_mac, 0, ETH_ALEN);
		memset(&bss->acct_data, 0, sizeof(bss->acct_data));
	}
}

static void qlink_generate_wds_key(const u8 *mbs_addr, const u8 *rbs_addr, const u8 *pmk,
	u8 *wds_key)
{
	const u8 *addr[3];
	size_t len[3];

	addr[0] = mbs_addr;
	len[0] = ETH_ALEN;
	addr[1] = rbs_addr;
	len[1] = ETH_ALEN;
	addr[2] = pmk;
	len[2] = PMK_LEN;

	sha256_vector(3, addr, len, wds_key);

	wpa_hexdump(MSG_ERROR, "mbs_mac:", mbs_addr, ETH_ALEN);
	wpa_hexdump(MSG_ERROR, "rbs_mac:", rbs_addr, ETH_ALEN);
	wpa_hexdump(MSG_ERROR, "pmk:", pmk, PMK_LEN);
	wpa_hexdump(MSG_ERROR, "wds_key:", wds_key, PMK_LEN);
}

static void qlink_wireless_event_wireless_custom(struct qlink_bss *bss, char *custom)
{
	wpa_printf(MSG_DEBUG, "Custom wireless event: '%s'", custom);

	if (strncmp(custom, "MLME-MICHAELMICFAILURE.indication", 33) == 0) {
		char *pos;
		/* Local - default to 'yes' */
		int local = 1;
		u8 addr[ETH_ALEN];
		pos = strstr(custom, "addr=");
		if (pos == NULL) {
			wpa_printf(MSG_DEBUG,
				   "MLME-MICHAELMICFAILURE.indication "
				   "without sender address ignored");
			return;
		}
		pos += 5;
		/* Quantenna - go into countermeasures regardless */
		if (strstr(custom, "qtn=1"))
		{
			/* Ensure for Quantenna devices we don't check the MAC address */
			local = 0;
		}
		if (hwaddr_aton(pos, addr) == 0) {
			union wpa_event_data data;
			os_memset(&data, 0, sizeof(data));
			data.michael_mic_failure.unicast = 1;
			data.michael_mic_failure.local = local;
			data.michael_mic_failure.src = addr;
			wpa_supplicant_event(bss->bss_ctx,
					     EVENT_MICHAEL_MIC_FAILURE, &data);
		} else {
			wpa_printf(MSG_DEBUG,
				   "MLME-MICHAELMICFAILURE.indication "
				   "with invalid MAC address");
		}
	} else if (strncmp(custom, "STA-TRAFFIC-STAT", 16) == 0) {
		char *key, *value;
		u32 val;
		key = custom;
		while ((key = strchr(key, '\n')) != NULL) {
			key++;
			value = strchr(key, '=');
			if (value == NULL)
				continue;
			*value++ = '\0';
			val = strtoul(value, NULL, 10);
			if (strcmp(key, "mac") == 0)
				hwaddr_aton(value, bss->acct_mac);
			else if (strcmp(key, "rx_packets") == 0)
				bss->acct_data.rx_packets = val;
			else if (strcmp(key, "tx_packets") == 0)
				bss->acct_data.tx_packets = val;
			else if (strcmp(key, "rx_bytes") == 0)
				bss->acct_data.rx_bytes = val;
			else if (strcmp(key, "tx_bytes") == 0)
				bss->acct_data.tx_bytes = val;
			key = value;
		}
	} else if (os_strncmp(custom, "WPS-BUTTON.indication", 21) == 0) {
		hostapd_wps_button_pushed(bss->bss_ctx, NULL);
	} else if (os_strncmp(custom, "STA-REQUIRE-LEAVE", 17) == 0) {
		u8 addr[ETH_ALEN];
		char *addr_str;
		addr_str = os_strchr(custom, '=');
		if (addr_str != NULL) {
			addr_str++;
			hwaddr_aton(addr_str, addr);
			hostapd_sta_require_leave(bss->bss_ctx, addr);
		}
	}
}

static int qlink_set_sta_authorized(void *priv, const u8 *addr, int authorized)
{
	struct qlink_bss *bss = priv;
	struct ieee80211req_mlme mlme;
	int ret;

	wpa_printf(MSG_DEBUG, "%s: addr=%s authorized=%d",
		   __func__, ether_sprintf(addr), authorized);

	if (authorized)
		mlme.im_op = IEEE80211_MLME_AUTHORIZE;
	else
		mlme.im_op = IEEE80211_MLME_UNAUTHORIZE;
	mlme.im_reason = 0;
	memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	ret = qlink_client_set_80211_priv(bss->drv->qc, bss->qid, IEEE80211_IOCTL_SETMLME,
		&mlme, sizeof(mlme));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to %sauthorize STA " MACSTR,
			   __func__, authorized ? "" : "un", MAC2STR(addr));
	}

	return ret;
}

/*
 * qlink operation callbacks.
 */

static void qlink_op_ping(void *ptr)
{
	struct qlink_driver_data *drv = ptr;

	if (!drv->active)
		return;

	eloop_cancel_timeout(&qlink_server_ping_timeout, drv, NULL);

	eloop_register_timeout_ms(drv->server_ping_timeout_ms, &qlink_server_ping_timeout, drv);
}

static void qlink_op_handshake(void *ptr, uint32_t version, const uint8_t *guid,
	uint32_t ping_timeout_ms)
{
	/*
	 * Just issue a warning here, we should ignore handshakes from the server during operation,
	 * we only handle handshakes on initialization.
	 */

	wpa_printf(MSG_WARNING, "qlink server handshake");
}

static int qlink_op_disconnect(void *ptr)
{
	struct qlink_driver_data *drv = ptr;

	if (!drv->active)
		return -1;

	wpa_printf(MSG_WARNING, "qlink server shutdown!");

	qlink_terminate(drv);

	return 0;
}

static int qlink_op_update_security_config(void *ptr, const char *security_config)
{
	struct qlink_driver_data *drv = ptr;

	if (!drv->active)
		return -1;

	wpa_printf(MSG_DEBUG, "op_update_security_config(%d)", (int)strlen(security_config));

	return qlink_write_security_config(drv->hapd->iface, security_config);
}

static int qlink_op_recv_packet(void *ptr, uint32_t bss_qid, uint16_t protocol,
	const uint8_t *src_addr, const uint8_t *buf, size_t len)
{
	struct qlink_driver_data *drv = ptr;
	struct qlink_bss *bss;

	if (!drv->active)
		return -1;

	bss = qlink_get_bss(drv, bss_qid);

	if (!bss)
		return -1;

	switch (protocol) {
	case ETH_P_EAPOL:
		if (len < sizeof(struct l2_ethhdr)) {
			wpa_printf(MSG_ERROR, "recv_packet: eapol packet too short");
			return -1;
		}

		drv_event_eapol_rx(bss->bss_ctx, src_addr, buf + sizeof(struct l2_ethhdr),
			len - sizeof(struct l2_ethhdr));
		break;
	case ETH_P_80211_RAW:
#ifdef CONFIG_WPS
		if (qlink_raw_recv_probe_req(bss, src_addr, buf, len) == 0)
			break;
#endif
#ifdef CONFIG_HS20
		qlink_raw_recv_hs20(bss, src_addr, buf, len);
#endif
		break;
	default:
		wpa_printf(MSG_ERROR, "recv_packet: bad protocol = 0x%X", (int)protocol);
		return -1;
	}

	return 0;
}

static int qlink_op_recv_iwevent(void *ptr, uint32_t bss_qid, uint16_t cmd, char *data, int len)
{
	struct qlink_driver_data *drv = ptr;
	struct qlink_bss *bss;

	if (!drv->active)
		return -1;

	bss = qlink_get_bss(drv, bss_qid);

	if (!bss)
		return -1;

	switch (cmd) {
	case IWEVEXPIRED:
		drv_event_disassoc(bss->bss_ctx, (u8 *)data);
		break;
	case IWEVREGISTERED:
		qlink_new_sta(bss, (u8 *)data);
		break;
	case IWEVCUSTOM:
		qlink_wireless_event_wireless_custom(bss, data);
		break;
	default:
		wpa_printf(MSG_ERROR, "recv_iwevent: unsupported cmd %d", (int)cmd);
		break;
	}

	return 0;
}

static int qlink_op_recv_ctrl(void *ptr, const char *addr, char *data, int len)
{
	struct qlink_driver_data *drv = ptr;
	struct qlink_ctrl_client *cc;

	if (!drv->active)
		return -1;

	cc = qlink_ctrl_client_get(drv, addr);

	if (!cc)
		return -1;

	return qlink_ctrl_client_send(cc, data, len);
}

static int qlink_op_invalidate_ctrl(void *ptr, const char *addr)
{
	struct qlink_driver_data *drv = ptr;

	if (!drv->active)
		return -1;

	qlink_ctrl_client_remove_by_addr(drv, addr);

	return 0;
}

static struct qlink_client_ops qlink_ops = {
	.ping = &qlink_op_ping,
	.handshake = &qlink_op_handshake,
	.disconnect = &qlink_op_disconnect,
	.update_security_config = &qlink_op_update_security_config,
	.recv_packet = &qlink_op_recv_packet,
	.recv_iwevent = &qlink_op_recv_iwevent,
	.recv_ctrl = &qlink_op_recv_ctrl,
	.invalidate_ctrl = &qlink_op_invalidate_ctrl,
};

/*
 * qlink driver callbacks.
 */

static int qlink_set_ieee8021x(void *priv, struct wpa_bss_params *params)
{
	struct qlink_bss *bss = priv;

	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __func__, params->enabled);

	if (!params->enabled) {
		/* XXX restore state */
		return qlink_client_set_80211_param(bss->drv->qc, bss->qid, IEEE80211_PARAM_AUTHMODE,
			IEEE80211_AUTH_AUTO);
	}
	if (!params->wpa && !params->ieee802_1x) {
		wpa_printf(MSG_WARNING, "No 802.1X or WPA enabled!");
		return -1;
	}
	if (params->wpa && qlink_configure_wpa(bss, params) != 0) {
		wpa_printf(MSG_WARNING, "Error configuring WPA state!");
		return -1;
	}
	if (qlink_client_set_80211_param(bss->drv->qc, bss->qid, IEEE80211_PARAM_AUTHMODE,
		(params->wpa ? IEEE80211_AUTH_WPA : IEEE80211_AUTH_8021X))) {
		wpa_printf(MSG_WARNING, "Error enabling WPA/802.1X!");
		return -1;
	}
	if (qlink_client_set_80211_param(bss->drv->qc, bss->qid, IEEE80211_PARAM_CONFIG_PMF,
			(params->ieee80211w ? params->ieee80211w + 1 : 0))) {
		wpa_printf(MSG_WARNING, "Error enabling PMF");
		return -1;
	}

	return 0;
}

static int qlink_set_privacy(void *priv, int enabled)
{
	if (priv == NULL) {
		return 0;
	}

	struct qlink_bss *bss = priv;

	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __func__, enabled);

	return qlink_client_set_80211_param(bss->drv->qc, bss->qid, IEEE80211_PARAM_PRIVACY,
		enabled);
}

static int qlink_sta_set_flags(void *priv, const u8 *addr,
	int total_flags, int flags_or, int flags_and)
{
	/* For now, only support setting Authorized flag */
	if (flags_or & WPA_STA_AUTHORIZED)
		return qlink_set_sta_authorized(priv, addr, 1);
	if (!(flags_and & WPA_STA_AUTHORIZED))
		return qlink_set_sta_authorized(priv, addr, 0);
	return 0;
}

static int qlink_brcm_info_ioctl(void *priv, uint8_t *data, uint32_t len)
{
	struct qlink_bss *bss = priv;

	if (qlink_client_brcm(bss->drv->qc, bss->qid, data, len) != 0) {
		wpa_printf(MSG_DEBUG, "Failed to do brcm info ioctl");
		return -1;
	}

	return 0;
}

static int qlink_set_key(const char *ifname, void *priv, enum wpa_alg alg,
	const u8 *addr, int key_idx, int set_tx,
	const u8 *seq, size_t seq_len,
	const u8 *key, size_t key_len)
{
	struct qlink_bss *bss = priv;
	struct ieee80211req_key wk;
	u_int8_t cipher;
	int ret;
	const char *vlanif;
	int vlanid;

	if (alg == WPA_ALG_NONE)
		return qlink_del_key(bss, addr, key_idx);

	wpa_printf(MSG_DEBUG, "%s: alg=%d addr=%s key_idx=%d",
		   __func__, alg, ether_sprintf(addr), key_idx);
	wpa_hexdump_key(MSG_DEBUG, "set-key", key, key_len);

	if (alg == WPA_ALG_WEP)
		cipher = IEEE80211_CIPHER_WEP;
	else if (alg == WPA_ALG_TKIP)
		cipher = IEEE80211_CIPHER_TKIP;
	else if (alg == WPA_ALG_CCMP)
		cipher = IEEE80211_CIPHER_AES_CCM;
	else if (alg == WPA_ALG_IGTK)
		cipher = IEEE80211_CIPHER_AES_CMAC;
	else {
		printf("%s: unknown/unsupported algorithm %d\n",
			__func__, alg);
		return -1;
	}

	if (key_len > sizeof(wk.ik_keydata)) {
		printf("%s: key length %lu too big\n", __func__,
		       (unsigned long) key_len);
		return -1;
	}

	memset(&wk, 0, sizeof(wk));
	wk.ik_type = cipher;
	wk.ik_flags = IEEE80211_KEY_RECV | IEEE80211_KEY_XMIT;
	if (addr == NULL || is_broadcast_ether_addr(addr)) {
		memset(wk.ik_macaddr, 0xff, IEEE80211_ADDR_LEN);
		wk.ik_keyix = key_idx;
		wk.ik_flags |= IEEE80211_KEY_DEFAULT;
	} else {
		memcpy(wk.ik_macaddr, addr, IEEE80211_ADDR_LEN);
		wk.ik_keyix = IEEE80211_KEYIX_NONE;
	}

	vlanif = strstr(ifname, "vlan");
	if (vlanif) {
		sscanf(ifname, "vlan%d", &vlanid);
		memset(wk.ik_macaddr, 0xff, IEEE80211_ADDR_LEN);
		wk.ik_vlan = host_to_le16(vlanid);
		wk.ik_flags |= (IEEE80211_KEY_VLANGROUP | IEEE80211_KEY_GROUP);
	}

	wk.ik_keylen = key_len;
	memcpy(wk.ik_keydata, key, key_len);

	ret = qlink_client_set_80211_priv(bss->drv->qc, bss->qid, IEEE80211_IOCTL_SETKEY, &wk,
		sizeof(wk));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to set key (addr %s"
			   " key_idx %d alg %d key_len %lu set_tx %d)",
			   __func__, ether_sprintf(wk.ik_macaddr), key_idx,
			   alg, (unsigned long) key_len, set_tx);
	}

	return ret;
}

static int qlink_get_seqnum(const char *ifname, void *priv, const u8 *addr, int idx,
	u8 *seq)
{
	struct qlink_bss *bss = priv;
	struct ieee80211req_key wk;

	wpa_printf(MSG_DEBUG, "%s: addr=%s idx=%d",
		   __func__, ether_sprintf(addr), idx);

	memset(&wk, 0, sizeof(wk));
	if (addr == NULL)
		memset(wk.ik_macaddr, 0xff, IEEE80211_ADDR_LEN);
	else
		memcpy(wk.ik_macaddr, addr, IEEE80211_ADDR_LEN);
	wk.ik_keyix = idx;

	if (qlink_client_set_80211_priv(bss->drv->qc, bss->qid, IEEE80211_IOCTL_GETKEY, &wk,
		sizeof(wk))) {
		wpa_printf(MSG_DEBUG, "%s: Failed to get encryption data "
			   "(addr " MACSTR " key_idx %d)",
			   __func__, MAC2STR(wk.ik_macaddr), idx);
		return -1;
	}

#ifdef WORDS_BIGENDIAN
	{
		/*
		 * wk.ik_keytsc is in host byte order (big endian), need to
		 * swap it to match with the byte order used in WPA.
		 */
		int i;
		u8 tmp[WPA_KEY_RSC_LEN];
		memcpy(tmp, &wk.ik_keytsc, sizeof(wk.ik_keytsc));
		for (i = 0; i < WPA_KEY_RSC_LEN; i++) {
			seq[i] = tmp[WPA_KEY_RSC_LEN - i - 1];
		}
	}
#else /* WORDS_BIGENDIAN */
	memcpy(seq, &wk.ik_keytsc, sizeof(wk.ik_keytsc));
#endif /* WORDS_BIGENDIAN */
	return 0;
}

static int qlink_read_sta_driver_data(void *priv, struct hostap_sta_driver_data *data,
	const u8 *addr)
{
	struct qlink_bss *bss = priv;
	struct ieee80211req_sta_stats stats;

	memset(data, 0, sizeof(*data));

	/*
	 * Fetch statistics for station from the system.
	 */
	memset(&stats, 0, sizeof(stats));
	memcpy(stats.is_u.macaddr, addr, IEEE80211_ADDR_LEN);
	if (qlink_client_set_80211_priv(bss->drv->qc, bss->qid,
#ifdef MADWIFI_NG
			 IEEE80211_IOCTL_STA_STATS,
#else /* MADWIFI_NG */
			 IEEE80211_IOCTL_GETSTASTATS,
#endif /* MADWIFI_NG */
			 &stats, sizeof(stats))) {
		wpa_printf(MSG_DEBUG, "%s: Failed to fetch STA stats (addr "
			   MACSTR ")", __func__, MAC2STR(addr));
		if (memcmp(addr, bss->acct_mac, ETH_ALEN) == 0) {
			memcpy(data, &bss->acct_data, sizeof(*data));
			return 0;
		}

		printf("Failed to get station stats information element.\n");
		return -1;
	}

	data->rx_packets = le_to_host32(stats.is_stats.ns_rx_data);
	data->rx_bytes = le_to_host64(stats.is_stats.ns_rx_bytes);
	data->tx_packets = le_to_host32(stats.is_stats.ns_tx_data);
	data->tx_bytes = le_to_host64(stats.is_stats.ns_tx_bytes);
	return 0;
}

static int qlink_sta_clear_stats(void *priv, const u8 *addr)
{
#if defined(MADWIFI_BSD) && defined(IEEE80211_MLME_CLEAR_STATS)
	struct qlink_bss *bss = priv;
	struct ieee80211req_mlme mlme;
	int ret;

	wpa_printf(MSG_DEBUG, "%s: addr=%s", __func__, ether_sprintf(addr));

	mlme.im_op = IEEE80211_MLME_CLEAR_STATS;
	memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	ret = qlink_client_set_80211_priv(bss->drv->qc, bss->qid, IEEE80211_IOCTL_SETMLME, &mlme,
			   sizeof(mlme));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to clear STA stats (addr "
			   MACSTR ")", __func__, MAC2STR(addr));
	}

	return ret;
#else /* MADWIFI_BSD && IEEE80211_MLME_CLEAR_STATS */
	return 0; /* FIX */
#endif /* MADWIFI_BSD && IEEE80211_MLME_CLEAR_STATS */
}

static int qlink_set_opt_ie(void *priv, const u8 *ie, size_t ie_len)
{
	/*
	 * Do nothing; we setup parameters at startup that define the
	 * contents of the beacon information element.
	 */
	return 0;
}

static int qlink_sta_deauth(void *priv, const u8 *own_addr, const u8 *addr,
	int reason_code)
{
	struct qlink_bss *bss = priv;
	struct ieee80211req_mlme mlme;
	int ret;

	if (priv == NULL) {
		return 0;
	}

	if (qlink_client_check_if(bss->drv->qc, bss->qid) != 0) {
		wpa_printf(MSG_DEBUG, "%s: Interface is not up.", __func__);
		return 0;
	}

	wpa_printf(MSG_DEBUG, "%s: addr=%s reason_code=%d",
		   __func__, ether_sprintf(addr), reason_code);

	mlme.im_op = IEEE80211_MLME_DEAUTH;
	mlme.im_reason = host_to_le16(reason_code);
	memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	ret = qlink_client_set_80211_priv(bss->drv->qc, bss->qid, IEEE80211_IOCTL_SETMLME,
		&mlme, sizeof(mlme));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to deauth STA (addr " MACSTR
			   " reason %d)",
			   __func__, MAC2STR(addr), reason_code);
	}

	return ret;
}

static int qlink_flush(void *priv)
{
#ifdef MADWIFI_BSD
	u8 allsta[IEEE80211_ADDR_LEN];
	memset(allsta, 0xff, IEEE80211_ADDR_LEN);
	return qlink_sta_deauth(priv, NULL, allsta,
				  IEEE80211_REASON_AUTH_LEAVE);
#else /* MADWIFI_BSD */
	return 0;		/* XXX */
#endif /* MADWIFI_BSD */
}

static int qlink_sta_disassoc(void *priv, const u8 *own_addr, const u8 *addr,
	int reason_code)
{
	struct qlink_bss *bss = priv;
	struct ieee80211req_mlme mlme;
	int ret;

	wpa_printf(MSG_DEBUG, "%s: addr=%s reason_code=%d",
		   __func__, ether_sprintf(addr), reason_code);

	mlme.im_op = IEEE80211_MLME_DISASSOC;
	mlme.im_reason = host_to_le16(reason_code);
	memcpy(mlme.im_macaddr, addr, IEEE80211_ADDR_LEN);
	ret = qlink_client_set_80211_priv(bss->drv->qc, bss->qid, IEEE80211_IOCTL_SETMLME,
		&mlme, sizeof(mlme));
	if (ret < 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to disassoc STA (addr "
			   MACSTR " reason %d)",
			   __func__, MAC2STR(addr), reason_code);
	}

	return ret;
}

#ifdef CONFIG_WPS
static int qlink_set_wps_ie(struct qlink_bss *bss, const u8 *ie, size_t len, u32 frametype)
{
	u8 buf[1024];
	struct ieee80211req_getset_appiebuf *beac_ie;

	wpa_printf(MSG_DEBUG, "%s buflen = %lu", __func__,
		   (unsigned long) len);

	if (len > (sizeof(buf) - offsetof(struct ieee80211req_getset_appiebuf, app_buf))) {
		wpa_printf(MSG_ERROR, "%s Transmitted WPS data exceeds max length (%lu) - "
			"reduce description lengths", __func__,
			sizeof(buf));
		return -1;
	}

	os_memset(buf, 0, sizeof(buf));
	beac_ie = (struct ieee80211req_getset_appiebuf *) buf;
	beac_ie->app_frmtype = host_to_le32(frametype);
	beac_ie->app_buflen = host_to_le32(len);
	os_memcpy(&(beac_ie->app_buf[0]), ie, len);

	return qlink_client_set_80211_priv(bss->drv->qc, bss->qid, IEEE80211_IOCTL_SET_APPIEBUF,
		beac_ie, sizeof(struct ieee80211req_getset_appiebuf) + len);
}

static int qlink_set_ap_wps_ie(void *priv, const struct wpabuf *beacon,
	const struct wpabuf *proberesp,
	const struct wpabuf *assocresp)
{
	struct qlink_bss *bss = priv;

	if (qlink_set_wps_ie(bss, beacon ? wpabuf_head(beacon) : NULL,
		beacon ? wpabuf_len(beacon) : 0,
		IEEE80211_APPIE_FRAME_BEACON) < 0) {
		return -1;
	}
	if (qlink_set_wps_ie(bss,
		proberesp ? wpabuf_head(proberesp) : NULL,
		proberesp ? wpabuf_len(proberesp) : 0,
		IEEE80211_APPIE_FRAME_PROBE_RESP) < 0) {
		return -1;
	}
	return qlink_set_wps_ie(bss,
		assocresp ? wpabuf_head(assocresp) : NULL,
		assocresp ? wpabuf_len(assocresp) : 0,
		IEEE80211_APPIE_FRAME_ASSOC_RESP);
}
#else /* CONFIG_WPS */
#define qlink_set_ap_wps_ie NULL
#endif /* CONFIG_WPS */

static int qlink_set_sta_vlan(void *priv, const u8 *addr,
	const char *ifname, int vlan_id)
{
	struct qlink_bss *bss = priv;
	char mac[20];
	int ret;

	snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
			addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

	wpa_printf(MSG_DEBUG, "%s: Bind %s with VLAN %d\n",
			ifname,
			mac,
			vlan_id);

	ret = qlink_client_vlan_set_sta(bss->drv->qc, addr, vlan_id);
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "%s: Bind STA VLAN failed\n", ifname);
		return -1;
	}

	return 0;
}

static int qlink_set_dyn_vlan(void *priv, const char *ifname, int enable)
{
	struct qlink_bss *bss = priv;
	const char *cmd;
	int ret;

	cmd = (enable ? "enable" : "disable");
	wpa_printf(MSG_DEBUG, "%s: %s dynamic VLAN ", ifname, cmd);

	ret = qlink_client_vlan_set_dyn(bss->drv->qc, ifname, enable);
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "%s: %s dynamic VLAN failed\n", ifname, cmd);
		return -1;
	}

        return 0;
}

static int qlink_vlan_group_add(void *priv, const char *ifname, int vlan_id)
{
	struct qlink_bss *bss = priv;
	int ret;

	ret = qlink_client_vlan_set_group(bss->drv->qc, ifname, vlan_id, 1);
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "add vlan group %d failed\n", vlan_id);
		return -1;
	}

	return 0;
}

static int qlink_vlan_group_remove(void *priv, const char *ifname, int vlan_id)
{
	struct qlink_bss *bss = priv;
	int ret;

	ret = qlink_client_vlan_set_group(bss->drv->qc, ifname, vlan_id, 0);
	if (ret < 0) {
		wpa_printf(MSG_ERROR, "remove vlan group %d failed\n", vlan_id);
		return -1;
	}

	return 0;
}

static int qlink_set_freq(void *priv, struct hostapd_freq_params *freq)
{
	struct qlink_bss *bss = priv;

	if (qlink_client_set_channel(bss->drv->qc, bss->qid, freq->channel) != 0) {
		wpa_printf(MSG_ERROR, "set_channel failed");
		return -1;
	}

	return 0;
}

static int qlink_send_eapol(void *priv, const u8 *addr, const u8 *data, size_t data_len,
	int encrypt, const u8 *own_addr, u32 flags)
{
	struct qlink_bss *bss = priv;
	unsigned char buf[QLINK_EAPOL_MAX_FRAME_SIZE + sizeof(struct l2_ethhdr)];
	unsigned char *bp = buf;
	struct l2_ethhdr *eth;
	size_t len;
	int status;

	/*
	 * Prepend the Ethernet header.  If the caller left us
	 * space at the front we could just insert it but since
	 * we don't know we copy to a local buffer.  Given the frequency
	 * and size of frames this probably doesn't matter.
	 */
	len = data_len + sizeof(struct l2_ethhdr);
	if (len > sizeof(buf)) {
		bp = malloc(len);
		if (bp == NULL) {
			printf("EAPOL frame discarded, cannot malloc temp "
			       "buffer of size %lu!\n", (unsigned long) len);
			return -1;
		}
	}
	eth = (struct l2_ethhdr *) bp;
	memcpy(eth->h_dest, addr, ETH_ALEN);
	memcpy(eth->h_source, own_addr, ETH_ALEN);
	eth->h_proto = host_to_be16(ETH_P_EAPOL);
	memcpy(eth+1, data, data_len);

	wpa_hexdump(MSG_MSGDUMP, "TX EAPOL", bp, len);

	/* FIXME this currently only supports EAPOL frames up to 2047 bytes */
	if (data_len > QLINK_EAPOL_MAX_FRAME_SIZE) {
		if (bp != buf) {
			free(bp);
		}
		return -1;
	}

	status = qlink_client_set_80211_priv(bss->drv->qc, bss->qid, IEEE80211_IOCTL_TXEAPOL,
		bp, len);

	if (bp != buf)
		free(bp);
	return status;
}

static int qlink_if_add(void *priv, enum wpa_driver_if_type type,
	const char *ifname, const u8 *addr,
	void *bss_ctx, void **drv_priv,
	char *force_ifname, u8 *if_addr,
	const char *bridge, int use_existing)
{
	struct qlink_bss *bss = priv;
	struct qlink_bss *new_bss;

	wpa_printf(MSG_DEBUG, "%s(type=%d ifname=%s bss_ctx=%p)\n",
		   __func__, type, ifname, bss_ctx);

	if (type != WPA_IF_AP_BSS) {
		return 0;
	}

	new_bss = qlink_init_bss(bss->drv, bss_ctx, ifname, bridge, 1, addr, if_addr);
	if (new_bss == NULL) {
		wpa_printf(MSG_ERROR, "%s: new BSS is null", __func__);
		return 1;
	}

	if (drv_priv)
		*drv_priv = new_bss;

	return 0;
}

static int qlink_if_remove(void *priv, enum wpa_driver_if_type type,
	const char *ifname)
{
	struct qlink_bss *bss = priv;

	wpa_printf(MSG_DEBUG, "%s(type=%d ifname=%s)", __func__, type, ifname);

	if (priv == NULL)
		return 0;

	if (type != WPA_IF_AP_BSS) {
		return 0;
	}

	qlink_bss_remove(bss);

	return 0;
}

static void *qlink_init(struct hostapd_data *hapd, struct wpa_init_params *params)
{
	int ret;
	struct qlink_driver_data *drv;
	struct qlink_bss *bss;
	struct timeval tv_start, tv_cur, tv;
	uint32_t passed_ms = 0;
	uint32_t client_flags = 0;

	if (strlen(hapd->conf->qlink_iface) == 0) {
		printf("qlink_iface not specified\n");
		return NULL;
	}

	if (is_zero_ether_addr(hapd->conf->qlink_server_addr)) {
		printf("qlink_server_addr not specified\n");
		return NULL;
	}

	drv = os_zalloc(sizeof(struct qlink_driver_data));

	if (drv == NULL) {
		printf("Could not allocate memory for qlink driver data\n");
		return NULL;
	}

	drv->hapd = hapd;

	drv->qc = qlink_client_init(&qlink_ops, drv, hapd->conf->qlink_iface,
		hapd->conf->qlink_server_addr);

	if (drv->qc == NULL) {
		goto fail_qlink_client;
	}

	qlink_client_set_active(drv->qc, 1);

	/*
	 * report client capabilities to the server, in particular server needs to know
	 * if client was compiled with WPS or HS2.0 support in order to setup proper filter for
	 * ETH_P_80211_RAW reception.
	 */

#ifdef CONFIG_WPS
	client_flags |= QLINK_FLAG_WPS;
#endif
#ifdef CONFIG_HS20
	client_flags |= QLINK_FLAG_HS20;
#endif

	gettimeofday(&tv_start, NULL);

	while (1) {
		/*
		 * generate a random guid, send a handshake and wait for a valid reply from server,
		 * try again after handshake timeout.
		 */

		os_get_random(drv->guid, sizeof(drv->guid));

		qlink_client_handshake(drv->qc, drv->guid, QLINK_PING_TIMEOUT, client_flags);

		ret = qlink_wait_valid_handshake(drv, drv->guid, QLINK_HANDSHAKE_TIMEOUT,
			&drv->server_ping_timeout_ms);

		if (ret < 0) {
			goto fail_qlink_handshake;
		}

		if (ret == 0) {
			break;
		}

		gettimeofday(&tv_cur, NULL);

		timersub(&tv_cur, &tv_start, &tv);

		passed_ms = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);

		if (passed_ms > QLINK_CONNECT_TIMEOUT) {
			printf("Timeout waiting for qlink handshake\n");

			goto fail_qlink_handshake;
		}
	}

	if (drv->server_ping_timeout_ms < 1000) {
		printf("Server ping timeout is too short\n");

		goto fail_qlink_setup;
	}

	drv->active = 1;

	dl_list_init(&drv->bss);

	ret = qlink_send_security_config(drv, hapd->iface);

	if (ret != 0) {
		goto fail_qlink_setup;
	}

	bss = qlink_init_bss(drv, hapd, params->ifname, hapd->conf->bridge, 0, NULL,
		params->own_addr);

	if (bss == NULL) {
		goto fail_qlink_setup;
	}

	printf("initial bss with address " MACSTR "\n", MAC2STR(params->own_addr));

	ret = qlink_client_handle_bridge_eapol(drv->qc, bss->qid);

	if (ret != 0) {
		goto fail_handle_bridge_eapol;
	}

	ret = qlink_client_get_driver_capa(drv->qc, bss->qid, &drv->we_version,
		&drv->extended_capa_len, &drv->extended_capa, &drv->extended_capa_mask);

	if (ret != 0) {
		goto fail_handle_bridge_eapol;
	}

	dl_list_init(&drv->ctrl_clients);

	drv->ctrl_addr.sun_family = AF_UNIX;
	drv->ctrl_addr.sun_path[0] = '\0';

	if (hapd->conf->ctrl_interface != NULL) {
		size_t len = os_strlen(hapd->conf->ctrl_interface) + os_strlen(bss->ifname) + 2;

		if (len > sizeof(drv->ctrl_addr.sun_path)) {
			printf("ctrl interface path is too long\n");
			goto fail_handle_bridge_eapol;
		}

		ret = qlink_client_init_ctrl(drv->qc, hapd->conf->ctrl_interface, bss->ifname);

		if (ret != 0) {
			goto fail_handle_bridge_eapol;
		}

		drv->ctrl_addr.sun_family = AF_UNIX;
		os_snprintf(drv->ctrl_addr.sun_path, len, "%s/%s",
			hapd->conf->ctrl_interface, hapd->conf->iface);
		drv->ctrl_addr.sun_path[len - 1] = '\0';
	}

	drv->server_ping_timeout_ms *= QLINK_MAX_MISSED_PINGS;

	/*
	 * once hostapd done with its thing it'll daemonize and run the event loop and that's where
	 * we come in - i.e. send the ready message.
	 */
	eloop_register_timeout_ms(0, &qlink_client_ready_timeout, drv);

	eloop_register_timeout_ms(drv->server_ping_timeout_ms, &qlink_server_ping_timeout, drv);

	eloop_register_timeout_ms(QLINK_PING_TIMEOUT, &qlink_client_ping_timeout, drv);

	return bss;

fail_handle_bridge_eapol:
	qlink_bss_remove(bss);
fail_qlink_setup:
	qlink_client_disconnect(drv->qc);
fail_qlink_handshake:
	qlink_client_deinit(drv->qc);
fail_qlink_client:
	os_free(drv);

	return NULL;
}

static void qlink_deinit(void *priv)
{
	struct qlink_bss *bss = priv;
	struct qlink_driver_data *drv = bss->drv;

	qlink_ctrl_client_remove_all(drv);

	eloop_cancel_timeout(&qlink_client_ready_timeout, drv, NULL);
	eloop_cancel_timeout(&qlink_server_ping_timeout, drv, NULL);
	eloop_cancel_timeout(&qlink_client_ping_timeout, drv, NULL);

	qlink_bss_remove(bss);
	qlink_client_disconnect(drv->qc);
	qlink_client_deinit(drv->qc);

	if (drv->extended_capa)
		os_free(drv->extended_capa);

	if (drv->extended_capa_mask)
		os_free(drv->extended_capa_mask);

	os_free(drv);
}

static int qlink_set_ssid(void *priv, const u8 *buf, int len)
{
	struct qlink_bss *bss = priv;

	if (qlink_client_set_ssid(bss->drv->qc, bss->qid, buf, len) != 0) {
		wpa_printf(MSG_ERROR, "set_ssid failed, len = %d", len);
		return -1;
	}

	return 0;
}

static int qlink_get_ssid(void *priv, u8 *buf, int len)
{
	struct qlink_bss *bss = priv;
	int ret = 0;

	ret = qlink_client_get_ssid(bss->drv->qc, bss->qid, buf, &len);

	if (ret != 0) {
		wpa_printf(MSG_ERROR, "get_ssid failed, len = %d", len);
		ret = -1;
	} else
		ret = len;

	return ret;
}

static int qlink_set_countermeasures(void *priv, int enabled)
{
	struct qlink_bss *bss = priv;
	wpa_printf(MSG_DEBUG, "%s: enabled=%d", __FUNCTION__, enabled);
	return qlink_client_set_80211_param(bss->drv->qc, bss->qid,
		IEEE80211_PARAM_COUNTERMEASURES, enabled);
}

static int qlink_commit(void *priv)
{
	struct qlink_bss *bss = priv;

	qlink_client_commit(bss->drv->qc, bss->qid);

	return 0;
}

static int qlink_set_intra_bss(void *priv, int enabled)
{
	struct qlink_bss *bss = priv;
	return qlink_client_set_80211_param(bss->drv->qc, bss->qid, IEEE80211_PARAM_AP_ISOLATE,
		enabled);
}

static int qlink_set_intra_per_bss(void *priv, int enabled)
{
	struct qlink_bss *bss = priv;

	qlink_client_set_80211_param(bss->drv->qc, bss->qid, IEEE80211_PARAM_INTRA_BSS_ISOLATE,
		enabled);

	return 0;
}

static int qlink_set_bss_isolate(void *priv, int enabled)
{
	struct qlink_bss *bss = priv;

	qlink_client_set_80211_param(bss->drv->qc, bss->qid, IEEE80211_PARAM_BSS_ISOLATE, enabled);

	return 0;
}

static int qlink_set_bss_assoc_limit(void *priv, int limit)
{
	struct qlink_bss *bss = priv;

	return qlink_client_set_80211_param(bss->drv->qc, bss->qid,
		IEEE80211_PARAM_BSS_ASSOC_LIMIT, limit);
}

static int qlink_set_total_assoc_limit(void *priv, int limit)
{
        struct qlink_bss *bss = priv;

        return qlink_client_set_80211_param(bss->drv->qc, bss->qid,
		IEEE80211_PARAM_ASSOC_LIMIT, limit);
}

static int qlink_set_broadcast_ssid(void *priv, int value)
{
	struct qlink_bss *bss = priv;
	return qlink_client_set_80211_param(bss->drv->qc, bss->qid, IEEE80211_PARAM_HIDESSID,
		value);
}

static void qlink_send_log(void *priv, const char *msg)
{
	struct qlink_bss *bss = priv;
	size_t len = strnlen(msg, MAX_WLAN_MSG_LEN);
	char *msg_copy = os_malloc(len);

	if (!msg_copy) {
		wpa_printf(MSG_ERROR, "cannot allocate msg_copy, no mem");
		return;
	}

	/*
	 * 'set_80211_priv' takes void*, i.e. it's allowed to modify that memory, msg is
	 * const char*, i.e. it's not always safe to modify it, so make a copy here
	 * and pass it instead.
	 */

	memcpy(msg_copy, msg, len);

	qlink_client_set_80211_priv(bss->drv->qc, bss->qid, IEEE80211_IOCTL_POSTEVENT,
		msg_copy, len);

	os_free(msg_copy);
}

static int qlink_send_action(void *priv, unsigned int freq,
	unsigned int wait_time,
	const u8 *dst_mac, const u8 *src_mac,
	const u8 *bssid,
	const u8 *data, size_t data_len,
	int no_cck)
{
	struct qlink_bss *bss = priv;
	struct app_action_frame_buf *app_action_frm_buf;
	int ret = 0;

	app_action_frm_buf = os_malloc(data_len  + sizeof(struct app_action_frame_buf));
	if (!app_action_frm_buf) {
		wpa_printf(MSG_DEBUG, "Memory allocation failed in "
			"func : %s line : %d \n", __func__, __LINE__);
		return -1;
	}

	/* data is Action frame payload. First byte of the data is action frame category
	 * and the second byte is action */
	app_action_frm_buf->cat = *data;
	app_action_frm_buf->action = *(data + 1);
	os_memcpy(app_action_frm_buf->dst_mac_addr, dst_mac, ETH_ALEN);

	app_action_frm_buf->frm_payload.length = host_to_le16(data_len);
	os_memcpy(app_action_frm_buf->frm_payload.data, data, data_len);

	ret = qlink_client_send_action(bss->drv->qc, bss->qid, app_action_frm_buf,
		data_len + sizeof(struct app_action_frame_buf));

	if (ret != 0) {
		wpa_printf(MSG_DEBUG, "%s: Failed to send action "
				"frame ioctl", __func__);
		ret = -1;
	}

	os_free(app_action_frm_buf);

	return ret;
}

static int qlink_send_mlme(void *priv, const void *msg, size_t len, int noack)
{
	struct qlink_bss *bss = priv;
	struct ieee80211_mgmt *mgmt = msg;
	uint16_t fc;

	if (!priv || !msg)
		return -1;

	if (len < offsetof(struct ieee80211_mgmt, u))
		return -1;

	fc = le_to_host16(mgmt->frame_control);

	if (WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_MGMT &&
			WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_ACTION) {
		size_t data_len = len - offsetof(struct ieee80211_mgmt, u);
		struct hostapd_data *hapd = bss->bss_ctx;

		return qlink_send_action(priv, hapd->iface->freq, 0,
					mgmt->da, mgmt->sa,
					mgmt->bssid, &mgmt->u.action,
					data_len, 0);
	} else {
		return -1;
	}
}

static int qlink_set_pairing_hash_ie(void *priv, const u8 *pairing_hash,
	size_t ies_len)
{
	struct qlink_bss *bss = priv;
	struct ieee80211req_getset_appiebuf *pairing_hash_ie;
	int ret;

	pairing_hash_ie = os_malloc(sizeof(*pairing_hash_ie) + ies_len);
	if (pairing_hash_ie == NULL)
		return -1;

	os_memset(pairing_hash_ie, 0, sizeof(*pairing_hash_ie) + ies_len);
	pairing_hash_ie->app_frmtype = host_to_le32(IEEE80211_APPIE_FRAME_ASSOC_RESP);
	pairing_hash_ie->app_buflen = host_to_le32(ies_len);
	pairing_hash_ie->flags = F_QTN_IEEE80211_PAIRING_IE;
	os_memcpy(pairing_hash_ie->app_buf, pairing_hash, ies_len);

	ret = qlink_client_set_80211_priv(bss->drv->qc, bss->qid, IEEE80211_IOCTL_SET_APPIEBUF,
		pairing_hash_ie,
		sizeof(struct ieee80211req_getset_appiebuf) +
		ies_len);

	os_free(pairing_hash_ie);

	return ret;
}

static int qlink_get_driver_capa(void *priv, struct wpa_driver_capa *capa)
{
	struct qlink_bss *bss = priv;
	struct qlink_driver_data *drv = bss->drv;

	if (drv->extended_capa) {
		capa->extended_capa_len = drv->extended_capa_len;
		capa->extended_capa = drv->extended_capa;
		capa->extended_capa_mask = drv->extended_capa_mask;
	} else {
		return -1;
	}

	return 0;
}

static int qlink_set_ap(void *priv, struct wpa_driver_ap_params *params)
{
	struct qlink_bss *bss = priv;

	/* TODO As of now this function is used for setting HESSID and
	* Access Network Type. It can be used for other elements in future
	* like set_authmode, set_privacy, set_ieee8021x, set_generic_elem
	* and hapd_set_ssid */

	if (qlink_client_set_interworking(bss->drv->qc, bss->qid, WLAN_EID_INTERWORKING,
		params->interworking, params->access_network_type, params->hessid)) {
		return -1;
	}

	if (qlink_client_set_80211_param(bss->drv->qc, bss->qid, IEEE80211_PARAM_HS2,
		params->hs20_enable)) {
		wpa_printf(MSG_DEBUG, "%s: Unable to set hs20 enabled to %d\n",
					__func__, params->hs20_enable);
		return -1;
	}

	if (qlink_client_set_80211_param(bss->drv->qc, bss->qid, IEEE80211_PARAM_DGAF_CONTROL,
					params->disable_dgaf)) {
		wpa_printf(MSG_DEBUG, "%s: Unable to set disable dgaf to %u\n",
					__func__, params->disable_dgaf);
		return -1;
	}

	if (qlink_client_set_80211_param(bss->drv->qc, bss->qid, IEEE80211_PARAM_PROXY_ARP,
					params->proxy_arp)) {
		wpa_printf(MSG_DEBUG, "%s: Unable to set proxy ARP to %u\n",
					__func__, params->proxy_arp);
		return -1;
	}

	if (qlink_client_set_80211_param(bss->drv->qc, bss->qid, IEEE80211_PARAM_DTIM_PERIOD,
					params->dtim_period)) {
		wpa_printf(MSG_DEBUG, "%s: Unable to set DTIM period to %u\n",
					__func__, params->dtim_period);
		return -1;
	}

#ifdef CONFIG_HS20
	if (params->osen) {
		struct wpa_bss_params wpa_params;

		memset(&wpa_params, 0, sizeof(wpa_params));
		wpa_params.enabled = 1;
		wpa_params.wpa = 2;
		wpa_params.ieee802_1x = 1;
		wpa_params.wpa_group = WPA_CIPHER_CCMP;
		wpa_params.wpa_pairwise = WPA_CIPHER_CCMP;
		wpa_params.wpa_key_mgmt = WPA_KEY_MGMT_IEEE8021X;
		if (qlink_set_privacy(priv, 1)) {
			wpa_printf(MSG_DEBUG, "%s: Unable to enable privacy\n", __func__);
			return -1;
		}
		if (qlink_set_ieee8021x(priv, &wpa_params)) {
			wpa_printf(MSG_DEBUG, "%s: Unable to set 802.1X params\n", __func__);
			return -1;
		}
		if (qlink_client_set_80211_param(bss->drv->qc, bss->qid, IEEE80211_PARAM_OSEN, 1)) {
			wpa_printf(MSG_DEBUG, "%s: Unable to set OSEN\n", __func__);
			return -1;
		}
	}
#endif

	return 0;
}

int qlink_set_qos_map(void *priv, const u8 *qos_map_set,
			   u8 qos_map_set_len)
{
	struct qlink_bss *bss = priv;
	u8 dscp2up[IP_DSCP_NUM];
	int up_start_idx;
	u8 *up_map;
	u8 up;
	u8 dscp;
	u8 dscp_low;
	u8 dscp_high;
	int i;

	if (qos_map_set_len < (IEEE8021P_PRIORITY_NUM * 2) ||
	    qos_map_set_len > ((IEEE8021P_PRIORITY_NUM + IEEE80211_DSCP_MAX_EXCEPTIONS) * 2) ||
	    (qos_map_set_len & 1)) {
		wpa_printf(MSG_ERROR, "%s invalid QoS Map length\n", __func__);
		return -1;
	}

	up_start_idx = qos_map_set_len - IEEE8021P_PRIORITY_NUM * 2;
	up_map = &qos_map_set[up_start_idx];
	os_memset(dscp2up, IEEE8021P_PRIORITY_NUM, sizeof(dscp2up));

	for (up = 0; up < IEEE8021P_PRIORITY_NUM; up++) {
		dscp_low = up_map[up * 2];
		dscp_high = up_map[up * 2 + 1];
		if (dscp_low < IP_DSCP_NUM &&
		    dscp_high < IP_DSCP_NUM &&
		    dscp_low <= dscp_high) {
			os_memset(&dscp2up[dscp_low], up, dscp_high - dscp_low + 1);
		}
	}
	for (i = 0; i < up_start_idx; i += 2) {
		dscp = qos_map_set[i];
		up = qos_map_set[i + 1];
		if (dscp < IP_DSCP_NUM) {
			dscp2up[dscp] = up;
		}
	}

	if (qlink_client_set_qos_map(bss->drv->qc, bss->qid, dscp2up) != 0) {
		wpa_printf(MSG_ERROR, "set_qos_map failed");
		return -1;
	}

	return 0;
}

const struct wpa_driver_ops wpa_driver_qlink_ops = {
	.name			= "qlink",
	.desc			= "QLINK support",
	.set_key		= qlink_set_key,
	.hapd_init		= qlink_init,
	.hapd_deinit		= qlink_deinit,
	.set_ieee8021x		= qlink_set_ieee8021x,
	.set_privacy		= qlink_set_privacy,
	.get_seqnum		= qlink_get_seqnum,
	.if_add			= qlink_if_add,
	.if_remove		= qlink_if_remove,
	.flush			= qlink_flush,
	.set_generic_elem	= qlink_set_opt_ie,
	.sta_set_flags		= qlink_sta_set_flags,
	.read_sta_data		= qlink_read_sta_driver_data,
	.hapd_send_eapol	= qlink_send_eapol,
	.sta_disassoc		= qlink_sta_disassoc,
	.sta_deauth		= qlink_sta_deauth,
	.hapd_set_ssid		= qlink_set_ssid,
	.hapd_get_ssid		= qlink_get_ssid,
	.hapd_set_countermeasures	= qlink_set_countermeasures,
	.sta_clear_stats	= qlink_sta_clear_stats,
	.commit			= qlink_commit,
	.set_ap_wps_ie		= qlink_set_ap_wps_ie,
	.set_freq		= qlink_set_freq,
	.set_intra_bss		= qlink_set_intra_bss,
	.set_intra_per_bss	= qlink_set_intra_per_bss,
	.set_bss_isolate	= qlink_set_bss_isolate,
	.set_bss_assoc_limit	= qlink_set_bss_assoc_limit,
	.set_total_assoc_limit	= qlink_set_total_assoc_limit,
	.set_brcm_ioctl		= qlink_brcm_info_ioctl,
	.set_sta_vlan		= qlink_set_sta_vlan,
	.set_dyn_vlan		= qlink_set_dyn_vlan,
	.vlan_group_add		= qlink_vlan_group_add,
	.vlan_group_remove	= qlink_vlan_group_remove,
	.set_broadcast_ssid	= qlink_set_broadcast_ssid,
	.set_pairing_hash_ie	= qlink_set_pairing_hash_ie,
	.send_log		= qlink_send_log,
	.send_action		= qlink_send_action,
	.get_capa		= qlink_get_driver_capa,
	.set_ap			= qlink_set_ap,
	.set_qos_map		= qlink_set_qos_map,
	.send_mlme		= qlink_send_mlme,
};
