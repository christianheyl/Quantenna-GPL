/****************************************************************************
*
* Copyright (c) 2015  Quantenna Communications, Inc.
*
* Permission to use, copy, modify, and/or distribute this software for any
* purpose with or without fee is hereby granted, provided that the above
* copyright notice and this permission notice appear in all copies.
*
* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
* WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
* SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
* RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
* NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
* USE OR PERFORMANCE OF THIS SOFTWARE.
*
*****************************************************************************/

#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include "qtn_cmd_parser.h"
#include "qsigma_tags.h"

#define N_ARRAY(arr)			(sizeof(arr)/sizeof(arr[0]))

static
const struct qtn_token_desc qtn_token_table[] = {
	{QTN_TOK_NAME,         "NAME"},
	{QTN_TOK_PROGRAM,      "PROGRAM"},
	{QTN_TOK_PROG,         "PROG"},
	{QTN_TOK_INTERFACE,    "INTERFACE"},
	{QTN_TOK_TYPE,         "TYPE"},
	{QTN_TOK_SSID,         "SSID"},
	{QTN_TOK_CHANNEL,      "CHANNEL"},
	{QTN_TOK_MODE,         "MODE"},
	{QTN_TOK_WME,          "WME"},
	{QTN_TOK_WMMPS,        "WMMPS"},
	{QTN_TOK_RTS,          "RTS"},
	{QTN_TOK_FRGMNT,       "FRGMNT"},
	{QTN_TOK_PWRSAVE,      "PWRSAVE"},
	{QTN_TOK_BCNINT,       "BCNINT"},
	{QTN_TOK_RADIO,        "RADIO"},
	{QTN_TOK_40_INTOLERANT, "40_INTOLERANT"},
	{QTN_TOK_ADDBA_REJECT,  "ADDBA_REJECT"},
	{QTN_TOK_AMPDU,        "AMPDU"},
	{QTN_TOK_AMPDU_EXP,    "AMPDU_EXP"},
	{QTN_TOK_AMSDU,        "AMSDU"},
	{QTN_TOK_GREENFIELD,   "GREENFIELD"},
	{QTN_TOK_OFFSET,       "OFFSET"},
	{QTN_TOK_MCS_32,       "MCS_32"},
	{QTN_TOK_MCS_FIXEDRATE, "MCS_FIXEDRATE"},
	{QTN_TOK_SPATIAL_RX_STREAM, "SPATIAL_RX_STREAM"},
	{QTN_TOK_SPATIAL_TX_STREAM, "SPATIAL_TX_STREAM"},
	{QTN_TOK_MPDU_MIN_START_SPACING, "MPDU_MIN_START_SPACING"},
	{QTN_TOK_RIFS_TEST,    "RIFS_TEST"},
	{QTN_TOK_SGI20,        "SGI20"},
	{QTN_TOK_STBC_TX,      "STBC_TX"},
	{QTN_TOK_WIDTH,        "WIDTH"},
	{QTN_TOK_WIDTH_SCAN,   "WIDTH_SCAN"},
	{QTN_TOK_CWMIN_VO,     "CWMIN_VO"},
	{QTN_TOK_CWMIN_VI,     "CWMIN_VI"},
	{QTN_TOK_CWMIN_BE,     "CWMIN_BE"},
	{QTN_TOK_CWMIN_BK,     "CWMIN_BK"},
	{QTN_TOK_CWMAX_VO,     "CWMAX_VO"},
	{QTN_TOK_CWMAX_VI,     "CWMAX_VI"},
	{QTN_TOK_CWMAX_BE,     "CWMAX_BE"},
	{QTN_TOK_CWMAX_BK,     "CWMAX_BK"},
	{QTN_TOK_AIFS_VO,      "AIFS_VO"},
	{QTN_TOK_AIFS_VI,      "AIFS_VI"},
	{QTN_TOK_AIFS_BE,      "AIFS_BE"},
	{QTN_TOK_AIFS_BK,      "AIFS_BK"},
	{QTN_TOK_TxOP_VO,      "TxOP_VO"},
	{QTN_TOK_TxOP_VI,      "TxOP_VI"},
	{QTN_TOK_TxOP_BE,      "TxOP_BE"},
	{QTN_TOK_TxOP_BK,      "TxOP_BK"},
	{QTN_TOK_ACM_VO,       "ACM_VO"},
	{QTN_TOK_ACM_VI,       "ACM_VI"},
	{QTN_TOK_ACM_BE,       "ACM_BE"},
	{QTN_TOK_ACM_BK,       "ACM_BK"},
	{QTN_TOK_STA_MAC_ADDRESS, "STA_MAC_ADDRESS"},
	{QTN_TOK_MINORCODE,    "MINORCODE"},
	{QTN_TOK_REGULATORY_MODE, "REGULATORY_MODE"},
	{QTN_TOK_COUNTRY_CODE, "CountryCode"},
	{QTN_TOK_DFS_MODE,     "DFS_MODE"},
	{QTN_TOK_DFS_CHAN,     "DFS_CHAN"},
	{QTN_TOK_NSS_MCS_OPT,  "NSS_MCS_OPT"},
	{QTN_TOK_OPT_MD_NOTIF_IE, "OPT_MD_NOTIF_IE"},
	{QTN_TOK_CHNUM_BAND,   "CHNUM_BAND"},
	{QTN_TOK_TID,          "TID"},
	{QTN_TOK_DEST_MAC,     "DEST_MAC"},
	{QTN_TOK_SUPPLICANT,   "SUPPLICANT"},
	{QTN_TOK_PREAMBLE,     "PREAMBLE"},
	{QTN_TOK_POWERSAVE,    "POWERSAVE"},
	{QTN_TOK_NOACK,        "NOACK"},
	{QTN_TOK_IGNORECHSWITCHPROHIBIT, "IGNORECHSWITCHPROHIBIT"},
	{QTN_TOK_TDLS,         "TDLS"},
	{QTN_TOK_TDLSMODE,     "TDLSMODE"},
	{QTN_TOK_WFDDEVTYPE,   "WFDDEVTYPE"},
	{QTN_TOK_UIBC_GEN,     "UIBC_GEN"},
	{QTN_TOK_UIBC_HID,     "UIBC_HID"},
	{QTN_TOK_UI_INPUT,     "UI_INPUT"},
	{QTN_TOK_UIBC_PREPARE, "UIBC_PREPARE"},
	{QTN_TOK_HDCP,         "HDCP"},
	{QTN_TOK_FRAMESKIP,    "FRAMESKIP"},
	{QTN_TOK_AVCHANGE,     "AVCHANGE"},
	{QTN_TOK_STANDBY,      "STANDBY"},
	{QTN_TOK_INPUTCONTENT, "INPUTCONTENT"},
	{QTN_TOK_VIDEOFORMAT,  "VIDEOFORMAT"},
	{QTN_TOK_AUDIOFORMAT,  "AUDIOFORMAT"},
	{QTN_TOK_I2C,          "I2C"},
	{QTN_TOK_VIDEORECOVERY, "VIDEORECOVERY"},
	{QTN_TOK_PREFDISPLAY,  "PREFDISPLAY"},
	{QTN_TOK_SERVICEDISCOVERY, "SERVICEDISCOVERY"},
	{QTN_TOK_3DVIDEO,      "3DVIDEO"},
	{QTN_TOK_MULTITXSTREAM, "MULTITXSTREAM"},
	{QTN_TOK_TIMESYNC,     "TIMESYNC"},
	{QTN_TOK_EDID,         "EDID"},
	{QTN_TOK_COUPLEDCAP,   "COUPLEDCAP"},
	{QTN_TOK_OPTIONALFEATURE, "OPTIONALFEATURE"},
	{QTN_TOK_SESSIONAVAILABILITY, "SESSIONAVAILABILITY"},
	{QTN_TOK_DEVICEDISCOVERABILITY, "DEVICEDISCOVERABILITY"},
	{QTN_TOK_WMM,          "WMM"},
	{QTN_TOK_STBC_RX,      "STBC_RX"},
	{QTN_TOK_MCS32,        "MCS32"},
	{QTN_TOK_SMPS,         "SMPS"},
	{QTN_TOK_TXSP_STREAM,  "TXSP_STREAM"},
	{QTN_TOK_RXSP_STREAM,  "RXSP_STREAM"},
	{QTN_TOK_BAND,         "BAND"},
	{QTN_TOK_DYN_BW_SGNL,  "DYN_BW_SGNL"},
	{QTN_TOK_SGI80,        "SGI80"},
	{QTN_TOK_TXBF,         "TXBF"},
	{QTN_TOK_LDPC,         "LDPC"},
	{QTN_TOK_NSS_MCS_CAP,  "NSS_MCS_CAP"},
	{QTN_TOK_TX_LGI_RATE,  "TX_LGI_RATE"},
	{QTN_TOK_ZERO_CRC,     "ZERO_CRC"},
	{QTN_TOK_VHT_TKIP,     "VHT_TKIP"},
	{QTN_TOK_VHT_WEP,      "VHT_WEP"},
	{QTN_TOK_BW_SGNL,      "BW_SGNL"},
	{QTN_TOK_PASSPHRASE,   "PASSPHRASE"},
	{QTN_TOK_KEYMGMTTYPE,  "KEYMGMTTYPE"},
	{QTN_TOK_ENCPTYPE,     "ENCPTYPE"},
	{QTN_TOK_PMF,          "PMF"},
	{QTN_TOK_MICALG,       "MICALG"},
	{QTN_TOK_PREFER,       "PREFER"},
	{QTN_TOK_FRAMENAME,    "FRAMENAME"},
	{QTN_TOK_CHANNEL_WIDTH,"CHANNEL_WIDTH"},
	{QTN_TOK_NSS,          "NSS"},
	{QTN_TOK_BSSID,        "BSSID"},
	{QTN_TOK_MONTH,        "MONTH"},
	{QTN_TOK_DATE,         "DATE"},
	{QTN_TOK_YEAR,         "YEAR"},
	{QTN_TOK_HOURS,        "HOURS"},
	{QTN_TOK_MINUTES,      "MINUTES"},
	{QTN_TOK_SECONDS,      "SECONDS"},
	{QTN_TOK_MAC,          "MAC"},
	{QTN_TOK_MAXSPLENGTH,  "MAXSPLENGTH"},
	{QTN_TOK_ACBE,         "ACBE"},
	{QTN_TOK_ACBK,         "ACBK"},
	{QTN_TOK_ACVI,         "ACVI"},
	{QTN_TOK_ACVO,         "ACVO"},
	{QTN_TOK_MU_TXBF,      "MU_TXBF"},
	{QTN_TOK_RTS_BWS,      "RTS_BWS"},
	{QTN_TOK_ARP,          "ARP"},
	{QTN_TOK_MU_DBG_GROUP_STA0, "MU_DBG_GROUP_STA0"},
	{QTN_TOK_MU_DBG_GROUP_STA1, "MU_DBG_GROUP_STA1"},
	{QTN_TOK_CTS_WIDTH,    "CTS_WIDTH"},
	{QTN_TOK_RTS_FORCE,    "RTS_FORCE"},
	{QTN_TOK_INTERWORKING, "INTERWORKING"},
	{QTN_TOK_ACCS_NET_TYPE, "ACCS_NET_TYPE"},
	{QTN_TOK_INTERNET, "INTERNET"},
	{QTN_TOK_VENUE_GRP, "VENUE_GRP"},
	{QTN_TOK_VENUE_TYPE, "VENUE_TYPE"},
	{QTN_TOK_HESSID, "HESSID"},
	{QTN_TOK_ROAMING_CONS, "ROAMING_CONS"},
	{QTN_TOK_DGAF_DISABLE, "DGAF_DISABLE"},
	{QTN_TOK_ANQP, "ANQP"},
	{QTN_TOK_NET_AUTH_TYPE, "NET_AUTH_TYPE"},
	{QTN_TOK_NAI_REALM_LIST, "NAI_REALM_LIST"},
	{QTN_TOK_DOMAIN_LIST, "DOMAIN_LIST"},
	{QTN_TOK_OPER_NAME, "OPER_NAME"},
	{QTN_TOK_VENUE_NAME, "VENUE_NAME"},
	{QTN_TOK_GAS_CB_DELAY, "GAS_CB_DELAY"},
	{QTN_TOK_MIH, "MIH"},
	{QTN_TOK_L2_TRAFFIC_INSPECT, "L2_TRAFFIC_INSPECT"},
	{QTN_TOK_BCST_UNCST, "BCST_UNCST"},
	{QTN_TOK_PLMN_MCC, "PLMN_MCC"},
	{QTN_TOK_PLMN_MNC, "PLMN_MNC"},
	{QTN_TOK_PROXY_ARP, "PROXY_ARP"},
	{QTN_TOK_WAN_METRICS, "WAN_METRICS"},
	{QTN_TOK_CONN_CAP, "CONN_CAP"},
	{QTN_TOK_IP_ADD_TYPE_AVAIL, "IP_ADD_TYPE_AVAIL"},
	{QTN_TOK_ICMPV4_ECHO, "ICMPV4_ECHO"},
	{QTN_TOK_CHSWITCHMODE, "CHSWITCHMODE"},
	{QTN_TOK_PEER, "PEER"},
	{QTN_TOK_OFFCHNUM, "OFFCHNUM"},
	{QTN_TOK_SECCHOFFSET, "SECCHOFFSET"},
	{QTN_TOK_UAPSD, "UAPSD"},
	{QTN_TOK_TXBANDWIDTH, "TXBANDWIDTH"}
};

static
const char *qtn_resp_status_table[] = {
	"COMPLETE",
	"ERROR",
	"INVALID",
	"RUNNING"
};

const struct qtn_token_desc *qtn_lookup_token_by_id(const enum qtn_token tok)
{
	int i;

	for (i = 0; i < N_ARRAY(qtn_token_table); i++) {
		if (qtn_token_table[i].tok_id == tok)
			return &qtn_token_table[i];
	}

	return NULL;
}

static
const struct qtn_token_desc *qtn_lookup_token_by_name(const char *name, int len)
{
	int i;

	if (name && *name && (len > 0)) {
		for (i = 0; i < N_ARRAY(qtn_token_table); i++) {
			if (strncasecmp(qtn_token_table[i].tok_text, name, len) == 0)
				if (strlen(qtn_token_table[i].tok_text) == len)
					return &qtn_token_table[i];
		}
	}

	return NULL;
}

static
int qtn_parse_params(const char *params_ptr, struct qtn_cmd_param *param_tab_ptr,
		int param_tab_size, int *error_count)
{
	const char *delim;
	const char *name_ptr;
	int name_len;
	const char *val_ptr;
	int val_len;
	const struct qtn_token_desc *token;
	struct qtn_cmd_param *param;
	int param_count = 0;
	int err_count = 0;
	const char *pair = params_ptr;

	while (pair && *pair && (param_count < param_tab_size)) {
		delim = strchr(pair, ',');

		if (!delim)
			break;

		name_ptr = pair;
		name_len = delim - pair;
		val_ptr = delim + 1;

		delim = strchr(val_ptr, ',');

		if (delim) {
			val_len = delim - val_ptr;
			pair = delim + 1;
		} else {
			val_len = strlen(val_ptr);
			pair = NULL;
		}

		while ((name_ptr[0] == ' ') && (name_len > 0)) {
			name_ptr++;
			name_len--;
		}

		if (name_len == 0) {
			err_count++;
			break;
		}

		while ((name_len > 0) && (name_ptr[name_len - 1] == ' ')) {
			name_len--;
		}

		token = qtn_lookup_token_by_name(name_ptr, name_len);

		if (token) {
			param = &param_tab_ptr[param_count];
			param->key_tok = token->tok_id;
			param->val_pos = val_ptr - params_ptr;
			param->val_len = val_len;

			param_count++;
		} else
			err_count++;
	}

	if (error_count)
		*error_count = err_count;

	return param_count;
}

int qtn_parse_params_encode_request(const char *params, char *buf_ptr, int buf_size)
{
	struct qtn_cmd_param param_tab[QTN_CMD_MAX_PARAM_COUNT];
	int param_count;
	int error_count;
	int buf_len = 0;
	int i;
	char key_buf[8];
	int key_len;

	param_count = qtn_parse_params(params, param_tab, N_ARRAY(param_tab), &error_count);

	for (i = 0; i < param_count; i++) {
		struct qtn_cmd_param *param = &param_tab[i];

		key_len = snprintf(key_buf, sizeof(key_buf), "%03d", (int)param->key_tok);

		if (key_len > 0) {
			if (buf_size > (buf_len + key_len + 1 + param->val_len + 1)) {
				strncpy(buf_ptr + buf_len, key_buf, key_len);
				buf_len += key_len;

				buf_ptr[buf_len] = ',';
				buf_len++;

				strncpy(buf_ptr + buf_len, params + param->val_pos, param->val_len);
				buf_len += param->val_len;

				buf_ptr[buf_len] = ',';
				buf_len++;
			} else
				return -ENOMEM;
		}
	}

	buf_ptr[buf_len] = 0;

	return buf_len;
}

static
const char* qtn_search_char(const char *start, const char *end, const char c)
{
	while (start < end) {
		if (*start == c)
			return start;
		start++;
	}

	return NULL;
}


static
int qtn_parse_request(const char *req_ptr, int req_len, struct qtn_cmd_param *param_tab_ptr,
		int param_tab_size, int *error_count)
{
	const char *delim;
	const char *name_ptr;
	int name_len;
	const char *val_ptr;
	int val_len;
	char key_buf[8];
	long key_tok;
	struct qtn_cmd_param *param;
	int param_count = 0;
	int err_count = 0;
	const char *pair = req_ptr;
	const char *req_end = req_ptr + req_len;

	while (pair && (pair < req_end) && (param_count < param_tab_size)) {
		delim = qtn_search_char(pair, req_end, ',');

		if (!delim)
			break;

		name_ptr = pair;
		name_len = delim - pair;
		val_ptr = delim + 1;

		delim = qtn_search_char(val_ptr, req_end, ',');

		if (delim) {
			val_len = delim - val_ptr;
			pair = delim + 1;
		} else {
			val_len = req_end - val_ptr;
			pair = NULL;
		}

		if ((name_len <= 0) || (name_len >= sizeof(key_buf))) {
			err_count++;
			break;
		}

		strncpy(key_buf, name_ptr, name_len);
		key_buf[name_len] = 0;

		key_tok = strtol(key_buf, NULL, 10);

		if ((key_tok == 0) || (key_tok == LONG_MAX) || (key_tok == LONG_MIN)) {
			err_count++;
			break;
		}

		param = &param_tab_ptr[param_count];
		param->key_tok = (enum qtn_token)key_tok;
		param->val_pos = val_ptr - req_ptr;
		param->val_len = val_len;

		param_count++;
	}

	if (error_count)
		*error_count = err_count;

	return param_count;
}

int qtn_init_cmd_request(struct qtn_cmd_request *cmd_req, int cmd_tag,
		const unsigned char *req_ptr, int req_len)
{
	if (!cmd_req)
		return -EINVAL;

	if (req_ptr && (req_len > 0)) {
		int error_count;

		cmd_req->param_count = qtn_parse_request((const char*)req_ptr, req_len,
				cmd_req->param_tab, N_ARRAY(cmd_req->param_tab),
				&error_count);

		cmd_req->req_ptr = (const char*)req_ptr;
		cmd_req->req_len = req_len;
	} else {
		cmd_req->param_count = 0;
		cmd_req->req_ptr = NULL;
		cmd_req->req_len = 0;
	}

	return 0;
}

static
int qtn_get_value(const struct qtn_cmd_request *cmd_req, enum qtn_token tok, const char **val_ptr,
	int *val_len)
{
	int i;
	const struct qtn_cmd_param *param;

	if (!cmd_req)
		return -EINVAL;

	for (i = 0; i < cmd_req->param_count; i++) {
		param = &cmd_req->param_tab[i];

		if (param->key_tok == tok) {
			*val_ptr = cmd_req->req_ptr + param->val_pos;
			*val_len = param->val_len;
			return 0;
		}
	}

	return -ENODATA;
}

int qtn_get_value_text(const struct qtn_cmd_request *cmd_req, enum qtn_token tok, char *buf_ptr,
	int buf_size)
{
	const char *val_ptr;
	int val_len;
	int ret;

	if (!buf_ptr || (buf_size <= 0))
		return -EINVAL;

	ret = qtn_get_value(cmd_req, tok, &val_ptr, &val_len);

	if (ret != 0)
		return ret;

	if (val_len >= buf_size)
		return -ENOMEM;

	if (val_len > 0)
		strncpy(buf_ptr, val_ptr, val_len);

	buf_ptr[val_len] = 0;

	return val_len;
}

int qtn_get_value_int(const struct qtn_cmd_request *cmd_req, enum qtn_token tok, int *value)
{
	const char *val_ptr;
	int val_len;
	char val_buf[32];
	int ret;

	if (!value)
		return -EINVAL;

	ret = qtn_get_value(cmd_req, tok, &val_ptr, &val_len);

	if (ret != 0)
		return ret;

	if (val_len >= sizeof(val_buf))
		return -ENOMEM;

	if (val_len > 0) {
		strncpy(val_buf, val_ptr, val_len);
		val_buf[val_len] = 0;
		*value = strtol(val_buf, NULL, 10);
	}

	return val_len;
}

/*
 * return Enable/Disable value
 */
int qtn_get_value_enable(const struct qtn_cmd_request *cmd_req, enum qtn_token tok, int *enable,
	int *conv_error)
{
	const char *val_ptr;
	int val_len;
	char val_buf[32];
	int ret;

	if (conv_error)
		*conv_error = 0;

	if (!enable)
		return -EINVAL;

	ret = qtn_get_value(cmd_req, tok, &val_ptr, &val_len);

	if (ret != 0)
		return ret;

	if (val_len >= sizeof(val_buf))
		return -EINVAL;

	if (val_len > 0) {
		strncpy(val_buf, val_ptr, val_len);
		val_buf[val_len] = 0;

		if (strcasecmp(val_buf, "Enable") == 0)
			*enable = 1;
		else if (strcasecmp(val_buf, "Disable") == 0)
			*enable = 0;
		else {
			/* complain about unrecognized value */
			val_len = -EINVAL;

			if (conv_error)
				*conv_error = -EINVAL;
		}
	}

	return val_len;
}

int qtn_validate_response_get_length(const char *buf_ptr, int buf_len)
{
	int i;
	int tok_len = 0;
	int tok_found = 0;
	int resp_len;

	if (!buf_ptr || (buf_len <= 0))
		return -EINVAL;

	for (i = 0; i < N_ARRAY(qtn_resp_status_table); i++) {
		tok_len = strlen(qtn_resp_status_table[i]);

		if (strncasecmp(buf_ptr, qtn_resp_status_table[i], tok_len) == 0) {
			tok_found = 1;
			break;
		}
	}

	if (tok_found) {
		if (buf_len > tok_len) {
			if (buf_ptr[tok_len] == 0) {
				/* received NULL-terminated string */
				return tok_len;

			} else if (buf_ptr[tok_len] == ',') {
				/* there are additional response parameters */
				resp_len = 0;
				while (buf_ptr[resp_len] && (resp_len < buf_len))
					resp_len++;
				return resp_len;
			}
		} else
			return tok_len;
	}

	return -EINVAL;
}
