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

#include <string.h>

#include "wfa_types.h"
#include "wfa_agtctrl.h"
#include "common/qsigma_log.h"
#include "common/qsigma_tags.h"
#include "ap/qtn_ap.h"
#include "sta/qtn_sta.h"
#include <qtn/qcsapi.h>

typeNameStr_t qtn_capi_to_tlv[] = {
	/* AP */
	{QSIGMA_AP_GET_INFO_TAG, "AP_GET_INFO", qnat_ap_get_info_tlv}
	,
	{QSIGMA_AP_SET_RADIUS_TAG, "AP_SET_RADIUS", qnat_ap_set_radius_request}
	,
	{QSIGMA_AP_SET_WIRELESS_TAG, "AP_SET_WIRELESS", qnat_ap_set_wireless_request}
	,
	{QSIGMA_AP_RESET_DEFAULT_TAG, "AP_RESET_DEFAULT", qtn_ca_encode_ap_reset_default}
	,
	{QSIGMA_AP_SET_11N_WIRELESS_TAG, "AP_SET_11N_WIRELESS", qtn_ca_encode_ap_set_11n_wireless}
	,
	{QSIGMA_AP_SET_SECURITY_TAG, "AP_SET_SECURITY", qnat_ap_set_security}
	,
	{QSIGMA_AP_REBOOT_TAG, "AP_REBOOT", qnat_ap_reboot}
	,
	{QSIGMA_AP_SET_APQOS_TAG, "AP_SET_APQOS", qtn_ca_encode_ap_set_apqos}
	,
	{QSIGMA_AP_SET_STAQOS_TAG, "AP_SET_STAQOS", qtn_ca_encode_ap_set_staqos}
	,
	{QSIGMA_AP_CONFIG_COMMIT_TAG, "AP_CONFIG_COMMIT", qtn_ca_encode_ap_config_commit}
	,
	{QSIGMA_AP_DEAUTH_STA_TAG, "AP_DEAUTH_STA", qtn_ca_encode_ap_deauth_sta}
	,
	{QSIGMA_AP_GET_MAC_ADDRESS_TAG, "AP_GET_MAC_ADDRESS", qtn_ca_encode_ap_get_mac_address}
	,
	{QSIGMA_AP_SET_11D_TAG, "AP_SET_11D", qtn_ca_encode_ap_set_11d}
	,
	{QSIGMA_AP_SET_11H_TAG, "AP_SET_11H", qtn_ca_encode_ap_set_11h}
	,
	{QSIGMA_AP_CA_VERSION_TAG, "AP_CA_VERSION", qnat_ap_ca_version}
	,
	{QSIGMA_AP_SET_PMF_TAG, "AP_SET_PMF", qnat_ap_set_pmf}
	,
	{QSIGMA_AP_SET_RFEATURE_TAG, "AP_SET_RFEATURE", qtn_ca_encode_ap_set_rfeature}
	,
	{QSIGMA_AP_SET_HS2_TAG, "AP_SET_HS2", qtn_ca_encode_ap_set_hs2}
	,

	/* STA */
	{QSIGMA_STA_CA_GET_VERSION_TAG, "CA_GET_VERSION", qnat_sta_ca_get_version}
	,
	{QSIGMA_STA_RESET_DEFAULT_TAG, "STA_RESET_DEFAULT", qtn_sta_encode_ap_reset_default}
	,
	{QSIGMA_STA_DEVICE_LIST_INTERFACES_TAG, "DEVICE_LIST_INTERFACES",
		qnat_sta_device_list_interfaces_request}
	,
	{QSIGMA_STA_PRESET_TESTPARAMETERS_TAG, "STA_PRESET_TESTPARAMETERS",
		qtn_ca_encode_sta_preset_testparameters}
	,
	{QSIGMA_STA_DISCONNECT_TAG, "STA_DISCONNECT", qtn_ca_encode_sta_disconnect}
	,
	{QSIGMA_STA_SEND_ADDBA_TAG, "STA_SEND_ADDBA", qtn_ca_encode_sta_send_addba}
	,
	{QSIGMA_STA_GET_MAC_ADDRESS_TAG, "STA_GET_MAC_ADDRESS", qtn_ca_encode_sta_get_mac_address}
	,
	{QSIGMA_STA_GET_INFO_TAG, "STA_GET_INFO", qtn_ca_encode_sta_get_info}
	,
	{QSIGMA_STA_SET_WIRELESS_TAG, "STA_SET_WIRELESS", qtn_ca_encode_sta_set_wireless}
	,
	{QSIGMA_STA_SET_RFEATURE_TAG, "STA_SET_RFEATURE", qtn_ca_encode_sta_set_rfeature}
	,
	{QSIGMA_STA_SET_IP_CONFIG_TAG, "STA_SET_IP_CONFIG", qtn_ca_encode_sta_set_ip_config}
	,
	{QSIGMA_STA_SET_PSK_TAG, "STA_SET_PSK", qtn_ca_encode_sta_set_psk}
	,
	{QSIGMA_STA_ASSOCIATE_TAG, "STA_ASSOCIATE", qtn_ca_encode_sta_associate}
	,
	{QSIGMA_STA_SET_ENCRYPTION_TAG, "STA_SET_ENCRYPTION", qtn_ca_encode_sta_set_encryption}
	,
	{QSIGMA_DEV_SEND_FRAME_TAG, "DEV_SEND_FRAME", qtn_ca_encode_dev_send_frame}
	,
	{QSIGMA_STA_REASSOC_TAG, "STA_REASSOC", qtn_ca_encode_sta_reassoc}
	,
	{QSIGMA_STA_SET_SYSTIME_TAG, "STA_SET_SYSTIME", qtn_ca_encode_sta_set_systime}
	,
	{QSIGMA_STA_SET_RADIO_TAG, "STA_SET_RADIO", qtn_ca_encode_sta_set_radio}
	,
	{QSIGMA_STA_SET_MACADDR_TAG, "STA_SET_MACADDR", qtn_ca_encode_sta_set_macaddr}
	,
	{QSIGMA_STA_SET_UAPSD_TAG, "STA_SET_UAPSD", qtn_ca_encode_sta_set_uapsd}
	,
	{QSIGMA_STA_RESET_PARM_TAG, "STA_RESET_PARM", qtn_ca_encode_sta_reset_parm}
	,
	{QSIGMA_STA_SET_11N_TAG, "STA_SET_11N", qtn_ca_encode_sta_set_11n}
	,
	{QSIGMA_STA_SET_POWER_SAVE_TAG, "SET_POWER_SAVE", qtn_ca_encode_sta_set_power_save}
	,
	{QSIGMA_STA_SET_SLEEP_TAG, "STA_SET_SLEEP", qtn_ca_encode_sta_set_sleep}
	,
	{QSIGMA_DEVICE_GET_INFO_TAG, "DEVICE_GET_INFO", qtn_ca_encode_device_get_info}
	,
	{-1, "", NULL}
	,
};

dutCommandRespFuncPtr qtn_dut_responce_map[] = {
	[QSIGMA_AP_GET_INFO_TAG - QSIGMA_TAG_BASE] = qnat_ap_get_info_resp,
	[QSIGMA_AP_SET_RADIUS_TAG - QSIGMA_TAG_BASE] = qnat_ap_generic_resp,
	[QSIGMA_AP_SET_WIRELESS_TAG - QSIGMA_TAG_BASE] = qnat_ap_generic_resp,
	[QSIGMA_AP_RESET_DEFAULT_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_ap,
	[QSIGMA_AP_SET_11N_WIRELESS_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_ap,
	[QSIGMA_AP_SET_SECURITY_TAG - QSIGMA_TAG_BASE] = qnat_ap_generic_resp,
	[QSIGMA_AP_SET_APQOS_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_ap,
	[QSIGMA_AP_SET_STAQOS_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_ap,
	[QSIGMA_AP_CONFIG_COMMIT_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_ap,
	[QSIGMA_AP_DEAUTH_STA_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_ap,
	[QSIGMA_AP_GET_MAC_ADDRESS_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_ap,
	[QSIGMA_AP_SET_11H_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_ap,
	[QSIGMA_AP_SET_11D_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_ap,
	[QSIGMA_AP_CA_VERSION_TAG - QSIGMA_TAG_BASE] = qnat_ap_ca_version_resp,
	[QSIGMA_AP_SET_PMF_TAG - QSIGMA_TAG_BASE] = qnat_ap_generic_resp,
	[QSIGMA_AP_REBOOT_TAG - QSIGMA_TAG_BASE] = qnat_ap_reboot_resp,
	[QSIGMA_AP_SET_RFEATURE_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_ap,
	[QSIGMA_AP_SET_HS2_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_ap,

	/* STA */
	[QSIGMA_STA_CA_GET_VERSION_TAG - QSIGMA_TAG_BASE] = qnat_sta_ca_get_version_resp,
	[QSIGMA_STA_RESET_DEFAULT_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_sta,
	[QSIGMA_STA_DEVICE_LIST_INTERFACES_TAG - QSIGMA_TAG_BASE] =
		qnat_sta_device_list_interfaces_resp,
	[QSIGMA_STA_PRESET_TESTPARAMETERS_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_sta,
	[QSIGMA_STA_DISCONNECT_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_sta,
	[QSIGMA_STA_SEND_ADDBA_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_sta,
	[QSIGMA_STA_GET_MAC_ADDRESS_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_sta,
	[QSIGMA_STA_GET_INFO_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_sta,
	[QSIGMA_STA_SET_WIRELESS_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_sta,
	[QSIGMA_STA_SET_RFEATURE_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_sta,
	[QSIGMA_STA_SET_IP_CONFIG_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_sta,
	[QSIGMA_STA_SET_PSK_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_sta,
	[QSIGMA_STA_ASSOCIATE_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_sta,
	[QSIGMA_STA_SET_ENCRYPTION_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_sta,
	[QSIGMA_DEV_SEND_FRAME_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_sta,
	[QSIGMA_STA_REASSOC_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_sta,
	[QSIGMA_STA_SET_SYSTIME_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_sta,
	[QSIGMA_STA_SET_RADIO_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_sta,
	[QSIGMA_STA_SET_MACADDR_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_sta,
	[QSIGMA_STA_SET_UAPSD_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_sta,
	[QSIGMA_STA_RESET_PARM_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_sta,
	[QSIGMA_STA_SET_11N_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_sta,
	[QSIGMA_STA_SET_POWER_SAVE_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_sta,
	[QSIGMA_STA_SET_SLEEP_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_sta,
	[QSIGMA_DEVICE_GET_INFO_TAG - QSIGMA_TAG_BASE] = qtn_ca_respond_sta,
};

void qtn_handle_dut_responce(unsigned tag, unsigned char *data)
{
	int index = tag - QSIGMA_TAG_BASE;
	if (tag >= QSIGMA_TAG_BASE && index < ARRAY_SIZE(qtn_dut_responce_map) &&
		qtn_dut_responce_map[index]) {
		qtn_log("handle dut response for tag 0x%x", tag);
		qtn_dut_responce_map[index] (data);
	} else {
		qtn_error("can't handle dut response for tag 0x%x, index %d", tag, index);
	}
}
