/*
 * hostapd / UNIX domain socket -based control interface
 * Copyright (c) 2004-2014, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#ifndef CONFIG_NATIVE_WINDOWS

#include <sys/un.h>
#include <sys/stat.h>
#include <stddef.h>

#include "utils/common.h"
#include "utils/eloop.h"
#include "common/version.h"
#include "common/ieee802_11_defs.h"
#include "drivers/driver.h"
#include "radius/radius_client.h"
#include "radius/radius_server.h"
#include "ap/hostapd.h"
#include "ap/ap_config.h"
#include "ap/ieee802_1x.h"
#include "ap/wpa_auth.h"
#include "ap/ieee802_11.h"
#include "ap/sta_info.h"
#include "ap/wps_hostapd.h"
#include "ap/ctrl_iface_ap.h"
#include "ap/ap_drv_ops.h"
#include "ap/hs20.h"
#include "ap/wnm_ap.h"
#include "ap/wpa_auth.h"
#include "wps/wps_defs.h"
#include "wps/wps.h"
#include "config_file.h"
#include "ctrl_iface.h"
#include "uuid.h"
#include "ap/wpa_auth_i.h"

struct wpa_ctrl_dst {
	struct wpa_ctrl_dst *next;
	struct sockaddr_un addr;
	socklen_t addrlen;
	int debug_level;
	int errors;
};


static struct hostapd_data *hostapd_get_bss(struct hostapd_iface *iface, const char *bss_name)
{
	int i;

	if ((iface == NULL) || (bss_name == NULL))
		return NULL;

	for (i = 0; i < iface->num_bss; i++) {
		if (strncmp(iface->bss[i]->conf->iface, bss_name, IFNAMSIZ) == 0)
			return iface->bss[i];
	}

	return NULL;
}


static void hostapd_ctrl_iface_send(struct hostapd_data *hapd, int level,
				    const char *buf, size_t len);


static int hostapd_ctrl_iface_attach(struct hostapd_data *hapd,
				     struct sockaddr_un *from,
				     socklen_t fromlen)
{
	struct wpa_ctrl_dst *dst;

	dst = os_zalloc(sizeof(*dst));
	if (dst == NULL)
		return -1;
	os_memcpy(&dst->addr, from, sizeof(struct sockaddr_un));
	dst->addrlen = fromlen;
	dst->debug_level = MSG_INFO;
	dst->next = hapd->ctrl_dst;
	hapd->ctrl_dst = dst;
	wpa_hexdump(MSG_DEBUG, "CTRL_IFACE monitor attached",
		    (u8 *) from->sun_path,
		    fromlen - offsetof(struct sockaddr_un, sun_path));
	return 0;
}


static int hostapd_ctrl_iface_detach(struct hostapd_data *hapd,
				     struct sockaddr_un *from,
				     socklen_t fromlen)
{
	struct wpa_ctrl_dst *dst, *prev = NULL;

	dst = hapd->ctrl_dst;
	while (dst) {
		if (fromlen == dst->addrlen &&
		    os_memcmp(from->sun_path, dst->addr.sun_path,
			      fromlen - offsetof(struct sockaddr_un, sun_path))
		    == 0) {
			wpa_hexdump(MSG_DEBUG, "CTRL_IFACE monitor detached",
				    (u8 *) from->sun_path,
				    fromlen -
				    offsetof(struct sockaddr_un, sun_path));
			if (prev == NULL)
				hapd->ctrl_dst = dst->next;
			else
				prev->next = dst->next;
			os_free(dst);
			return 0;
		}
		prev = dst;
		dst = dst->next;
	}
	return -1;
}


static int hostapd_ctrl_iface_level(struct hostapd_data *hapd,
				    struct sockaddr_un *from,
				    socklen_t fromlen,
				    char *level)
{
	struct wpa_ctrl_dst *dst;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE LEVEL %s", level);

	dst = hapd->ctrl_dst;
	while (dst) {
		if (fromlen == dst->addrlen &&
		    os_memcmp(from->sun_path, dst->addr.sun_path,
			      fromlen - offsetof(struct sockaddr_un, sun_path))
		    == 0) {
			wpa_hexdump(MSG_DEBUG, "CTRL_IFACE changed monitor "
				    "level", (u8 *) from->sun_path, fromlen -
				    offsetof(struct sockaddr_un, sun_path));
			dst->debug_level = atoi(level);
			return 0;
		}
		dst = dst->next;
	}

	return -1;
}

extern int wpa_debug_level;
extern int wpa_debug_timestamp;

typedef struct {
	char *str;
	int  value;
} tuple;

const tuple tuple_logger_module[] = {
	{"IEEE80211", HOSTAPD_MODULE_IEEE80211},
	{"IEEE8021X", HOSTAPD_MODULE_IEEE8021X},
	{"RADIUS", HOSTAPD_MODULE_RADIUS},
	{"WPA", HOSTAPD_MODULE_WPA},
	{"DRIVER", HOSTAPD_MODULE_DRIVER},
	{"IAPP", HOSTAPD_MODULE_IAPP},
	{"MLME", HOSTAPD_MODULE_MLME},
};

const tuple tuple_logger_level[] = {
	{"VERBOSE", HOSTAPD_LEVEL_DEBUG_VERBOSE},
	{"DEBUG", HOSTAPD_LEVEL_DEBUG},
	{"INFO", HOSTAPD_LEVEL_INFO},
	{"NOTICE", HOSTAPD_LEVEL_NOTICE},
	{"WARNING", HOSTAPD_LEVEL_WARNING},
};

const tuple tuple_debug_level[] = {
	{"EXCESSIVE", MSG_EXCESSIVE},
	{"MSGDUMP", MSG_MSGDUMP},
	{"DEBUG", MSG_DEBUG},
	{"INFO", MSG_INFO},
	{"WARNING", MSG_WARNING},
	{"ERROR", MSG_ERROR},
};

static int str_to_value(const tuple tuple_array[], int size, const char *str)
{
	unsigned int iter;

	for (iter = 0; iter < size; iter++)
		if (strcasecmp(tuple_array[iter].str, str) == 0)
			return tuple_array[iter].value;

	return -1;
}

static const char *value_to_str(const tuple tuple_array[], int size, int value)
{
	unsigned int iter;

	for (iter = 0; iter < size; iter++)
		if (tuple_array[iter].value == value)
			return tuple_array[iter].str;

	return "?";
}

static int hostapd_ctrl_iface_log_level(struct hostapd_data *hapd, const char *cmd,
                           char *buf, size_t buflen)
{
    char *pos, *end, *stamp;
    int ret;

    if (cmd == NULL) {
        return -1;
    }

    if (*cmd == '\0') {
        pos = buf;
        end = buf + buflen;
        ret = os_snprintf(pos, end - pos, "Current level: %s\n"
                  "Timestamp: %d\n",
                  value_to_str(tuple_debug_level, ARRAY_SIZE(tuple_debug_level),
								wpa_debug_level), wpa_debug_timestamp);
        if (ret < 0 || ret >= end - pos)
            ret = 0;

        return ret;
    }

    while (*cmd == ' ')
        cmd++;

    stamp = os_strchr(cmd, ' ');
    if (stamp) {
        *stamp++ = '\0';
        while (*stamp == ' ') {
            stamp++;
        }
    }

    if (cmd && os_strlen(cmd)) {
        int level = str_to_value(tuple_debug_level, ARRAY_SIZE(tuple_debug_level), cmd);
        if (level < 0)
            return -1;
        wpa_debug_level = level;
    }

    if (stamp && os_strlen(stamp))
        wpa_debug_timestamp = atoi(stamp);

    return 3;
}

static int hostapd_ctrl_iface_logger_level(struct hostapd_data *hapd, const char *cmd,
                           char *buf, size_t buflen)
{
	char *pos, *end;
	char *module, *level, *next;
	int ret, module_val, level_val;

	if (cmd == NULL)
		return -1;

    if (*cmd == '\0') {
        pos = buf;
        end = buf + buflen;
		ret = os_snprintf(pos, end - pos, "Module and Level: %s:%s\n",
				value_to_str(tuple_logger_module, ARRAY_SIZE(tuple_logger_module), hapd->conf->logger_syslog),
				value_to_str(tuple_logger_level, ARRAY_SIZE(tuple_logger_level), hapd->conf->logger_syslog_level));
		if (ret < 0 || ret >= (end - pos))
			ret = 0;

		return ret;
	}

	module = strtok_r(cmd, " 	", &next);
	if (module == NULL)
		return -1;
	level = strtok_r(NULL, " 	", &next);
	if (level == NULL)
		return -1;

	module_val = str_to_value(tuple_logger_module, ARRAY_SIZE(tuple_logger_module), module);
	if (module_val < 0)
		return -1;
	level_val = str_to_value(tuple_logger_level, ARRAY_SIZE(tuple_logger_level), level);
	if (level_val < 0)
		return -1;
	hapd->conf->logger_syslog = module_val;
	hapd->conf->logger_syslog_level = level_val;

	return 3;
}

static int hostapd_ctrl_iface_new_sta(struct hostapd_data *hapd,
				      const char *txtaddr)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE NEW_STA %s", txtaddr);

	if (hwaddr_aton(txtaddr, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (sta)
		return 0;

	wpa_printf(MSG_DEBUG, "Add new STA " MACSTR " based on ctrl_iface "
		   "notification", MAC2STR(addr));
	sta = ap_sta_add(hapd, addr);
	if (sta == NULL)
		return -1;

	hostapd_new_assoc_sta(hapd, sta, 0);
	return 0;
}


#ifdef CONFIG_IEEE80211W
#ifdef NEED_AP_MLME
static int hostapd_ctrl_iface_sa_query(struct hostapd_data *hapd,
				       const char *txtaddr)
{
	u8 addr[ETH_ALEN];
	u8 trans_id[WLAN_SA_QUERY_TR_ID_LEN];

	wpa_printf(MSG_DEBUG, "CTRL_IFACE SA_QUERY %s", txtaddr);

	if (hwaddr_aton(txtaddr, addr) ||
	    os_get_random(trans_id, WLAN_SA_QUERY_TR_ID_LEN) < 0)
		return -1;

	ieee802_11_send_sa_query_req(hapd, addr, trans_id);

	return 0;
}
#endif /* NEED_AP_MLME */
#endif /* CONFIG_IEEE80211W */


#ifdef CONFIG_WPS
static int hostapd_ctrl_iface_wps_pin(struct hostapd_data *hapd, char *txt)
{
	char *pin = os_strchr(txt, ' ');
	char *timeout_txt;
	int timeout;
	u8 addr_buf[ETH_ALEN], *addr = NULL;
	char *pos;

	if (pin == NULL)
		return -1;
	*pin++ = '\0';

	timeout_txt = os_strchr(pin, ' ');
	if (timeout_txt) {
		*timeout_txt++ = '\0';
		timeout = atoi(timeout_txt);
		pos = os_strchr(timeout_txt, ' ');
		if (pos) {
			*pos++ = '\0';
			if (hwaddr_aton(pos, addr_buf) == 0)
				addr = addr_buf;
		}
	} else
		timeout = WPS_PIN_DEFAULT_TIMEOUT;

	return hostapd_wps_add_pin(hapd, addr, txt, pin, timeout);
}


static int hostapd_ctrl_iface_wps_check_pin(
	struct hostapd_data *hapd, char *cmd, char *buf, size_t buflen)
{
	char pin[9];
	size_t len;
	char *pos;
	int ret;

	wpa_hexdump_ascii_key(MSG_DEBUG, "WPS_CHECK_PIN",
			      (u8 *) cmd, os_strlen(cmd));
	for (pos = cmd, len = 0; *pos != '\0'; pos++) {
		if (*pos < '0' || *pos > '9')
			continue;
		pin[len++] = *pos;
		if (len == 9) {
			wpa_printf(MSG_DEBUG, "WPS: Too long PIN");
			return -1;
		}
	}
	if (len != 4 && len != 8) {
		wpa_printf(MSG_DEBUG, "WPS: Invalid PIN length %d", (int) len);
		return -1;
	}
	pin[len] = '\0';

	if (len == 8) {
		unsigned int pin_val;
		pin_val = atoi(pin);
		if (!wps_pin_valid(pin_val)) {
			wpa_printf(MSG_DEBUG, "WPS: Invalid checksum digit");
			ret = os_snprintf(buf, buflen, "FAIL-CHECKSUM\n");
			if (ret < 0 || (size_t) ret >= buflen)
				return -1;
			return ret;
		}
	}

	ret = os_snprintf(buf, buflen, "%s", pin);
	if (ret < 0 || (size_t) ret >= buflen)
		return -1;

	return ret;
}


#ifdef CONFIG_WPS_NFC
static int hostapd_ctrl_iface_wps_nfc_tag_read(struct hostapd_data *hapd,
					       char *pos)
{
	size_t len;
	struct wpabuf *buf;
	int ret;

	len = os_strlen(pos);
	if (len & 0x01)
		return -1;
	len /= 2;

	buf = wpabuf_alloc(len);
	if (buf == NULL)
		return -1;
	if (hexstr2bin(pos, wpabuf_put(buf, len), len) < 0) {
		wpabuf_free(buf);
		return -1;
	}

	ret = hostapd_wps_nfc_tag_read(hapd, buf);
	wpabuf_free(buf);

	return ret;
}


static int hostapd_ctrl_iface_wps_nfc_config_token(struct hostapd_data *hapd,
						   char *cmd, char *reply,
						   size_t max_len)
{
	int ndef;
	struct wpabuf *buf;
	int res;

	if (os_strcmp(cmd, "WPS") == 0)
		ndef = 0;
	else if (os_strcmp(cmd, "NDEF") == 0)
		ndef = 1;
	else
		return -1;

	buf = hostapd_wps_nfc_config_token(hapd, ndef);
	if (buf == NULL)
		return -1;

	res = wpa_snprintf_hex_uppercase(reply, max_len, wpabuf_head(buf),
					 wpabuf_len(buf));
	reply[res++] = '\n';
	reply[res] = '\0';

	wpabuf_free(buf);

	return res;
}


static int hostapd_ctrl_iface_wps_nfc_token_gen(struct hostapd_data *hapd,
						char *reply, size_t max_len,
						int ndef)
{
	struct wpabuf *buf;
	int res;

	buf = hostapd_wps_nfc_token_gen(hapd, ndef);
	if (buf == NULL)
		return -1;

	res = wpa_snprintf_hex_uppercase(reply, max_len, wpabuf_head(buf),
					 wpabuf_len(buf));
	reply[res++] = '\n';
	reply[res] = '\0';

	wpabuf_free(buf);

	return res;
}


static int hostapd_ctrl_iface_wps_nfc_token(struct hostapd_data *hapd,
					    char *cmd, char *reply,
					    size_t max_len)
{
	if (os_strcmp(cmd, "WPS") == 0)
		return hostapd_ctrl_iface_wps_nfc_token_gen(hapd, reply,
							    max_len, 0);

	if (os_strcmp(cmd, "NDEF") == 0)
		return hostapd_ctrl_iface_wps_nfc_token_gen(hapd, reply,
							    max_len, 1);

	if (os_strcmp(cmd, "enable") == 0)
		return hostapd_wps_nfc_token_enable(hapd);

	if (os_strcmp(cmd, "disable") == 0) {
		hostapd_wps_nfc_token_disable(hapd);
		return 0;
	}

	return -1;
}


static int hostapd_ctrl_iface_nfc_get_handover_sel(struct hostapd_data *hapd,
						   char *cmd, char *reply,
						   size_t max_len)
{
	struct wpabuf *buf;
	int res;
	char *pos;
	int ndef;

	pos = os_strchr(cmd, ' ');
	if (pos == NULL)
		return -1;
	*pos++ = '\0';

	if (os_strcmp(cmd, "WPS") == 0)
		ndef = 0;
	else if (os_strcmp(cmd, "NDEF") == 0)
		ndef = 1;
	else
		return -1;

	if (os_strcmp(pos, "WPS-CR") == 0)
		buf = hostapd_wps_nfc_hs_cr(hapd, ndef);
	else
		buf = NULL;
	if (buf == NULL)
		return -1;

	res = wpa_snprintf_hex_uppercase(reply, max_len, wpabuf_head(buf),
					 wpabuf_len(buf));
	reply[res++] = '\n';
	reply[res] = '\0';

	wpabuf_free(buf);

	return res;
}


static int hostapd_ctrl_iface_nfc_report_handover(struct hostapd_data *hapd,
						  char *cmd)
{
	size_t len;
	struct wpabuf *req, *sel;
	int ret;
	char *pos, *role, *type, *pos2;

	role = cmd;
	pos = os_strchr(role, ' ');
	if (pos == NULL)
		return -1;
	*pos++ = '\0';

	type = pos;
	pos = os_strchr(type, ' ');
	if (pos == NULL)
		return -1;
	*pos++ = '\0';

	pos2 = os_strchr(pos, ' ');
	if (pos2 == NULL)
		return -1;
	*pos2++ = '\0';

	len = os_strlen(pos);
	if (len & 0x01)
		return -1;
	len /= 2;

	req = wpabuf_alloc(len);
	if (req == NULL)
		return -1;
	if (hexstr2bin(pos, wpabuf_put(req, len), len) < 0) {
		wpabuf_free(req);
		return -1;
	}

	len = os_strlen(pos2);
	if (len & 0x01) {
		wpabuf_free(req);
		return -1;
	}
	len /= 2;

	sel = wpabuf_alloc(len);
	if (sel == NULL) {
		wpabuf_free(req);
		return -1;
	}
	if (hexstr2bin(pos2, wpabuf_put(sel, len), len) < 0) {
		wpabuf_free(req);
		wpabuf_free(sel);
		return -1;
	}

	if (os_strcmp(role, "RESP") == 0 && os_strcmp(type, "WPS") == 0) {
		ret = hostapd_wps_nfc_report_handover(hapd, req, sel);
	} else {
		wpa_printf(MSG_DEBUG, "NFC: Unsupported connection handover "
			   "reported: role=%s type=%s", role, type);
		ret = -1;
	}
	wpabuf_free(req);
	wpabuf_free(sel);

	return ret;
}

#endif /* CONFIG_WPS_NFC */


static int hostapd_ctrl_iface_wps_ap_pin(struct hostapd_data *hapd, char *txt,
					 char *buf, size_t buflen)
{
	int timeout = WPS_AP_PIN_DEFAULT_TIMEOUT;
	char *pos;
	const char *pin_txt;

	pos = os_strchr(txt, ' ');
	if (pos)
		*pos++ = '\0';

	if (os_strcmp(txt, "enable") == 0) {
		if (pos)
			timeout = atoi(pos);
		hostapd_wps_ap_pin_enable(hapd, timeout);
		return os_snprintf(buf, buflen, "OK\n");
	}

	if (os_strcmp(txt, "disable") == 0) {
		hostapd_wps_ap_pin_disable(hapd);
		return os_snprintf(buf, buflen, "OK\n");
	}

	if (os_strcmp(txt, "random") == 0) {
		if (pos)
			timeout = atoi(pos);
		pin_txt = hostapd_wps_ap_pin_random(hapd, timeout);
		if (pin_txt == NULL)
			return -1;
		return os_snprintf(buf, buflen, "%s", pin_txt);
	}

	if (os_strcmp(txt, "get") == 0) {
		pin_txt = hostapd_wps_ap_pin_get(hapd);
		if (pin_txt == NULL)
			return -1;
		return os_snprintf(buf, buflen, "%s", pin_txt);
	}

	if (os_strcmp(txt, "set") == 0) {
		char *pin;
		if (pos == NULL)
			return -1;
		pin = pos;
		pos = os_strchr(pos, ' ');
		if (pos) {
			*pos++ = '\0';
			timeout = atoi(pos);
		}
		if (os_strlen(pin) > buflen)
			return -1;
		if (hostapd_wps_ap_pin_set(hapd, pin, timeout) < 0)
			return -1;
		return os_snprintf(buf, buflen, "%s", pin);
	}

	return -1;
}


static int hostapd_ctrl_iface_wps_config(struct hostapd_data *hapd, char *txt)
{
	char *pos;
	char *ssid, *auth, *encr = NULL, *key = NULL;

	ssid = txt;
	pos = os_strchr(txt, ' ');
	if (!pos)
		return -1;
	*pos++ = '\0';

	auth = pos;
	pos = os_strchr(pos, ' ');
	if (pos) {
		*pos++ = '\0';
		encr = pos;
		pos = os_strchr(pos, ' ');
		if (pos) {
			*pos++ = '\0';
			key = pos;
		}
	}

	return hostapd_wps_config_ap(hapd, ssid, auth, encr, key);
}


static const char * pbc_status_str(enum pbc_status status)
{
	switch (status) {
	case WPS_PBC_STATUS_DISABLE:
		return "Disabled";
	case WPS_PBC_STATUS_ACTIVE:
		return "Active";
	case WPS_PBC_STATUS_TIMEOUT:
		return "Timed-out";
	case WPS_PBC_STATUS_OVERLAP:
		return "Overlap";
	default:
		return "Unknown";
	}
}

static const char *wps_ext_state_to_str(enum wps_external_state external_state)
{
        static const struct {
                enum wps_external_state wps_external_state;
                char *state_as_str;
        } state_to_str[] = {
                { WPS_EXT_STATE_INI, "WPS_INITIAL" },
                { WPS_EXT_STATE_PROCESS_START, "WPS_START" },
                { WPS_EXT_STATE_PROCESS_SUCCESS, "WPS_SUCCESS" },
                { WPS_EXT_STATE_MSG_EX_ERROR, "WPS_ERROR" },
                { WPS_EXT_STATE_TIMEOUT_ERROR, "WPS_TIMEOUT" },
                { WPS_EXT_STATE_MSG_EX_OVERLAP, "WPS_OVERLAP" },
                { WPS_EXT_STATE_EAP_M2_SEND, "WPS_M2_SEND" },
                { WPS_EXT_STATE_EAP_M8_SEND, "WPS_M8_SEND" },
                { WPS_EXT_STATE_STA_CANCEL, "WPS_STA_CANCEL" },
                { WPS_EXT_STATE_STA_PIN_FAIL, "WPS_STA_PIN_ERR" },
                { WPS_EXT_STATE_AP_PIN_SUCCESS, "WPS_AP_PIN_SUC"},
                { WPS_EXT_STATE_AP_PIN_FAIL, "WPS_AP_PIN_ERR" },
		{ WPS_EXT_STATE_PIN_TIMEOUT, "WPS_PIN_TIMEOUT" },
                { WPS_EXT_STATE_UNKNOWN, NULL }
        };

        int              i;
        const char      *retaddr = NULL;

        for (i = 0; state_to_str[i].wps_external_state !=
                        WPS_EXT_STATE_UNKNOWN && retaddr == NULL; i++) {
                if (external_state == state_to_str[i].wps_external_state) {
                        retaddr = state_to_str[i].state_as_str;
                }
        }

        return retaddr;
}


static int qtn_ctrl_iface_wps_get_status(struct hostapd_data *hapd,
					     char *status_str, size_t status_len)
{
	int retval = -1;
	const char *equiv_status_str  = NULL;
	char numeric_value[12] = "";
	size_t equiv_status_len = 0;
	size_t numeric_value_len = 0;
	size_t total_status_len = 0;

	if (status_str == NULL || status_len < 1 || hapd == NULL ||
			hapd->wps == NULL)
		return -1;

	equiv_status_str = wps_ext_state_to_str(hapd->wps->wps_external_state);

	if (equiv_status_str == NULL)
		equiv_status_str = "unknown";

	snprintf(&numeric_value[0], sizeof(numeric_value),
		  "%d", (int)(hapd->wps->wps_external_state));
	numeric_value_len = strlen(&numeric_value[0]);

	equiv_status_len = strnlen(equiv_status_str, status_len);

	/*
	 * format is "NN (XXXXXXXX)\n", where NN is the numeric value and
	 * XXXXXXXX is the equivalent string (returned from
	 * wps_ext_state_to_str).  Total length is thus numeric_value_len +
	 * equiv_status_len + 4.
	 */

	total_status_len = equiv_status_len + numeric_value_len + 4;

	if (equiv_status_len + numeric_value_len + 4 < status_len) {
		snprintf(status_str, status_len, "%s (%s)\n",
				&numeric_value[0], equiv_status_str);
		retval = (int)total_status_len;
	}

	return retval;
}

static int hostapd_ctrl_iface_wps_get_status(struct hostapd_data *hapd,
					     char *buf, size_t buflen)
{
	int ret;
	char *pos, *end;
	const char *equiv_status_str  = NULL;

	pos = buf;
	end = buf + buflen;

	ret = os_snprintf(pos, end - pos, "PBC Status: %s\n",
			  pbc_status_str(hapd->wps_stats.pbc_status));

	equiv_status_str = wps_ext_state_to_str(hapd->wps->wps_external_state);

	if (ret < 0 || ret >= end - pos)
		return pos - buf;
	pos += ret;

	ret = os_snprintf(pos, end - pos, "Last WPS result: %s\n",
			  (hapd->wps_stats.status == WPS_STATUS_SUCCESS ?
			   "Success":
			   (hapd->wps_stats.status == WPS_STATUS_FAILURE ?
			    "Failed" : "None")));

	if (ret < 0 || ret >= end - pos)
		return pos - buf;
	pos += ret;

	/* If status == Failure - Add possible Reasons */
	if(hapd->wps_stats.status == WPS_STATUS_FAILURE &&
	   hapd->wps_stats.failure_reason > 0) {
		ret = os_snprintf(pos, end - pos,
				  "Failure Reason: %s\n",
				  wps_ei_str(hapd->wps_stats.failure_reason));

		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
	}

	if (hapd->wps_stats.status) {
		ret = os_snprintf(pos, end - pos, "Peer Address: " MACSTR "\n",
				  MAC2STR(hapd->wps_stats.peer_addr));

		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
	}

	return pos - buf;
}

#endif /* CONFIG_WPS */

#ifdef CONFIG_HS20

static int hostapd_ctrl_iface_hs20_wnm_notif(struct hostapd_data *hapd,
					     const char *cmd)
{
	u8 addr[ETH_ALEN];
	const char *url;

	if (hwaddr_aton(cmd, addr))
		return -1;
	url = cmd + 17;
	if (*url == '\0') {
		url = NULL;
	} else {
		if (*url != ' ')
			return -1;
		url++;
		if (*url == '\0')
			url = NULL;
	}

	return hs20_send_wnm_notification(hapd, addr, 1, url);
}


static int hostapd_ctrl_iface_hs20_deauth_req(struct hostapd_data *hapd,
					      const char *cmd)
{
	u8 addr[ETH_ALEN];
	int code, reauth_delay, ret;
	const char *pos;
	size_t url_len;
	struct wpabuf *req;

	/* <STA MAC Addr> <Code(0/1)> <Re-auth-Delay(sec)> [URL] */
	if (hwaddr_aton(cmd, addr))
		return -1;

	pos = os_strchr(cmd, ' ');
	if (pos == NULL)
		return -1;
	pos++;
	code = atoi(pos);

	pos = os_strchr(pos, ' ');
	if (pos == NULL)
		return -1;
	pos++;
	reauth_delay = atoi(pos);

	url_len = 0;
	pos = os_strchr(pos, ' ');
	if (pos) {
		pos++;
		url_len = os_strlen(pos);
	}

	req = wpabuf_alloc(4 + url_len);
	if (req == NULL)
		return -1;
	wpabuf_put_u8(req, code);
	wpabuf_put_le16(req, reauth_delay);
	wpabuf_put_u8(req, url_len);
	if (pos)
		wpabuf_put_data(req, pos, url_len);

	wpa_printf(MSG_DEBUG, "HS 2.0: Send WNM-Notification to " MACSTR
		   " to indicate imminent deauthentication (code=%d "
		   "reauth_delay=%d)", MAC2STR(addr), code, reauth_delay);
	ret = hs20_send_wnm_notification_deauth_req(hapd, addr, req);
	wpabuf_free(req);
	return ret;
}

#endif /* CONFIG_HS20 */


#ifdef CONFIG_INTERWORKING

static int hostapd_ctrl_iface_set_qos_map_set(struct hostapd_data *hapd,
					      const char *cmd)
{
	u8 qos_map_set[16 + 2 * 21], count = 0;
	const char *pos = cmd;
	int val, ret;

	for (;;) {
		if (count == sizeof(qos_map_set)) {
			wpa_printf(MSG_ERROR, "Too many qos_map_set parameters");
			return -1;
		}

		val = atoi(pos);
		if (val < 0 || val > 255) {
			wpa_printf(MSG_INFO, "Invalid QoS Map Set");
			return -1;
		}

		qos_map_set[count++] = val;
		pos = os_strchr(pos, ',');
		if (!pos)
			break;
		pos++;
	}

	if (count < 16 || count & 1) {
		wpa_printf(MSG_INFO, "Invalid QoS Map Set");
		return -1;
	}

	ret = hostapd_drv_set_qos_map(hapd, qos_map_set, count);
	if (ret) {
		wpa_printf(MSG_INFO, "Failed to set QoS Map Set");
		return -1;
	}

	os_memcpy(hapd->conf->qos_map_set, qos_map_set, count);
	hapd->conf->qos_map_set_len = count;

	return 0;
}


static int hostapd_ctrl_iface_send_qos_map_conf(struct hostapd_data *hapd,
						const char *cmd)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;
	struct wpabuf *buf;
	u8 *qos_map_set = hapd->conf->qos_map_set;
	u8 qos_map_set_len = hapd->conf->qos_map_set_len;
	int ret;

	if (!qos_map_set_len) {
		wpa_printf(MSG_INFO, "QoS Map Set is not set");
		return -1;
	}

	if (hwaddr_aton(cmd, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (sta == NULL) {
		wpa_printf(MSG_DEBUG, "Station " MACSTR " not found "
			   "for QoS Map Configuration message",
			   MAC2STR(addr));
		return -1;
	}

	if (!sta->qos_map_enabled) {
		wpa_printf(MSG_DEBUG, "Station " MACSTR " did not indicate "
			   "support for QoS Map", MAC2STR(addr));
		return -1;
	}

	buf = wpabuf_alloc(2 + 2 + qos_map_set_len);
	if (buf == NULL)
		return -1;

	wpabuf_put_u8(buf, WLAN_ACTION_QOS);
	wpabuf_put_u8(buf, QOS_QOS_MAP_CONFIG);

	/* QoS Map Set Element */
	wpabuf_put_u8(buf, WLAN_EID_QOS_MAP_SET);
	wpabuf_put_u8(buf, qos_map_set_len);
	wpabuf_put_data(buf, qos_map_set, qos_map_set_len);

	ret = hostapd_drv_send_action(hapd, hapd->iface->freq, 0, addr,
				      wpabuf_head(buf), wpabuf_len(buf));
	wpabuf_free(buf);

	return ret;
}

#endif /* CONFIG_INTERWORKING */


#ifdef CONFIG_WNM

static int hostapd_ctrl_iface_disassoc_imminent(struct hostapd_data *hapd,
						const char *cmd)
{
	u8 addr[ETH_ALEN];
	int disassoc_timer;
	struct sta_info *sta;

	if (hwaddr_aton(cmd, addr))
		return -1;
	if (cmd[17] != ' ')
		return -1;
	disassoc_timer = atoi(cmd + 17);

	sta = ap_get_sta(hapd, addr);
	if (sta == NULL) {
		wpa_printf(MSG_DEBUG, "Station " MACSTR
			   " not found for disassociation imminent message",
			   MAC2STR(addr));
		return -1;
	}

	return wnm_send_disassoc_imminent(hapd, sta, disassoc_timer);
}


static int hostapd_ctrl_iface_ess_disassoc(struct hostapd_data *hapd,
					   const char *cmd)
{
	u8 addr[ETH_ALEN];
	const char *url, *timerstr;
	int disassoc_timer;
	struct sta_info *sta;

	if (hwaddr_aton(cmd, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (sta == NULL) {
		wpa_printf(MSG_DEBUG, "Station " MACSTR
			   " not found for ESS disassociation imminent message",
			   MAC2STR(addr));
		return -1;
	}

	timerstr = cmd + 17;
	if (*timerstr != ' ')
		return -1;
	timerstr++;
	disassoc_timer = atoi(timerstr);
	if (disassoc_timer < 0 || disassoc_timer > 65535)
		return -1;

	url = os_strchr(timerstr, ' ');
	if (url == NULL)
		return -1;
	url++;

	return wnm_send_ess_disassoc_imminent(hapd, sta, url, disassoc_timer);
}

#endif /* CONFIG_WNM */


static int hostapd_ctrl_iface_get_config(struct hostapd_data *hapd,
					 char *buf, size_t buflen)
{
	int ret;
	char *pos, *end;

	pos = buf;
	end = buf + buflen;

	ret = os_snprintf(pos, end - pos, "bssid=" MACSTR "\n"
			  "ssid=%s\n",
			  MAC2STR(hapd->own_addr),
			  wpa_ssid_txt(hapd->conf->ssid.ssid,
				       hapd->conf->ssid.ssid_len));
	if (ret < 0 || ret >= end - pos)
		return pos - buf;
	pos += ret;

#ifdef CONFIG_WPS
	ret = os_snprintf(pos, end - pos, "wps_state=%s\n",
			  hapd->conf->wps_state == 0 ? "disabled" :
			  (hapd->conf->wps_state == 1 ? "not configured" :
			   "configured"));
	if (ret < 0 || ret >= end - pos)
		return pos - buf;
	pos += ret;

	if (hapd->conf->wps_state && hapd->conf->wpa &&
	    hapd->conf->ssid.wpa_passphrase) {
		ret = os_snprintf(pos, end - pos, "passphrase=%s\n",
				  hapd->conf->ssid.wpa_passphrase);
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
	}

	if (hapd->conf->wps_state && hapd->conf->wpa &&
	    hapd->conf->ssid.wpa_psk &&
	    hapd->conf->ssid.wpa_psk->group) {
		char hex[PMK_LEN * 2 + 1];
		wpa_snprintf_hex(hex, sizeof(hex),
				 hapd->conf->ssid.wpa_psk->psk, PMK_LEN);
		ret = os_snprintf(pos, end - pos, "psk=%s\n", hex);
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
	}
#endif /* CONFIG_WPS */

	if (hapd->conf->wpa && hapd->conf->wpa_key_mgmt) {
		ret = os_snprintf(pos, end - pos, "key_mgmt=");
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;

		if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_PSK) {
			ret = os_snprintf(pos, end - pos, "WPA-PSK ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		}
		if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_IEEE8021X) {
			ret = os_snprintf(pos, end - pos, "WPA-EAP ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		}
#ifdef CONFIG_IEEE80211R
		if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_FT_PSK) {
			ret = os_snprintf(pos, end - pos, "FT-PSK ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		}
		if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_FT_IEEE8021X) {
			ret = os_snprintf(pos, end - pos, "FT-EAP ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		}
#endif /* CONFIG_IEEE80211R */
#ifdef CONFIG_IEEE80211W
		if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_PSK_SHA256) {
			ret = os_snprintf(pos, end - pos, "WPA-PSK-SHA256 ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		}
		if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_IEEE8021X_SHA256) {
			ret = os_snprintf(pos, end - pos, "WPA-EAP-SHA256 ");
			if (ret < 0 || ret >= end - pos)
				return pos - buf;
			pos += ret;
		}
#endif /* CONFIG_IEEE80211W */

		ret = os_snprintf(pos, end - pos, "\n");
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
	}

	if (hapd->conf->wpa) {
		ret = os_snprintf(pos, end - pos, "group_cipher=%s\n",
				  wpa_cipher_txt(hapd->conf->wpa_group));
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
	}

	if ((hapd->conf->wpa & WPA_PROTO_RSN) && hapd->conf->rsn_pairwise) {
		ret = os_snprintf(pos, end - pos, "rsn_pairwise_cipher=");
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;

		ret = wpa_write_ciphers(pos, end, hapd->conf->rsn_pairwise,
					" ");
		if (ret < 0)
			return pos - buf;
		pos += ret;

		ret = os_snprintf(pos, end - pos, "\n");
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
	}

	if ((hapd->conf->wpa & WPA_PROTO_WPA) && hapd->conf->wpa_pairwise) {
		ret = os_snprintf(pos, end - pos, "wpa_pairwise_cipher=");
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;

		ret = wpa_write_ciphers(pos, end, hapd->conf->rsn_pairwise,
					" ");
		if (ret < 0)
			return pos - buf;
		pos += ret;

		ret = os_snprintf(pos, end - pos, "\n");
		if (ret < 0 || ret >= end - pos)
			return pos - buf;
		pos += ret;
	}

	return pos - buf;
}


static int hostapd_ctrl_iface_set(struct hostapd_data *hapd, char *cmd)
{
	char *value;
	int ret = 0;

	value = os_strchr(cmd, ' ');
	if (value == NULL)
		return -1;
	*value++ = '\0';

	wpa_printf(MSG_DEBUG, "CTRL_IFACE SET '%s'='%s'", cmd, value);
	if (0) {
#ifdef CONFIG_WPS_TESTING
	} else if (os_strcasecmp(cmd, "wps_version_number") == 0) {
		long int val;
		val = strtol(value, NULL, 0);
		if (val < 0 || val > 0xff) {
			ret = -1;
			wpa_printf(MSG_DEBUG, "WPS: Invalid "
				   "wps_version_number %ld", val);
		} else {
			wps_version_number = val;
			wpa_printf(MSG_DEBUG, "WPS: Testing - force WPS "
				   "version %u.%u",
				   (wps_version_number & 0xf0) >> 4,
				   wps_version_number & 0x0f);
			hostapd_wps_update_ie(hapd);
		}
	} else if (os_strcasecmp(cmd, "wps_testing_dummy_cred") == 0) {
		wps_testing_dummy_cred = atoi(value);
		wpa_printf(MSG_DEBUG, "WPS: Testing - dummy_cred=%d",
			   wps_testing_dummy_cred);
	} else if (os_strcasecmp(cmd, "wps_corrupt_pkhash") == 0) {
		wps_corrupt_pkhash = atoi(value);
		wpa_printf(MSG_DEBUG, "WPS: Testing - wps_corrupt_pkhash=%d",
			   wps_corrupt_pkhash);
#endif /* CONFIG_WPS_TESTING */
#ifdef CONFIG_INTERWORKING
	} else if (os_strcasecmp(cmd, "gas_frag_limit") == 0) {
		int val = atoi(value);
		if (val <= 0)
			ret = -1;
		else
			hapd->gas_frag_limit = val;
#endif /* CONFIG_INTERWORKING */
#ifdef CONFIG_TESTING_OPTIONS
	} else if (os_strcasecmp(cmd, "ext_mgmt_frame_handling") == 0) {
		hapd->ext_mgmt_frame_handling = atoi(value);
#endif /* CONFIG_TESTING_OPTIONS */
	} else if (os_strcasecmp(cmd, "default_pbc_bss") == 0) {
		struct hostapd_iface *iface = hapd->iface;
		int i;

		if (os_strcasecmp(value, "null") == 0) {
			iface->default_pbc_bss= NULL;
		} else {
			for (i = 0; i < iface->num_bss; i++) {
				if (os_strncmp(iface->bss[i]->conf->iface, value, IFNAMSIZ) == 0 &&
					iface->bss[i]->wps != NULL) {
					iface->default_pbc_bss = iface->bss[i];
					break;
				}
			}

			if (i == iface->num_bss)
				ret = -1;
		}
	} else {
		ret = hostapd_set_iface(hapd->iconf, hapd->conf, cmd, value);
	}

	return ret;
}


static int hostapd_ctrl_iface_get(struct hostapd_data *hapd, char *cmd,
				  char *buf, size_t buflen)
{
	int res;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE GET '%s'", cmd);

	if (os_strcmp(cmd, "version") == 0) {
		res = os_snprintf(buf, buflen, "%s", VERSION_STR);
		if (res < 0 || (unsigned int) res >= buflen)
			return -1;
		return res;
	} else if (os_strcmp(cmd, "default_pbc_bss") == 0) {
		struct hostapd_iface *iface = hapd->iface;

		if (iface->default_pbc_bss != NULL) {
			res = snprintf(buf, buflen, "%s", iface->default_pbc_bss->conf->iface);
		} else {
			res = snprintf(buf, buflen, "null");
		}

		if (res < 0 || (unsigned int) res >= buflen)
			return -1;
		else
			return res;
	}

	return -1;
}


static int hostapd_ctrl_iface_enable(struct hostapd_iface *iface)
{
	if (hostapd_enable_iface(iface) < 0) {
		wpa_printf(MSG_ERROR, "Enabling of interface failed");
		return -1;
	}
	return 0;
}


static int hostapd_ctrl_iface_reload(struct hostapd_iface *iface)
{
	if (hostapd_reload_iface(iface) < 0) {
		wpa_printf(MSG_ERROR, "Reloading of interface failed");
		return -1;
	}
	return 0;
}


static int hostapd_ctrl_iface_disable(struct hostapd_iface *iface)
{
	if (hostapd_disable_iface(iface) < 0) {
		wpa_printf(MSG_ERROR, "Disabling of interface failed");
		return -1;
	}
	return 0;
}


#ifdef CONFIG_TESTING_OPTIONS

static int hostapd_ctrl_iface_radar(struct hostapd_data *hapd, char *cmd)
{
	union wpa_event_data data;
	char *pos, *param;
	enum wpa_event_type event;

	wpa_printf(MSG_DEBUG, "RADAR TEST: %s", cmd);

	os_memset(&data, 0, sizeof(data));

	param = os_strchr(cmd, ' ');
	if (param == NULL)
		return -1;
	*param++ = '\0';

	if (os_strcmp(cmd, "DETECTED") == 0)
		event = EVENT_DFS_RADAR_DETECTED;
	else if (os_strcmp(cmd, "CAC-FINISHED") == 0)
		event = EVENT_DFS_CAC_FINISHED;
	else if (os_strcmp(cmd, "CAC-ABORTED") == 0)
		event = EVENT_DFS_CAC_ABORTED;
	else if (os_strcmp(cmd, "NOP-FINISHED") == 0)
		event = EVENT_DFS_NOP_FINISHED;
	else {
		wpa_printf(MSG_DEBUG, "Unsupported RADAR test command: %s",
			   cmd);
		return -1;
	}

	pos = os_strstr(param, "freq=");
	if (pos)
		data.dfs_event.freq = atoi(pos + 5);

	pos = os_strstr(param, "ht_enabled=1");
	if (pos)
		data.dfs_event.ht_enabled = 1;

	pos = os_strstr(param, "chan_offset=");
	if (pos)
		data.dfs_event.chan_offset = atoi(pos + 12);

	pos = os_strstr(param, "chan_width=");
	if (pos)
		data.dfs_event.chan_width = atoi(pos + 11);

	pos = os_strstr(param, "cf1=");
	if (pos)
		data.dfs_event.cf1 = atoi(pos + 4);

	pos = os_strstr(param, "cf2=");
	if (pos)
		data.dfs_event.cf2 = atoi(pos + 4);

	wpa_supplicant_event(hapd, event, &data);

	return 0;
}


static int hostapd_ctrl_iface_mgmt_tx(struct hostapd_data *hapd, char *cmd)
{
	size_t len;
	u8 *buf;
	int res;

	wpa_printf(MSG_DEBUG, "External MGMT TX: %s", cmd);

	len = os_strlen(cmd);
	if (len & 1)
		return -1;
	len /= 2;

	buf = os_malloc(len);
	if (buf == NULL)
		return -1;

	if (hexstr2bin(cmd, buf, len) < 0) {
		os_free(buf);
		return -1;
	}

	res = hostapd_drv_send_mlme(hapd, buf, len, 0);
	os_free(buf);
	return res;
}

#endif /* CONFIG_TESTING_OPTIONS */


static int hostapd_ctrl_iface_chan_switch(struct hostapd_data *hapd, char *pos)
{
#ifdef NEED_AP_MLME
	struct csa_settings settings;
	int ret = hostapd_parse_csa_settings(pos, &settings);

	if (ret)
		return ret;

	return hostapd_switch_channel(hapd, &settings);
#else /* NEED_AP_MLME */
	return -1;
#endif /* NEED_AP_MLME */
}


static int hostapd_ctrl_iface_mib(struct hostapd_data *hapd, char *reply,
				  int reply_size, const char *param)
{
#ifdef RADIUS_SERVER
	if (os_strcmp(param, "radius_server") == 0) {
		return radius_server_get_mib(hapd->radius_srv, reply,
					     reply_size);
	}
#endif /* RADIUS_SERVER */
	return -1;
}


static int hostapd_ctrl_iface_wps_pin_bss(struct hostapd_data *hapd, char *txt)
{
	char *uuid_txt = os_strchr(txt, ' ');
	struct hostapd_data *sel_bss;

	if (uuid_txt == NULL)
		return -1;
	*uuid_txt++ = '\0';

	sel_bss = hostapd_get_bss(hapd->iface, txt);
	if (sel_bss == NULL)
		return -1;

	return hostapd_ctrl_iface_wps_pin(sel_bss, uuid_txt);
}


static int hostapd_ctrl_iface_bss_set(struct hostapd_data *hapd, char *cmd)
{
	char *bss_name;
	char *value;
	struct hostapd_data *sel_bss;
	int ret = 0;

	bss_name = os_strchr(cmd, ' ');
	if (NULL == bss_name)
		return -1;
	*bss_name++ = '\0';

	value = os_strchr(bss_name, ' ');
	if (value == NULL)
		return -1;
	*value++ = '\0';

	if ((sel_bss = hostapd_get_bss(hapd->iface, bss_name)) == NULL)
		return -1;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE SET BSS interface[%s] '%s'='%s'", bss_name, cmd, value);
	if (os_strcasecmp(cmd, "force_broadcast_uuid") == 0) {
		int flag = !!atoi(value);

		if (sel_bss->force_broadcast_uuid != flag) {
			sel_bss->force_broadcast_uuid = flag;
			ret = hostapd_update_bss(sel_bss->iface, sel_bss->conf->iface);
		}
	} else if (os_strcasecmp(cmd, "wps_on_hidden_ssid") == 0) {
		int enable = !!atoi(value);

		if (sel_bss->qtn_wps_on_hidden_ssid != enable) {
			sel_bss->qtn_wps_on_hidden_ssid = enable;
			ret = hostapd_update_bss(sel_bss->iface, sel_bss->conf->iface);
		}
	} else if (os_strcasecmp(cmd, "ap_setup_locked") == 0) {
		int val = !!atoi(value);

		if (sel_bss->wps && sel_bss->current_wps_lockdown == WPS_LOCKDOWN_AUTO) {
			if (val != sel_bss->auto_ld.force_ap_setup_locked) {
				if (!val) {
					sel_bss->auto_ld.fail_count = 0;
				}
				sel_bss->auto_ld.force_ap_setup_locked = val;
				sel_bss->wps->ap_setup_locked = val;
				ret = wps_registrar_update_ie(sel_bss->wps->registrar);
			}
		} else
			ret = -1;
	} else if (os_strcasecmp(cmd, "auto_lockdown_fail_count") == 0) {
		int val = atoi(value);

		if (sel_bss->wps && sel_bss->current_wps_lockdown == WPS_LOCKDOWN_AUTO) {
			sel_bss->auto_ld.fail_count = val;
		}
	} else if (os_strcasecmp(cmd, "pbc_min_detect") == 0) {
		int val = atoi(value);

		if (sel_bss->pbc_detect_enhance)
			sel_bss->pbc_detect_interval = val;
	} else if (os_strcasecmp(cmd, "eapol_resp_delay_s") == 0) {
		int val = atoi(value);

		if (sel_bss->pbc_detect_enhance)
			sel_bss->eapol_resp_delay_s = val;
	} else if (os_strcasecmp(cmd, "eapol_resp_delay_us") == 0) {
		int val = atoi(value);

		if (sel_bss->pbc_detect_enhance)
			sel_bss->eapol_resp_delay_us = val;
	} else if (os_strcasecmp(cmd, "third_party_band") == 0) {
		u8 wps_third_party_band;
		if (os_strcasecmp(value, "2.4g") == 0)
			wps_third_party_band = WPS_RF_24GHZ;
		else if (os_strcasecmp(value, "5g") == 0)
			wps_third_party_band = WPS_RF_50GHZ;
		else
			wps_third_party_band = 0;

		if (sel_bss->wps_third_party_band != wps_third_party_band) {
			sel_bss->wps_third_party_band = wps_third_party_band;
			ret = hostapd_update_bss(sel_bss->iface, sel_bss->conf->iface);
		}
	} else {
		ret = -1;
	}

	return ret;
}

static int hostapd_ctrl_iface_bss_get(struct hostapd_data *hapd, char *cmd, char *reply, int reply_size)
{
	char *bss_name;
	struct hostapd_data *sel_bss;
	int ret = 0;

	bss_name = os_strchr(cmd, ' ');
	if (bss_name == NULL)
		return -1;
	*bss_name++ = '\0';

	if ((sel_bss = hostapd_get_bss(hapd->iface, bss_name)) == NULL)
		return -1;

	if (sel_bss->wps && os_strcmp(cmd, "uuid") == 0) {
		/* This is a 16-size hexs array, here we convert them to string in UUID format*/
		ret = uuid_bin2str(sel_bss->wps->uuid, reply, 41);
		if (!ret)
			ret = strlen(reply); /*get out the uuid's length*/
		if (ret < 0 || (unsigned int) ret >= reply_size)
			return -1;

		return ret;
	} else if (sel_bss->wps && os_strcmp(cmd, "device_name") == 0) {
		if (sel_bss->wps->dev.device_name) {
			ret = os_snprintf(reply, reply_size, "%s", sel_bss->wps->dev.device_name);
		} else {
			ret = -1;
		}
		if (ret < 0 || (unsigned int) ret >= reply_size)
			return -1;

		return ret;
	} else if (sel_bss->wps && os_strcmp(cmd, "os_version") == 0) {
		u8 os_buf[4] = {0};

		WPA_PUT_BE32(os_buf, sel_bss->wps->dev.os_version);
		ret = sprintf(reply, "%02x%02x%02x%02x", os_buf[0], os_buf[1], os_buf[2], os_buf[3]);

		if (ret < 0 || (unsigned int) ret > reply_size)
			return -1;

		return ret;
	} else if (sel_bss->wps && os_strcmp(cmd, "config_methods") == 0) {
		ret = os_snprintf(reply, reply_size,
#ifdef CONFIG_WPS2
				"%s%s%s%s"
#else
				"%s%s"
#endif
				"%s%s%s%s%s%s%s",

#ifdef CONFIG_WPS2
			    ((sel_bss->wps->config_methods & WPS_CONFIG_VIRT_DISPLAY)
					== WPS_CONFIG_VIRT_DISPLAY) ? " virtual_display" : "",
			    ((sel_bss->wps->config_methods & WPS_CONFIG_PHY_DISPLAY)
					== WPS_CONFIG_PHY_DISPLAY) ? " physical_display" : "",
			    ((sel_bss->wps->config_methods & WPS_CONFIG_VIRT_PUSHBUTTON)
					== WPS_CONFIG_VIRT_PUSHBUTTON) ? " virtual_push_button" : "",
			    ((sel_bss->wps->config_methods & WPS_CONFIG_PHY_PUSHBUTTON)
					== WPS_CONFIG_PHY_PUSHBUTTON) ? " physical_push_button" : "",
#else
			    sel_bss->wps->config_methods & WPS_CONFIG_DISPLAY ? " display" : "",
			    sel_bss->wps->config_methods & WPS_CONFIG_PUSHBUTTON ? " push_button" : "",
#endif
			    sel_bss->wps->config_methods & WPS_CONFIG_USBA ? " usba" : "",
			    sel_bss->wps->config_methods & WPS_CONFIG_ETHERNET ? " ethernet" : "",
			    sel_bss->wps->config_methods & WPS_CONFIG_LABEL ? " label" : "",
			    sel_bss->wps->config_methods& WPS_CONFIG_EXT_NFC_TOKEN ? " ext_nfc_token" : "",
			    sel_bss->wps->config_methods & WPS_CONFIG_INT_NFC_TOKEN ? " int_nfc_token" : "",
			    sel_bss->wps->config_methods & WPS_CONFIG_NFC_INTERFACE ? " nfc_interface" : "",
			    sel_bss->wps->config_methods & WPS_CONFIG_KEYPAD ? " keypad" : ""

		);

		if (ret < 0 || (unsigned int) ret >= reply_size)
			return -1;
		return ret;
	} else if (sel_bss->wps && os_strcmp(cmd, "ap_setup_locked") == 0) {
		ret = sprintf(reply, "%d", sel_bss->wps->ap_setup_locked);
		if (ret < 0 || (unsigned int) ret >= reply_size)
			return -1;
		return ret;
	} else if (os_strcasecmp(cmd, "force_broadcast_uuid") == 0) {
		ret = snprintf(reply, reply_size, "%s",
				sel_bss->force_broadcast_uuid ? "on" : "off");
		if (ret < 0 || ret >= reply_size)
			ret = -1;
	} else if (os_strcasecmp(cmd, "wps_on_hidden_ssid") == 0) {
		ret = snprintf(reply, reply_size, "%s",
				sel_bss->qtn_wps_on_hidden_ssid ? "on" : "off");
		if (ret < 0 || ret >= reply_size)
			ret = -1;
	} else if (sel_bss->wps && os_strcasecmp(cmd, "wps_vendor_spec") == 0) {
		switch (sel_bss->wps->vendor) {
		case WPS_VENDOR_NONE:
			ret = snprintf(reply, reply_size, "NO Vendor");
			break;
		case WPS_VENDOR_NETGEAR:
			ret = snprintf(reply, reply_size, "Netgear");
			break;
		default:
			ret = snprintf(reply, reply_size, "Unknown Vendor");
			break;
		}
		if (ret < 0 || ret >= reply_size)
			ret = -1;
	} else if (os_strcasecmp(cmd, "ap_pin_fail_method") == 0) {
		if (sel_bss->current_wps_lockdown == WPS_LOCKDOWN_DEFAULT)
			ret = snprintf(reply, reply_size, "default");
		else if (sel_bss->current_wps_lockdown == WPS_LOCKDOWN_AUTO)
			ret = snprintf(reply, reply_size, "auto_lockdown");
		else
			ret = snprintf(reply, reply_size, "unknown");

		if (ret < 0 || ret >= reply_size)
			ret = -1;
	} else if (os_strcasecmp(cmd,"auto_lockdown_max_retry") == 0) {
		if (sel_bss->current_wps_lockdown == WPS_LOCKDOWN_AUTO) {
			ret = snprintf(reply, reply_size, "%d", sel_bss->auto_ld.max_fail_retry);
			if (ret < 0 || ret >= reply_size)
				ret = -1;
		} else {
			ret = -1;
		}
	} else if (os_strcasecmp(cmd,"auto_lockdown_fail_count") == 0) {
		if (sel_bss->current_wps_lockdown == WPS_LOCKDOWN_AUTO) {
			ret = snprintf(reply, reply_size, "%d",
					sel_bss->auto_ld.fail_count);
			if (ret < 0 || ret >= reply_size)
				ret = -1;
		} else {
			ret = -1;
		}
	} else if (os_strcasecmp(cmd, "last_wps_client") == 0) {
		if (sel_bss->wps != NULL) {
			ret = snprintf(reply, reply_size,  MACSTR "-%s",
				MAC2STR(sel_bss->wps->last_wps_client),
				sel_bss->wps->last_wps_client_wps_type);
			if (ret < 0 || ret >= reply_size)
				ret = -1;
		} else {
			ret = -1;
		}
	} else if (os_strcasecmp(cmd, "last_wps_client_devname") == 0) {
		if (sel_bss->wps != NULL &&
			!is_zero_ether_addr(sel_bss->wps->last_wps_client)) {
			ret = wps_registrar_get_peer_devname(sel_bss->wps->registrar,
					sel_bss->wps->last_wps_client, reply, reply_size);
		} else {
			ret = -1;
		}
	} else if (os_strcasecmp(cmd, "pbc_detect_enhance") == 0) {
		ret = snprintf(reply, reply_size, "%d\n", sel_bss->pbc_detect_enhance);
		if (ret < 0 || ret >= reply_size)
			ret = -1;
	} else if (os_strcasecmp(cmd, "pbc_detect_interval") == 0) {
		ret = snprintf(reply, reply_size, "%d\n", sel_bss->pbc_detect_interval);
		if (ret < 0 || ret >= reply_size)
			ret = -1;
	} else if (os_strcasecmp(cmd, "eapol_resp_delay_s") == 0) {
		ret = snprintf(reply, reply_size, "%d\n", sel_bss->eapol_resp_delay_s);
		if (ret < 0 || ret >= reply_size)
			ret = -1;
	} else if (os_strcasecmp(cmd, "eapol_resp_delay_us") == 0) {
		ret = snprintf(reply, reply_size, "%d\n", sel_bss->eapol_resp_delay_us);
		if (ret < 0 || ret >= reply_size)
			ret = -1;
	} else if (os_strcasecmp(cmd, "third_party_band") == 0) {
		ret = snprintf(reply, reply_size, "%s",
				sel_bss->wps_third_party_band == WPS_RF_24GHZ ? "2.4g" : (sel_bss->wps_third_party_band == WPS_RF_50GHZ ? "5g" : "none"));
		if (ret < 0 || ret >= reply_size)
			ret = -1;
	} else {
		ret = -1;
	}

	return ret;
}

#ifdef CONFIG_WPS
static int hostapd_ctrl_iface_get_wps_configured_state(struct hostapd_data *hapd,
					 char *buf, size_t buflen)
{
	int ret;
	char *pos, *end;
	pos = buf;
	end = buf + buflen;

	ret = os_snprintf(pos, end - pos, "%s\n",
			  hapd->conf->wps_state == 0 ? "disabled" :
			  (hapd->conf->wps_state == 1 ? "not configured" :
			   "configured"));
	if (ret < 0 || ret >= end - pos)
		return pos - buf;
	pos += ret;

	return pos - buf;
}

static int hostapd_ctrl_iface_wps_timeout(struct hostapd_data *hapd, char *txt, char *reply, int size)
{
#define WPS_TIMEOUT_MAX		600
#define WPS_TIMEOUT_MIN		(WPS_PBC_WALK_TIME)
	int timeout_val;
	char tmp_buf[256];

	timeout_val = atoi(txt);
	if ((timeout_val < WPS_TIMEOUT_MIN) || (timeout_val > WPS_TIMEOUT_MAX)) {
		os_snprintf(tmp_buf, sizeof(tmp_buf), "FAIL: wps timeout should be set to [%d,%d]\n",
				WPS_TIMEOUT_MIN, WPS_TIMEOUT_MAX);
	} else {
		qtn_wps_pbc_timeout = timeout_val;
		os_snprintf(tmp_buf, sizeof(tmp_buf), "OK\n");
	}
	os_strlcpy(reply, tmp_buf, size);

	return (strlen(tmp_buf) + 1);
#undef WPS_TIMEOUT_MAX
#undef WPS_TIMEOUT_MIN
}

static int hostapd_ctrl_iface_pbc_in_srcm(struct hostapd_data *hapd, char *txt)
{
	struct hostapd_iface *iface;
	struct hostapd_data * hapd_data;
	int flag;
	int i;

	if (hapd == NULL || txt == NULL)
		return -1;

	iface = hapd->iface;
	flag = atoi(txt);
	if (flag != hapd->iconf->pbc_in_srcm) {
		hapd->iconf->pbc_in_srcm = flag;

		for (i = 0; i < iface->num_bss; i++) {
			hapd_data = iface->bss[i];
			if (hapd_data && (hapd_data->wps))
				hostapd_update_wps(hapd_data);
		}
	}

	return 0;
}
#endif

#ifdef CONFIG_WPS_UPNP
static int hostapd_ctrl_iface_wps_upnp_enable(struct hostapd_data *hapd, char *option,
		  char *buf, size_t buflen)
{
	int enable;

	if (hapd->wps == NULL)
		return -1;

	enable = atoi(option);
	if ((strlen(option) != 1) || (isdigit(option[0]) == 0) || ((enable != 0) && (enable != 1))) {
		return -1;
	}

	if (enable && hapd->wps_upnp == NULL) {
		if (hapd->conf->upnp_iface == NULL) {
			hapd->conf->upnp_iface = os_strdup("br0");
		}
		hostapd_wps_upnp_init(hapd, hapd->wps);
	}

	if (!enable && hapd->wps_upnp != NULL) {
		hostapd_wps_upnp_deinit(hapd);
	}

	return 0;
}

static int hostapd_ctrl_iface_wps_upnp_status(struct hostapd_data *hapd, char *buf, size_t buflen)
{
	char *response[] = {"0\n", "1\n"};
	int select_resp;

	if (hapd->wps_upnp != NULL)
		select_resp = 1;
	else
		select_resp = 0;
	os_memcpy(buf, response[select_resp], strlen(response[select_resp]) + 1);

	return strlen(response[select_resp]) + 1;
}
#endif

#define WPA_HANDSHAKING_STR		"WPA_HANDSHAKING"
#define NO_WPA_HANDSHAKING_STR		"NO_WPA_HANDSHAKING"
#define WPA_SUCCESS_STR			"WPA_SUCCESS"
#define WPA_INITIAL_STR			"WPA_INITIAL"
#define MAC_ADDR_STR_LEN		17
static int hostapd_ctrl_iface_wpa_status(struct hostapd_data *hapd, char *mac_addr,
		  char *buf, size_t buflen)
{
	struct sta_info *sta;
	u8 addr[ETH_ALEN], zero_addr[ETH_ALEN] = {0};
	int ap_wpa_handshaking = 0;
	int sta_exist = 0;
	int res = -1;

	if (strlen(mac_addr) != MAC_ADDR_STR_LEN || hwaddr_aton(mac_addr, addr))
		return res;

	for (sta = hapd->sta_list; sta; sta = sta->next) {
		if (os_memcmp(sta->addr, addr, sizeof(addr)) == 0) {
			if ((sta->flags & WLAN_STA_AUTH) && !(sta->flags & WLAN_STA_AUTHORIZED)) {
				res = sprintf(buf, "%s", WPA_HANDSHAKING_STR);
			} else if (sta->flags & WLAN_STA_AUTHORIZED) {
				res = sprintf(buf, "%s", WPA_SUCCESS_STR);
			} else if (!(sta->flags & WLAN_STA_AUTH)) {
				res = sprintf(buf, "%s", WPA_INITIAL_STR);
			}
			sta_exist = 1;
			break;
		} else if (os_memcmp(addr, zero_addr, sizeof(addr)) ==  0) {
			if ((sta->flags & WLAN_STA_AUTH) && !(sta->flags & WLAN_STA_AUTHORIZED)) {
				ap_wpa_handshaking++;
				wpa_printf(MSG_DEBUG, "STA " MACSTR " handshaking with the AP",
					   MAC2STR(sta->addr));
			}
		}
	}

	/*
	 * Mac addr 00:00:00:00:00:00 here is used to check whether a WPA handshaking exists
	 * in the AP without specific address
	 */
	if (os_memcmp(addr, zero_addr, sizeof(addr)) ==  0) {
		if (ap_wpa_handshaking) {
			res = sprintf(buf, "%s", WPA_HANDSHAKING_STR);
		} else {
			res = sprintf(buf, "%s", NO_WPA_HANDSHAKING_STR);
		}

		sta_exist = 1;

	}

	if (sta_exist == 0) {
		res = sprintf(buf, "%s", "station not found");
	}

	return res;
}

static int hostapd_ctrl_iface_get_psk_auth_failure(struct hostapd_data *hapd,
		char *ifname, char *buf, size_t buflen)
{
	int res;
	int i;
	struct hostapd_data *hapd2 = NULL;
	struct wpa_authenticator *wpa_auth = NULL;
	struct hostapd_bss_config *bss_config = NULL;
	struct hostapd_iface *hapd_iface = hapd->iface;

	if (ifname == NULL || buf == NULL)
		return -1;

	for (i = 0; i < hapd_iface->num_bss; i++) {
		bss_config = hapd_iface->bss[i]->conf;
		if (os_strcmp(ifname, bss_config->iface) == 0) {
			hapd2 = hapd_iface->bss[i];
			break;
		}
	}

	if (hapd2 == NULL)
		return -1;
	if ((wpa_auth = hapd2->wpa_auth) == NULL)
		return -1;

	res = os_snprintf(buf, buflen, "%u", wpa_auth->dot11RSNA4WayHandshakeFailures);
	if (res < 0 || (unsigned int)res >= buflen)
		return -1;

	return res;
}

/* add quantenna commands here */
static int hostapd_qtn_ctrl_iface_process(struct hostapd_data *hapd,
		char *buf, int len, char *reply, int reply_size)
{
	int reply_len;

	/* default response string "OK" */
	reply_len = 3;

	if (os_strcmp(buf, "RECONFIGURE") == 0) {
		if (hostapd_reload_config(hapd->iface))
			reply_len = -1;
	} else if (os_strncmp(buf, "UPDATE_BSSCONFIG ", 17) == 0) {
		if (hostapd_update_bss(hapd->iface, buf + 17))
			reply_len = -1;
	} else if (os_strncmp(buf, "CREATE_BSSCONFIG ", 17) == 0) {
		if (hostapd_add_bss(hapd->iface, buf + 17))
			reply_len = -1;
	} else if (os_strncmp(buf, "REMOVE_BSSCONFIG ", 17) == 0) {
		if (hostapd_del_bss(hapd->iface, buf + 17))
			reply_len = -1;
#ifdef CONFIG_WPS
	} else if (os_strncmp(buf, "WPS_PBC ", 8) == 0) {
		struct hostapd_data *sel_hapd;

		sel_hapd = hostapd_get_bss(hapd->iface, buf + 8);
		if (sel_hapd == NULL)
			reply_len = -1;
		else {
			int ret;

			ret = hostapd_wps_button_pushed_interface(sel_hapd);
			if (ret == -2) {
				reply_len = os_snprintf(reply, reply_size, "WPS overlap");
				reply_len++;
			} else if (ret != 0) {
				reply_len = -1;
			}
		}
	} else if (os_strcmp(buf, "WPS_STATUS") == 0) {
		reply_len = qtn_ctrl_iface_wps_get_status(hapd, reply, reply_size);
	} else if (os_strncmp(buf, "WPS_STATUS ", 11) == 0) {
		struct hostapd_data *sel_bss;

		sel_bss = qtn_hostapd_find_bss(hapd->iface, buf + strlen("WPS_STATUS "));
		if (sel_bss == NULL)
			reply_len = -1;
		else
			reply_len = qtn_ctrl_iface_wps_get_status(sel_bss,
							reply, reply_size);
	} else if (os_strncmp(buf, "WPS_PIN_BSS ", 12) == 0) {
		if (hostapd_ctrl_iface_wps_pin_bss(hapd, buf + 12))
			reply_len = -1;
	} else if (os_strncmp(buf, "WPS_AP_PIN_BSS ", 15) == 0) {
		char *bss_name = buf + 15;
		char *param;
		struct hostapd_data *sel_bss;

		param = os_strchr(bss_name, ' ');
		if (param == NULL) {
			reply_len = -1;
		} else {
			*param++ = '\0';
			sel_bss = hostapd_get_bss(hapd->iface, bss_name);
			if (sel_bss == NULL) {
				reply_len = -1;
			} else {
				reply_len = hostapd_ctrl_iface_wps_ap_pin(sel_bss,
						param, reply, reply_size);
			}
		}
	} else if (os_strcmp(buf, "WPS_CONFIGURED_STATE") == 0) {
		reply_len = hostapd_ctrl_iface_get_wps_configured_state(hapd,
							reply, reply_size);
	} else if (os_strncmp(buf, "WPS_CONFIGURED_STATE ", 21) == 0) {
		struct hostapd_data *sel_bss;

		sel_bss = hostapd_get_bss(hapd->iface, buf + 21);
		if (sel_bss == NULL)
			reply_len = -1;
		else
			reply_len = hostapd_ctrl_iface_get_wps_configured_state(sel_bss,
							reply, reply_size);
	} else if (os_strncmp(buf, "WPS_TIMEOUT ", strlen("WPS_TIMEOUT ")) == 0) {
		reply_len = hostapd_ctrl_iface_wps_timeout(hapd, buf + strlen("WPS_TIMEOUT "),
								reply, reply_size);
#ifdef CONFIG_WPS_UPNP
	} else if (os_strncmp(buf, "WPS_UPNP_ENABLE ", strlen("WPS_UPNP_ENABLE ")) == 0) {
		reply_len = hostapd_ctrl_iface_wps_upnp_enable(hapd, buf + strlen("WPS_UPNP_ENABLE "),
								reply, reply_size);
	} else if (os_strncmp(buf, "WPS_UPNP_STATUS", strlen("WPS_UPNP_STATUS")) == 0) {
		reply_len = hostapd_ctrl_iface_wps_upnp_status(hapd, reply, reply_size);
#endif
	} else if (os_strncmp(buf, "WPS_CANCEL ", 11) == 0) {
		struct hostapd_data *sel_bss;

		sel_bss = hostapd_get_bss(hapd->iface, buf + 11);
		if (sel_bss == NULL)
			reply_len = -1;
		else
			reply_len = hostapd_wps_cancel(sel_bss) ? -1 : reply_len;
	} else if (os_strncmp(buf, "WPS_PBC_IN_SRCM ", strlen("WPS_PBC_IN_SRCM ")) == 0) {
		if (hostapd_ctrl_iface_pbc_in_srcm(hapd, buf + strlen("WPS_PBC_IN_SRCM ")) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "GET_PBC_IN_SRCM", strlen("GET_PBC_IN_SRCM")) == 0) {
		reply_len = os_snprintf(reply, reply_size, "%d", hapd->iconf->pbc_in_srcm);
		if (reply_len < 0 || (unsigned int) reply_len >= reply_size)
			reply_len = -1;
#endif
	} else if (os_strncmp(buf, "BSS_SET ", 8) == 0) {
		if (hostapd_ctrl_iface_bss_set(hapd, buf + 8) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "BSS_GET ", 8) == 0) {
		reply_len = hostapd_ctrl_iface_bss_get(hapd, buf + 8, reply,
						reply_size);
	} else if (os_strncmp(buf, "GET_PSK_AUTH_FAILURE ",
		strlen("GET_PSK_AUTH_FAILURE ")) == 0) {
		reply_len = hostapd_ctrl_iface_get_psk_auth_failure(hapd,
				buf + strlen("GET_PSK_AUTH_FAILURE "), reply, reply_size);
	} else if (os_strncmp(buf, "STATUS ", strlen("STATUS ")) == 0) {
		reply_len = hostapd_ctrl_iface_wpa_status(hapd, buf + strlen("STATUS "), reply,
								reply_size);
	} else if (os_strncmp(buf, "LOG_LEVEL", 9) == 0) {
		reply_len = hostapd_ctrl_iface_log_level(hapd, buf + 9, reply, reply_size);
	} else if (os_strncmp(buf, "LOGGER_LEVEL", 12) == 0) {
		reply_len = hostapd_ctrl_iface_logger_level(hapd, buf + 12, reply, reply_size);
	} else {
		os_memcpy(reply, "UNKNOWN COMMAND\n", 16);
		reply_len = 16;
	}

	return reply_len;
}


static void hostapd_ctrl_iface_receive(int sock, void *eloop_ctx,
				       void *sock_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	char buf[4096];
	int res;
	struct sockaddr_un from;
	socklen_t fromlen = sizeof(from);
	char *reply;
	const int reply_size = 4096;
	int reply_len;
	int level = MSG_DEBUG;

	res = recvfrom(sock, buf, sizeof(buf) - 1, 0,
		       (struct sockaddr *) &from, &fromlen);
	if (res < 0) {
		perror("recvfrom(ctrl_iface)");
		return;
	}
	buf[res] = '\0';
	if (os_strcmp(buf, "PING") == 0)
		level = MSG_EXCESSIVE;
	wpa_hexdump_ascii(level, "RX ctrl_iface", (u8 *) buf, res);

	reply = os_malloc(reply_size);
	if (reply == NULL) {
		sendto(sock, "FAIL\n", 5, 0, (struct sockaddr *) &from,
		       fromlen);
		return;
	}

	os_memcpy(reply, "OK\n", 3);
	reply_len = 3;

	if (os_strcmp(buf, "PING") == 0) {
		os_memcpy(reply, "PONG\n", 5);
		reply_len = 5;
	} else if (os_strncmp(buf, "RELOG", 5) == 0) {
		if (wpa_debug_reopen_file() < 0)
			reply_len = -1;
	} else if (os_strcmp(buf, "STATUS") == 0) {
		reply_len = hostapd_ctrl_iface_status(hapd, reply,
						      reply_size);
	} else if (os_strcmp(buf, "STATUS-DRIVER") == 0) {
		reply_len = hostapd_drv_status(hapd, reply, reply_size);
	} else if (os_strcmp(buf, "MIB") == 0) {
		reply_len = ieee802_11_get_mib(hapd, reply, reply_size);
		if (reply_len >= 0) {
			res = wpa_get_mib(hapd->wpa_auth, reply + reply_len,
					  reply_size - reply_len);
			if (res < 0)
				reply_len = -1;
			else
				reply_len += res;
		}
		if (reply_len >= 0) {
			res = ieee802_1x_get_mib(hapd, reply + reply_len,
						 reply_size - reply_len);
			if (res < 0)
				reply_len = -1;
			else
				reply_len += res;
		}
#ifndef CONFIG_NO_RADIUS
		if (reply_len >= 0) {
			res = radius_client_get_mib(hapd->radius,
						    reply + reply_len,
						    reply_size - reply_len);
			if (res < 0)
				reply_len = -1;
			else
				reply_len += res;
		}
#endif /* CONFIG_NO_RADIUS */
	} else if (os_strncmp(buf, "MIB ", 4) == 0) {
		reply_len = hostapd_ctrl_iface_mib(hapd, reply, reply_size,
						   buf + 4);
	} else if (os_strcmp(buf, "STA-FIRST") == 0) {
		reply_len = hostapd_ctrl_iface_sta_first(hapd, reply,
							 reply_size);
	} else if (os_strncmp(buf, "STA ", 4) == 0) {
		reply_len = hostapd_ctrl_iface_sta(hapd, buf + 4, reply,
						   reply_size);
	} else if (os_strncmp(buf, "STA-NEXT ", 9) == 0) {
		reply_len = hostapd_ctrl_iface_sta_next(hapd, buf + 9, reply,
							reply_size);
	} else if (os_strcmp(buf, "ATTACH") == 0) {
		if (hostapd_ctrl_iface_attach(hapd, &from, fromlen))
			reply_len = -1;
	} else if (os_strcmp(buf, "DETACH") == 0) {
		if (hostapd_ctrl_iface_detach(hapd, &from, fromlen))
			reply_len = -1;
	} else if (os_strncmp(buf, "LEVEL ", 6) == 0) {
		if (hostapd_ctrl_iface_level(hapd, &from, fromlen,
						    buf + 6))
			reply_len = -1;
	} else if (os_strncmp(buf, "NEW_STA ", 8) == 0) {
		if (hostapd_ctrl_iface_new_sta(hapd, buf + 8))
			reply_len = -1;
	} else if (os_strncmp(buf, "DEAUTHENTICATE ", 15) == 0) {
		if (hostapd_ctrl_iface_deauthenticate(hapd, buf + 15))
			reply_len = -1;
	} else if (os_strncmp(buf, "DISASSOCIATE ", 13) == 0) {
		if (hostapd_ctrl_iface_disassociate(hapd, buf + 13))
			reply_len = -1;
#ifdef CONFIG_IEEE80211W
#ifdef NEED_AP_MLME
	} else if (os_strncmp(buf, "SA_QUERY ", 9) == 0) {
		if (hostapd_ctrl_iface_sa_query(hapd, buf + 9))
			reply_len = -1;
#endif /* NEED_AP_MLME */
#endif /* CONFIG_IEEE80211W */
#ifdef CONFIG_WPS
	} else if (os_strncmp(buf, "WPS_PIN ", 8) == 0) {
		if (hostapd_ctrl_iface_wps_pin(hapd, buf + 8))
			reply_len = -1;
	} else if (os_strncmp(buf, "WPS_CHECK_PIN ", 14) == 0) {
		reply_len = hostapd_ctrl_iface_wps_check_pin(
			hapd, buf + 14, reply, reply_size);
	} else if (os_strcmp(buf, "WPS_PBC") == 0) {
		if (hapd->iface->default_pbc_bss) {
			if (hostapd_wps_button_pushed(hapd->iface->default_pbc_bss, NULL))
				reply_len = -1;
		} else {
			struct hostapd_data *hapd2;
			int i;
			int ret;

			for (i = 0; i < hapd->iface->num_bss; i++) {
				hapd2 = hapd->iface->bss[i];
				if (hapd2->wps != NULL)
					ret |= hostapd_wps_button_pushed_interface(hapd2);
			}
			if (ret)
				reply_len = -1;
		}
	} else if (os_strcmp(buf, "WPS_CANCEL") == 0) {
		if (hostapd_wps_cancel(hapd))
			reply_len = -1;
	} else if (os_strncmp(buf, "WPS_AP_PIN ", 11) == 0) {
		reply_len = hostapd_ctrl_iface_wps_ap_pin(hapd, buf + 11,
							  reply, reply_size);
	} else if (os_strncmp(buf, "WPS_CONFIG ", 11) == 0) {
		if (hostapd_ctrl_iface_wps_config(hapd, buf + 11) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "WPS_GET_STATUS", 13) == 0) {
		reply_len = hostapd_ctrl_iface_wps_get_status(hapd, reply,
							      reply_size);
#ifdef CONFIG_WPS_NFC
	} else if (os_strncmp(buf, "WPS_NFC_TAG_READ ", 17) == 0) {
		if (hostapd_ctrl_iface_wps_nfc_tag_read(hapd, buf + 17))
			reply_len = -1;
	} else if (os_strncmp(buf, "WPS_NFC_CONFIG_TOKEN ", 21) == 0) {
		reply_len = hostapd_ctrl_iface_wps_nfc_config_token(
			hapd, buf + 21, reply, reply_size);
	} else if (os_strncmp(buf, "WPS_NFC_TOKEN ", 14) == 0) {
		reply_len = hostapd_ctrl_iface_wps_nfc_token(
			hapd, buf + 14, reply, reply_size);
	} else if (os_strncmp(buf, "NFC_GET_HANDOVER_SEL ", 21) == 0) {
		reply_len = hostapd_ctrl_iface_nfc_get_handover_sel(
			hapd, buf + 21, reply, reply_size);
	} else if (os_strncmp(buf, "NFC_REPORT_HANDOVER ", 20) == 0) {
		if (hostapd_ctrl_iface_nfc_report_handover(hapd, buf + 20))
			reply_len = -1;
#endif /* CONFIG_WPS_NFC */
#endif /* CONFIG_WPS */
#ifdef CONFIG_INTERWORKING
	} else if (os_strncmp(buf, "SET_QOS_MAP_SET ", 16) == 0) {
		if (hostapd_ctrl_iface_set_qos_map_set(hapd, buf + 16))
			reply_len = -1;
	} else if (os_strncmp(buf, "SEND_QOS_MAP_CONF ", 18) == 0) {
		if (hostapd_ctrl_iface_send_qos_map_conf(hapd, buf + 18))
			reply_len = -1;
#endif /* CONFIG_INTERWORKING */
#ifdef CONFIG_HS20
	} else if (os_strncmp(buf, "HS20_WNM_NOTIF ", 15) == 0) {
		if (hostapd_ctrl_iface_hs20_wnm_notif(hapd, buf + 15))
			reply_len = -1;
	} else if (os_strncmp(buf, "HS20_DEAUTH_REQ ", 16) == 0) {
		if (hostapd_ctrl_iface_hs20_deauth_req(hapd, buf + 16))
			reply_len = -1;
#endif /* CONFIG_HS20 */
#ifdef CONFIG_WNM
	} else if (os_strncmp(buf, "DISASSOC_IMMINENT ", 18) == 0) {
		if (hostapd_ctrl_iface_disassoc_imminent(hapd, buf + 18))
			reply_len = -1;
	} else if (os_strncmp(buf, "ESS_DISASSOC ", 13) == 0) {
		if (hostapd_ctrl_iface_ess_disassoc(hapd, buf + 13))
			reply_len = -1;
#endif /* CONFIG_WNM */
	} else if (os_strcmp(buf, "GET_CONFIG") == 0) {
		reply_len = hostapd_ctrl_iface_get_config(hapd, reply,
							  reply_size);
	} else if (os_strncmp(buf, "SET ", 4) == 0) {
		if (hostapd_ctrl_iface_set(hapd, buf + 4))
			reply_len = -1;
	} else if (os_strncmp(buf, "GET ", 4) == 0) {
		reply_len = hostapd_ctrl_iface_get(hapd, buf + 4, reply,
						   reply_size);
	} else if (os_strncmp(buf, "ENABLE", 6) == 0) {
		if (hostapd_ctrl_iface_enable(hapd->iface))
			reply_len = -1;
	} else if (os_strncmp(buf, "RELOAD", 6) == 0) {
		if (hostapd_ctrl_iface_reload(hapd->iface))
			reply_len = -1;
	} else if (os_strncmp(buf, "DISABLE", 7) == 0) {
		if (hostapd_ctrl_iface_disable(hapd->iface))
			reply_len = -1;
#ifdef CONFIG_TESTING_OPTIONS
	} else if (os_strncmp(buf, "RADAR ", 6) == 0) {
		if (hostapd_ctrl_iface_radar(hapd, buf + 6))
			reply_len = -1;
	} else if (os_strncmp(buf, "MGMT_TX ", 8) == 0) {
		if (hostapd_ctrl_iface_mgmt_tx(hapd, buf + 8))
			reply_len = -1;
#endif /* CONFIG_TESTING_OPTIONS */
	} else if (os_strncmp(buf, "CHAN_SWITCH ", 12) == 0) {
		if (hostapd_ctrl_iface_chan_switch(hapd, buf + 12))
			reply_len = -1;
        } else if (os_strncmp(buf, "NON_WPS_PP_ENABLE ", strlen("NON_WPS_PP_ENABLE ")) == 0) {
		if (qtn_non_wps_pp_enable(hapd, buf + strlen("NON_WPS_PP_ENABLE ")) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "NON_WPS_PP_STATUS ", strlen("NON_WPS_PP_STATUS ")) == 0) {
                reply_len = qtn_non_wps_pp_status(hapd, buf + strlen("NON_WPS_PP_STATUS "), reply, reply_size);
	} else {
		reply_len = hostapd_qtn_ctrl_iface_process(hapd, buf, res, reply, reply_size);
	}

	if (reply_len < 0) {
		os_memcpy(reply, "FAIL\n", 5);
		reply_len = 5;
	}

	sendto(sock, reply, reply_len, 0, (struct sockaddr *) &from, fromlen);
	os_free(reply);
}


static char * hostapd_ctrl_iface_path(struct hostapd_data *hapd)
{
	char *buf;
	size_t len;

	if (hapd->conf->ctrl_interface == NULL)
		return NULL;

	len = os_strlen(hapd->conf->ctrl_interface) +
		os_strlen(hapd->conf->iface) + 2;
	buf = os_malloc(len);
	if (buf == NULL)
		return NULL;

	os_snprintf(buf, len, "%s/%s",
		    hapd->conf->ctrl_interface, hapd->conf->iface);
	buf[len - 1] = '\0';
	return buf;
}


static void hostapd_ctrl_iface_msg_cb(void *ctx, int level, int global,
				      const char *txt, size_t len)
{
	struct hostapd_data *hapd = ctx;
	if (hapd == NULL)
		return;
	hostapd_ctrl_iface_send(hapd, level, txt, len);
}


int hostapd_ctrl_iface_init(struct hostapd_data *hapd)
{
	struct sockaddr_un addr;
	int s = -1;
	char *fname = NULL;

	if (!hapd->primary_interface)
		return 0;

	if (hapd->ctrl_sock > -1) {
		wpa_printf(MSG_DEBUG, "ctrl_iface already exists!");
		return 0;
	}

	if (hapd->conf->ctrl_interface == NULL)
		return 0;

	if (mkdir(hapd->conf->ctrl_interface, S_IRWXU | S_IRWXG) < 0) {
		if (errno == EEXIST) {
			wpa_printf(MSG_DEBUG, "Using existing control "
				   "interface directory.");
		} else {
			perror("mkdir[ctrl_interface]");
			goto fail;
		}
	}

	if (hapd->conf->ctrl_interface_gid_set &&
	    chown(hapd->conf->ctrl_interface, -1,
		  hapd->conf->ctrl_interface_gid) < 0) {
		perror("chown[ctrl_interface]");
		return -1;
	}

	if (!hapd->conf->ctrl_interface_gid_set &&
	    hapd->iface->interfaces->ctrl_iface_group &&
	    chown(hapd->conf->ctrl_interface, -1,
		  hapd->iface->interfaces->ctrl_iface_group) < 0) {
		perror("chown[ctrl_interface]");
		return -1;
	}

#ifdef ANDROID
	/*
	 * Android is using umask 0077 which would leave the control interface
	 * directory without group access. This breaks things since Wi-Fi
	 * framework assumes that this directory can be accessed by other
	 * applications in the wifi group. Fix this by adding group access even
	 * if umask value would prevent this.
	 */
	if (chmod(hapd->conf->ctrl_interface, S_IRWXU | S_IRWXG) < 0) {
		wpa_printf(MSG_ERROR, "CTRL: Could not chmod directory: %s",
			   strerror(errno));
		/* Try to continue anyway */
	}
#endif /* ANDROID */

	if (os_strlen(hapd->conf->ctrl_interface) + 1 +
	    os_strlen(hapd->conf->iface) >= sizeof(addr.sun_path))
		goto fail;

	s = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("socket(PF_UNIX)");
		goto fail;
	}

	os_memset(&addr, 0, sizeof(addr));
#ifdef __FreeBSD__
	addr.sun_len = sizeof(addr);
#endif /* __FreeBSD__ */
	addr.sun_family = AF_UNIX;
	fname = hostapd_ctrl_iface_path(hapd);
	if (fname == NULL)
		goto fail;
	os_strlcpy(addr.sun_path, fname, sizeof(addr.sun_path));
	if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		wpa_printf(MSG_DEBUG, "ctrl_iface bind(PF_UNIX) failed: %s",
			   strerror(errno));
		if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
			wpa_printf(MSG_DEBUG, "ctrl_iface exists, but does not"
				   " allow connections - assuming it was left"
				   "over from forced program termination");
			if (unlink(fname) < 0) {
				perror("unlink[ctrl_iface]");
				wpa_printf(MSG_ERROR, "Could not unlink "
					   "existing ctrl_iface socket '%s'",
					   fname);
				goto fail;
			}
			if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) <
			    0) {
				perror("hostapd-ctrl-iface: bind(PF_UNIX)");
				goto fail;
			}
			wpa_printf(MSG_DEBUG, "Successfully replaced leftover "
				   "ctrl_iface socket '%s'", fname);
		} else {
			wpa_printf(MSG_INFO, "ctrl_iface exists and seems to "
				   "be in use - cannot override it");
			wpa_printf(MSG_INFO, "Delete '%s' manually if it is "
				   "not used anymore", fname);
			os_free(fname);
			fname = NULL;
			goto fail;
		}
	}

	if (hapd->conf->ctrl_interface_gid_set &&
	    chown(fname, -1, hapd->conf->ctrl_interface_gid) < 0) {
		perror("chown[ctrl_interface/ifname]");
		goto fail;
	}

	if (!hapd->conf->ctrl_interface_gid_set &&
	    hapd->iface->interfaces->ctrl_iface_group &&
	    chown(fname, -1, hapd->iface->interfaces->ctrl_iface_group) < 0) {
		perror("chown[ctrl_interface/ifname]");
		goto fail;
	}

	if (chmod(fname, S_IRWXU | S_IRWXG) < 0) {
		perror("chmod[ctrl_interface/ifname]");
		goto fail;
	}
	os_free(fname);

	hapd->ctrl_sock = s;
	eloop_register_read_sock(s, hostapd_ctrl_iface_receive, hapd,
				 NULL);
	hapd->msg_ctx = hapd;
	wpa_msg_register_cb(hostapd_ctrl_iface_msg_cb);

	return 0;

fail:
	if (s >= 0)
		close(s);
	if (fname) {
		unlink(fname);
		os_free(fname);
	}
	return -1;
}


void hostapd_ctrl_iface_deinit(struct hostapd_data *hapd)
{
	struct wpa_ctrl_dst *dst, *prev;

	if (!hapd->primary_interface)
		return;

	if (hapd->ctrl_sock > -1) {
		char *fname;
		eloop_unregister_read_sock(hapd->ctrl_sock);
		close(hapd->ctrl_sock);
		hapd->ctrl_sock = -1;
		fname = hostapd_ctrl_iface_path(hapd);
		if (fname)
			unlink(fname);
		os_free(fname);

		if (hapd->conf->ctrl_interface &&
		    rmdir(hapd->conf->ctrl_interface) < 0) {
			if (errno == ENOTEMPTY) {
				wpa_printf(MSG_DEBUG, "Control interface "
					   "directory not empty - leaving it "
					   "behind");
			} else {
				wpa_printf(MSG_ERROR,
					   "rmdir[ctrl_interface=%s]: %s",
					   hapd->conf->ctrl_interface,
					   strerror(errno));
			}
		}
	}

	dst = hapd->ctrl_dst;
	while (dst) {
		prev = dst;
		dst = dst->next;
		os_free(prev);
	}
}


static int hostapd_ctrl_iface_add(struct hapd_interfaces *interfaces,
				  char *buf)
{
	if (hostapd_add_iface(interfaces, buf) < 0) {
		wpa_printf(MSG_ERROR, "Adding interface %s failed", buf);
		return -1;
	}
	return 0;
}


static int hostapd_ctrl_iface_remove(struct hapd_interfaces *interfaces,
				     char *buf)
{
	if (hostapd_remove_iface(interfaces, buf) < 0) {
		wpa_printf(MSG_ERROR, "Removing interface %s failed", buf);
		return -1;
	}
	return 0;
}


static void hostapd_ctrl_iface_flush(struct hapd_interfaces *interfaces)
{
#ifdef CONFIG_WPS_TESTING
	wps_version_number = 0x20;
	wps_testing_dummy_cred = 0;
	wps_corrupt_pkhash = 0;
#endif /* CONFIG_WPS_TESTING */
}


static void hostapd_global_ctrl_iface_receive(int sock, void *eloop_ctx,
					      void *sock_ctx)
{
	void *interfaces = eloop_ctx;
	char buf[256];
	int res;
	struct sockaddr_un from;
	socklen_t fromlen = sizeof(from);
	char reply[24];
	int reply_len;

	res = recvfrom(sock, buf, sizeof(buf) - 1, 0,
		       (struct sockaddr *) &from, &fromlen);
	if (res < 0) {
		perror("recvfrom(ctrl_iface)");
		return;
	}
	buf[res] = '\0';
	wpa_printf(MSG_DEBUG, "Global ctrl_iface command: %s", buf);

	os_memcpy(reply, "OK\n", 3);
	reply_len = 3;

	if (os_strcmp(buf, "PING") == 0) {
		os_memcpy(reply, "PONG\n", 5);
		reply_len = 5;
	} else if (os_strncmp(buf, "RELOG", 5) == 0) {
		if (wpa_debug_reopen_file() < 0)
			reply_len = -1;
	} else if (os_strcmp(buf, "FLUSH") == 0) {
		hostapd_ctrl_iface_flush(interfaces);
	} else if (os_strncmp(buf, "ADD ", 4) == 0) {
		if (hostapd_ctrl_iface_add(interfaces, buf + 4) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "REMOVE ", 7) == 0) {
		if (hostapd_ctrl_iface_remove(interfaces, buf + 7) < 0)
			reply_len = -1;
	} else {
		wpa_printf(MSG_DEBUG, "Unrecognized global ctrl_iface command "
			   "ignored");
		reply_len = -1;
	}

	if (reply_len < 0) {
		os_memcpy(reply, "FAIL\n", 5);
		reply_len = 5;
	}

	sendto(sock, reply, reply_len, 0, (struct sockaddr *) &from, fromlen);
}


static char * hostapd_global_ctrl_iface_path(struct hapd_interfaces *interface)
{
	char *buf;
	size_t len;

	if (interface->global_iface_path == NULL)
		return NULL;

	len = os_strlen(interface->global_iface_path) +
		os_strlen(interface->global_iface_name) + 2;
	buf = os_malloc(len);
	if (buf == NULL)
		return NULL;

	os_snprintf(buf, len, "%s/%s", interface->global_iface_path,
		    interface->global_iface_name);
	buf[len - 1] = '\0';
	return buf;
}


int hostapd_global_ctrl_iface_init(struct hapd_interfaces *interface)
{
	struct sockaddr_un addr;
	int s = -1;
	char *fname = NULL;

	if (interface->global_iface_path == NULL) {
		wpa_printf(MSG_DEBUG, "ctrl_iface not configured!");
		return 0;
	}

	if (mkdir(interface->global_iface_path, S_IRWXU | S_IRWXG) < 0) {
		if (errno == EEXIST) {
			wpa_printf(MSG_DEBUG, "Using existing control "
				   "interface directory.");
		} else {
			perror("mkdir[ctrl_interface]");
			goto fail;
		}
	} else if (interface->ctrl_iface_group &&
		   chown(interface->global_iface_path, -1,
			 interface->ctrl_iface_group) < 0) {
		perror("chown[ctrl_interface]");
		goto fail;
	}

	if (os_strlen(interface->global_iface_path) + 1 +
	    os_strlen(interface->global_iface_name) >= sizeof(addr.sun_path))
		goto fail;

	s = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("socket(PF_UNIX)");
		goto fail;
	}

	os_memset(&addr, 0, sizeof(addr));
#ifdef __FreeBSD__
	addr.sun_len = sizeof(addr);
#endif /* __FreeBSD__ */
	addr.sun_family = AF_UNIX;
	fname = hostapd_global_ctrl_iface_path(interface);
	if (fname == NULL)
		goto fail;
	os_strlcpy(addr.sun_path, fname, sizeof(addr.sun_path));
	if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		wpa_printf(MSG_DEBUG, "ctrl_iface bind(PF_UNIX) failed: %s",
			   strerror(errno));
		if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
			wpa_printf(MSG_DEBUG, "ctrl_iface exists, but does not"
				   " allow connections - assuming it was left"
				   "over from forced program termination");
			if (unlink(fname) < 0) {
				perror("unlink[ctrl_iface]");
				wpa_printf(MSG_ERROR, "Could not unlink "
					   "existing ctrl_iface socket '%s'",
					   fname);
				goto fail;
			}
			if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) <
			    0) {
				perror("bind(PF_UNIX)");
				goto fail;
			}
			wpa_printf(MSG_DEBUG, "Successfully replaced leftover "
				   "ctrl_iface socket '%s'", fname);
		} else {
			wpa_printf(MSG_INFO, "ctrl_iface exists and seems to "
				   "be in use - cannot override it");
			wpa_printf(MSG_INFO, "Delete '%s' manually if it is "
				   "not used anymore", fname);
			os_free(fname);
			fname = NULL;
			goto fail;
		}
	}

	if (interface->ctrl_iface_group &&
	    chown(fname, -1, interface->ctrl_iface_group) < 0) {
		perror("chown[ctrl_interface]");
		goto fail;
	}

	if (chmod(fname, S_IRWXU | S_IRWXG) < 0) {
		perror("chmod[ctrl_interface/ifname]");
		goto fail;
	}
	os_free(fname);

	interface->global_ctrl_sock = s;
	eloop_register_read_sock(s, hostapd_global_ctrl_iface_receive,
				 interface, NULL);

	return 0;

fail:
	if (s >= 0)
		close(s);
	if (fname) {
		unlink(fname);
		os_free(fname);
	}
	return -1;
}


void hostapd_global_ctrl_iface_deinit(struct hapd_interfaces *interfaces)
{
	char *fname = NULL;

	if (interfaces->global_ctrl_sock > -1) {
		eloop_unregister_read_sock(interfaces->global_ctrl_sock);
		close(interfaces->global_ctrl_sock);
		interfaces->global_ctrl_sock = -1;
		fname = hostapd_global_ctrl_iface_path(interfaces);
		if (fname) {
			unlink(fname);
			os_free(fname);
		}

		if (interfaces->global_iface_path &&
		    rmdir(interfaces->global_iface_path) < 0) {
			if (errno == ENOTEMPTY) {
				wpa_printf(MSG_DEBUG, "Control interface "
					   "directory not empty - leaving it "
					   "behind");
			} else {
				wpa_printf(MSG_ERROR,
					   "rmdir[ctrl_interface=%s]: %s",
					   interfaces->global_iface_path,
					   strerror(errno));
			}
		}
		os_free(interfaces->global_iface_path);
		interfaces->global_iface_path = NULL;
	}
}


static void hostapd_ctrl_iface_send(struct hostapd_data *hapd, int level,
				    const char *buf, size_t len)
{
	struct wpa_ctrl_dst *dst, *next;
	struct msghdr msg;
	int idx;
	struct iovec io[2];
	char levelstr[10];

	dst = hapd->ctrl_dst;
	if (hapd->ctrl_sock < 0 || dst == NULL)
		return;

	os_snprintf(levelstr, sizeof(levelstr), "<%d>", level);
	io[0].iov_base = levelstr;
	io[0].iov_len = os_strlen(levelstr);
	io[1].iov_base = (char *) buf;
	io[1].iov_len = len;
	os_memset(&msg, 0, sizeof(msg));
	msg.msg_iov = io;
	msg.msg_iovlen = 2;

	idx = 0;
	while (dst) {
		next = dst->next;
		if (level >= dst->debug_level) {
			wpa_hexdump(MSG_DEBUG, "CTRL_IFACE monitor send",
				    (u8 *) dst->addr.sun_path, dst->addrlen -
				    offsetof(struct sockaddr_un, sun_path));
			msg.msg_name = &dst->addr;
			msg.msg_namelen = dst->addrlen;
			if (sendmsg(hapd->ctrl_sock, &msg, 0) < 0) {
				int _errno = errno;
				wpa_printf(MSG_INFO, "CTRL_IFACE monitor[%d]: "
					   "%d - %s",
					   idx, errno, strerror(errno));
				dst->errors++;
				if (dst->errors > 10 || _errno == ENOENT) {
					hostapd_ctrl_iface_detach(
						hapd, &dst->addr,
						dst->addrlen);
				}
			} else
				dst->errors = 0;
		}
		idx++;
		dst = next;
	}
}

#endif /* CONFIG_NATIVE_WINDOWS */
