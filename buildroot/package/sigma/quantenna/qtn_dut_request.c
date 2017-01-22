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

#include "common/qsigma_log.h"
#include "common/qsigma_tags.h"
#include "qtn_dut_ap_handler.h"
#include "qtn_dut_sta_handler.h"

void qtn_handle_dut_req(int tag, int len, unsigned char *params, int *out_len, unsigned char *out)
{
	qtn_log("handle cmd, tag %x, len %x", tag, len);

	switch (tag) {
	case QSIGMA_AP_GET_INFO_TAG:
		qtn_handle_ap_get_info(len, params, out_len, out);
		break;
	case QSIGMA_AP_SET_RADIUS_TAG:
		qtn_handle_ap_set_radius(len, params, out_len, out);
		break;
	case QSIGMA_AP_SET_WIRELESS_TAG:
		qtn_handle_ap_set_wireless(len, params, out_len, out);
		break;

	case QSIGMA_AP_SET_SECURITY_TAG:
		qtn_handle_ap_set_security(len, params, out_len, out);
		break;

	case QSIGMA_AP_RESET_DEFAULT_TAG:
		qtn_handle_ap_reset_default(tag, len, params, out_len, out);
		break;

	case QSIGMA_AP_SET_11N_WIRELESS_TAG:
		qtn_handle_ap_set_11n_wireless(tag, len, params, out_len, out);
		break;

	case QSIGMA_AP_REBOOT_TAG:
		qtn_handle_ap_reset(len, params, out_len, out);
		break;

	case QSIGMA_AP_SET_APQOS_TAG:
		qtn_handle_ap_set_qos(tag, len, params, out_len, out);
		break;

	case QSIGMA_AP_SET_STAQOS_TAG:
		qtn_handle_ap_set_qos(tag, len, params, out_len, out);
		break;

	case QSIGMA_AP_CONFIG_COMMIT_TAG:
		qtn_handle_ap_config_commit(tag, len, params, out_len, out);
		break;

	case QSIGMA_AP_CA_VERSION_TAG:
		qtn_handle_ca_version(tag, len, params, out_len, out);
		break;

	case QSIGMA_AP_GET_MAC_ADDRESS_TAG:
		qtn_handle_ap_get_mac_address(tag, len, params, out_len, out);
		break;

	case QSIGMA_AP_SET_HS2_TAG:
		qtn_handle_ap_set_hs2(tag, len, params, out_len, out);
		break;

	case QSIGMA_AP_DEAUTH_STA_TAG:
		qtn_handle_ap_deauth_sta(tag, len, params, out_len, out);
		break;

	case QSIGMA_AP_SET_11D_TAG:
		qtn_handle_ap_set_11d(tag, len, params, out_len, out);
		break;

	case QSIGMA_AP_SET_11H_TAG:
		qtn_handle_ap_set_11h(tag, len, params, out_len, out);
		break;

	case QSIGMA_AP_SET_RFEATURE_TAG:
		qtn_handle_ap_set_rfeature(tag, len, params, out_len, out);
		break;

	case QSIGMA_AP_SET_PMF_TAG:
		qtn_handle_ap_set_pmf(len, params, out_len, out);
		break;

	case QSIGMA_STA_CA_GET_VERSION_TAG:
		qtn_handle_ca_version(tag, len, params, out_len, out);
		break;

	case QSIGMA_STA_RESET_DEFAULT_TAG:
		qtn_handle_sta_reset_default(tag, len, params, out_len, out);
		break;

	case QSIGMA_STA_DEVICE_LIST_INTERFACES_TAG:
		qnat_sta_device_list_interfaces(tag, len, params, out_len, out);
		break;

	case QSIGMA_STA_PRESET_TESTPARAMETERS_TAG:
		qtn_handle_sta_preset_testparameters(tag, len, params, out_len, out);
		break;

	case QSIGMA_STA_DISCONNECT_TAG:
		qtn_handle_sta_disconnect(tag, len, params, out_len, out);
		break;

	case QSIGMA_STA_SEND_ADDBA_TAG:
		qtn_handle_sta_send_addba(tag, len, params, out_len, out);
		break;

	case QSIGMA_STA_GET_MAC_ADDRESS_TAG:
		qtn_handle_sta_get_mac_address(tag, len, params, out_len, out);
		break;

	case QSIGMA_STA_GET_INFO_TAG:
		qtn_handle_sta_get_info(tag, len, params, out_len, out);
		break;

	case QSIGMA_STA_SET_WIRELESS_TAG:
		qtn_handle_sta_set_wireless(tag, len, params, out_len, out);
		break;

	case QSIGMA_STA_SET_RFEATURE_TAG:
		qtn_handle_sta_set_rfeature(tag, len, params, out_len, out);
		break;

	case QSIGMA_STA_SET_IP_CONFIG_TAG:
		qtn_handle_sta_set_ip_config(tag, len, params, out_len, out);
		break;

	case QSIGMA_STA_SET_PSK_TAG:
		qtn_handle_sta_set_psk(tag, len, params, out_len, out);
		break;

	case QSIGMA_STA_ASSOCIATE_TAG:
		qtn_handle_sta_associate(tag, len, params, out_len, out);
		break;

	case QSIGMA_STA_SET_ENCRYPTION_TAG:
		qtn_handle_sta_set_encryption(tag, len, params, out_len, out);
		break;

	case QSIGMA_DEV_SEND_FRAME_TAG:
		qtn_handle_dev_send_frame(tag, len, params, out_len, out);
		break;

	case QSIGMA_STA_REASSOC_TAG:
		qtn_handle_sta_reassoc(tag, len, params, out_len, out);
		break;

	case QSIGMA_STA_SET_SYSTIME_TAG:
		qtn_handle_sta_set_systime(tag, len, params, out_len, out);
		break;

	case QSIGMA_STA_SET_RADIO_TAG:
		qtn_handle_sta_set_radio(tag, len, params, out_len, out);
		break;

	case QSIGMA_STA_SET_MACADDR_TAG:
		qtn_handle_sta_set_macaddr(tag, len, params, out_len, out);
		break;

	case QSIGMA_STA_SET_UAPSD_TAG:
		qtn_handle_sta_set_uapsd(tag, len, params, out_len, out);
		break;

	case QSIGMA_STA_RESET_PARM_TAG:
		qtn_handle_sta_reset_parm(tag, len, params, out_len, out);
		break;

	case QSIGMA_STA_SET_11N_TAG:
		qtn_handle_sta_set_11n(tag, len, params, out_len, out);
		break;

	case QSIGMA_STA_SET_POWER_SAVE_TAG:
		qtn_handle_sta_set_power_save(tag, len, params, out_len, out);
		break;

	case QSIGMA_STA_SET_SLEEP_TAG:
		qtn_handle_sta_set_sleep(tag, len, params, out_len, out);
		break;

	case QSIGMA_DEVICE_GET_INFO_TAG:
		qtn_handle_device_get_info(tag, len, params, out_len, out);
		break;

	default:
		qtn_log("unsupported command %x, use default handler", tag);
		qtn_handle_unknown_command(tag, len, params, out_len, out);
		break;
	}
}
