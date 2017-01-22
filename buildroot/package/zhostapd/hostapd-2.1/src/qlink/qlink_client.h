/*
 * hostapd - qlink client code
 * Copyright (c) 2008 - 2015 Quantenna Communications Inc
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 *
 */

#ifndef _QLINK_CLIENT_H_
#define _QLINK_CLIENT_H_

#include "include/qlink_protocol.h"
#include "includes.h"
#include "common.h"

struct qlink_client_ops
{
	void (*ping)(void *ptr);

	void (*handshake)(void *ptr, uint32_t version, const uint8_t *guid,
		uint32_t ping_timeout_ms);

	int (*disconnect)(void *ptr);

	int (*update_security_config)(void *ptr, const char *security_config);

	int (*recv_packet)(void *ptr, uint32_t bss_qid, uint16_t protocol,
		const uint8_t *src_addr, const uint8_t *buf, size_t len);

	int (*recv_iwevent)(void *ptr, uint32_t bss_qid, uint16_t cmd, char *data, int len);

	int (*recv_ctrl)(void *ptr, const char *addr, char *data, int len);

	int (*invalidate_ctrl)(void *ptr, const char *addr);
};

struct qlink_client;

struct qlink_client *qlink_client_init(struct qlink_client_ops *ops, void *ops_ptr,
	const char *ifname, const uint8_t *server_addr);

void qlink_client_deinit(struct qlink_client *qc);

void qlink_client_handshake(struct qlink_client *qc, const uint8_t *guid,
	uint32_t ping_timeout_ms, uint32_t flags);

int qlink_client_wait_handshake(struct qlink_client *qc, uint32_t timeout_ms, uint32_t *version,
	uint8_t *guid, uint32_t *ping_timeout_ms);

void qlink_client_set_active(struct qlink_client *qc, int active);

void qlink_client_ping(struct qlink_client *qc);

int qlink_client_disconnect(struct qlink_client *qc);

int qlink_client_update_security_config(struct qlink_client *qc, const char *security_config);

int qlink_client_bss_add(struct qlink_client *qc, uint32_t qid, const char *ifname,
	const char *brname, uint8_t *own_addr);

int qlink_client_bss_remove(struct qlink_client *qc, uint32_t bss_qid);

int qlink_client_handle_bridge_eapol(struct qlink_client *qc, uint32_t bss_qid);

int qlink_client_get_driver_capa(struct qlink_client *qc, uint32_t bss_qid, int *we_version,
	uint8_t *extended_capa_len, uint8_t **extended_capa, uint8_t **extended_capa_mask);

int qlink_client_set_ssid(struct qlink_client *qc, uint32_t bss_qid, const uint8_t *buf, int len);

int qlink_client_set_80211_param(struct qlink_client *qc, uint32_t bss_qid, int op, int arg);

/*
 * Note that 'data' is opaque and will be sent to server as is, ensure proper alignment
 * and endianess if needed.
 */
int qlink_client_set_80211_priv(struct qlink_client *qc, uint32_t bss_qid, int op, void *data,
	int len);

int qlink_client_commit(struct qlink_client *qc, uint32_t bss_qid);

int qlink_client_set_interworking(struct qlink_client *qc, uint32_t bss_qid, uint8_t eid,
	int interworking, uint8_t access_network_type, const uint8_t *hessid);

int qlink_client_brcm(struct qlink_client *qc, uint32_t bss_qid, uint8_t *data, uint32_t len);

int qlink_client_check_if(struct qlink_client *qc, uint32_t bss_qid);

int qlink_client_set_channel(struct qlink_client *qc, uint32_t bss_qid, int channel);

int qlink_client_get_ssid(struct qlink_client *qc, uint32_t bss_qid, uint8_t *buf, int *len);

int qlink_client_send_action(struct qlink_client *qc, uint32_t bss_qid, const void *data, int len);

int qlink_client_init_ctrl(struct qlink_client *qc, const char *ctrl_dir,
	const char *ctrl_name);

int qlink_client_send_ctrl(struct qlink_client *qc, const char *addr, const char *data, int len);

int qlink_client_vap_add(struct qlink_client *qc, uint32_t qid, const uint8_t *bssid,
	const char *ifname, const char *brname, uint8_t *own_addr);

int qlink_client_vlan_set_sta(struct qlink_client *qc, const uint8_t *addr, int vlan_id);

int qlink_client_vlan_set_dyn(struct qlink_client *qc, const char *ifname, int enable);

int qlink_client_vlan_set_group(struct qlink_client *qc, const char *ifname, int vlan_id,
	int enable);

int qlink_client_ready(struct qlink_client *qc);

#endif
