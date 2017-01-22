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

#include "qtn_sta.h"
#include "common/qsigma_log.h"
#include "common/qsigma_tags.h"
#include "common/qsigma_common.h"
#include "common/qtn_ca_common.h"
#include "wfa_types.h"
#include "wfa_tlv.h"
#include "wfa_sock.h"

#include <qtn/qcsapi.h>

extern char gRespStr[];
extern int gCaSockfd;

int qnat_sta_ca_get_version(char *pcmdStr, unsigned char *aBuf, int *aLen)
{
	qtn_log("run %s", __FUNCTION__);
	struct qtn_dut_request request = { {0} };

	wfaEncodeTLV(QSIGMA_STA_CA_GET_VERSION_TAG, sizeof(request), (BYTE *) & request, aBuf);

	*aLen = 4 + sizeof(request);

	return WFA_SUCCESS;
}

int qnat_sta_ca_get_version_resp(unsigned char *cmd_buf)
{
	struct qtn_dut_response *resp = (struct qtn_dut_response *)(cmd_buf + 4);
	struct qtn_ca_version ca_version;

	memcpy(&ca_version, &resp->ca_version, sizeof(ca_version));
	qtn_log("run %s", __FUNCTION__);

	sprintf(gRespStr, "status,COMPLETE,version,%s\r\n", ca_version.version);

	wfaCtrlSend(gCaSockfd, (unsigned char *)gRespStr, strlen(gRespStr));
	return WFA_SUCCESS;
}

int qnat_sta_device_list_interfaces_request(char *pcmdStr, unsigned char *aBuf, int *aLen)
{
	qtn_log("run %s", __FUNCTION__);

	if (aBuf == NULL)
		return WFA_FAILURE;

	wfaEncodeTLV(QSIGMA_STA_DEVICE_LIST_INTERFACES_TAG, 0, NULL, aBuf);

	*aLen = 4;

	return WFA_SUCCESS;
}

int qnat_sta_device_list_interfaces_resp(unsigned char *cmd_buf)
{
	struct qtn_dut_response *getverResp = (struct qtn_dut_response *)(cmd_buf + 4);
	qtn_log("run %s", __FUNCTION__);

	sprintf(gRespStr, "status,COMPLETE,interfaceType,802.11,interfaceID,%s\r\n",
		getverResp->ap_info.interface_list);

	wfaCtrlSend(gCaSockfd, (unsigned char *)gRespStr, strlen(gRespStr));
	return WFA_SUCCESS;
}

int qtn_sta_encode_ap_reset_default(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_STA_RESET_DEFAULT_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_sta_disconnect(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_STA_DISCONNECT_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_sta_send_addba(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_STA_SEND_ADDBA_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_sta_preset_testparameters(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_STA_PRESET_TESTPARAMETERS_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_sta_get_mac_address(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_STA_GET_MAC_ADDRESS_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_sta_get_info(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_STA_GET_INFO_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_sta_set_wireless(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_STA_SET_WIRELESS_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_sta_set_rfeature(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_STA_SET_RFEATURE_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_sta_set_ip_config(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_STA_SET_IP_CONFIG_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_sta_set_psk(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_STA_SET_PSK_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_sta_associate(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_STA_ASSOCIATE_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_sta_set_encryption(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_STA_SET_ENCRYPTION_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_dev_send_frame(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_DEV_SEND_FRAME_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_sta_reassoc(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_STA_REASSOC_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_sta_set_systime(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_STA_SET_SYSTIME_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_sta_set_radio(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_STA_SET_RADIO_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_sta_set_macaddr(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_STA_SET_MACADDR_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_sta_set_uapsd(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_STA_SET_UAPSD_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_sta_reset_parm(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_STA_RESET_PARM_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_sta_set_11n(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_STA_SET_11N_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_sta_set_power_save(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_STA_SET_POWER_SAVE_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_sta_set_sleep(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_STA_SET_SLEEP_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_encode_device_get_info(char *cmd_str, unsigned char *tlv_buf, int *tlv_len)
{
	qtn_log("%s", __FUNCTION__);

	return qtn_ca_encode_cmd(QSIGMA_DEVICE_GET_INFO_TAG, cmd_str, tlv_buf, tlv_len);
}

int qtn_ca_respond_sta(unsigned char *tlv_buf)
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
