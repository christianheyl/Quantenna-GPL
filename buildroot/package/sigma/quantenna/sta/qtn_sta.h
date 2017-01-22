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

#ifndef QTN_STA_H
#define QTN_STA_H

#include <stdint.h>

int qnat_sta_ca_get_version(char *pcmdStr, unsigned char *aBuf, int *aLen);
int qnat_sta_ca_get_version_resp(unsigned char *cmd_buf);
int qtn_sta_encode_ap_reset_default(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_sta_disconnect(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_sta_send_addba(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_sta_set_rfeature(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_sta_set_ip_config(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_sta_set_psk(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_sta_associate(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_sta_set_encryption(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_dev_send_frame(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_sta_reassoc(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_sta_set_systime(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_sta_set_radio(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_sta_set_macaddr(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_sta_set_uapsd(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_sta_reset_parm(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_sta_set_11n(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qnat_sta_device_list_interfaces_request(char *pcmdStr, unsigned char *aBuf, int *aLen);
int qtn_ca_respond_sta(unsigned char *tlv_buf);
int qnat_sta_device_list_interfaces_resp(unsigned char *tlv_buf);
int qtn_ca_encode_sta_preset_testparameters(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_sta_get_mac_address(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_sta_get_info(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_sta_set_wireless(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_sta_set_power_save(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_sta_set_sleep(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);
int qtn_ca_encode_device_get_info(char *cmd_str, unsigned char *tlv_buf, int *tlv_len);

#endif
