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

#ifndef QTN_AP_H
#define QTN_AP_H

#include "common/qsigma_common.h"
#include <stdint.h>

int qnat_ap_generic_resp(unsigned char *cmd_buf);
int qnat_ap_reboot_resp(unsigned char *cmd_buf);

int qnat_ap_get_info_tlv(char *pcmdStr, unsigned char *aBuf, int *aLen);
int qnat_ap_get_info_resp(unsigned char *cmd_buf);

int qnat_ap_set_radius_request(char *pcmdStr, unsigned char *aBuf, int *aLen);

int qnat_ap_set_wireless_request(char *pcmdStr, unsigned char *aBuf, int *aLen);

int qnat_ap_set_security(char *pcmdStr, unsigned char *aBuf, int *aLen);

int qnat_ap_ca_version(char *pcmdStr, unsigned char *aBuf, int *aLen);
int qnat_ap_ca_version_resp(unsigned char *cmd_buf);

int qnat_ap_set_pmf(char *pcmdStr, unsigned char *aBuf, int *aLen);
int qnat_ap_reboot(char *pcmdStr, unsigned char *aBuf, int *aLen);
int qnat_ap_deauth_sta(char *pcmdStr, unsigned char *aBuf, int *aLen);

int qtn_ca_encode_ap_reset_default(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_ap_set_11n_wireless(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_ap_set_apqos(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_ap_set_staqos(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_ap_config_commit(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_ap_get_mac_address(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_ap_deauth_sta(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_ap_set_11d(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_ap_set_11h(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_ap_set_rfeature(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_ap_set_hs2(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_respond_ap(unsigned char *tlv_buf);

#endif				/* QTN_AP_H */
