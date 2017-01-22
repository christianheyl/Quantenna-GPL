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

#ifndef QTN_DUT_STA_HANDLER_H_
#define QTN_DUT_STA_HANDLER_H_

void qtn_handle_sta_reset_default(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);

void qtn_handle_sta_disconnect(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);

void qtn_handle_sta_send_addba(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);

void qtn_handle_sta_set_rfeature(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);

void qtn_handle_sta_set_ip_config(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);

void qtn_handle_sta_set_psk(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);

void qtn_handle_sta_associate(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);

void qtn_handle_sta_set_encryption(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);

void qtn_handle_dev_send_frame(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);

void qtn_handle_sta_reassoc(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);

void qtn_handle_sta_set_systime(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);

void qtn_handle_sta_set_radio(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);

void qtn_handle_sta_set_macaddr(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);

void qtn_handle_sta_set_uapsd(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);

void qtn_handle_sta_reset_parm(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);

void qtn_handle_sta_set_11n(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);

void qnat_sta_device_list_interfaces(int tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);

void qtn_handle_sta_preset_testparameters(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);

void qtn_handle_sta_get_mac_address(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);

void qtn_handle_sta_get_info(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);

void qtn_handle_sta_set_wireless(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);

void qtn_handle_sta_set_power_save(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);

void qtn_handle_sta_set_sleep(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);

void qtn_handle_device_get_info(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);

#endif				/* QTN_DUT_STA_HANDLER_H_ */
