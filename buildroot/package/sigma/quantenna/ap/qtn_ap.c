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

#include "qtn_ap.h"
#include "common/qsigma_log.h"
#include "common/qsigma_tags.h"
#include "common/qtn_ca_common.h"
#include "wfa_types.h"
#include "wfa_tlv.h"
#include "wfa_sock.h"

#include <qtn/qcsapi.h>

extern char gRespStr[];
extern int gCaSockfd;

int qnat_ap_get_info_tlv(char *pcmdStr, unsigned char *aBuf, int *aLen)
{
	qtn_log("start qnat_ap_get_info_tlv");

	if (aBuf == NULL)
		return WFA_FAILURE;

	wfaEncodeTLV(QSIGMA_AP_GET_INFO_TAG, 0, NULL, aBuf);

	*aLen = 4;

	return WFA_SUCCESS;
}

int qnat_ap_get_info_resp(unsigned char *cmd_buf)
{
	struct qtn_dut_response *getverResp = (struct qtn_dut_response *)(cmd_buf + 4);
	qtn_log("ap_get_info responce");

	sprintf(gRespStr, "status,COMPLETE,interface,%s,firmware,%s,agent,%s\r\n",
		getverResp->ap_info.interface_list,
		getverResp->ap_info.firmware_version, getverResp->ap_info.agent_version);

	wfaCtrlSend(gCaSockfd, (unsigned char *)gRespStr, strlen(gRespStr));
	return WFA_SUCCESS;
}

int qnat_ap_set_radius_request(char *pcmdStr, unsigned char *aBuf, int *aLen)
{
	qtn_log("run %s", __FUNCTION__);
	struct qtn_dut_request request = { {0} };
	char *save;

	for (char *str = strtok_r(pcmdStr, ",", &save); str && str[0];
		str = strtok_r(NULL, ",", &save)) {

		if (strcasecmp(str, "IPADDR") == 0 && (str = strtok_r(NULL, ",", &save))) {
			snprintf(request.ap_radius.ip, sizeof(request.ap_radius.ip), "%s", str);
		} else if (strcasecmp(str, "PORT") == 0 && (str = strtok_r(NULL, ",", &save))) {
			sscanf(str, "%hu", &request.ap_radius.port);
		} else if (strcasecmp(str, "PASSWORD") == 0 && (str = strtok_r(NULL, ",", &save))) {
			snprintf(request.ap_radius.password,
				sizeof(request.ap_radius.password), "%s", str);
		} else if (strcasecmp(str, "INTERFACE") == 0 && (str = strtok_r(NULL, ",", &save))) {
			snprintf(request.ap_radius.if_name,
				sizeof(request.ap_radius.if_name), "%s", str);
		}
	}

	wfaEncodeTLV(QSIGMA_AP_SET_RADIUS_TAG, sizeof(request), (BYTE *) & request, aBuf);

	*aLen = 4 + sizeof(request);

	return WFA_SUCCESS;
}

int qnat_ap_set_wireless_request(char *pcmdStr, unsigned char *aBuf, int *aLen)
{
	qtn_log("run %s", __FUNCTION__);
	struct qtn_dut_request request = { {0} };
	char *save;
	char *name;
	char *value;

	for (name = strtok_r(pcmdStr, ",", &save), value = strtok_r(NULL, ",", &save);
		name && *name && value;
		name = strtok_r(NULL, ",", &save), value = strtok_r(NULL, ",", &save)) {

		qtn_log("%s -> %s", name, value);
		if (strcasecmp(name, "NAME") == 0) {
		} else if (strcasecmp(name, "Program") == 0) {
			snprintf(request.wireless.programm,
				sizeof(request.wireless.programm), "%s", value);
		} else if (strcasecmp(name, "INTERFACE") == 0) {
			snprintf(request.wireless.if_name,
				sizeof(request.wireless.if_name), "%s", value);
		} else if (strcasecmp(name, "SSID") == 0) {
			snprintf(request.wireless.ssid, sizeof(request.wireless.ssid), "%s", value);
		} else if (strcasecmp(name, "CHANNEL") == 0) {
			// channel number
			// ; sepaated for dual band
			// 36;6
			if (sscanf(value, "%d;%d", request.wireless.channels,
					request.wireless.channels + 1) != 2) {
				sscanf(value, "%d", request.wireless.channels);
			}
		} else if (strcasecmp(name, "MODE") == 0) {
			// 11b, 11bg, 11bgn, 11a, 11na,11ac
			// or 11ac;11ng
			if (strstr(value, ";") != NULL) {
				sscanf(value, "%[^;];%[^;]", request.wireless.mode[0],
						request.wireless.mode[1]);
			} else {
				snprintf(request.wireless.mode[0], sizeof(request.wireless.mode[0]),
						"%s", value);
			}
		} else if (strcasecmp(name, "WME") == 0) {
			// WME on/off , String
			request.wireless.wmm = strcasecmp(value, "off") != 0;
			request.wireless.has_wmm = 1;
		} else if (strcasecmp(name, "WMMPS") == 0) {
			// APSD on/off
			request.wireless.apsd = strcasecmp(value, "off") != 0;
			request.wireless.has_apsd = 1;
		} else if (strcasecmp(name, "RTS") == 0) {
			// Threshold, Short Integer
			sscanf(value, "%d", &request.wireless.rts_threshold);
			request.wireless.has_rts_threshold = 1;
		} else if (strcasecmp(name, "FRGMNT") == 0) {
			// Fragmentation, Short Integer
		} else if (strcasecmp(name, "PWRSAVE") == 0) {
			// Power Save, String
			request.wireless.power_save = strcasecmp(value, "off") != 0;
			request.wireless.has_power_save = 1;
		} else if (strcasecmp(name, "BCNINT") == 0) {
			// Beacon Interval
			request.wireless.beacon_interval =
				sscanf(value, "%u", &request.wireless.beacon_interval) == 1;
		} else if (strcasecmp(name, "RADIO") == 0) {
			// On/Off the radio of given interface
			request.wireless.rf_enable = strcasecmp(value, "off") != 0;
			request.wireless.has_rf_enable = 1;
		} else if (strcasecmp(name, "ADDBA_REJECT") == 0) {
			// Reject any ADDBA request by sending ADDBA response with status decline
			request.wireless.has_addba_reject = 1;
			request.wireless.addba_reject = strcasecmp(value, "Disable") != 0;
		} else if (strcasecmp(name, "AMPDU") == 0) {
			//AMPDU Aggregation: Enable, Disable
			request.wireless.has_ampdu = 1;
			request.wireless.ampdu = strcasecmp(value, "Disable") != 0;
		} else if (strcasecmp(name, "AMPDU_EXP") == 0) {
			// Maximum AMPDU Exponent: Short Integer
		} else if (strcasecmp(name, "AMSDU") == 0) {
			// AMSDU Aggregation: Enable, Disable
			request.wireless.amsdu = strcasecmp(value, "Disable") != 0;
			request.wireless.has_amsdu = 1;
		} else if (strcasecmp(name, "OFFSET") == 0) {
			// Secondary channel offset: Above, Below
			request.wireless.has_offset = 1;
			request.wireless.offset = strcasecmp(value, "Above") == 0 ? 0 : 1;
		} else if (strcasecmp(name, "MCS_FIXEDRATE") == 0) {
			// Depending upon the MODE' parameter, two options - For mode=11na - MCS rate varies from 0 to 31 and
			// For mode=11ac, MCS rate varies from 0 to 9.
			request.wireless.has_mcs_rate =
				sscanf(value, "%hu", &request.wireless.mcs_rate) == 1;
		} else if (strcasecmp(name, "SPATIAL_RX_STREAM") == 0) {
			// Depending upon the MODE' parameter, two options.
			// For mode=11na - Sets the Rx spacial streams of the AP and
			// which means the Rx MCS Rates capability.
			// For mode=11ac - Sets the Rx spacial streams of the AP.
			// No inter-dependency of number of spatial streams and MCS rates.
			snprintf(request.wireless.nss_rx, sizeof(request.wireless.nss_rx),
				"%s", value);
		} else if (strcasecmp(name, "SPATIAL_TX_STREAM") == 0) {
			// Depending upon the MODE' parameter, two options.
			// For mode=11na - Sets the Tx spacial streams of the AP and which means the Tx MCS Rates capability.
			// For mode=11ac - Sets the Tx spacial streams of the AP. No inter-dependency of number of spatial streams and MCS rates.
			snprintf(request.wireless.nss_tx, sizeof(request.wireless.nss_tx),
				"%s", value);
		} else if (strcasecmp(name, "MPDU_MIN_START_SPACING") == 0) {
			// Minimum MPDU Start Spacing, Short Integer
		} else if (strcasecmp(name, "RIFS_TEST") == 0) {
			// Set Up (Tear Down) RIFS Transmission Test as instructed within Appendix H of the 11n Test Plan
		} else if (strcasecmp(name, "SGI20") == 0) {
			// Short Guard Interval
		} else if (strcasecmp(name, "STBC_TX") == 0) {
			request.wireless.has_stbc_tx = sscanf(value, "%d;%d",
				&request.wireless.stbc_tx[0], &request.wireless.stbc_tx[1]) == 2;
			// STBC Transmit Streams
		} else if (strcasecmp(name, "WIDTH") == 0) {
			// 20, 40, 80,160,Auto
			if (strcasecmp(value, "auto") == 0) {
				request.wireless.bandwidth = 0;
				request.wireless.has_bandwidth = 1;
			} else if (sscanf(value, "%hu", &request.wireless.bandwidth) == 1) {
				request.wireless.has_bandwidth = 1;
			}
		} else if (strcasecmp(name, "WIDTH_SCAN") == 0) {
			// BSS channel width trigger scan interval in seconds
		} else if (strcasecmp(name, "ChannelUsage") == 0) {
			// Set the channel usage, String
		} else if (strcasecmp(name, "DTIM") == 0) {
			// DTIM Count
			request.wireless.has_dtim =
				sscanf(value, "%u", &request.wireless.dtim) == 1;
		} else if (strcasecmp(name, "DYN_BW_SGNL") == 0) {
			// If DYN_BW_SGNL is enabled then the AP sends the RTS frame with dynamic
			// bandwidth signaling, otherwise AP sends RTS with static bandwidth signaling.
			// BW signaling is enabled on the use of this command
			request.wireless.has_dyn_bw_sgnl = 1;
			request.wireless.dyn_bw_sgnl = strcasecmp(value, "Disable") != 0;
		} else if (strcasecmp(name, "SGI80") == 0) {
			// Enable Short guard interval at 80 Mhz. String Enable/Disable
			request.wireless.short_gi = strcasecmp(value, "Disable") != 0;
			request.wireless.has_short_gi = 1;

		} else if (strcasecmp(name, "TxBF") == 0) {
			// To enable or disable SU TxBF beamformer capability with explicit feedback.
			// String: Enable/Disable
			request.wireless.su_beamformer = strcasecmp(value, "Enable") == 0;
			request.wireless.has_su_beamformer = 1;
		} else if (strcasecmp(name, "LDPC") == 0) {
			// Enable the use of LDPC code at the physical layer for both Tx and Rx side.
			// String: Enable/Disable
			request.wireless.ldpc = strcasecmp(value, "Enable") == 0;
			request.wireless.has_ldpc = 1;
		} else if (strcasecmp(name, "nss_mcs_cap") == 0) {
			// Refers to nss_capabilty;mcs_capability. This parameter gives a description
			// of the supported spatial streams and mcs rate capabilities of the STA
			// String. For example  If a STA supports 2SS with MCS 0-9, then nss_mcs_cap,2;0-9
		} else if (strcasecmp(name, "Tx_lgi_rate") == 0) {
			// Used to set the Tx Highest Supported Long Gi Data Rate subfield
			// Integer
		} else if (strcasecmp(name, "SpectrumMgt") == 0) {
			// Enable/disable Spectrum management feature with minimum number of beacons
			// with switch announcement IE = 10, channel switch mode = 1
			// String: Enable/Disable
		} else if (strcasecmp(name, "Vht_tkip") == 0) {
			// To enable TKIP in VHT mode.
			// String: Enable/Disable
			request.wireless.vht_tkip = strcasecmp(value, "Enable") == 0;
			request.wireless.has_vht_tkip = 1;
		} else if (strcasecmp(name, "Vht_wep") == 0) {
			// To enable WEP in VHT mode.
			// String: Enable/Disable
		} else if (strcasecmp(name, "BW_SGNL") == 0) {
			// To enable/disable the ability to send out RTS with bandwidth signaling
			// String: Enable/Disable
			request.wireless.bw_sgnl = strcasecmp(value, "Enable") == 0;
			request.wireless.has_bw_sgnl = 1;
		} else if (strcasecmp(name, "HTC-VHT") == 0) {
			// Indicates support for receiving a VHT variant HT control field
			// String: Enable/Disable
		} else if (strcasecmp(name, "Zero_crc") == 0) {
			// To set the CRC field to all 0s
			// String: Enable/Disable
		} else if (strcasecmp(name, "CountryCode") == 0) {
			// 2 character country code in Country Information Element
			// String: For Example  US
			snprintf(request.wireless.country_code,
				sizeof(request.wireless.country_code), "%s", value);
		} else if (strcasecmp(name, "Protect_mode") == 0) {
			// To enable/disable the default protection mode between 2 VHT devices
			// i.e turn RTS/CTS and CTS-to-Self on/off
			// String: Enable/Disable
		} else if (strcasecmp(name, "MU_TxBF") == 0) {
			// To enable or disable Multi User (MU) TxBF beamformer capability
			// eith explicit feedback
			// String: Enable/Disable
			request.wireless.mu_beamformer = strcasecmp(value, "Enable") == 0;
			request.wireless.has_mu_beamformer = 1;
		} else if (strcasecmp(name, "GROUP_ID") == 0) {
			// Command to set preconfigured Group ID after SSID is configured.
			// Integer- Range 0 to 63
			request.wireless.group_id = strtol(value, NULL, 10);
			request.wireless.has_group_id = 1;
		} else if (strcasecmp(name, "RTS_FORCE") == 0) {
			// Force STA to send RTS
			// String: Enable/Disable
			request.wireless.rts_force = strcasecmp(value, "Enable") == 0;
			request.wireless.has_rts_force = 1;
		}
	}

	wfaEncodeTLV(QSIGMA_AP_SET_WIRELESS_TAG, sizeof(request), (BYTE *) & request, aBuf);

	*aLen = 4 + sizeof(request);

	return WFA_SUCCESS;
}

int qnat_ap_set_security(char *pcmdStr, unsigned char *aBuf, int *aLen)
{
	qtn_log("run %s", __FUNCTION__);
	struct qtn_dut_request request = { {0} };
	char *save;
	char *name;
	char *value;

	// NAME
	// KEYMGNT
	// ----
	// INTERFACE
	// PSK
	// WEPKEY
	// SSID
	// PMF
	// SHA256AD
	// ENCRYPT

	for (name = strtok_r(pcmdStr, ",", &save), value = strtok_r(NULL, ",", &save);
		name && *name && value;
		name = strtok_r(NULL, ",", &save), value = strtok_r(NULL, ",", &save)) {

		if (strcasecmp(name, "NAME") == 0) {
		} else if (strcasecmp(name, "KEYMGNT") == 0) {
			snprintf(request.secutiry.keymgnt, sizeof(request.secutiry.keymgnt),
				"%s", value);
		} else if (strcasecmp(name, "INTERFACE") == 0) {
			snprintf(request.secutiry.if_name, sizeof(request.secutiry.if_name),
				"%s", value);
		} else if (strcasecmp(name, "PSK") == 0) {
			snprintf(request.secutiry.passphrase, sizeof(request.secutiry.passphrase),
				"%s", value);
		} else if (strcasecmp(name, "WEPKEY") == 0) {
			snprintf(request.secutiry.wepkey, sizeof(request.secutiry.wepkey),
				"%s", value);
		} else if (strcasecmp(name, "SSID") == 0) {
			snprintf(request.secutiry.ssid, sizeof(request.secutiry.ssid), "%s", value);
		} else if (strcasecmp(name, "PMF") == 0) {
			request.secutiry.has_pmf = 1;
			if (strcasecmp(value, "Required") == 0) {
				request.secutiry.pmf = qcsapi_pmf_required;
			} else if (strcasecmp(value, "Optional") == 0) {
				request.secutiry.pmf = qcsapi_pmf_optional;
			} else if (strcasecmp(value, "Disabled") == 0) {
				request.secutiry.pmf = qcsapi_pmf_disabled;
			} else {
				request.secutiry.has_pmf = 0;
			}
		} else if (strcasecmp(name, "SHA256AD") == 0) {
			request.secutiry.has_sha256ad = 1;
			if (strcasecmp(value, "Enabled") == 0) {
				request.secutiry.sha256ad = 1;
			} else if (strcasecmp(value, "Disabled") == 0) {
				request.secutiry.sha256ad = 0;
			} else {
				request.secutiry.has_sha256ad = 0;
			}
		} else if (strcasecmp(name, "ENCRYPT") == 0) {
			snprintf(request.secutiry.encryption, sizeof(request.secutiry.encryption),
				"%s", value);
		}
	}

	wfaEncodeTLV(QSIGMA_AP_SET_SECURITY_TAG, sizeof(request), (BYTE *) & request, aBuf);

	*aLen = 4 + sizeof(request);

	return WFA_SUCCESS;
}

int qnat_ap_ca_version(char *pcmdStr, unsigned char *aBuf, int *aLen)
{
	qtn_log("run %s", __FUNCTION__);
	struct qtn_dut_request request = { {0} };

	wfaEncodeTLV(QSIGMA_AP_CA_VERSION_TAG, sizeof(request), (BYTE *) & request, aBuf);

	*aLen = 4 + sizeof(request);

	return WFA_SUCCESS;
}

int qnat_ap_set_pmf(char *pcmdStr, unsigned char *aBuf, int *aLen)
{
	qtn_log("run %s", __FUNCTION__);
	struct qtn_dut_request request = { {0} };

	wfaEncodeTLV(QSIGMA_AP_SET_PMF_TAG, sizeof(request), (BYTE *) & request, aBuf);

	*aLen = 4 + sizeof(request);

	return WFA_SUCCESS;
}

int qnat_ap_reboot(char *pcmdStr, unsigned char *aBuf, int *aLen)
{
	qtn_log("run %s", __FUNCTION__);
	struct qtn_dut_request request = { {0} };

	wfaEncodeTLV(QSIGMA_AP_REBOOT_TAG, sizeof(request), (BYTE *) & request, aBuf);

	*aLen = 4 + sizeof(request);

	return WFA_SUCCESS;
}

int qnat_ap_deauth_sta(char *pcmdStr, unsigned char *aBuf, int *aLen)
{
	qtn_log("run %s", __FUNCTION__);
	struct qtn_dut_request request = { {0} };

	wfaEncodeTLV(QSIGMA_AP_DEAUTH_STA_TAG, sizeof(request), (BYTE *) & request, aBuf);

	*aLen = 4 + sizeof(request);

	return WFA_SUCCESS;
}

int qnat_ap_ca_version_resp(unsigned char *cmd_buf)
{
	struct qtn_dut_response *resp = (struct qtn_dut_response *)(cmd_buf + 4);
	struct qtn_ca_version ca_version;

	memcpy(&ca_version, &resp->ca_version, sizeof(ca_version));
	qtn_log("run %s", __FUNCTION__);

	sprintf(gRespStr, "status,COMPLETE,version,%s\r\n", ca_version.version);

	wfaCtrlSend(gCaSockfd, (unsigned char *)gRespStr, strlen(gRespStr));
	return WFA_SUCCESS;
}

int qnat_ap_generic_resp(unsigned char *cmd_buf)
{
	struct qtn_dut_response *getverResp = (struct qtn_dut_response *)(cmd_buf + 4);
	qtn_log("%s", __FUNCTION__);

	if (getverResp->status == STATUS_COMPLETE) {
		sprintf(gRespStr, "status,COMPLETE\r\n");
	} else {
		sprintf(gRespStr, "status,ERROR,errorCode,%d\r\n", getverResp->qcsapi_error);
	}

	wfaCtrlSend(gCaSockfd, (unsigned char *)gRespStr, strlen(gRespStr));
	return WFA_SUCCESS;
}

int qnat_ap_reboot_resp(unsigned char *cmd_buf)
{
	qtn_log("%s", __FUNCTION__);

	sprintf(gRespStr, "status,COMPLETE,after_action,wait_reboot\r\n");

	wfaCtrlSend(gCaSockfd, (unsigned char *)gRespStr, strlen(gRespStr));
	return WFA_SUCCESS;
}

int qtn_ca_encode_ap_reset_default(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_AP_RESET_DEFAULT_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_ap_set_11n_wireless(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_AP_SET_11N_WIRELESS_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_ap_set_apqos(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_AP_SET_APQOS_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_ap_set_staqos(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_AP_SET_STAQOS_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_ap_config_commit(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_AP_CONFIG_COMMIT_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_ap_get_mac_address(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_AP_GET_MAC_ADDRESS_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_ap_deauth_sta(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_AP_DEAUTH_STA_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_ap_set_11d(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_AP_SET_11D_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_ap_set_11h(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_AP_SET_11H_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_ap_set_rfeature(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_AP_SET_RFEATURE_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_ap_set_hs2(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_AP_SET_HS2_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_respond_ap(unsigned char *tlv_buf)
{
	int ret;
	char out_buf[512];

	qtn_log("%s", __FUNCTION__);

	ret = qtn_ca_make_response(tlv_buf, out_buf, sizeof(out_buf));

	if (ret != WFA_SUCCESS)
		return ret;

	wfaCtrlSend(gCaSockfd, (unsigned char *)out_buf, strlen(out_buf));

	return WFA_SUCCESS;
}
