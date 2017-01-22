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
#include <unistd.h>

#include "qtn_dut_sta_handler.h"
#include "common/qtn_cmd_parser.h"
#include "common/qtn_dut_common.h"
#include "common/qtn_defconf.h"

#include "common/qsigma_log.h"
#include "common/qsigma_tags.h"
#include "common/qsigma_common.h"
#include "wfa_types.h"
#include "wfa_tlv.h"
#include "wfa_tg.h"
#include "wfa_cmds.h"

#include "drivers/qdrv/qdrv_bld.h"
#include "qtn/qcsapi.h"
#include <linux/wireless.h>
#include <net80211/ieee80211.h>
#include <net80211/ieee80211_ioctl.h>
#include <sys/ioctl.h>

static int set_sta_encryption(const char *ifname, const char* ssid, const char *enc)
{
	int i;

	static const struct {
		const char *sigma_enc;
		const char *encryption;
	} map[] = {
		{
		.sigma_enc = "tkip",.encryption = "TKIPEncryption"}, {
		.sigma_enc = "aes-ccmp",.encryption = "AESEncryption"}, {
		.sigma_enc = "aes-ccmp-tkip",.encryption = "TKIPandAESEncryption"}, {
		NULL}
	};

	for (i = 0; map[i].sigma_enc != NULL; ++i) {
		if (strcasecmp(enc, map[i].sigma_enc) == 0) {
			break;
		}
	}

	if (map[i].sigma_enc == NULL) {
		return -EINVAL;
	}

	return qcsapi_SSID_set_encryption_modes(ifname, ssid, map[i].encryption);
}

void qnat_sta_device_list_interfaces(int tag, int len, unsigned char *params, int *out_len,
	unsigned char *out)
{
	struct qtn_dut_response rsp = { 0 };

	/* can't use qcsapi_get_interface_by_index() since it works for AP only */
	snprintf(rsp.ap_info.interface_list, sizeof(rsp.ap_info.interface_list), "%s",
		QCSAPI_PRIMARY_WIFI_IFNAME);

	rsp.status = STATUS_COMPLETE;
	wfaEncodeTLV(tag, sizeof(rsp), (BYTE *) & rsp, out);

	*out_len = WFA_TLV_HDR_LEN + sizeof(rsp);
}

void qtn_handle_sta_reset_default(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out)
{
	struct qtn_cmd_request cmd_req;
	int status;
	int ret;
	qcsapi_wifi_mode current_mode;
	char ifname[IFNAMSIZ];
	char cert_prog[16];
	char conf_type[16];

	ret = qtn_init_cmd_request(&cmd_req, cmd_tag, params, len);
	if (ret != 0) {
		status = STATUS_INVALID;
		goto respond;
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_INTERFACE, ifname, sizeof(ifname)) <= 0) {
		snprintf(ifname, sizeof(ifname), "%s", QCSAPI_PRIMARY_WIFI_IFNAME);
	}

	if ((ret = qcsapi_wifi_get_mode(ifname, &current_mode)) < 0) {
		qtn_error("can't get mode, error %d", ret);
		status = STATUS_ERROR;
		goto respond;
	}

	if (current_mode != qcsapi_station) {
		qtn_error("mode %d is wrong, should be STA", current_mode);
		status = STATUS_ERROR;
		ret = -qcsapi_only_on_STA;
		goto respond;
	}

	/* disassociate to be sure that we start disassociated. possible error is ignored. */
	qcsapi_wifi_disassociate(ifname);

	/* mandatory certification program, e.g. VHT */
	if (qtn_get_value_text(&cmd_req, QTN_TOK_PROG, cert_prog, sizeof(cert_prog)) <= 0) {
		ret = -EINVAL;
		status = STATUS_ERROR;
		goto respond;
	}

	/* optional configuration type, e.g. DUT or Testbed */
	if (qtn_get_value_text(&cmd_req, QTN_TOK_TYPE, conf_type, sizeof(conf_type)) <= 0) {
		/* not specified */
		*conf_type = 0;
	}

	/* Certification program shall be: PMF, WFD, P2P or VHT */
	if (strcasecmp(cert_prog, "VHT") == 0) {
		if (strcasecmp(conf_type, "Testbed") == 0)
			ret = qtn_defconf_vht_testbed_sta(ifname);
		else
			ret = qtn_defconf_vht_dut_sta(ifname);

		if (ret < 0) {
			qtn_error("error: default configuration, errcode %d", ret);
			status = STATUS_ERROR;
			goto respond;
		}
	} else if (strcasecmp(cert_prog, "11n") == 0) {
		if (strcasecmp(conf_type, "Testbed") == 0) {
			ret = qtn_defconf_11n_testbed(ifname);
		} else {
			ret = qtn_defconf_11n_dut(ifname);
		}

		if (ret < 0) {
			qtn_error("error: default configuration, errcode %d", ret);
			status = STATUS_ERROR;
			goto respond;
		}
	} else if (strcasecmp(cert_prog, "PMF") == 0) {
		ret = qtn_defconf_pmf_dut(ifname);
		if (ret < 0) {
			qtn_error("error: default configuration, errcode %d", ret);
			status = STATUS_ERROR;
			goto respond;
		}
	} else if (strcasecmp(cert_prog, "TDLS") == 0) {
		ret = qtn_defconf_tdls_dut(ifname);
		if (ret < 0) {
			qtn_error("error: default configuration, errcode %d", ret);
			status = STATUS_ERROR;
			goto respond;
		}
	} else {
		/* TODO: processing for other programs */
		qtn_error("error: prog %s is not supported", cert_prog);
		ret = -ENOTSUP;
		status = STATUS_ERROR;
		goto respond;
	}

	/* TODO: Other options */
	ret = qcsapi_wifi_set_option(ifname, qcsapi_autorate_fallback, 1);
	if (ret < 0) {
		qtn_error("error: cannot set autorate, errcode %d", ret);
		status = STATUS_ERROR;
		goto respond;
	}

	status = STATUS_COMPLETE;

respond:
	qtn_dut_make_response_none(cmd_tag, status, ret, out_len, out);
}

void qtn_handle_sta_disconnect(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out)
{
	struct qtn_cmd_request cmd_req;
	int status;
	int ret;
	char ifname[IFNAMSIZ];

	ret = qtn_init_cmd_request(&cmd_req, cmd_tag, params, len);
	if (ret != 0) {
		status = STATUS_INVALID;
		goto respond;
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_INTERFACE, ifname, sizeof(ifname)) <= 0) {
		snprintf(ifname, sizeof(ifname), "%s", QCSAPI_PRIMARY_WIFI_IFNAME);
	}

	if ((ret = qcsapi_wifi_disassociate(ifname)) < 0) {
		qtn_error("can't disassociate interface %s, error %d", ifname, ret);
	}

	status = ret >= 0 ? STATUS_COMPLETE : STATUS_ERROR;
respond:
	qtn_dut_make_response_none(cmd_tag, status, ret, out_len, out);
}

void qtn_handle_sta_send_addba(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out)
{
	struct qtn_cmd_request cmd_req;
	int status;
	int ret;
	char ifname[IFNAMSIZ];
	char cmd[128];
	int tid;

	ret = qtn_init_cmd_request(&cmd_req, cmd_tag, params, len);
	if (ret != 0) {
		status = STATUS_INVALID;
		goto respond;
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_INTERFACE, ifname, sizeof(ifname)) <= 0) {
		snprintf(ifname, sizeof(ifname), "%s", QCSAPI_PRIMARY_WIFI_IFNAME);
	}

	if (qtn_get_value_int(&cmd_req, QTN_TOK_TID, &tid) <= 0) {
		qtn_error("no TID in request");
		status = STATUS_INVALID;
		goto respond;
	}

	snprintf(cmd, sizeof(cmd), "iwpriv %s htba_addba %d", ifname, tid);
	ret = system(cmd);
	if (ret != 0) {
		qtn_log("can't send addba using [%s], error %d", cmd, ret);
	}

	status = ret >= 0 ? STATUS_COMPLETE : STATUS_ERROR;
respond:
	qtn_dut_make_response_none(cmd_tag, status, ret, out_len, out);
}

void qtn_handle_sta_preset_testparameters(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out)
{
	struct qtn_cmd_request cmd_req;
	int status;
	char ifname_buf[IFNAMSIZ];
	const char *ifname;
	char val_buf[32];
	int ret;

	ret = qtn_init_cmd_request(&cmd_req, cmd_tag, params, len);

	if (ret != 0) {
		status = STATUS_INVALID;
		goto respond;
	}

	ret = qtn_get_value_text(&cmd_req, QTN_TOK_INTERFACE, ifname_buf, sizeof(ifname_buf));
	ifname = (ret > 0) ? ifname_buf : QCSAPI_PRIMARY_WIFI_IFNAME;

	ret = qtn_get_value_text(&cmd_req, QTN_TOK_MODE, val_buf, sizeof(val_buf));
	if (ret > 0) {
		const char *phy_mode = val_buf;
		qcsapi_unsigned_int old_bw;
		if (qcsapi_wifi_get_bw(ifname, &old_bw) < 0) {
			old_bw = 80;
		}

		ret = qcsapi_wifi_set_phy_mode(ifname, phy_mode);

		if (ret < 0) {
			status = STATUS_ERROR;
			goto respond;
		}

		qtn_log("try to restore %d mode since phy change", old_bw);
		qcsapi_wifi_set_bw(ifname, old_bw);
	}

	ret = qtn_get_value_text(&cmd_req, QTN_TOK_WMM, val_buf, sizeof(val_buf));
	if (ret > 0) {
		char tmpbuf[64];
		int wmm_on = (strncasecmp(val_buf, "on", 2) == 0) ? 1 : 0;

		/* TODO: qcsapi specifies enable/disable WMM only for AP */
		snprintf(tmpbuf, sizeof(tmpbuf), "iwpriv %s wmm %d", ifname, wmm_on);
		system(tmpbuf);
	}

	/* TODO: RTS FRGMNT
	 *   sta_preset_testparameters,interface,rtl8192s ,supplicant,ZeroConfig,mode,11ac,RTS,500
	 *   sta_preset_testparameters,interface,eth0,supplicant,ZeroConfig,mode,11ac,FRGMNT,2346
	 */

	status = STATUS_COMPLETE;

respond:
	qtn_dut_make_response_none(cmd_tag, status, ret, out_len, out);
}

void qtn_handle_sta_get_mac_address(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out)
{
	struct qtn_cmd_request cmd_req;
	int status;
	int ret;
	char ifname_buf[16];
	const char *ifname;
	unsigned char macaddr[IEEE80211_ADDR_LEN];

	ret = qtn_init_cmd_request(&cmd_req, cmd_tag, params, len);

	if (ret != 0) {
		status = STATUS_INVALID;
		goto respond;
	}

	*ifname_buf = 0;
	ret = qtn_get_value_text(&cmd_req, QTN_TOK_INTERFACE, ifname_buf, sizeof(ifname_buf));

	ifname = (ret > 0) ? ifname_buf : QCSAPI_PRIMARY_WIFI_IFNAME;

	ret = qcsapi_interface_get_mac_addr(ifname, macaddr);

	if (ret < 0) {
		status = STATUS_ERROR;
		goto respond;
	}

	status = STATUS_COMPLETE;

respond:
	qtn_dut_make_response_macaddr(cmd_tag, status, ret, macaddr, out_len, out);
}

void qtn_handle_sta_get_info(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out)
{
	struct qtn_cmd_request cmd_req;
	int status;
	int ret;
	char ifname_buf[16];
	const char *ifname;
	char info_buf[128] = {0};
	int info_len = 0;

	ret = qtn_init_cmd_request(&cmd_req, cmd_tag, params, len);

	if (ret != 0) {
		status = STATUS_INVALID;
		goto respond;
	}

	*ifname_buf = 0;
	ret = qtn_get_value_text(&cmd_req, QTN_TOK_INTERFACE, ifname_buf, sizeof(ifname_buf));

	ifname = (ret > 0) ? ifname_buf : QCSAPI_PRIMARY_WIFI_IFNAME;

	ret = snprintf(info_buf + info_len, sizeof(info_buf) - info_len,
			"vendor,%s,build_name,%s", "Quantenna", QDRV_BLD_NAME);

	if (ret < 0) {
		status = STATUS_ERROR;
		goto respond;
	}

	info_len += ret;

	/* TODO: add other information */

	status = STATUS_COMPLETE;

respond:
	qtn_dut_make_response_vendor_info(cmd_tag, status, ret, info_buf, out_len, out);
}

void qtn_handle_sta_set_wireless(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out)
{
	struct qtn_cmd_request cmd_req;
	int status;
	int ret;
	char ifname_buf[16];
	const char *ifname;
	char cert_prog[32];
	int vht_prog;
	int feature_enable;
	int feature_val;
	char val_buf[128];
	char cmd[128];
	int conv_err = 0;
	struct qtn_dut_config *conf;

	ret = qtn_init_cmd_request(&cmd_req, cmd_tag, params, len);

	if (ret != 0) {
		status = STATUS_INVALID;
		goto respond;
	}

	*ifname_buf = 0;
	ret = qtn_get_value_text(&cmd_req, QTN_TOK_INTERFACE, ifname_buf, sizeof(ifname_buf));

	ifname = (ret > 0) ? ifname_buf : QCSAPI_PRIMARY_WIFI_IFNAME;
	conf = qtn_dut_get_config(ifname);

	ret = qtn_get_value_text(&cmd_req, QTN_TOK_PROGRAM, cert_prog, sizeof(cert_prog));
	if (ret <= 0) {
		/* mandatory parameter */
		status = STATUS_ERROR;
		goto respond;
	}

	vht_prog = (strcasecmp(cert_prog, "VHT") == 0) ? 1 : 0;

	/* ADDBA_REJECT, (enable/disable) */
	if (qtn_get_value_enable(&cmd_req, QTN_TOK_ADDBA_REJECT, &feature_enable, &conv_err) > 0) {
		char tmpbuf[64];
		int ba_control;

		/* ADDBA_REJECT:enabled => ADDBA.Request:disabled */
		ba_control = (feature_enable) ? 0 : 0xFFFF;

		snprintf(tmpbuf, sizeof(tmpbuf), "iwpriv %s ba_control %d", ifname, ba_control);
		system(tmpbuf);

	} else if (conv_err < 0) {
		ret = conv_err;
		status = STATUS_ERROR;
		goto respond;
	}

	/* AMPDU, (enable/disable) */
	if (qtn_get_value_enable(&cmd_req, QTN_TOK_AMPDU, &feature_enable, &conv_err) > 0) {
		char tmpbuf[64];
		int ba_control;

		/* AMPDU:enabled => ADDBA.Request:enabled */
		ba_control = (feature_enable) ? 0xFFFF : 0;

		snprintf(tmpbuf, sizeof(tmpbuf), "iwpriv %s ba_control %d", ifname, ba_control);
		system(tmpbuf);

		/* TODO: check if AuC is able to make AMSDU aggregation for VHT single AMPDU */

	} else if (conv_err < 0) {
		ret = conv_err;
		status = STATUS_ERROR;
		goto respond;
	}

	/* AMSDU, (enable/disable) */
	if (qtn_get_value_enable(&cmd_req, QTN_TOK_AMSDU, &feature_enable, &conv_err) > 0) {
		ret = qcsapi_wifi_set_tx_amsdu(ifname, feature_enable);

		if (ret < 0) {
			status = STATUS_ERROR;
			goto respond;
		}

	} else if (conv_err < 0) {
		ret = conv_err;
		status = STATUS_ERROR;
		goto respond;
	}

	/* STBC_RX, int (0/1) */
	if (qtn_get_value_int(&cmd_req, QTN_TOK_STBC_RX, &feature_val) > 0) {
		/* enable/disable STBC */
		ret = qcsapi_wifi_set_option(ifname, qcsapi_stbc, feature_val);

		if (ret < 0) {
			status = STATUS_ERROR;
			goto respond;
		}

		if (feature_val > 0) {
			/* TODO: set number of STBC Receive Streams */
		}
	}

	/* WIDTH, int (80/40/20) */
	if (qtn_get_value_int(&cmd_req, QTN_TOK_WIDTH, &feature_val) > 0) {
		/* channel width */
		ret = qcsapi_wifi_set_bw(ifname, (unsigned) feature_val);
		if (ret < 0) {
			status = STATUS_ERROR;
			qtn_log("can't set width to %d, error %d", feature_val, ret);
			goto respond;
		}

		snprintf(cmd, sizeof(cmd), "set_fixed_bw -b %d", feature_val);
		system(cmd);
	}

	/* SMPS, SM Power Save Mode, NOT Supported */
	if (qtn_get_value_int(&cmd_req, QTN_TOK_SMPS, &feature_val) > 0) {
		ret = -EOPNOTSUPP;

		if (ret < 0) {
			status = STATUS_ERROR;
			goto respond;
		}
	}

	/* TXSP_STREAM, (1SS/2SS/3SS) */
	if (qtn_get_value_text(&cmd_req, QTN_TOK_TXSP_STREAM, val_buf, sizeof(val_buf)) > 0) {
		int nss = 0;
		qcsapi_mimo_type mt = vht_prog ? qcsapi_mimo_vht : qcsapi_mimo_ht;

		ret = sscanf(val_buf, "%dSS", &nss);

		if (ret != 1) {
			ret = -EINVAL;
			status = STATUS_ERROR;
			goto respond;
		}

		ret = qcsapi_wifi_set_nss_cap(ifname, mt, nss);

		if (ret < 0) {
			status = STATUS_ERROR;
			goto respond;
		}
	}

	/* RXSP_STREAM, (1SS/2SS/3SS) */
	if (qtn_get_value_text(&cmd_req, QTN_TOK_RXSP_STREAM, val_buf, sizeof(val_buf)) > 0) {
		int nss = 0;
		qcsapi_mimo_type mt = vht_prog ? qcsapi_mimo_vht : qcsapi_mimo_ht;

		ret = sscanf(val_buf, "%dSS", &nss);

		if (ret != 1) {
			ret = -EINVAL;
			status = STATUS_ERROR;
			goto respond;
		}

		ret = qcsapi_wifi_set_nss_cap(ifname, mt, nss);

		if (ret < 0) {
			status = STATUS_ERROR;
			goto respond;
		}
	}

	/* Band, NOT Supported */
	if (qtn_get_value_text(&cmd_req, QTN_TOK_BAND, val_buf, sizeof(val_buf)) > 0) {
		ret = -EOPNOTSUPP;

		if (ret < 0) {
			status = STATUS_ERROR;
			goto respond;
		}
	}

	/* DYN_BW_SGNL, (enable/disable) */
	if (qtn_get_value_enable(&cmd_req, QTN_TOK_DYN_BW_SGNL, &feature_enable, &conv_err) > 0) {
		if (conf) {
			conf->bws_dynamic = (unsigned char)feature_enable;
			conf->update_settings = 1;
		} else {
			ret = -EFAULT;
			status = STATUS_ERROR;
			goto respond;
		}

	} else if (conv_err < 0) {
		ret = conv_err;
		status = STATUS_ERROR;
		goto respond;
	}

	/* BW_SGNL, (enable/disable) */
	if (qtn_get_value_enable(&cmd_req, QTN_TOK_BW_SGNL, &feature_enable, &conv_err) > 0) {
		if (conf) {
			conf->bws_enable = (unsigned char)feature_enable;
			conf->update_settings = 1;
		} else {
			ret = -EFAULT;
			status = STATUS_ERROR;
			goto respond;
		}

	} else if (conv_err < 0) {
		ret = conv_err;
		status = STATUS_ERROR;
		goto respond;
	}

	if (qtn_get_value_enable(&cmd_req, QTN_TOK_RTS_FORCE, &feature_enable, &conv_err) > 0) {
		if (conf) {
			conf->force_rts = (unsigned char)feature_enable;
			conf->update_settings = 1;
		} else {
			ret = -EFAULT;
			status = STATUS_ERROR;
			goto respond;
		}

	} else if (conv_err < 0) {
		ret = conv_err;
		status = STATUS_ERROR;
		goto respond;
	}


	if (conf && conf->update_settings) {
		qtn_set_rts_settings(ifname, conf);
	}

	/* SGI80, (enable/disable) */
	if (qtn_get_value_enable(&cmd_req, QTN_TOK_SGI80, &feature_enable, &conv_err) > 0) {
		/* disable dynamic GI selection */
		qcsapi_wifi_set_option(ifname, qcsapi_GI_probing, 0);
		/* ^^ ignore error since qcsapi_GI_probing does not work for RFIC6 */


		/* TODO: it sets general capability for short GI, not only SGI80 */
		ret = qcsapi_wifi_set_option(ifname, qcsapi_short_GI, feature_enable);

		if (ret < 0) {
			status = STATUS_ERROR;
			goto respond;
		}

	} else if (conv_err < 0) {
		ret = conv_err;
		status = STATUS_ERROR;
		goto respond;
	}

	/* TxBF, (enable/disable) */
	if (qtn_get_value_enable(&cmd_req, QTN_TOK_TXBF, &feature_enable, &conv_err) > 0) {
		/* TODO: check, that we enable/disable SU TxBF beamformee capability
		 * with explicit feedback */
		ret = qcsapi_wifi_set_option(ifname, qcsapi_beamforming, feature_enable);

		if (ret < 0) {
			status = STATUS_ERROR;
			goto respond;
		}

	} else if (conv_err < 0) {
		ret = conv_err;
		status = STATUS_ERROR;
		goto respond;
	}

	/* LDPC, (enable/disable) */
	if (qtn_get_value_enable(&cmd_req, QTN_TOK_LDPC, &feature_enable, &conv_err) > 0) {
		char tmpbuf[64];

		snprintf(tmpbuf, sizeof(tmpbuf), "iwpriv %s set_ldpc %d", ifname, feature_enable);
		system(tmpbuf);

		/* TODO: what about IEEE80211_PARAM_LDPC_ALLOW_NON_QTN ?
		 *       Allow non QTN nodes to use LDPC */

	} else if (conv_err < 0) {
		ret = conv_err;
		status = STATUS_ERROR;
		goto respond;
	}

	/* Opt_md_notif_ie, (NSS=1 & BW=20Mhz => 1;20) */
	if (qtn_get_value_text(&cmd_req, QTN_TOK_OPT_MD_NOTIF_IE, val_buf, sizeof(val_buf)) > 0) {
		int nss = 0;
		int bw = 0;
		uint8_t chwidth;
		uint8_t rxnss;
		uint8_t rxnss_type = 0;
		uint8_t vhtop_notif_mode;
		char tmpbuf[64];

		ret = sscanf(val_buf, "%d;%d", &nss, &bw);

		if (ret != 2) {
			ret = -EINVAL;
			status = STATUS_ERROR;
			goto respond;
		}

		switch (bw) {
		case 20:
			chwidth = IEEE80211_CWM_WIDTH20;
			break;
		case 40:
			chwidth = IEEE80211_CWM_WIDTH40;
			break;
		case 80:
			chwidth = IEEE80211_CWM_WIDTH80;
			break;
		default:
			ret = -EINVAL;
			status = STATUS_ERROR;
			goto respond;
		}

		if ((nss < 1) || (nss > IEEE80211_AC_MCS_NSS_MAX)) {
			ret = -EINVAL;
			status = STATUS_ERROR;
			goto respond;
		}

		rxnss = nss - 1;

		vhtop_notif_mode = SM(chwidth, IEEE80211_VHT_OPMODE_CHWIDTH) |
				SM(rxnss, IEEE80211_VHT_OPMODE_RXNSS) |
				SM(rxnss_type, IEEE80211_VHT_OPMODE_RXNSS_TYPE);

		snprintf(tmpbuf, sizeof(tmpbuf), "iwpriv %s set_vht_opmntf %d",
				ifname,
				vhtop_notif_mode);
		system(tmpbuf);
	}

	if (qtn_get_value_int(&cmd_req, QTN_TOK_MCS_FIXEDRATE, &feature_val) > 0) {
		char tmpbuf[64];
		snprintf(tmpbuf, sizeof(tmpbuf), "MCS%d", feature_val);
		ret = qcsapi_wifi_set_mcs_rate(ifname, tmpbuf);
		if (ret < 0) {
			status = STATUS_ERROR;
			qtn_error("can't set mcs to %d, error %d", feature_val, ret);
			goto respond;
		}
	}

	/* nss_mcs_cap, (nss_capabilty;mcs_capability => 2;0-9) */
	if (qtn_get_value_text(&cmd_req, QTN_TOK_NSS_MCS_CAP, val_buf, sizeof(val_buf)) > 0) {
		int nss = 0;
		int mcs_high = 0;
		int mcs_cap;
		char tmpbuf[64];

		ret = sscanf(val_buf, "%d;0-%d", &nss, &mcs_high);

		if (ret != 2) {
			ret = -EINVAL;
			status = STATUS_ERROR;
			goto respond;
		}

		/* NSS capability */
		ret = qcsapi_wifi_set_nss_cap(ifname, qcsapi_mimo_vht, nss);

		if (ret < 0) {
			status = STATUS_ERROR;
			goto respond;
		}

		/* MCS capability */
		switch (mcs_high) {
		case 7:
			mcs_cap = IEEE80211_VHT_MCS_0_7;
			break;
		case 8:
			mcs_cap = IEEE80211_VHT_MCS_0_8;
			break;
		case 9:
			mcs_cap = IEEE80211_VHT_MCS_0_9;
			break;
		default:
			ret = -EINVAL;
			status = STATUS_ERROR;
			goto respond;
		}

		snprintf(tmpbuf, sizeof(tmpbuf), "iwpriv %s set_vht_mcs_cap %d",
						ifname,
						mcs_cap);
		system(tmpbuf);
	}

	/* Tx_lgi_rate, int (0) */
	if (qtn_get_value_int(&cmd_req, QTN_TOK_TX_LGI_RATE, &feature_val) > 0) {
		/* setting Tx Highest Supported Long GI Data Rate
		 */
		if (feature_val != 0) {
			/* we support only 0 */
			ret = -EOPNOTSUPP;
			status = STATUS_ERROR;
			goto respond;
		}
	}

	/* Zero_crc (enable/disable) */
	if (qtn_get_value_enable(&cmd_req, QTN_TOK_ZERO_CRC, &feature_enable, &conv_err) > 0) {
		/* setting VHT SIGB CRC to fixed value (e.g. all "0") not supported
		 * for current hardware platform
		 * VHT SIGB CRC is always calculated
		 * tests: 4.2.26
		 */

		ret = -EOPNOTSUPP;

		if (ret < 0) {
			status = STATUS_ERROR;
			goto respond;
		}

	} else if (conv_err < 0) {
		ret = conv_err;
		status = STATUS_ERROR;
		goto respond;
	}

	/* Vht_tkip (enable/disable) */
	if (qtn_get_value_enable(&cmd_req, QTN_TOK_VHT_TKIP, &feature_enable, &conv_err) > 0) {
		/* enable TKIP in VHT mode
		 * Tests: 4.2.44
		 * Testbed Wi-Fi CERTIFIED ac with the capability of setting TKIP and VHT
		 * and ability to generate a probe request.
		 */
		char tmpbuf[64];

		snprintf(tmpbuf, sizeof(tmpbuf), "iwpriv %s set_vht_tkip %d",
				ifname, feature_enable);
		system(tmpbuf);

	} else if (conv_err < 0) {
		ret = conv_err;
		status = STATUS_ERROR;
		goto respond;
	}

	/* Vht_wep, (enable/disable), NOT USED IN TESTS (as STA testbed) */

	/* BW_SGNL, (enable/disable) */
	if (qtn_get_value_enable(&cmd_req, QTN_TOK_BW_SGNL, &feature_enable, &conv_err) > 0) {
		/* Tests: 4.2.51
		 * STA1: Testbed Wi-Fi CERTIFIED ac STA supporting the optional feature RTS
		 *       with BW signaling
		 */

		struct qtn_dut_config *conf = qtn_dut_get_config(ifname);

		if (conf) {
			conf->bws_enable = (unsigned char)feature_enable;
		} else {
			ret = -EFAULT;
			status = STATUS_ERROR;
			goto respond;
		}

	} else if (conv_err < 0) {
		ret = conv_err;
		status = STATUS_ERROR;
		goto respond;
	}

	/* MU_TxBF, (enable/disable) */
	if (qtn_get_value_enable(&cmd_req, QTN_TOK_MU_TXBF, &feature_enable, &conv_err) > 0) {
		/* TODO: enable/disable Multi User (MU) TxBF beamformee capability
		 * with explicit feedback
		 *
		 * Tests: 4.2.56
		 */
		int su_status = 0;
		if (feature_enable &&
			qcsapi_wifi_get_option(ifname, qcsapi_beamforming, &su_status) >= 0
			&& su_status == 0) {
			/* have to have SU enabled if we enable MU */
			qcsapi_wifi_set_option(ifname, qcsapi_beamforming, 1);
		}

		ret = qtn_set_mu_enable((unsigned)feature_enable);

		if (ret < 0) {
			status = STATUS_ERROR;
			goto respond;
		}

	} else if (conv_err < 0) {
		ret = conv_err;
		status = STATUS_ERROR;
		goto respond;
	}

	/* CTS_WIDTH, int (0) */
	if (qtn_get_value_int(&cmd_req, QTN_TOK_CTS_WIDTH, &feature_val) > 0) {
		char tmpbuf[64];

		snprintf(tmpbuf, sizeof(tmpbuf), "iwpriv %s set_cts_bw %d",
				ifname, feature_val);
		system(tmpbuf);
	}


	/* RTS_BWS, (enable/disable) */
	if (qtn_get_value_enable(&cmd_req, QTN_TOK_RTS_BWS, &feature_enable, &conv_err) > 0) {
		/* TODO: enable RTS with Bandwidth Signaling Feature
		 *
		 * Tests: 4.2.59
		 */

		ret = -EOPNOTSUPP;

		if (ret < 0) {
			status = STATUS_ERROR;
			goto respond;
		}

	} else if (conv_err < 0) {
		ret = conv_err;
		status = STATUS_ERROR;
		goto respond;
	}

	if (qtn_get_value_int(&cmd_req, QTN_TOK_TXBANDWIDTH, &feature_val) > 0 &&
		(ret = set_tx_bandwidth(ifname, feature_val)) < 0) {
		qtn_error("can't set bandwidth to %d, error %d", feature_val, ret);
		status = STATUS_ERROR;
		goto respond;
	}

	status = STATUS_COMPLETE;

respond:
	qtn_dut_make_response_none(cmd_tag, status, ret, out_len, out);
}

void qtn_handle_sta_set_rfeature(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out)
{
	struct qtn_cmd_request cmd_req;
	int status;
	int ret;
	char ifname[IFNAMSIZ];
	char val_str[128];
	int feature_val;
	int conv_err;
	int num_ss;
	int mcs;
	int need_tdls_channel_switch = 0;


	ret = qtn_init_cmd_request(&cmd_req, cmd_tag, params, len);
	if (ret != 0) {
		status = STATUS_INVALID;
		goto respond;
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_INTERFACE, ifname, sizeof(ifname)) <= 0) {
		snprintf(ifname, sizeof(ifname), "%s", QCSAPI_PRIMARY_WIFI_IFNAME);
	}

	status = STATUS_ERROR;

	if (qtn_get_value_text(&cmd_req, QTN_TOK_NSS_MCS_OPT, val_str, sizeof(val_str)) > 0 &&
		sscanf(val_str, "%d;%d", &num_ss, &mcs) == 2) {

		snprintf(val_str, sizeof(val_str), "MCS%d0%d", num_ss, mcs);
		if ((ret = qcsapi_wifi_set_mcs_rate(ifname, val_str)) < 0) {
			qtn_error("can't set mcs rate to %s, error %d", val_str, ret);
			goto respond;
		}
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_CHSWITCHMODE, val_str, sizeof(val_str)) > 0) {

		int mode = 0;
		if (strcasecmp(val_str, "Initiate") == 0) {
			mode = 0;
			need_tdls_channel_switch = 1;
		} else if (strcasecmp(val_str, "Passive") == 0) {
			char peer[128];
			mode = 2;

			if (qtn_get_value_text(&cmd_req, QTN_TOK_PEER, peer, sizeof(peer)) > 0) {
				qcsapi_wifi_set_tdls_params(ifname,
					qcsapi_tdls_chan_switch_off_chan, 0);
				qcsapi_wifi_set_tdls_params(ifname,
					qcsapi_tdls_chan_switch_off_chan_bw, 0);
				qcsapi_wifi_tdls_operate(ifname,
					qcsapi_tdls_oper_switch_chan, peer, 0);
			}
		}

		ret = qcsapi_wifi_set_tdls_params(ifname, qcsapi_tdls_chan_switch_mode, mode);
		if (ret < 0) {
			qtn_error("can't tdls_chan_switch_mode to %s, error %d", val_str, ret);
			goto respond;
		}
	}

	if (qtn_get_value_int(&cmd_req, QTN_TOK_OFFCHNUM, &feature_val) > 0) {
		ret = qcsapi_wifi_set_tdls_params(ifname, qcsapi_tdls_chan_switch_off_chan,
				feature_val);
		if (ret < 0) {
			qtn_error("can't off_chan to %d, error %d", feature_val, ret);
			goto respond;
		}
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_SECCHOFFSET, val_str, sizeof(val_str)) > 0) {
		int off_chan_bw = -1;
		qcsapi_unsigned_int currect_bw = 0;
		if (strcasecmp(val_str, "40above") == 0 || strcasecmp(val_str, "40below") == 0) {
			off_chan_bw = 40;
		} else if (strcasecmp(val_str, "20") == 0) {
			off_chan_bw = 20;
		}

		qcsapi_wifi_get_bw(ifname, &currect_bw);
		if (currect_bw < off_chan_bw) {
			qcsapi_wifi_set_bw(ifname, off_chan_bw);
		}

		ret = qcsapi_wifi_set_tdls_params(ifname, qcsapi_tdls_chan_switch_off_chan_bw,
							off_chan_bw);
		if (ret < 0) {
			qtn_error("can't tdls_chan_switch_off_chan_bw to %d, error %d",
				off_chan_bw, ret);
			goto respond;
		}
	}

	if (need_tdls_channel_switch &&
		qtn_get_value_text(&cmd_req, QTN_TOK_PEER, val_str, sizeof(val_str)) > 0) {
		ret = qcsapi_wifi_tdls_operate(ifname, qcsapi_tdls_oper_switch_chan, val_str, 1000);
		if (ret < 0) {
			qtn_error("can't run switch_chan, error %d", ret);
			goto respond;
		}
	}

	if (qtn_get_value_enable(&cmd_req, QTN_TOK_UAPSD, &feature_val, &conv_err) > 0) {
		ret = qcsapi_wifi_set_option(ifname, qcsapi_uapsd, feature_val);
		if (ret < 0) {
			qtn_error("can't set uapsd to %d, error %d", feature_val, ret);
			goto respond;
		}
	} else if (conv_err < 0) {
		ret = conv_err;
		goto respond;
	}

	status = ret >= 0 ? STATUS_COMPLETE : STATUS_ERROR;
respond:
	qtn_dut_make_response_none(cmd_tag, status, ret, out_len, out);
}

void qtn_handle_sta_set_ip_config(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out)
{
	struct qtn_cmd_request cmd_req;
	int status;
	int ret;

	ret = qtn_init_cmd_request(&cmd_req, cmd_tag, params, len);
	if (ret != 0) {
		status = STATUS_INVALID;
		goto respond;
	}

	/* empty for now */


	status = ret >= 0 ? STATUS_COMPLETE : STATUS_ERROR;
respond:
	qtn_dut_make_response_none(cmd_tag, status, ret, out_len, out);
}

void qtn_handle_sta_set_psk(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out)
{
	struct qtn_cmd_request cmd_req;
	int status = STATUS_INVALID;
	int ret;
	char ifname[IFNAMSIZ];
	char ssid_str[128];
	char pass_str[128];
	char key_type[128];
	char enc_type[128];
	char pmf_type[128];

	ret = qtn_init_cmd_request(&cmd_req, cmd_tag, params, len);
	if (ret != 0) {
		status = STATUS_INVALID;
		goto respond;
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_INTERFACE, ifname, sizeof(ifname)) <= 0) {
		snprintf(ifname, sizeof(ifname), "%s", QCSAPI_PRIMARY_WIFI_IFNAME);
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_SSID, ssid_str, sizeof(ssid_str)) <= 0) {
		qtn_error("can't get ssid");
		goto respond;
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_PASSPHRASE, pass_str, sizeof(pass_str)) <= 0) {
		qtn_error("can't get pass phrase");
		goto respond;
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_KEYMGMTTYPE, key_type, sizeof(key_type)) <= 0) {
		qtn_error("can't get pass key_type");
		goto respond;
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_ENCPTYPE, enc_type, sizeof(enc_type)) <= 0) {
		qtn_error("can't get enc_type");
		goto respond;
	}

	status = STATUS_ERROR;

	if (qcsapi_SSID_verify_SSID(ifname, ssid_str) < 0 &&
			(ret = qcsapi_SSID_create_SSID(ifname, ssid_str)) < 0) {
		qtn_error("can't create SSID %s, error %d", ssid_str, ret);
		goto respond;
	}

	if ((ret = set_sta_encryption(ifname, ssid_str, enc_type)) < 0) {
		qtn_error("can't set enc to %s, error %d", enc_type, ret);
		goto respond;
	}

	if ((ret = qcsapi_SSID_set_authentication_mode(ifname, ssid_str, "PSKAuthentication")) < 0) {
		qtn_error("can't set PSK authentication, error %d", ret);
		goto respond;
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_PMF, pmf_type, sizeof(pmf_type)) > 0) {
		int pmf_cap = -1;
		/* pmf_cap values according to wpa_supplicant manual:
			0 = disabled (default unless changed with the global pmf parameter)
			1 = optional
			2 = required
		*/

		if (strcasecmp(pmf_type, "Required") == 0
			|| strcasecmp(pmf_type, "Forced_Required") == 0) {
			pmf_cap = 2;
		} else if (strcasecmp(pmf_type, "Optional") == 0) {
			pmf_cap = 1;
		} else if (strcasecmp(pmf_type, "Disable") == 0
			|| strcasecmp(pmf_type, "Forced_Disabled") == 0) {
			pmf_cap = 0;
		}

		if (pmf_cap != -1 && (ret = qcsapi_SSID_set_pmf(ifname, ssid_str, pmf_cap)) < 0) {
			qtn_error("can't set pmf to %d, error %d, ssid %s", pmf_cap, ret, ssid_str);
			goto respond;
		}

		if (pmf_cap > 0 && (ret = qcsapi_SSID_set_authentication_mode(
			ifname, ssid_str, "SHA256PSKAuthenticationMixed")) < 0) {
			qtn_error("can't set authentication for PMF, error %d, ssid %s",
					ret, ssid_str);
			goto respond;
		}
	}

	/* possible values for key_type: wpa/wpa2/wpa-psk/wpa2-psk/wpa2-ft/wpa2-wpa-psk */
	const int is_psk = strcasecmp(key_type, "wpa-psk") == 0 ||
				strcasecmp(key_type, "wpa2-psk") == 0 ||
				strcasecmp(key_type, "wpa2-wpa-psk") == 0;

	if (is_psk && (ret = qcsapi_SSID_set_pre_shared_key(ifname, ssid_str, 0, pass_str)) < 0) {
		qtn_error("can't set psk: ifname %s, ssid %s, key_type %s, pass %s, error %d",
			ifname, ssid_str, key_type, pass_str, ret);
	} else if (!is_psk &&
			(ret = qcsapi_SSID_set_key_passphrase(ifname, ssid_str, 0, pass_str)) < 0) {
		qtn_error("can't set pass: ifname %s, ssid %s, key_type %s, pass %s, error %d",
			ifname, ssid_str, key_type, pass_str, ret);
	}

	status = ret >= 0 ? STATUS_COMPLETE : STATUS_ERROR;
respond:
	qtn_dut_make_response_none(cmd_tag, status, ret, out_len, out);
}

void qtn_handle_sta_associate(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out)
{
	struct qtn_cmd_request cmd_req;
	int status;
	int ret;
	char ifname[IFNAMSIZ];
	char ssid_str[128];

	ret = qtn_init_cmd_request(&cmd_req, cmd_tag, params, len);
	if (ret != 0) {
		status = STATUS_INVALID;
		goto respond;
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_INTERFACE, ifname, sizeof(ifname)) <= 0) {
		snprintf(ifname, sizeof(ifname), "%s", QCSAPI_PRIMARY_WIFI_IFNAME);
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_SSID, ssid_str, sizeof(ssid_str)) <= 0) {
		qtn_error("can't get ssid");
		status = STATUS_INVALID;
		goto respond;
	}

	if ((ret = qcsapi_wifi_associate(ifname, ssid_str)) < 0) {
		qtn_error("can't associate, ifname %s, ssid %s, error %d", ifname, ssid_str, ret);
	}

	status = ret >= 0 ? STATUS_COMPLETE : STATUS_ERROR;
respond:
	qtn_dut_make_response_none(cmd_tag, status, ret, out_len, out);
}

void qtn_handle_sta_set_encryption(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out)
{
	struct qtn_cmd_request cmd_req;
	int status;
	int ret = 0;
	char ifname[IFNAMSIZ];
	char ssid_str[128];
	char encryption[128];

	ret = qtn_init_cmd_request(&cmd_req, cmd_tag, params, len);
	if (ret != 0) {
		status = STATUS_INVALID;
		goto respond;
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_INTERFACE, ifname, sizeof(ifname)) <= 0) {
		snprintf(ifname, sizeof(ifname), "%s", QCSAPI_PRIMARY_WIFI_IFNAME);
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_SSID, ssid_str, sizeof(ssid_str)) <= 0) {
		qtn_error("can't get ssid");
		status = STATUS_INVALID;
		goto respond;
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_ENCPTYPE, encryption, sizeof(encryption)) <= 0) {
		qtn_error("can't get encryption");
		status = STATUS_INVALID;
		goto respond;
	}

	status = STATUS_ERROR;

	if (strcasecmp(encryption, "wep") == 0) {
		qtn_log("wep is not supported");
		ret = -EINVAL;
		goto respond;
	}

	if (qcsapi_SSID_verify_SSID(ifname, ssid_str) < 0 &&
			(ret = qcsapi_SSID_create_SSID(ifname, ssid_str)) < 0) {
		qtn_error("can't create SSID %s, error %d", ssid_str, ret);
		goto respond;
	}

	if (strcasecmp(encryption, "none") == 0 &&
			(ret = qcsapi_SSID_set_authentication_mode(ifname, ssid_str, "NONE")) < 0) {
		qtn_log("can't set authentication to %s, ssid %s error %d",
				encryption, ssid_str, ret);
	}

	status = ret >= 0 ? STATUS_COMPLETE : STATUS_ERROR;
respond:
	qtn_dut_make_response_none(cmd_tag, status, ret, out_len, out);
}

static
int qtn_send_vht_opmode_action(const char* ifname, const unsigned char *dest_mac, int cbw, int nss)
{
	struct iwreq iwr;
	unsigned char frame_buf[64];
	struct app_action_frame_buf *action_frm = (struct app_action_frame_buf*)frame_buf;
	int ioctl_sock;
	uint8_t chwidth;
	uint8_t rxnss;
	uint8_t rxnss_type = 0;
	uint8_t vhtop_notif_mode;
	int ret;

	switch (cbw) {
	case 20:
		chwidth = IEEE80211_CWM_WIDTH20;
		break;
	case 40:
		chwidth = IEEE80211_CWM_WIDTH40;
		break;
	case 80:
		chwidth = IEEE80211_CWM_WIDTH80;
		break;
	default:
		return -EINVAL;
	}

	if ((nss < 1) || (nss > IEEE80211_AC_MCS_NSS_MAX)) {
		return -EINVAL;
	}

	rxnss = nss - 1;

	vhtop_notif_mode = SM(chwidth, IEEE80211_VHT_OPMODE_CHWIDTH) |
			SM(rxnss, IEEE80211_VHT_OPMODE_RXNSS) |
			SM(rxnss_type, IEEE80211_VHT_OPMODE_RXNSS_TYPE);

	ioctl_sock = socket(PF_INET, SOCK_DGRAM, 0);

	if (ioctl_sock < 0)
		return -errno;

	action_frm->cat = IEEE80211_ACTION_CAT_VHT;
	action_frm->action = IEEE80211_ACTION_VHT_OPMODE_NOTIFICATION;
	memcpy(action_frm->dst_mac_addr, dest_mac, IEEE80211_ADDR_LEN);
	action_frm->frm_payload.length = 1;
	action_frm->frm_payload.data[0] = vhtop_notif_mode;

	/* send action frame */
	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, ifname, IFNAMSIZ);

	iwr.u.data.flags = SIOCDEV_SUBIO_SEND_ACTION_FRAME;
	iwr.u.data.pointer = action_frm;
	iwr.u.data.length = sizeof(struct app_action_frame_buf) + action_frm->frm_payload.length;

	ret = ioctl(ioctl_sock, IEEE80211_IOCTL_EXT, &iwr);
	if (ret < 0) {
		qtn_error("failed to send action frame");
	}

	close(ioctl_sock);

	return ret;
}

void qtn_handle_dev_send_frame(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out)
{
	struct qtn_cmd_request cmd_req;
	int status;
	int ret;
	char ifname[IFNAMSIZ];
	char program[16];
	char tmpbuf[128];
	char peer[128];
	unsigned char dest_mac[IEEE80211_ADDR_LEN];
	int chan_width;
	int nss;

	ret = qtn_init_cmd_request(&cmd_req, cmd_tag, params, len);
	if (ret != 0) {
		status = STATUS_INVALID;
		goto respond;
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_INTERFACE, ifname, sizeof(ifname)) <= 0) {
		snprintf(ifname, sizeof(ifname), "%s", QCSAPI_PRIMARY_WIFI_IFNAME);
	}

	status = STATUS_ERROR;

	ret = qtn_get_value_text(&cmd_req, QTN_TOK_PROGRAM, program, sizeof(program));
	if (ret <= 0) {
		/* mandatory parameter */
		qtn_error("can't get program");
		status = STATUS_ERROR;
		goto respond;
	}

	if (strcasecmp(program, "VHT") == 0) {
		/* Two mandatory arguments: FrameName and Dest_mac */
		ret = qtn_get_value_text(&cmd_req, QTN_TOK_FRAMENAME, tmpbuf,
				sizeof(tmpbuf));

		if (ret <= 0) {
			qtn_error("can't get frame_name");
			status = STATUS_ERROR;
			goto respond;
		}

		/* we support only "Op_md_notif_frm" */
		if (strcasecmp(tmpbuf, "Op_md_notif_frm") != 0) {
			qtn_error("support only Op_md_notif_frm");
			ret = -EOPNOTSUPP;
			status = STATUS_ERROR;
			goto respond;
		}

		ret = qtn_get_value_text(&cmd_req, QTN_TOK_DEST_MAC, tmpbuf, sizeof(tmpbuf));
		if (ret <= 0) {
			qtn_error("can't get dest_mac");
			status = STATUS_ERROR;
			goto respond;
		}

		ret = qtn_parse_mac(tmpbuf, dest_mac);
		if (ret < 0) {
			qtn_error("invalid macaddr");
			status = STATUS_ERROR;
			goto respond;
		}

		/* optional arguments */
		if (qtn_get_value_int(&cmd_req, QTN_TOK_CHANNEL_WIDTH, &chan_width) <= 0) {
			/* get current bw capability */
			qcsapi_unsigned_int bw_cap;

			ret = qcsapi_wifi_get_bw(ifname, &bw_cap);

			if (ret < 0) {
				qtn_error("unable to get bw capability");
				status = STATUS_ERROR;
				goto respond;
			}

			chan_width = (int)bw_cap;
		}

		if (qtn_get_value_int(&cmd_req, QTN_TOK_NSS, &nss) <= 0) {
			/* get current nss capability */
			qcsapi_unsigned_int nss_cap;

			ret = qcsapi_wifi_get_nss_cap(ifname, qcsapi_mimo_vht, &nss_cap);

			if (ret < 0) {
				qtn_error("unable to get nss capability");
				status = STATUS_ERROR;
				goto respond;
			}

			nss = (int)nss_cap;
		}

		/* send action frame */
		ret = qtn_send_vht_opmode_action(ifname, dest_mac, chan_width, nss);

		if (ret < 0) {
			status = STATUS_ERROR;
			goto respond;
		}
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_TYPE, tmpbuf, sizeof(tmpbuf)) > 0 &&
		qtn_get_value_text(&cmd_req, QTN_TOK_PEER, peer, sizeof(peer)) > 0) {

		qcsapi_tdls_oper oper = qcsapi_tdls_nosuch_oper;

		if (strcasecmp(tmpbuf, "Setup") == 0) {
			oper = qcsapi_tdls_oper_setup;
		} else if (strcasecmp(tmpbuf, "channelSwitchReq") == 0) {
			oper = qcsapi_tdls_oper_switch_chan;
		} else if (strcasecmp(tmpbuf, "discovery") == 0) {
			oper = qcsapi_tdls_oper_discover;
		}  else if (strcasecmp(tmpbuf, "teardown") == 0) {
			oper = qcsapi_tdls_oper_teardown;
		}

		if (oper != qcsapi_tdls_nosuch_oper &&
				(ret = qcsapi_wifi_tdls_operate(ifname, oper, peer, 1000)) < 0) {
			qtn_error("can't set tdls_operate to %s, error %d", tmpbuf, ret);
			goto respond;
		}
	}

	status = STATUS_COMPLETE;

respond:
	qtn_dut_make_response_none(cmd_tag, status, ret, out_len, out);
}

void qtn_handle_sta_reassoc(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out)
{
	struct qtn_cmd_request cmd_req;
	int status = STATUS_INVALID;
	int ret = 0;
	char ifname[IFNAMSIZ];
	char bssid[64];

	ret = qtn_init_cmd_request(&cmd_req, cmd_tag, params, len);
	if (ret != 0) {
		goto respond;
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_INTERFACE, ifname, sizeof(ifname)) <= 0) {
		snprintf(ifname, sizeof(ifname), "%s", QCSAPI_PRIMARY_WIFI_IFNAME);
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_BSSID, bssid, sizeof(bssid)) <= 0) {
		qtn_error("can't get bssid");
		goto respond;
	}

	if ((ret = qcsapi_wifi_reassociate(ifname)) < 0) {
		qtn_error("can't reassociate, error %d", ret);
	}

	status = ret >= 0 ? STATUS_COMPLETE : STATUS_ERROR;
respond:
	qtn_dut_make_response_none(cmd_tag, status, ret, out_len, out);
}

void qtn_handle_sta_set_systime(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out)
{
	struct qtn_cmd_request cmd_req;
	int status = STATUS_INVALID;
	int ret = 0;
	char cmd[128];
	int month;
	int date;
	int year;
	int hours;
	int minutes;
	int seconds;

	ret = qtn_init_cmd_request(&cmd_req, cmd_tag, params, len);
	if (ret != 0) {
		goto respond;
	}

	if (qtn_get_value_int(&cmd_req, QTN_TOK_YEAR, &year) <= 0) {
		qtn_error("can't get year");
		goto respond;
	}

	if (qtn_get_value_int(&cmd_req, QTN_TOK_MONTH, &month) <= 0) {
		qtn_error("can't get month");
		goto respond;
	}

	if (qtn_get_value_int(&cmd_req, QTN_TOK_DATE, &date) <= 0) {
		qtn_error("can't get date");
		goto respond;
	}

	if (qtn_get_value_int(&cmd_req, QTN_TOK_HOURS, &hours) <= 0) {
		qtn_error("can't get hours");
		goto respond;
	}

	if (qtn_get_value_int(&cmd_req, QTN_TOK_MINUTES, &minutes) <= 0) {
		qtn_error("can't get minutes");
		goto respond;
	}

	if (qtn_get_value_int(&cmd_req, QTN_TOK_SECONDS, &seconds) <= 0) {
		qtn_error("can't get seconds");
		goto respond;
	}

	snprintf(cmd, sizeof(cmd), "date -s %2.2d%2.2d%2.2d%2.2d%4.4d.%2.2d",
		month, date, hours, minutes, year, seconds);
	ret = system(cmd);
	if (ret != 0) {
		qtn_error("can't set time. error %d, cmd %s", ret, cmd);
	}

	status = ret >= 0 ? STATUS_COMPLETE : STATUS_ERROR;
respond:
	qtn_dut_make_response_none(cmd_tag, status, ret, out_len, out);
}

void qtn_handle_sta_set_radio(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out)
{
	struct qtn_cmd_request cmd_req;
	int status = STATUS_INVALID;
	int ret = 0;
	char mode[64];

	ret = qtn_init_cmd_request(&cmd_req, cmd_tag, params, len);
	if (ret != 0) {
		goto respond;
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_MODE, mode, sizeof(mode)) <= 0) {
		qtn_error("can't get mode");
		goto respond;
	}

	if ((ret = qcsapi_wifi_rfenable(strcasecmp(mode, "On") == 0 ? 1 : 0)) < 0) {
		qtn_error("can't set rf to %s, error %d", mode, ret);
	}

	status = ret >= 0 ? STATUS_COMPLETE : STATUS_ERROR;
respond:
	qtn_dut_make_response_none(cmd_tag, status, ret, out_len, out);
}

void qtn_handle_sta_set_macaddr(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out)
{
	struct qtn_cmd_request cmd_req;
	int status = STATUS_INVALID;
	int ret = 0;
	char ifname[IFNAMSIZ];
	char mac_str[64];
	qcsapi_mac_addr mac;

	ret = qtn_init_cmd_request(&cmd_req, cmd_tag, params, len);
	if (ret != 0) {
		goto respond;
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_INTERFACE, ifname, sizeof(ifname)) <= 0) {
		qtn_error("can't get ifname");
		goto respond;
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_MAC, mac_str, sizeof(mac_str)) <= 0) {
		qtn_error("can't get mac");
		goto respond;
	}

	if (sscanf(mac_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
			&mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6) {
		qtn_error("can't parse mac_str %s", mac_str);
		goto respond;
	}

	qtn_log("try to set mac on %s to %s", ifname, mac_str);

	if ((ret = qcsapi_interface_set_mac_addr(ifname, mac)) < 0) {
		qtn_error("can't set mac to %s, error %d", mac_str, ret);
	}

	status = ret >= 0 ? STATUS_COMPLETE : STATUS_ERROR;
respond:
	qtn_dut_make_response_none(cmd_tag, status, ret, out_len, out);
}

void qtn_handle_sta_set_uapsd(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out)
{
	struct qtn_cmd_request cmd_req;
	int status = STATUS_INVALID;
	int ret = 0;
	char ifname[IFNAMSIZ];
	char cmd[128];
	int maxsplength = 4;
	int acbe = 1;
	int acbk = 1;
	int acvi = 1;
	int acvo = 1;

	ret = qtn_init_cmd_request(&cmd_req, cmd_tag, params, len);
	if (ret != 0) {
		goto respond;
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_INTERFACE, ifname, sizeof(ifname)) <= 0) {
		qtn_error("can't get ifname");
		goto respond;
	}

	qtn_get_value_int(&cmd_req, QTN_TOK_MAXSPLENGTH, &maxsplength);
	qtn_get_value_int(&cmd_req, QTN_TOK_ACBE, &acbe);
	qtn_get_value_int(&cmd_req, QTN_TOK_ACBK, &acbk);
	qtn_get_value_int(&cmd_req, QTN_TOK_ACVI, &acvi);
	qtn_get_value_int(&cmd_req, QTN_TOK_ACVO, &acvo);

	uint8_t uapsdinfo = WME_CAPINFO_UAPSD_EN;
	if (acbe) {
		uapsdinfo |= WME_CAPINFO_UAPSD_BE;
	}

	if (acbk) {
		uapsdinfo |= WME_CAPINFO_UAPSD_BK;
	}

	if (acvi) {
		uapsdinfo |= WME_CAPINFO_UAPSD_VI;
	}

	if (acvo) {
		uapsdinfo |= WME_CAPINFO_UAPSD_VO;
	}

	uapsdinfo |= (maxsplength & WME_CAPINFO_UAPSD_MAXSP_MASK) << WME_CAPINFO_UAPSD_MAXSP_SHIFT;

	snprintf(cmd, sizeof(cmd), "iwpriv %s setparam %d %d",
			ifname, IEEE80211_PARAM_UAPSDINFO, uapsdinfo);
	ret = system(cmd);

	status = ret >= 0 ? STATUS_COMPLETE : STATUS_ERROR;
respond:
	qtn_dut_make_response_none(cmd_tag, status, ret, out_len, out);
}

void qtn_handle_sta_reset_parm(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out)
{
	struct qtn_cmd_request cmd_req;
	int status = STATUS_INVALID;
	int ret = 0;
	char ifname[IFNAMSIZ];
	char arp[64];
	char cmd[128];

	ret = qtn_init_cmd_request(&cmd_req, cmd_tag, params, len);
	if (ret != 0) {
		goto respond;
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_INTERFACE, ifname, sizeof(ifname)) <= 0) {
		qtn_error("can't get ifname");
		goto respond;
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_ARP, ifname, sizeof(ifname)) <= 0) {
		qtn_error("can't get arp");
		goto respond;
	}

	if (strcasecmp(arp, "all") == 0) {
		snprintf(cmd, sizeof(cmd), "for ip in `grep %s /proc/net/arp | awk '{print $1}'`; "
				"do arp -i %s -d $ip; done", ifname, ifname);
	} else {
		snprintf(cmd, sizeof(cmd), "arp -i %s -d %s", ifname, arp);
	}

	ret = system(cmd);
	status = ret >= 0 ? STATUS_COMPLETE : STATUS_ERROR;
respond:
	qtn_dut_make_response_none(cmd_tag, status, ret, out_len, out);
}

void qtn_handle_sta_set_11n(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out)
{
	struct qtn_cmd_request cmd_req;
	int status = STATUS_INVALID;
	int ret = 0;
	char ifname[IFNAMSIZ];
	char width_str[128];

	ret = qtn_init_cmd_request(&cmd_req, cmd_tag, params, len);
	if (ret != 0) {
		goto respond;
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_INTERFACE, ifname, sizeof(ifname)) <= 0) {
		qtn_error("can't get ifname");
		goto respond;
	}

	status = STATUS_ERROR;

	if (qtn_get_value_text(&cmd_req, QTN_TOK_WIDTH, width_str, sizeof(width_str)) <= 0) {
		qcsapi_unsigned_int bw;
		if (strcasecmp(width_str, "auto") == 0) {
			bw = 40;
		} else {
			sscanf(width_str, "%u", &bw);
		}

		if ((ret = qcsapi_wifi_set_bw(ifname, bw)) < 0) {
			qtn_error("can't set bw to %d, error %d", bw, ret);
			goto respond;
		}
	}

	int tx_ss = -1;
	int rx_ss = -1;

	qtn_get_value_int(&cmd_req, QTN_TOK_TXSP_STREAM, &tx_ss);
	qtn_get_value_int(&cmd_req, QTN_TOK_RXSP_STREAM, &rx_ss);

	if (tx_ss == rx_ss && tx_ss != -1) {
		/* sta_set_11n is used only for 11n, so hardcode qcsapi_mimo_ht */
		ret = qcsapi_wifi_set_nss_cap(ifname, qcsapi_mimo_ht, tx_ss);
		if (ret < 0) {
			qtn_error("can't set NSS to %d, error %d", tx_ss, ret);
		}
	} else if (tx_ss != -1 || rx_ss != -1) {
		qtn_error("can't handle number of SS separatly for RX and TX");
		ret = -EINVAL;
	}

	status = ret >= 0 ? STATUS_COMPLETE : STATUS_ERROR;
respond:
	qtn_dut_make_response_none(cmd_tag, status, ret, out_len, out);
}

void qtn_handle_sta_set_power_save(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out)
{
	struct qtn_cmd_request cmd_req;
	int status = STATUS_INVALID;
	int ret = 0;
	char ifname[IFNAMSIZ];
	char val_str[128];

	ret = qtn_init_cmd_request(&cmd_req, cmd_tag, params, len);
	if (ret != 0) {
		goto respond;
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_INTERFACE, ifname, sizeof(ifname)) <= 0) {
		qtn_error("can't get ifname");
		goto respond;
	}

	status = STATUS_ERROR;

	if (qtn_get_value_text(&cmd_req, QTN_TOK_POWERSAVE, val_str, sizeof(val_str)) > 0) {
		if (strcasecmp(val_str, "off") == 0) {
			// power save does not exist by default
			ret = 0;
		} else {
			qtn_error("can't set power save to %s since poser save is not supported",
				val_str);
			ret = -EOPNOTSUPP;
			goto respond;
		}
	}


	status = ret >= 0 ? STATUS_COMPLETE : STATUS_ERROR;
respond:
	qtn_dut_make_response_none(cmd_tag, status, ret, out_len, out);
}

void qtn_handle_sta_set_sleep(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out)
{
	struct qtn_cmd_request cmd_req;
	int status = STATUS_INVALID;
	int ret = 0;
	char ifname[IFNAMSIZ];
	char cmd[128];

	ret = qtn_init_cmd_request(&cmd_req, cmd_tag, params, len);
	if (ret != 0) {
		goto respond;
	}

	if (qtn_get_value_text(&cmd_req, QTN_TOK_INTERFACE, ifname, sizeof(ifname)) <= 0) {
		snprintf(ifname, sizeof(ifname), "%s", QCSAPI_PRIMARY_WIFI_IFNAME);

	}

	status = STATUS_ERROR;

	snprintf(cmd, sizeof(cmd), "iwpriv %s sleep 0", ifname);
	ret = system(cmd);
	if (ret != 0) {
		qtn_error("can't set sleep, error %d", ret);
		goto respond;
	}

	status = ret >= 0 ? STATUS_COMPLETE : STATUS_ERROR;
respond:
	qtn_dut_make_response_none(cmd_tag, status, ret, out_len, out);

}

void qtn_handle_device_get_info(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out)
{
	struct qtn_cmd_request cmd_req;
	int status;
	int ret;
	char info_buf[128] = {0};
	char firmware_version[128] = {0};
	string_64 hw_version;
	static const char vendor[] = "Quantenna";

	ret = qtn_init_cmd_request(&cmd_req, cmd_tag, params, len);
	if (ret != 0) {
		status = STATUS_INVALID;
		goto respond;
	}

	ret = qcsapi_firmware_get_version(firmware_version, sizeof(firmware_version));
	if (ret < 0) {
		qtn_error("can't get fw version, error %d", ret);
		status = STATUS_ERROR;
		goto respond;
	}

	ret = qcsapi_get_board_parameter(qcsapi_hw_id, hw_version);
	if (ret < 0) {
		qtn_error("can't get HW id, error %d", ret);
		status = STATUS_ERROR;
		goto respond;
	}

	ret = snprintf(info_buf, sizeof(info_buf),
			"vendor,%s,model,%s,version,%s", vendor, hw_version, firmware_version);
	if (ret < 0) {
		status = STATUS_ERROR;
		goto respond;
	}

	status = STATUS_COMPLETE;

respond:
	qtn_dut_make_response_vendor_info(cmd_tag, status, ret, info_buf, out_len, out);
}

