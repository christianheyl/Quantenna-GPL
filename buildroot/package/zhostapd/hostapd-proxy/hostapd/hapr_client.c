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

#include "hapr_client.h"
#include "hapr_eloop.h"
#include "hapr_log.h"
#include "hapr_bss.h"
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>

extern struct hapr_eloop *main_eloop;
extern char config_path[];

struct hapr_ctrl_client
{
	struct hapr_list list;
	struct sockaddr_un addr;
};

static void hapr_ctrl_client_remove(struct hapr_ctrl_client *cc)
{
	HAPR_LOG(TRACE, "ctrl client %s destroyed", cc->addr.sun_path);
	hapr_list_remove(&cc->list);
	free(cc);
}

static void hapr_client_ctrl_receive(int sock, void *user_data)
{
	struct hapr_client *client = user_data;
	char buf[QLINK_CTRL_MSG_BUF_SIZE];
	int ret;
	struct sockaddr_un from;
	socklen_t fromlen = sizeof(from);
	struct hapr_ctrl_client *cc;

	do {
		ret = recvfrom(sock, buf, sizeof(buf) - 1, 0, (struct sockaddr *)&from, &fromlen);
	} while ((ret < 0) && (errno == EINTR));

	if (ret < 0) {
		HAPR_LOG(ERROR, "recvfrom error: %s", strerror(errno));
		return;
	}

	buf[ret] = '\0';

	hapr_list_for_each(struct hapr_ctrl_client, cc, &client->ctrl_clients, list) {
		if (strcmp(cc->addr.sun_path, from.sun_path) == 0) {
			goto out;
		}
	}

	cc = malloc(sizeof(*cc));

	if (!cc) {
		HAPR_LOG(ERROR, "cannot allocate ctrl_client, no mem");
		return;
	}

	memset(cc, 0, sizeof(*cc));

	memcpy(&cc->addr, &from, sizeof(cc->addr));

	hapr_list_add(&client->ctrl_clients, &cc->list);

	HAPR_LOG(TRACE, "new ctrl client %s", from.sun_path);

out:
	if ((strcmp(buf, "RECONFIGURE") == 0) ||
		(strncmp(buf, "UPDATE_BSSCONFIG ", 17) == 0) ||
		(strncmp(buf, "CREATE_BSSCONFIG ", 17) == 0) ||
		(strncmp(buf, "REMOVE_BSSCONFIG ", 17) == 0)) {
		/*
		 * we need to send security config before these commands, qcsapi seems to not
		 * SIGHUP hostapd before issuing these.
		 */
		char *security_config = hapr_read_config_file(config_path);
		if (security_config) {
			qlink_server_update_security_config(client->qs, security_config);
			free(security_config);
		}
	}

	qlink_server_recv_ctrl(client->qs, from.sun_path, buf, ret);
}

static void hapr_ctrl_client_check_timeout(void *user_data)
{
	struct hapr_client *client = user_data;
	struct hapr_ctrl_client *cc, *tmp;
	struct stat st;

	hapr_list_for_each_safe(struct hapr_ctrl_client, cc, tmp, &client->ctrl_clients,
		list) {
		if (stat(cc->addr.sun_path, &st) != 0) {
			qlink_server_invalidate_ctrl(client->qs, cc->addr.sun_path);
			hapr_ctrl_client_remove(cc);
		}
	}

	hapr_eloop_register_timeout(main_eloop, &hapr_ctrl_client_check_timeout, client,
		client->ctrl_client_check_timeout_ms);
}

static void hapr_client_ping_timeout(void *user_data)
{
	struct hapr_client *client = user_data;

	qlink_server_ping(client->qs);

	hapr_eloop_register_timeout(main_eloop, &hapr_client_ping_timeout, client,
		client->ping_timeout_ms);
}

static void hapr_client_start(void *user_data)
{
	struct hapr_client *client = user_data;

	hapr_eloop_register_timeout(main_eloop, &hapr_client_ping_timeout, client,
		client->ping_timeout_ms);
}

static void hapr_client_ctrl_start(void *user_data)
{
	struct hapr_client *client = user_data;

	hapr_eloop_register_read_sock(main_eloop, client->ctrl_sock, hapr_client_ctrl_receive,
		client);

	hapr_eloop_register_timeout(main_eloop, &hapr_ctrl_client_check_timeout, client,
		client->ctrl_client_check_timeout_ms);
}

static void hapr_client_stop(void *user_data)
{
	struct hapr_client *client = user_data;

	hapr_eloop_cancel_timeout(main_eloop, &hapr_client_ping_timeout, client);

	if (client->ctrl_sock != -1) {
		struct hapr_ctrl_client *cc, *tmp;

		hapr_list_for_each_safe(struct hapr_ctrl_client, cc, tmp, &client->ctrl_clients,
			list) {
			hapr_ctrl_client_remove(cc);
		}

		hapr_eloop_unregister_read_sock(main_eloop, client->ctrl_sock);
		hapr_eloop_cancel_timeout(main_eloop, &hapr_ctrl_client_check_timeout, client);

		unlink(client->ctrl_addr.sun_path);
		close(client->ctrl_sock);
	}

	HAPR_LOG(TRACE, "client %p destroyed", client);

	free(client);
}

struct hapr_client *hapr_client_create(struct qlink_server *qs, const uint8_t *addr,
	uint32_t ping_timeout_ms, uint32_t flags, uint32_t ctrl_client_check_timeout_ms)
{
	struct hapr_client *client;

	client = malloc(sizeof(*client));

	if (client == NULL) {
		HAPR_LOG(ERROR, "cannot allocate client, no mem");
		goto fail_alloc;
	}

	memset(client, 0, sizeof(*client));

	client->qs = qs;
	client->ping_timeout_ms = ping_timeout_ms;
	client->flags = flags;
	client->ctrl_client_check_timeout_ms = ctrl_client_check_timeout_ms;
	hapr_list_init(&client->bss_list);

	client->ctrl_sock = -1;
	hapr_list_init(&client->ctrl_clients);

	HAPR_LOG(TRACE, "new client %p", client);

	qlink_server_set_client_addr(qs, addr);

	hapr_eloop_register_timeout(main_eloop, &hapr_client_start, client, 0);

	return client;

fail_alloc:

	return NULL;
}

void hapr_client_destroy(struct hapr_client *client)
{
	struct hapr_bss *bss, *tmp;

	qlink_server_set_client_addr(client->qs, NULL);

	hapr_list_for_each_safe(struct hapr_bss, bss, tmp, &client->bss_list, list) {
		hapr_bss_destroy(bss);
	}

	hapr_eloop_register_timeout(main_eloop, &hapr_client_stop, client, 0);
}

struct hapr_bss *hapr_client_get_bss(struct hapr_client *client, uint32_t qid)
{
	struct hapr_bss *bss;

	hapr_list_for_each(struct hapr_bss, bss, &client->bss_list, list) {
		if (bss->qid == qid) {
			return bss;
		}
	}

	HAPR_LOG(ERROR, "bss %u not found", qid);

	return NULL;
}

int hapr_client_ctrl_init(struct hapr_client *client, const char *ctrl_dir, const char *ctrl_name)
{
	struct sockaddr_un addr;
	int sock;
	size_t len = strlen(ctrl_dir) + strlen(ctrl_name) + 2;

	if (client->ctrl_sock != -1) {
		HAPR_LOG(ERROR, "ctrl interface already started");
		goto fail_started;
	}

	if (len > sizeof(addr.sun_path)) {
		HAPR_LOG(ERROR, "ctrl interface path is too long");
		goto fail_check_path;
	}

	if (mkdir(ctrl_dir, S_IRWXU | S_IRWXG) < 0) {
		if (errno == EEXIST) {
			HAPR_LOG(DEBUG, "Using existing control interface directory");
		} else {
			HAPR_LOG(ERROR, "mkdir[ctrl_interface]");
			goto fail_check_path;
		}
	}

	sock = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (sock < 0) {
		HAPR_LOG(ERROR, "socket(PF_UNIX)");
		goto fail_check_path;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, len, "%s/%s", ctrl_dir, ctrl_name);
	addr.sun_path[len - 1] = '\0';

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		HAPR_LOG(DEBUG, "ctrl_iface bind(PF_UNIX) failed: %s", strerror(errno));
		if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			HAPR_LOG(DEBUG, "ctrl_iface exists, but does not"
				" allow connections - assuming it was left"
				" over from forced program termination");
			if (unlink(addr.sun_path) < 0) {
				HAPR_LOG(ERROR, "Could not unlink "
					"existing ctrl_iface socket '%s'",
					addr.sun_path);
				goto fail_bind;
			}
			if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
				HAPR_LOG(ERROR, "hostapd-ctrl-iface: bind(PF_UNIX)");
				goto fail_bind;
			}
			HAPR_LOG(DEBUG, "Successfully replaced leftover "
				"ctrl_iface socket '%s'", addr.sun_path);
		} else {
			HAPR_LOG(INFO, "ctrl_iface exists and seems to "
				"be in use - cannot override it");
			HAPR_LOG(INFO, "Delete '%s' manually if it is "
				"not used anymore", addr.sun_path);
			goto fail_bind;
		}
	}

	if (chmod(addr.sun_path, S_IRWXU | S_IRWXG) < 0) {
		HAPR_LOG(ERROR, "chmod[ctrl_interface/ifname]");
		goto fail_chmod;
	}

	client->ctrl_sock = sock;
	memcpy(&client->ctrl_addr, &addr, sizeof(client->ctrl_addr));

	hapr_eloop_register_timeout(main_eloop, &hapr_client_ctrl_start, client, 0);

	return 0;

fail_chmod:
	unlink(addr.sun_path);
fail_bind:
	close(sock);
fail_check_path:
fail_started:

	return -1;
}

int hapr_client_ctrl_send(struct hapr_client *client, const char *addr, const char *data, int len)
{
	struct sockaddr_un to;
	socklen_t tolen = sizeof(to);
	int ret;

	if (client->ctrl_sock == -1) {
		HAPR_LOG(ERROR, "ctrl interface not started");
		return -1;
	}

	to.sun_family = AF_UNIX;
	strncpy(to.sun_path, addr, sizeof(to.sun_path) - 1);

	do {
		ret = sendto(client->ctrl_sock, data, len, 0, (struct sockaddr *)&to, tolen);
	} while ((ret < 0) && (errno == EINTR));

	if (ret != len) {
		HAPR_LOG(ERROR, "sendto(%s) returned %d, len = %d", addr, ret, len);
		return -1;
	}

	return 0;
}
