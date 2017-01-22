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

#include "hapr_server.h"
#include "hapr_log.h"
#include "hapr_eloop.h"
#include "hapr_client.h"
#include "hapr_bss.h"
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#define QLINK_TEMPORARY_CONFIG_FILE "hapr.conf"

#define QLINK_PING_TIMEOUT 1000
#define QLINK_CTRL_CLIENT_CHECK_TIMEOUT 3000
#define QLINK_MAX_MISSED_PINGS 15

char config_path[1024] = { '\0' };

static struct hapr_eloop *server_eloop;
static struct qlink_server *qs;
static pthread_t thread_handle;

static int daemon_parent_fd = -1;

static struct hapr_client *client = NULL;
static uint32_t client_ping_timeout_ms = 0;

static void hapr_server_ping_timeout(void *user_data)
{
	HAPR_LOG(TRACE, "enter");

	hapr_client_destroy(client);
	client = NULL;
	client_ping_timeout_ms = 0;
}

static void hapr_server_op_ping(void *ptr)
{
	if (!client)
		return;

	hapr_eloop_cancel_timeout(server_eloop, &hapr_server_ping_timeout, NULL);

	hapr_eloop_register_timeout(server_eloop, &hapr_server_ping_timeout, NULL,
		client_ping_timeout_ms);
}

static void hapr_server_op_handshake(void *ptr, const uint8_t *addr, uint32_t version,
	const uint8_t *guid, uint32_t ping_timeout_ms, uint32_t flags)
{
	HAPR_LOG(INFO, "enter: " MACSTR ", %u, %u, 0x%X", MAC2STR(addr), version, ping_timeout_ms,
		flags);

	if (version != QLINK_VERSION) {
		HAPR_LOG(ERROR, "bad client version %d", version);
		if (client == NULL) {
			/*
			 * But answer anyway so that the client knows about this.
			 */
			qlink_server_set_client_addr(qs, addr);
			qlink_server_handshake(qs, guid, QLINK_PING_TIMEOUT);
			qlink_server_set_client_addr(qs, NULL);
		}
		return;
	}

	if (ping_timeout_ms < 1000) {
		HAPR_LOG(ERROR, "client ping timeout is too short");
		return;
	}

	if (client) {
		hapr_eloop_cancel_timeout(server_eloop, &hapr_server_ping_timeout, NULL);
		hapr_client_destroy(client);
		client = NULL;
		client_ping_timeout_ms = 0;
	}

	client = hapr_client_create(qs, addr, QLINK_PING_TIMEOUT, flags,
		QLINK_CTRL_CLIENT_CHECK_TIMEOUT);

	if (client == NULL) {
		return;
	}

	client_ping_timeout_ms = ping_timeout_ms * QLINK_MAX_MISSED_PINGS;

	hapr_eloop_register_timeout(server_eloop, &hapr_server_ping_timeout, NULL,
		client_ping_timeout_ms);

	qlink_server_handshake(qs, guid, QLINK_PING_TIMEOUT);
}

static int hapr_server_op_disconnect(void *ptr)
{
	if (!client)
		return -1;

	HAPR_LOG(INFO, "client disconnected");

	hapr_eloop_cancel_timeout(server_eloop, &hapr_server_ping_timeout, NULL);

	hapr_client_destroy(client);
	client = NULL;
	client_ping_timeout_ms = 0;

	return 0;
}

static int hapr_server_op_update_security_config(void *ptr, const char *security_config)
{
	char tmp_path[PATH_MAX * 2];
	char *config_file;

	HAPR_LOG(TRACE, "enter: %d", (int)strlen(security_config));

	if (!client)
		return -1;

	config_file = strrchr(config_path, '/');
	if (config_file) {
		memcpy(tmp_path, config_path, (config_file - &config_path[0]));
		tmp_path[config_file - &config_path[0]] = '\0';
		strcat(tmp_path, "/");
		strcat(tmp_path, QLINK_TEMPORARY_CONFIG_FILE);
	} else {
		strcpy(tmp_path, QLINK_TEMPORARY_CONFIG_FILE);
	}

	return hapr_write_config_file(security_config, config_path, tmp_path);
}

static int hapr_server_op_bss_add(void *ptr, uint32_t qid, const char *ifname, const char *brname,
	uint8_t *own_addr)
{
	struct hapr_bss *bss;

	HAPR_LOG(TRACE, "enter: %u, %s, %s", qid, ifname, brname);

	if (!client)
		return -1;

	bss = hapr_bss_create(client, qid, ifname, brname, 0);

	if (bss) {
		memcpy(own_addr, bss->own_addr, sizeof(bss->own_addr));
	}

	return bss ? 0 : -1;
}

static int hapr_server_op_bss_remove(void *ptr, uint32_t qid)
{
	struct hapr_bss *bss;

	HAPR_LOG(TRACE, "enter: %u", qid);

	if (!client)
		return -1;

	bss = hapr_client_get_bss(client, qid);

	if (!bss)
		return -1;

	hapr_bss_destroy(bss);

	return 0;
}

static int hapr_server_op_handle_bridge_eapol(void *ptr, uint32_t bss_qid)
{
	struct hapr_bss *bss;

	HAPR_LOG(TRACE, "enter: %u", bss_qid);

	if (!client)
		return -1;

	bss = hapr_client_get_bss(client, bss_qid);

	if (!bss)
		return -1;

	return hapr_bss_handle_bridge_eapol(bss);
}

static int hapr_server_op_get_driver_capa(void *ptr, uint32_t bss_qid, int *we_version,
	uint8_t *extended_capa_len, uint8_t **extended_capa, uint8_t **extended_capa_mask)
{
	struct hapr_bss *bss;

	HAPR_LOG(TRACE, "enter: %u", bss_qid);

	if (!client)
		return -1;

	bss = hapr_client_get_bss(client, bss_qid);

	if (!bss)
		return -1;

	*we_version = bss->we_version;

	return hapr_bss_get_extended_capa(bss, extended_capa_len, extended_capa,
		extended_capa_mask);
}

int hapr_server_op_set_ssid(void *ptr, uint32_t bss_qid, const uint8_t *buf, int len)
{
	struct hapr_bss *bss;

	HAPR_LOG(TRACE, "enter: %u, %s, %d", bss_qid, buf, len);

	if (!client)
		return -1;

	bss = hapr_client_get_bss(client, bss_qid);

	if (!bss)
		return -1;

	return hapr_bss_set_ssid(bss, buf, len);
}

int hapr_server_op_set_80211_param(void *ptr, uint32_t bss_qid, int op, int arg)
{
	struct hapr_bss *bss;

	HAPR_LOG(TRACE, "enter: %u, op = %d, arg = %d", bss_qid, op, arg);

	if (!client)
		return -1;

	bss = hapr_client_get_bss(client, bss_qid);

	if (!bss)
		return -1;

	return hapr_bss_set80211param(bss, op, arg);
}

int hapr_server_op_set_80211_priv(void *ptr, uint32_t bss_qid, int op, void *data, int len)
{
	struct hapr_bss *bss;

	HAPR_LOG(TRACE, "enter: %u, op = %d, len = %d", bss_qid, op, len);

	if (!client)
		return -1;

	bss = hapr_client_get_bss(client, bss_qid);

	if (!bss)
		return -1;

	return hapr_bss_set80211priv(bss, op, data, len);
}

int hapr_server_op_commit(void *ptr, uint32_t bss_qid)
{
	struct hapr_bss *bss;

	HAPR_LOG(TRACE, "enter: %u", bss_qid);

	if (!client)
		return -1;

	bss = hapr_client_get_bss(client, bss_qid);

	if (!bss)
		return -1;

	return hapr_bss_commit(bss);
}

int hapr_server_op_set_interworking(void *ptr, uint32_t bss_qid, uint8_t eid, int interworking,
	uint8_t access_network_type, const uint8_t *hessid)
{
	struct hapr_bss *bss;

	HAPR_LOG(TRACE, "enter: %u, eid = %d, interworking = %d, access_network_type = %d",
		bss_qid, (int)eid, interworking, (int)access_network_type);

	if (!client)
		return -1;

	bss = hapr_client_get_bss(client, bss_qid);

	if (!bss)
		return -1;

	return hapr_bss_set_interworking(bss, eid, interworking, access_network_type, hessid);
}

static int hapr_server_op_brcm(void *ptr, uint32_t bss_qid, uint8_t *data, uint32_t len)
{
	struct hapr_bss *bss;

	HAPR_LOG(TRACE, "enter: %u, len = %u", bss_qid, len);

	if (!client)
		return -1;

	bss = hapr_client_get_bss(client, bss_qid);

	if (!bss)
		return -1;

	return hapr_bss_brcm(bss, data, len);
}

static int hapr_server_op_check_if(void *ptr, uint32_t bss_qid)
{
	struct hapr_bss *bss;

	HAPR_LOG(TRACE, "enter: %u", bss_qid);

	if (!client)
		return -1;

	bss = hapr_client_get_bss(client, bss_qid);

	if (!bss)
		return -1;

	return hapr_bss_check_if(bss);
}

static int hapr_server_op_set_channel(void *ptr, uint32_t bss_qid, int channel)
{
	struct hapr_bss *bss;

	HAPR_LOG(TRACE, "enter: %u, channel = %d", bss_qid, channel);

	if (!client)
		return -1;

	bss = hapr_client_get_bss(client, bss_qid);

	if (!bss)
		return -1;

	return hapr_bss_set_channel(bss, channel);
}

static int hapr_server_op_get_ssid(void *ptr, uint32_t bss_qid, uint8_t *buf, int *len)
{
	struct hapr_bss *bss;

	HAPR_LOG(TRACE, "enter: %u, len = %d", bss_qid, *len);

	if (!client)
		return -1;

	bss = hapr_client_get_bss(client, bss_qid);

	if (!bss)
		return -1;

	return hapr_bss_get_ssid(bss, buf, len);
}

static int hapr_server_op_send_action(void *ptr, uint32_t bss_qid, const void *data, int len)
{
	struct hapr_bss *bss;

	HAPR_LOG(TRACE, "enter: %u, len = %d", bss_qid, len);

	if (!client)
		return -1;

	bss = hapr_client_get_bss(client, bss_qid);

	if (!bss)
		return -1;

	return hapr_bss_send_action(bss, data, len);
}

static int hapr_server_op_init_ctrl(void *ptr, const char *ctrl_dir, const char *ctrl_name)
{
	HAPR_LOG(TRACE, "enter: %s, %s", ctrl_dir, ctrl_name);

	if (!client)
		return -1;

	return hapr_client_ctrl_init(client, ctrl_dir, ctrl_name);
}

static int hapr_server_op_send_ctrl(void *ptr, const char *addr, const char *data, int len)
{
	if (!client)
		return -1;

	return hapr_client_ctrl_send(client, addr, data, len);
}

static int hapr_server_op_vap_add(void *ptr, uint32_t qid, const uint8_t *bssid,
	const char *ifname, const char *brname, uint8_t *own_addr)
{
	struct hapr_bss *bss;
	int ret;
	char buf[MADWIFI_CMD_BUF_SIZE];

	HAPR_LOG(TRACE, "enter: %u, " MACSTR ", %s, %s", qid, MAC2STR(bssid), ifname, brname);

	if (!client)
		return -1;

	ret = hapr_qdrv_vap_start(bssid, ifname);

	if (ret != 0)
		return -1;

	bss = hapr_bss_create(client, qid, ifname, brname, 1);
	if (!bss) {
		hapr_qdrv_vap_stop(ifname);
		return -1;
	}

	memcpy(own_addr, bss->own_addr, sizeof(bss->own_addr));

	snprintf(buf, sizeof(buf), "/scripts/tc_prio -dev %s -join > /dev/null", ifname);
	system(buf);

	return 0;
}

static int hapr_server_op_vlan_set_sta(void *ptr, const uint8_t *addr, int vlan_id)
{
	char mac[20];
	char buf[MADWIFI_CMD_BUF_SIZE];
	int ret;

	HAPR_LOG(TRACE, "enter: " MACSTR ", %d", MAC2STR(addr), vlan_id);

	if (!client)
		return -1;

	snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
		addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);

	snprintf(buf, sizeof(buf), "set dyn-vlan %s %d\n", mac, vlan_id);

	ret = hapr_qdrv_write_control(buf);
	if (ret < 0) {
		HAPR_LOG(ERROR, "bind STA VLAN failed");
		return -1;
	}

	return 0;
}

static int hapr_server_op_vlan_set_dyn(void *ptr, const char *ifname, int enable)
{
	char buf[MADWIFI_CMD_BUF_SIZE];
	int ret;

	HAPR_LOG(TRACE, "enter: %s, %d", ifname, enable);

	if (!client)
		return -1;

	snprintf(buf, sizeof(buf), "set vlan %s %s 1", ifname, enable ? "dynamic" : "undynamic");

	ret = hapr_qdrv_write_control(buf);
	if (ret < 0) {
		HAPR_LOG(ERROR, "dynamic VLAN failed");
		return -1;
	}

	return 0;
}

static int hapr_server_op_vlan_set_group(void *ptr, const char *ifname, int vlan_id, int enable)
{
	char buf[MADWIFI_CMD_BUF_SIZE];
	int ret;

	HAPR_LOG(TRACE, "enter: %s, %d, %d", ifname, vlan_id, enable);

	if (!client)
		return -1;

	snprintf(buf, sizeof(buf), "set vlan-group %s %d %d", ifname, vlan_id, !!enable);

	ret = hapr_qdrv_write_control(buf);
	if (ret < 0) {
		HAPR_LOG(ERROR, "set vlan group failed");
		return -1;
	}

	return 0;
}

static void hapr_server_op_ready(void *ptr)
{
	HAPR_LOG(TRACE, "enter");

	if (!client)
		return;

	if (daemon_parent_fd != -1) {
		hapr_signal_daemon_parent(daemon_parent_fd, 0);
		daemon_parent_fd = -1;
	}
}

static int hapr_server_op_set_qos_map(void *ptr, uint32_t bss_qid, uint8_t *buf)
{
	struct hapr_bss *bss;

	HAPR_LOG(TRACE, "enter: %u", bss_qid);

	if (!client)
		return -1;

	bss = hapr_client_get_bss(client, bss_qid);

	if (!bss)
		return -1;

	return hapr_bss_set_qos_map(bss, buf);
}

static struct qlink_server_ops hapr_server_ops = {
	.ping = &hapr_server_op_ping,
	.handshake = &hapr_server_op_handshake,
	.disconnect = &hapr_server_op_disconnect,
	.update_security_config = &hapr_server_op_update_security_config,
	.bss_add = &hapr_server_op_bss_add,
	.bss_remove = &hapr_server_op_bss_remove,
	.handle_bridge_eapol = &hapr_server_op_handle_bridge_eapol,
	.get_driver_capa = &hapr_server_op_get_driver_capa,
	.set_ssid = &hapr_server_op_set_ssid,
	.set_80211_param = &hapr_server_op_set_80211_param,
	.set_80211_priv = &hapr_server_op_set_80211_priv,
	.commit = &hapr_server_op_commit,
	.set_interworking = &hapr_server_op_set_interworking,
	.brcm = &hapr_server_op_brcm,
	.check_if = &hapr_server_op_check_if,
	.set_channel = &hapr_server_op_set_channel,
	.get_ssid = &hapr_server_op_get_ssid,
	.send_action = &hapr_server_op_send_action,
	.init_ctrl = &hapr_server_op_init_ctrl,
	.send_ctrl = &hapr_server_op_send_ctrl,
	.vap_add = &hapr_server_op_vap_add,
	.vlan_set_sta = &hapr_server_op_vlan_set_sta,
	.vlan_set_dyn = &hapr_server_op_vlan_set_dyn,
	.vlan_set_group = &hapr_server_op_vlan_set_group,
	.ready = &hapr_server_op_ready,
	.set_qos_map = &hapr_server_op_set_qos_map,
};

static void *hapr_server_thread(void *data)
{
	HAPR_LOG(TRACE, "enter");

	hapr_eloop_run(server_eloop);

	HAPR_LOG(TRACE, "exit");

	return NULL;
}

struct qlink_server *hapr_server_init(const char *config_file, const char *bind_iface)
{
	const char *ifname;

	HAPR_LOG(TRACE, "enter: %s", config_file);

	memset(config_path, 0, sizeof(config_path));
	strncpy(config_path, config_file, sizeof(config_path) - 1);

	server_eloop = hapr_eloop_create();

	if (server_eloop == NULL) {
		HAPR_LOG(ERROR, "cannot allocate server eloop, no mem");
		return NULL;
	}

	if (bind_iface) {
		HAPR_LOG(INFO, "force bind to %s iface", bind_iface);
		if (if_nametoindex(bind_iface) == 0) {
			HAPR_LOG(ERROR, "failed to bind to %s", bind_iface);
			goto fail_netif;
		}
		ifname = bind_iface;
	} else if (if_nametoindex("br0")) {
		ifname = "br0";
	} else if (if_nametoindex("bond0")) {
		ifname = "bond0";
	} else if (if_nametoindex("pcie0")) {
		ifname = "pcie0";
	} else if (if_nametoindex("eth1_0")) {
		ifname = "eth1_0";
	} else if (if_nametoindex("eth1_1")) {
		ifname = "eth1_1";
	} else {
		HAPR_LOG(ERROR, "no suitable netif to create qlink server on");
		goto fail_netif;
	}

	HAPR_LOG(INFO, "using iface %s", ifname);

	qs = qlink_server_create(server_eloop, &hapr_server_ops, NULL, ifname);

fail_netif:
	if (qs == NULL) {
		hapr_eloop_destroy(server_eloop);
	}

	HAPR_LOG(TRACE, "exit");

	return qs;
}

void hapr_server_run(int global_daemon_parent_fd)
{
	int ret;
	sigset_t signals, old_signals;

	HAPR_LOG(TRACE, "enter");

	daemon_parent_fd = global_daemon_parent_fd;

	sigemptyset(&signals);

	sigaddset(&signals, SIGHUP);
	sigaddset(&signals, SIGTERM);
	sigaddset(&signals, SIGINT);
	sigaddset(&signals, SIGCHLD);
	sigaddset(&signals, SIGQUIT);
	sigaddset(&signals, SIGALRM);
	sigaddset(&signals, SIGUSR1);
	sigaddset(&signals, SIGUSR2);
	sigaddset(&signals, SIGPIPE);

	/*
	 * qlink server thread should not handle any signals.
	 */

        ret = pthread_sigmask(SIG_SETMASK, &signals, &old_signals);

	if (pthread_create(&thread_handle, NULL, &hapr_server_thread, NULL) != 0) {
		HAPR_LOG(ERROR, "cannot create server thread");
	}

	if (ret == 0)
		pthread_sigmask(SIG_SETMASK, &old_signals, 0);

	HAPR_LOG(TRACE, "exit");
}

void hapr_server_deinit(void)
{
	HAPR_LOG(TRACE, "enter");

	if (thread_handle) {
		if (client)
			qlink_server_disconnect(qs);

		hapr_eloop_terminate(server_eloop);

		pthread_join(thread_handle, NULL);

		if (client) {
			/*
			 * we're in process of shutting down, but client is still here, destroy it.
			 * it'll only destroy critical stuff like VAPs, etc. we don't bother
			 * with handling remaining timeouts, sockets, etc.
			 */
			hapr_client_destroy(client);
			client = NULL;
		}
	}

	qlink_server_destroy(qs);

	hapr_eloop_destroy(server_eloop);

	HAPR_LOG(TRACE, "exit");
}
