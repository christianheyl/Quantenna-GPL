/**
  Copyright (c) 2008 - 2015 Quantenna Communications Inc
  All Rights Reserved

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

 **/

#ifndef _QLINK_SERVER_H_
#define _QLINK_SERVER_H_

#include "hapr_types.h"
#include "hapr_eloop.h"

struct qlink_server_ops
{
	void (*ping)(void *ptr);

	void (*handshake)(void *ptr, const uint8_t *addr, uint32_t version,
		const uint8_t *guid, uint32_t ping_timeout_ms, uint32_t flags);

	int (*disconnect)(void *ptr);

	int (*update_security_config)(void *ptr, const char *security_config);

	int (*bss_add)(void *ptr, uint32_t qid, const char *ifname, const char *brname,
		uint8_t *own_addr);

	int (*bss_remove)(void *ptr, uint32_t qid);

	int (*handle_bridge_eapol)(void *ptr, uint32_t bss_qid);

	int (*get_driver_capa)(void *ptr, uint32_t bss_qid, int *we_version,
		uint8_t *extended_capa_len, uint8_t **extended_capa, uint8_t **extended_capa_mask);

	int (*set_ssid)(void *ptr, uint32_t bss_qid, const uint8_t *buf, int len);

	int (*set_80211_param)(void *ptr, uint32_t bss_qid, int op, int arg);

	int (*set_80211_priv)(void *ptr, uint32_t bss_qid, int op, void *data, int len);

	int (*commit)(void *ptr, uint32_t bss_qid);

	int (*set_interworking)(void *ptr, uint32_t bss_qid, uint8_t eid, int interworking,
		uint8_t access_network_type, const uint8_t *hessid);

	int (*brcm)(void *ptr, uint32_t bss_qid, uint8_t *data, uint32_t len);

	int (*check_if)(void *ptr, uint32_t bss_qid);

	int (*set_channel)(void *ptr, uint32_t bss_qid, int channel);

	int (*get_ssid)(void *ptr, uint32_t bss_qid, uint8_t *buf, int *len);

	int (*send_action)(void *ptr, uint32_t bss_qid, const void *data, int len);

	int (*init_ctrl)(void *ptr, const char *ctrl_dir, const char *ctrl_name);

	int (*send_ctrl)(void *ptr, const char *addr, const char *data, int len);

	int (*vap_add)(void *ptr, uint32_t qid, const uint8_t *bssid, const char *ifname,
		const char *brname, uint8_t *own_addr);

	int (*vlan_set_sta)(void *ptr, const uint8_t *addr, int vlan_id);

	int (*vlan_set_dyn)(void *ptr, const char *ifname, int enable);

	int (*vlan_set_group)(void *ptr, const char *ifname, int vlan_id, int enable);

	int (*set_qos_map)(void *ptr, uint32_t bss_qid, uint8_t *buf);

	void (*ready)(void *ptr);
};

struct qlink_server;

struct qlink_server *qlink_server_create(struct hapr_eloop *eloop, struct qlink_server_ops *ops,
	void *ops_ptr, const char *ifname);

void qlink_server_destroy(struct qlink_server *qs);

void qlink_server_ping(struct qlink_server *qs);

void qlink_server_handshake(struct qlink_server *qs, const uint8_t *guid,
	uint32_t ping_timeout_ms);

int qlink_server_disconnect(struct qlink_server *qs);

void qlink_server_set_client_addr(struct qlink_server *qs, const uint8_t *client_addr);

int qlink_server_update_security_config(struct qlink_server *qs, const char *security_config);

int qlink_server_recv_packet(struct qlink_server *qs, uint32_t bss_qid, uint16_t protocol,
	const uint8_t *src_addr, const uint8_t *buf, size_t len);

int qlink_server_recv_iwevent(struct qlink_server *qs, uint32_t bss_qid, uint16_t wcmd,
	const char *data, int len);

int qlink_server_recv_ctrl(struct qlink_server *qs, const char *addr, char *data, int len);

int qlink_server_invalidate_ctrl(struct qlink_server *qs, const char *addr);

#endif
