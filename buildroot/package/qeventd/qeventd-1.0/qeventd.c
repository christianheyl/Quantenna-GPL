/*
 *		Quantenna private events daemon
 *
 *		Quantenna Communications, Inc. 2015
 *
 * Main code for "qeventd". It's mainly used to receive the Quantenna private
 * events from driver and process these events. It also maintains the
 * Quantenna private connection state machine and control the automatic switch
 *  for Quantenna private connection.
 *
 * This file is copyrighted by Quantenna Communications, Inc.
 */

/***************************** INCLUDES *****************************/

#include <sys/types.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/time.h>
#include <getopt.h>
#include <time.h>
#include <stdarg.h>
#include <signal.h>

#include <net/if.h>
#include <net/ethernet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if_packet.h>

#include "wireless.h"
#include "net80211/ieee80211_ioctl.h"
#include "qcsapi.h"

#define MD5_MAC_LEN 16
#define MD5_STR_BUF_LEN (MD5_MAC_LEN * 2 + 1)
#define SHA1_ITERATION_NUM 4096

#define QTN_WDS_EXT_SCRIPT "qtn_wds_ext.sh"
#define QTN_DEFAULT_IFACE "wifi0"
#define QTN_EVENT_MSG_BUF_LEN 8192
#define QTN_WDS_EXT_CMD_LEN 256
#define QTN_WDS_KEY_LEN 32
#define QTN_WPA_PASSPHRASE_MIN_LEN 8
#define QTN_WPA_PASSPHRASE_MAX_LEN 63
#define QTN_WPA_PSK_LEN 64
#define QTN_SSID_MAX_LEN 32
#define PMK_LEN 32

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02X:%02X:%02X:%02X:%02X:%02X"
#endif

static const struct option long_opts_qevent[] = {
	{"help", no_argument, NULL, 'h'},
	{"version", no_argument, NULL, 'v'},
	{"debug", no_argument, NULL, 'd'},
	{NULL, 0, NULL, 0 }
};

static int debug_enable = 0;
static int loop_terminated = 0;

struct rtnl_handle
{
	int			fd;
	struct sockaddr_nl	local;
	struct sockaddr_nl	peer;
	uint32_t		seq;
};

static inline int
snprintf_hex(char *buf, size_t buf_size,
	const uint8_t *data, size_t len)
{
	size_t i;
	char *pos = buf, *end = buf + buf_size;
	int ret;

	if (buf_size == 0)
		return 0;

	for (i = 0; i < len; i++) {
		ret = snprintf(pos, end - pos, "%02x", data[i]);
		if (ret < 0 || ret >= end - pos) {
			end[-1] = '\0';
			return pos - buf;
		}
		pos += ret;
	}
	end[-1] = '\0';
	return pos - buf;
}

static inline int
hex2num(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static inline int
hex2byte(const char *hex)
{
	int a, b;
	a = hex2num(*hex++);
	if (a < 0)
		return -1;
	b = hex2num(*hex++);
	if (b < 0)
		return -1;
	return (a << 4) | b;
}

static int
hexstr2bin(const char *hex, uint8_t *buf, size_t len)
{
	size_t i;
	int a;
	const char *ipos = hex;
	uint8_t *opos = buf;

	for (i = 0; i < len; i++) {
		a = hex2byte(ipos);
		if (a < 0)
			return -1;
		*opos++ = a;
		ipos += 2;
	}
	return 0;
}

static inline void
qevent_rtnl_close(struct rtnl_handle *rth)
{
	close(rth->fd);
}

static inline int
qevent_rtnl_open(struct rtnl_handle *rth, unsigned subscriptions)
{
	int addr_len;

	memset(rth, 0, sizeof(*rth));

	rth->fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (rth->fd < 0) {
		perror("Cannot open netlink socket");
		return -1;
	}

	rth->local.nl_family = AF_NETLINK;
	rth->local.nl_groups = subscriptions;

	if (bind(rth->fd, (struct sockaddr*)&rth->local, sizeof(rth->local)) < 0) {
		perror("Cannot bind netlink socket");
		return -1;
	}
	addr_len = sizeof(rth->local);
	if (getsockname(rth->fd, (struct sockaddr*)&rth->local,
			(socklen_t *)&addr_len) < 0) {
		perror("Cannot getsockname");
		return -1;
	}
	if (addr_len != sizeof(rth->local)) {
		fprintf(stderr, "%s: Wrong address length %d\n", __func__, addr_len);
		return -1;
	}
	if (rth->local.nl_family != AF_NETLINK) {
		fprintf(stderr, "%s: Wrong address family %d\n",
				__func__, rth->local.nl_family);
		return -1;
	}
	rth->seq = time(NULL);
	return 0;
}

static inline void
qevent_fprintf(FILE *stream, const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	if (debug_enable)
		vfprintf(stream, fmt, args);
	va_end(args);
}

static void
qevent_sigext(int signum)
{
	fprintf(stdout, "%s: receive terminal signal and exit\n", __func__);

	loop_terminated = 1;
	exit(0);
}

static int
qevent_md5_sum(const char *passphrase, int len, char *res_buf)
{
        int i;
        char *psk_use;
        unsigned char hex_buf[MD5_MAC_LEN];
        uint8_t *passphrase_vec[2];
        size_t len_vec[2];
        char buf_use[MD5_STR_BUF_LEN] = {0};
        char *pos, *end;
        int ret;

        psk_use = (char *)malloc(len + 1);
        if (!psk_use) {
                qevent_fprintf(stderr, "%s: memory alloction fails\n", __func__);
                return -1;
        }

        memcpy(psk_use, passphrase, len);
        psk_use[len] = '\n';
        passphrase_vec[0] = (uint8_t *)psk_use;
        len_vec[0] = len + 1;

        if(md5_vector(1, passphrase_vec, len_vec, hex_buf) < 0) {
                qevent_fprintf(stderr, "%s: MD5 conversion fails\n", __func__);
                free(psk_use);
                return -1;
        }

        pos = buf_use;
        end = pos + sizeof(buf_use);
        for(i = 0; i < MD5_MAC_LEN; i++) {
                ret = snprintf(pos, (end - pos), "%02x", hex_buf[i]);
                if (ret < 0 || ret > (end - pos)) {
                        qevent_fprintf(stderr, "%s: hex to str error\n");
                        free(psk_use);
                        return -1;
                }
                pos += ret;
        }
        memcpy(res_buf, buf_use, (MD5_STR_BUF_LEN - 1));
        free(psk_use);

        return 0;
}

static int
qevent_md5_convert_passphrase(const string_64 psk_web, string_64 pre_shared_key)
{
        int key_size;
        char passphrase_md5_res[MD5_STR_BUF_LEN] = {0};

        if (!psk_web || !pre_shared_key)
                return -1;

        key_size = strlen(psk_web);
        if ((key_size < QTN_WPA_PASSPHRASE_MIN_LEN) || (key_size > QTN_WPA_PSK_LEN))
                return -1;

        memset(pre_shared_key, 0, sizeof(string_64));
        if (qevent_md5_sum(psk_web, key_size, passphrase_md5_res) < 0) {
                qevent_fprintf(stderr, "%s: PSK MD5 convertion fails\n", __func__);
                return -1;
        }

        qevent_fprintf(stderr, "MD5 conversion of PSK is %s\n", passphrase_md5_res);

        if (key_size <= (MD5_STR_BUF_LEN - 1)) {
                memcpy(pre_shared_key, passphrase_md5_res, key_size);
        } else {
                memcpy(pre_shared_key, passphrase_md5_res, (MD5_STR_BUF_LEN - 1));
                strncpy(pre_shared_key + (MD5_STR_BUF_LEN - 1),
			psk_web + (MD5_STR_BUF_LEN - 1),
			key_size - (MD5_STR_BUF_LEN - 1));
        }

        return 0;
}

static int
qevent_get_pmk(qcsapi_wifi_mode mode, uint8_t *pmk)
{
	qcsapi_SSID ssid = {'\0'};
	string_64 psk = {'\0'};
	string_64 psk_md5 = {'\0'};
	uint8_t psk_len = 0;
	uint8_t ssid_len = 0;

	if (qcsapi_wifi_get_SSID(QTN_DEFAULT_IFACE, ssid) < 0) {
		qevent_fprintf(stderr, "%s: Failed to get the SSID\n", __func__);
		return -1;
	}

	qevent_fprintf(stdout, "%s: SSID %s\n", __func__, ssid);

	if (mode == qcsapi_station) {
		if ((qcsapi_SSID_get_pre_shared_key(QTN_DEFAULT_IFACE, ssid, 0, psk) == 0) &&
				(strnlen(psk, QTN_WPA_PSK_LEN) == QTN_WPA_PSK_LEN)) {
			qevent_fprintf(stdout, "%s: STA PSK %s\n", __func__, psk);

			if (qevent_md5_convert_passphrase(psk, psk_md5) < 0) {
				qevent_fprintf(stderr, "%s: MD5 coversion for STA PSK fails\n",
					__func__);
				return -1;
			}

			qevent_fprintf(stdout, "%s: STA PSK_MD5 %s\n", __func__, psk_md5);

			if (hexstr2bin(psk_md5, pmk, PMK_LEN) < 0) {
				qevent_fprintf(stderr, "%s: failed to convert the psk"
					" string to hex for STA\n", __func__);
				return -1;
			}
		} else {
			qevent_fprintf(stdout, "%s: don't find psk and try to"
				" get passphrase for STA\n", __func__);

			if (qcsapi_SSID_get_key_passphrase(QTN_DEFAULT_IFACE, ssid, 0, psk)) {
				qevent_fprintf(stderr, "%s: Failed to get psk or"
					" passphrase for STA\n", __func__);
				return -1;
			}

			qevent_fprintf(stdout, "%s: STA Passphrase %s\n", __func__, psk);

			if (qevent_md5_convert_passphrase(psk, psk_md5) < 0) {
				qevent_fprintf(stderr, "%s: MD5 coversion for STA"
					" passphrase fails\n", __func__);
				return -1;
			}

			qevent_fprintf(stdout, "%s: STA Passphrase_MD5 %s\n", __func__, psk_md5);

			psk_len = strnlen(psk, QTN_WPA_PSK_LEN);
			if ((psk_len < QTN_WPA_PASSPHRASE_MIN_LEN) ||
				(psk_len > QTN_WPA_PASSPHRASE_MAX_LEN)) {
				qevent_fprintf(stderr, "%s: invalid passpharse for STA\n",
					__func__);
				return -1;
			}

			ssid_len = strnlen(ssid, QTN_SSID_MAX_LEN);
			pbkdf2_sha1(psk_md5, ssid, ssid_len, SHA1_ITERATION_NUM, pmk, PMK_LEN);
		}
	} else if (mode == qcsapi_access_point) {
		if (qcsapi_wifi_get_pre_shared_key(QTN_DEFAULT_IFACE, 0, psk) == 0) {
			qevent_fprintf(stdout, "%s: AP PSK %s\n", __func__, psk);

			if (qevent_md5_convert_passphrase(psk, psk_md5) < 0) {
				qevent_fprintf(stderr, "%s: MD5 coversion for AP PSK fails\n",
					__func__);
				return -1;
			}

			qevent_fprintf(stdout, "%s: AP PSK_MD5 %s\n", __func__, psk_md5);

			if (hexstr2bin(psk_md5, pmk, PMK_LEN) < 0) {
				qevent_fprintf(stderr, "%s: failed to convert the psk"
					" string to hex for AP\n", __func__);
				return -1;
			}
		} else {
			qevent_fprintf(stdout, "%s: don't find psk and try to"
				" get passphrase for AP\n", __func__);

			if (qcsapi_wifi_get_key_passphrase(QTN_DEFAULT_IFACE, 0, psk)) {
				qevent_fprintf(stderr, "%s: Failed to get psk"
					" or passphrase for AP\n", __func__);
				return -1;
			}

			qevent_fprintf(stdout, "%s: AP Passphrase %s\n", __func__, psk);

			if (qevent_md5_convert_passphrase(psk, psk_md5) < 0) {
				qevent_fprintf(stderr, "%s: MD5 coversion for AP"
					" passphrase fails\n", __func__);
				return -1;
			}

			qevent_fprintf(stdout, "%s: AP Passphrase_MD5 %s\n", __func__, psk_md5);

			psk_len = strnlen(psk, QTN_WPA_PSK_LEN);
			if ((psk_len < QTN_WPA_PASSPHRASE_MIN_LEN) ||
				(psk_len > QTN_WPA_PASSPHRASE_MAX_LEN)) {
				qevent_fprintf(stderr, "%s: invalid passpharse for AP\n",
					__func__);
				return -1;
			}

			ssid_len = strnlen(ssid, QTN_SSID_MAX_LEN);
			pbkdf2_sha1(psk_md5, ssid, ssid_len, SHA1_ITERATION_NUM, pmk, PMK_LEN);
		}
	} else {
		qevent_fprintf(stderr, "%s: invalid wifi mode %u\n", __func__, mode);
		return -1;
	}

	return 0;
}

static int
qevent_generate_wds_key(uint8_t *mbs_addr, uint8_t *rbs_addr, uint8_t *wds_key)
{
	qcsapi_wifi_mode mode = qcsapi_nosuch_mode;
	uint8_t pmk[PMK_LEN] = {0};
	uint8_t *addr[3];
	size_t len[3];

	if (qcsapi_wifi_get_mode(QTN_DEFAULT_IFACE, &mode) < 0) {
	      qevent_fprintf(stderr, "%s: failed to get wifi mode\n", __func__);
		return -1;
	}

	if (qevent_get_pmk(mode, pmk) < 0) {
		qevent_fprintf(stderr, "%s: failed to get pmk\n", __func__);
		return -1;
	}

	addr[0] = mbs_addr;
	len[0] = ETH_ALEN;
	addr[1] = rbs_addr;
	len[1] = ETH_ALEN;
	addr[2] = pmk;
	len[2] = PMK_LEN;

	sha256_vector(3, addr, len, wds_key);

	return 0;
}

static void
qevent_ap_process_wds_ext_event(struct qtn_wds_ext_event_data *event_data,
	char *cmd, uint8_t len)
{
	uint8_t own_addr[ETH_ALEN] = {0};
	uint8_t wds_key[QTN_WDS_KEY_LEN] = {0};
	char wds_key_hex[QTN_WDS_KEY_LEN * 2 + 1] = {'\0'};
	string_16 wpa_encrypt = "Basic";

	if (qcsapi_interface_get_mac_addr(QTN_DEFAULT_IFACE, own_addr) < 0) {
		qevent_fprintf(stderr, "%s: failed to get own mac address\n", __func__);
		return;
	}
	qcsapi_wifi_get_beacon_type(QTN_DEFAULT_IFACE, wpa_encrypt);

	switch (event_data->cmd) {
	case WDS_EXT_RECEIVED_MBS_IE:
		if (strncmp(wpa_encrypt, "Basic", 5)) {
			if (qevent_generate_wds_key(event_data->mac,
					own_addr, wds_key) < 0) {
				qevent_fprintf(stderr, "%s: failed to generate wds key\n",
					__func__);
				return;
			}
			snprintf_hex(wds_key_hex, sizeof(wds_key_hex),
						wds_key, sizeof(wds_key));
		} else {
			snprintf(wds_key_hex, sizeof(wds_key_hex), "NULL");
		}

		snprintf(cmd, len, "%s %s peer=" MACSTR " channel=%d wds_key=%s &",
				QTN_WDS_EXT_SCRIPT, "RBS-CREATE-WDS-LINK",
				MAC2STR(event_data->mac), event_data->channel,
				wds_key_hex);
		break;
	case WDS_EXT_RECEIVED_RBS_IE:
		if (strncmp(wpa_encrypt, "Basic", 5)) {
			if (qevent_generate_wds_key(own_addr,
					event_data->mac, wds_key) < 0) {
				qevent_fprintf(stderr, "%s: failed to generate wds key\n",
					__func__);
				return;
			}
			snprintf_hex(wds_key_hex, sizeof(wds_key_hex),
						wds_key, sizeof(wds_key));
		} else {
			snprintf(wds_key_hex, sizeof(wds_key_hex), "NULL");
		}

		snprintf(cmd, len, "%s %s peer=" MACSTR " wds_key=%s &",
				QTN_WDS_EXT_SCRIPT, "MBS-CREATE-WDS-LINK",
				MAC2STR(event_data->mac), wds_key_hex);
		break;
	case WDS_EXT_LINK_STATUS_UPDATE:
		if (event_data->extender_role == IEEE80211_EXTENDER_ROLE_MBS) {
			snprintf(cmd, len, "%s %s peer=" MACSTR " &",
					QTN_WDS_EXT_SCRIPT, "MBS-REMOVE-WDS-LINK",
					MAC2STR(event_data->mac));
		} else if (event_data->extender_role == IEEE80211_EXTENDER_ROLE_RBS) {
			snprintf(cmd, len, "%s %s peer=" MACSTR " &",
					QTN_WDS_EXT_SCRIPT, "RBS-REMOVE-WDS-LINK",
					MAC2STR(event_data->mac));
		}
		break;
	case WDS_EXT_RBS_OUT_OF_BRR:
		snprintf(cmd, len, "%s %s peer=" MACSTR " &",
				QTN_WDS_EXT_SCRIPT, "START-STA-RBS",
				MAC2STR(event_data->mac));
		break;
	case WDS_EXT_RBS_SET_CHANNEL:
		snprintf(cmd, len, "%s %s channel=%d &",
				QTN_WDS_EXT_SCRIPT, "RBS-SET-CHANNEL",
				event_data->channel);
		break;
	case WDS_EXT_CLEANUP_WDS_LINK:
		snprintf(cmd, len, "%s %s peer=" MACSTR " &",
				QTN_WDS_EXT_SCRIPT, "REMOVE-WDS-LINK",
				MAC2STR(event_data->mac));
		break;
	default:
		qevent_fprintf(stderr, "%s: unsupported event command %d\n",
			__func__, event_data->cmd);
		break;
	}
}

static void
qevent_sta_process_wds_ext_event(struct qtn_wds_ext_event_data *event_data,
	char *cmd, uint8_t len)
{
	qcsapi_SSID ssid = {'\0'};
	uint8_t own_addr[ETH_ALEN] = {0};
	uint8_t wds_key[QTN_WDS_KEY_LEN] = {0};
	char wds_key_hex[QTN_WDS_KEY_LEN * 2 + 1] = {'\0'};
	string_32 wpa_encrypt = "NONE";

	if (qcsapi_interface_get_mac_addr(QTN_DEFAULT_IFACE, own_addr) < 0) {
		qevent_fprintf(stderr, "%s: failed to get own mac address\n", __func__);
		return;
	}

	if (qcsapi_wifi_get_SSID(QTN_DEFAULT_IFACE, ssid) < 0) {
		qevent_fprintf(stderr, "%s: Failed to get the SSID\n", __func__);
		return;
	}

	qcsapi_SSID_get_authentication_mode(QTN_DEFAULT_IFACE, ssid, wpa_encrypt);

	switch (event_data->cmd) {
	case WDS_EXT_RECEIVED_MBS_IE:
		if (strncmp(wpa_encrypt, "NONE", 4)) {
			if (qevent_generate_wds_key(event_data->mac,
					own_addr, wds_key) < 0) {
				qevent_fprintf(stderr, "%s: failed to generate wds key\n",
					__func__);
				return;
			}
			snprintf_hex(wds_key_hex, sizeof(wds_key_hex),
						wds_key, sizeof(wds_key));
		} else {
			snprintf(wds_key_hex, sizeof(wds_key_hex), "NULL");
		}

		snprintf(cmd, len, "%s %s peer=" MACSTR " wds_key=%s channel=%d bw=%d &",
			QTN_WDS_EXT_SCRIPT, "START-AP-RBS", MAC2STR(event_data->mac),
			wds_key_hex, event_data->channel, event_data->bandwidth);
		break;
	case WDS_EXT_LINK_STATUS_UPDATE:
		snprintf(cmd, len, "%s %s &",
			QTN_WDS_EXT_SCRIPT, "START-STA-RBS");
		break;
	default:
		qevent_fprintf(stderr, "%s: unsupported event command %d\n",
			__func__, event_data->cmd);
		break;
	}
}

static void
qevent_handle_wds_ext_event(void *custom)
{
	char cmd[QTN_WDS_EXT_CMD_LEN] = {'\0'};
	qcsapi_wifi_mode mode = qcsapi_nosuch_mode;
	char *mode_str = "invalid";

	struct qtn_wds_ext_event_data *event_data =
		(struct qtn_wds_ext_event_data*)custom;

	qcsapi_wifi_get_mode(QTN_DEFAULT_IFACE, &mode);
	if (mode == qcsapi_access_point) {
		mode_str = "AP";
	} else if (mode == qcsapi_station) {
		mode_str = "STA";
	} else {
		qevent_fprintf(stderr, "%s: invalid wifi mode %d\n",
			__func__, mode);
		return;
	}

	qevent_fprintf(stdout, "%s: %s received QTN-WDS-EXT message, "
			"cmd = %d, mac = " MACSTR " peer role=%d\n",
			__func__, mode_str, event_data->cmd,
			MAC2STR(event_data->mac),
			event_data->extender_role);

	if (mode == qcsapi_access_point)
		qevent_ap_process_wds_ext_event(event_data, cmd, QTN_WDS_EXT_CMD_LEN - 1);
	else if (mode == qcsapi_station)
		qevent_sta_process_wds_ext_event(event_data, cmd, QTN_WDS_EXT_CMD_LEN - 1);

	qevent_fprintf(stdout, "%s: call command - %s\n", __func__, cmd);
	system(cmd);
}


static void
qevent_handle_wireless_event_custom(char *custom)
{
	if (strncmp(custom, "QTN-WDS-EXT", 11) == 0)
		qevent_handle_wds_ext_event(custom);
	else
		qevent_fprintf(stdout, "%s: unsupported event %s\n",
			__func__, custom);
}

static void
qevent_handle_wireless_event(char *data, int len)
{
	struct iw_event iwe_buf, *iwe = &iwe_buf;
	char *pos, *end, *custom, *buf;

	pos = data;
	end = data + len;

	while (pos + IW_EV_LCP_LEN <= end) {
		/* Event data may be unaligned, so make a local, aligned copy
		 * before processing. */
		memcpy(&iwe_buf, pos, IW_EV_LCP_LEN);
		qevent_fprintf(stdout, "Wireless event: cmd=0x%x len=%d\n",
			   iwe->cmd, iwe->len);
		if (iwe->len <= IW_EV_LCP_LEN)
			return;

		custom = pos + IW_EV_POINT_LEN;
		if (iwe->cmd == IWEVCUSTOM) {
			char *dpos = (char *) &iwe_buf.u.data.length;
			int dlen = dpos - (char *) &iwe_buf;
			memcpy(dpos, pos + IW_EV_LCP_LEN,
			       sizeof(struct iw_event) - dlen);
		} else {
			memcpy(&iwe_buf, pos, sizeof(struct iw_event));
			custom += IW_EV_POINT_OFF;
		}

		switch (iwe->cmd) {
		case IWEVCUSTOM:
			if (custom + iwe->u.data.length > end)
				return;
			buf = malloc(iwe->u.data.length + 1);
			if (buf == NULL)
				return;

			memcpy(buf, custom, iwe->u.data.length);
			buf[iwe->u.data.length] = '\0';
			qevent_handle_wireless_event_custom(buf);
			free(buf);
			break;
		}

		pos += iwe->len;
	}
}

static void
qevent_wireless_event_rtm_newlink(struct ifinfomsg *ifi,
				   uint8_t *buf, size_t len)
{
	int attrlen, rta_len;
	struct rtattr *attr;

	attrlen = len;
	attr = (struct rtattr *) buf;

	rta_len = RTA_ALIGN(sizeof(struct rtattr));
	while (RTA_OK(attr, attrlen)) {
		if (attr->rta_type == IFLA_WIRELESS) {
			qevent_handle_wireless_event(
				((char *) attr) + rta_len,
				attr->rta_len - rta_len);
		}
		attr = RTA_NEXT(attr, attrlen);
	}
}


static inline void
qevent_handle_netlink_events(struct rtnl_handle *rth)
{
	while (1) {
		struct sockaddr_nl sanl;
		socklen_t sanllen = sizeof(struct sockaddr_nl);
		char buf[QTN_EVENT_MSG_BUF_LEN] = {0};
		struct nlmsghdr *h;
		int rest;

		rest = recvfrom(rth->fd, buf, sizeof(buf), MSG_DONTWAIT,
				(struct sockaddr*)&sanl, &sanllen);
		if (rest < 0) {
			if(errno != EINTR && errno != EAGAIN)
				qevent_fprintf(stderr, "%s: error reading netlink: %s.\n",
					__func__, strerror(errno));
				return;
		}

		if (rest == 0) {
			qevent_fprintf(stdout, "%s: EOF on netlink\n", __func__);
			return;
		}

		h = (struct nlmsghdr*)buf;
		while (rest >= (int)sizeof(*h)) {
			int len = h->nlmsg_len;
			int data_len = len - sizeof(*h);

			if (data_len < 0 || len > rest) {
				qevent_fprintf(stderr, "%s: malformed netlink message: len=%d\n",
					__func__, len);
				break;
			}

			switch (h->nlmsg_type) {
			case RTM_NEWLINK:
				qevent_wireless_event_rtm_newlink(NLMSG_DATA(h),
					(uint8_t *) NLMSG_DATA(h) + NLMSG_ALIGN(sizeof(struct ifinfomsg)),
					NLMSG_PAYLOAD(h, sizeof(struct ifinfomsg)));
				break;
			default:
				break;
			}

			len = NLMSG_ALIGN(len);
			rest -= len;
			h = (struct nlmsghdr*)((char*)h + len);
		}

		if (rest > 0)
			qevent_fprintf(stderr, "%s: redundant size %d on netlink\n", __func__, rest);
    }
}

static inline int
qevent_wait_for_event(struct rtnl_handle *rth)
{
	while (!loop_terminated) {
		fd_set rfds;	/* File descriptors for select */
		int last_fd;	/* Last fd */
		int ret;

		FD_ZERO(&rfds);
		FD_SET(rth->fd, &rfds);
		last_fd = rth->fd;

		ret = select(last_fd + 1, &rfds, NULL, NULL, NULL);

		/* Check if there was an error */
		if (ret < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			fprintf(stderr, "Unhandled signal - exiting...\n");
			break;
		}

		/* Check if there was a timeout */
		if (ret == 0)
			continue;

		/* Check for interface discovery events. */
		if(FD_ISSET(rth->fd, &rfds))
			qevent_handle_netlink_events(rth);
	}

	return 0;
}

static void
qevent_usage(int status)
{
	fprintf(status ? stderr : stdout,
		"Usage: qevent_server [OPTIONS]\n"
		"   Receive qevent wireless events and manage qevent state machine\n"
		"   Options are:\n"
		"     -h,--help      Print this message.\n"
		"     -v,--version   Show version of this program.\n"
		"     -d,--debug     Print debugging messages.\n"
		);
	exit(status);
}

int
main(int argc, char * argv[])
{
	struct rtnl_handle rth;
	int opt;

	/* Check command line options */
	while((opt = getopt_long(argc, argv, "hvd", long_opts_qevent, NULL)) > 0) {
		switch(opt) {
		case 'h':
			qevent_usage(0);
			break;
		case 'v':
			fprintf(stdout, "qevent_server: 1.0\n");
			break;
		case 'd':
			debug_enable = 1;
			break;
		default:
			qevent_usage(1);
			break;
		}
	}

	if (optind < argc) {
		fprintf(stderr, "Too many arguments.\n");
		qevent_usage(1);
	}

	/* Open netlink channel */
	if(qevent_rtnl_open(&rth, RTMGRP_LINK) < 0) {
		perror("Can't initialize rtnetlink socket");
		return 1;
	}

	/*establish signal handler*/
	signal(SIGINT, qevent_sigext);
	signal(SIGTERM, qevent_sigext);

	/* Wait for event */
	qevent_wait_for_event(&rth);

	/* Cleanup - only if you are pedantic */
	qevent_rtnl_close(&rth);

	return 0;
}

