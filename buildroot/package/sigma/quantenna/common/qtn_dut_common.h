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

#ifndef QTN_DUT_COMMON_H_
#define QTN_DUT_COMMON_H_

#define	SM(_v, _f)	(((_v) << _f##_S) & _f)
#define	MS(_v, _f)	(((_v) & _f) >> _f##_S)

struct qtn_dut_config {
	char ifname[8];
	unsigned char bws_enable;
	unsigned char bws_dynamic;
	unsigned char force_rts;
	unsigned char update_settings;
};

void qtn_dut_reset_config(struct qtn_dut_config *conf);
struct qtn_dut_config * qtn_dut_get_config(const char* ifname);

void qtn_dut_make_response_none(int tag, int status, int err_code, int *out_len,
	unsigned char *out_buf);

void qtn_dut_make_response_macaddr(int tag, int status, int err_code, const unsigned char *macaddr,
	int *out_len, unsigned char *out_buf);

void qtn_dut_make_response_vendor_info(int tag, int status, int err_code,
	const char *vendor_info, int *out_len, unsigned char *out_buf);

int qtn_parse_mac(const char *mac_str, unsigned char *mac);

void qtn_set_rts_settings(const char* ifname, struct qtn_dut_config* conf);
int set_tx_bandwidth(const char* ifname, unsigned bandwidth);
int qtn_set_mu_enable(int enable);

#endif				/* QTN_DUT_COMMON_H_ */
