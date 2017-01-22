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

#ifndef QSIGMA_COMMON_H
#define QSIGMA_COMMON_H

#include <stdint.h>

enum {
	QTN_VERSION_LEN = 32,
	QTN_PROGRAMM_NAME_LEN = 16,
	QTN_IP_LEN = sizeof("xxx.xxx.xxx.xxx"),
	QTN_PASSWORD_LEN = 128,
	QTN_INTERFACE_LEN = 32,
	QNT_SSID_LEN = 64,
	QNT_ENCRYPTION_LEN = 32,
	QNT_NS_LEN = 64,
	QNT_WIFI_MODE_LEN = 64,
	QTN_MX_INTERFACES = 4,
	QTN_INTERFACE_LIST_LEN = QTN_INTERFACE_LEN * QTN_MX_INTERFACES,
	QTN_KEYMGNT_LEN = 64,
	QTN_COUNTRY_CODE_LEN = 16,
	QTN_SCAN_TIMEOUT_SEC = 100,
};

struct qtn_ca_version {
	char version[QTN_VERSION_LEN];
};

struct qtn_ap_get_info_result {
	char interface_list[QTN_INTERFACE_LIST_LEN];
	char agent_version[QTN_VERSION_LEN];
	char firmware_version[QTN_VERSION_LEN];
};

struct qtn_ap_set_radius {
	char if_name[QTN_INTERFACE_LEN];
	char ip[QTN_IP_LEN];
	uint16_t port;
	char password[QTN_PASSWORD_LEN];
};

struct qtn_ap_set_wireless {
	char programm[QTN_PROGRAMM_NAME_LEN];
	char if_name[QTN_INTERFACE_LEN];
	char ssid[QNT_SSID_LEN];
	int32_t channels[2];
	char mode[2][QNT_WIFI_MODE_LEN];

	uint8_t wmm;		//  wireless multimedia extensions: 0 - disable
	uint8_t has_wmm;

	uint8_t apsd;
	uint8_t has_apsd;

	int32_t rts_threshold;
	uint8_t has_rts_threshold;

	uint8_t power_save;	// 0 - disable
	uint8_t has_power_save;

	uint32_t beacon_interval;
	uint8_t has_beacon_interval;

	uint8_t rf_enable;	// 0 - disable, 1 - enable
	uint8_t has_rf_enable;

	uint8_t amsdu;
	uint8_t has_amsdu;

	uint16_t mcs_rate;
	uint8_t has_mcs_rate;

	char nss_rx[QNT_NS_LEN];
	char nss_tx[QNT_NS_LEN];

	int stbc_tx[2];		/* space-time block coding,
				 * [0] is number of spatial stream,  [1] - space time streams */
	uint8_t has_stbc_tx;

	uint16_t bandwidth;	// 0 - auto
	uint8_t has_bandwidth;

	uint32_t dtim;
	uint8_t has_dtim;

	uint8_t short_gi;	// Short GI
	uint8_t has_short_gi;

	uint8_t su_beamformer;
	uint8_t has_su_beamformer;

	uint8_t mu_beamformer;
	uint8_t has_mu_beamformer;

	char country_code[QTN_COUNTRY_CODE_LEN];

	uint8_t has_addba_reject;
	uint8_t addba_reject;

	uint8_t has_ampdu;
	uint8_t ampdu;

	uint8_t has_dyn_bw_sgnl;
	uint8_t dyn_bw_sgnl;

	uint8_t has_vht_tkip;
	uint8_t vht_tkip;

	uint8_t has_bw_sgnl;
	uint8_t bw_sgnl;

	uint8_t has_group_id;
	uint8_t group_id;

	uint8_t has_rts_force;
	uint8_t rts_force;

	uint8_t has_offset;
	uint8_t offset; /* 0 means above and 1 means below */

	uint8_t has_ldpc;
	uint8_t ldpc;
};

struct qtn_ap_set_security {
	char keymgnt[QTN_KEYMGNT_LEN];
	char if_name[QTN_INTERFACE_LEN];
	char passphrase[QTN_PASSWORD_LEN];
	char wepkey[QTN_PASSWORD_LEN];
	char ssid[QNT_SSID_LEN];

	uint8_t pmf;		// same as qcsapi_pmf
	uint8_t has_pmf;

	uint8_t sha256ad;	// 0 - disable
	uint8_t has_sha256ad;

	char encryption[QNT_ENCRYPTION_LEN];
};

struct qtn_dut_request {
	union {
		int empty_placeholder;
		struct qtn_ap_set_radius ap_radius;
		struct qtn_ap_set_wireless wireless;
		struct qtn_ap_set_security secutiry;
	};
};

struct qtn_dut_response {
	int32_t status;
	int32_t qcsapi_error;
	union {
		struct qtn_ap_get_info_result ap_info;
		struct qtn_ca_version ca_version;
	};
};

#endif
