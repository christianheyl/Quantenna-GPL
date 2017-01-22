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

#ifndef QTN_DUT_AP_HANDLER_H_
#define QTN_DUT_AP_HANDLER_H_

void qtn_handle_ap_get_info(int len, unsigned char *params, int *out_len, unsigned char *out);
void qtn_handle_ap_set_radius(int len, unsigned char *params, int *out_len, unsigned char *out);
void qtn_handle_ap_set_wireless(int len, unsigned char *params, int *out_len, unsigned char *out);
void qtn_handle_ap_set_security(int len, unsigned char *params, int *out_len, unsigned char *out);
void qtn_handle_ap_reset(int len, unsigned char *params, int *out_len, unsigned char *out);
void qtn_handle_ca_version(int tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);
void qtn_handle_unknown_command(int tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);
void qtn_handle_ap_reset_default(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);
void qtn_handle_ap_set_11n_wireless(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);
void qtn_handle_ap_set_qos(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);
void qtn_handle_ap_config_commit(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);
void qtn_handle_ap_get_mac_address(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);
void qtn_handle_ap_set_hs2(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);
void qtn_handle_ap_deauth_sta(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);
void qtn_handle_ap_set_11d(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);
void qtn_handle_ap_set_11h(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);
void qtn_handle_ap_set_rfeature(int cmd_tag, int len, unsigned char *params, int *out_len,
	unsigned char *out);
void qtn_handle_ap_set_pmf(int len, unsigned char *params, int *out_len, unsigned char *out);

#endif				/* QTN_DUT_AP_HANDLER_H_ */
