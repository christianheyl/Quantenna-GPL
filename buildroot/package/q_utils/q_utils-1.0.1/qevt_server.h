/**
* Copyright (c) 2016 Quantenna Communications, Inc.
* All rights reserved.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**/

#ifndef __QEVT_SERVER_H_
#define __QEVT_SERVER_H_
#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/time.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <signal.h>
#include <wpa_ctrl.h>
#include <dirent.h>

#include <sys/inotify.h>
#include <limits.h>
#include <sys/un.h>

#include <qtn_logging.h>
#include <pthread.h>

#include "iwlib.h"
#include "qevt_event_id.h"

#define QEVT_DEFAULT_PORT 3490
#define QEVT_TX_BUF_SIZE 1024
#define QEVT_RX_BUF_SIZE 4096
#define QEVT_TXT_PREFIX	"QEVT: "
#define QEVT_CONFIG_VERSION "v1.10"
#define QEVT_CONFIG_OLD_VERSION "v1.00"
#define QEVT_VERSION "QEVT_VERSION"
#define QEVT_CONFIG  "QEVT_CONFIG"
#define QEVT_CONFIG_RESET QEVT_CONFIG"_RESET"
#define QEVT_CONFIG_RX_BUF 256
#define QEVT_MAX_MSG_CONFIGS 32
#define QEVT_MAX_PREFIX_LEN 15
#define QEVT_WPA_PREFIX "WPACTRL"
#define QEVT_HOSTAPD "/var/run/hostapd/"
#define QEVT_WPA     "/var/run/wpa_supplicant/"
#define QEVT_WIFI_INTERFACE_NAME "wifi"
#define QEVT_MAX_CLIENT_CONNECTIONS 4

#define MILLISECONDS_IN_ONE_SECOND	1000
#define MICROSECONDS_IN_ONE_SECOND	1000000
#define NANOSECONDS_IN_ONE_SECOND	1000000000

#define MICROSECONDS_IN_ONE_MILLISECOND	(MICROSECONDS_IN_ONE_SECOND / MILLISECONDS_IN_ONE_SECOND)
#define NANOSECONDS_IN_ONE_MICROSECOND	(NANOSECONDS_IN_ONE_SECOND / MICROSECONDS_IN_ONE_SECOND)

#define DEFAULT_CORE_FILE_PATH	"/var/log/core/"
#define QEVT_SYSTEM_EVENT_SOCKET_PATH "/tmp/qevt_system_event_socket"

/*
 * Static information about wireless interface.
 * We cache this info for performance reasons.
 */
struct qevt_wifi_iface {
	struct qevt_wifi_iface	*next;			/* Linked list */
	int			ifindex;		/* Interface index */
	char			ifname[IFNAMSIZ + 1];	/* Interface name */
	struct			iw_range range;		/* Wireless static data */
	int			has_range;
};

struct qevt_wpa_iface {
	struct qevt_wpa_iface	*next;
	struct wpa_ctrl		*wpa_event;
	struct wpa_ctrl		*wpa_control;
	int			fd;
	uint32_t		event_check	:1;
	uint32_t		dead		:1;
	char			ifname[IFNAMSIZ + 1];	/* Interface name */
};

struct qevt_config_flags {
	uint32_t dynamic	: 1;
	uint32_t seen		: 1;
	uint32_t disabled	: 1;
	uint32_t prfx		: 1;
	uint32_t interface	: 1;
	uint32_t timestamp	: 1;
	uint32_t message	: 1;
	uint32_t newline	: 1;
	uint32_t seqnum		: 1;
};

struct qevt_msg {
	struct qevt_msg			*next;
	char				*prefix;
	struct qevt_config_flags	flags;
};

struct qevt_msg_config {
	struct qevt_msg *msg;
	uint32_t msg_count;
};

struct qevt_client {
	int			client_socket;
#define CONFIG_TYPE_MSG 0	/* message based */
#define CONFIG_TYPE_EVENT_ID 1	/* event_id based */
	uint8_t			config_type;	/* 0: message based, 1: event_id based */
	union {
		struct qevt_msg_config		msg_config;
		struct qevt_event_id_config	event_id_config;
	} u;
};

struct qevt_server_config {
	struct qevt_wifi_iface		*if_cache;		/* Cache of wireless interfaces */
	struct qevt_wpa_iface		*if_wpa;
	pthread_t			wpa_thread;
	pthread_mutex_t			wpa_mutex;
	volatile int			running;
	struct sockaddr_nl		netlink_dst;
	int				netlink_socket;
	struct qevt_client		client[QEVT_MAX_CLIENT_CONNECTIONS];
	int				num_client_connections;
	int				server_socket;
	uint16_t			port;
	struct qevt_event_id_mapping	*event_id_mapping;
	int				coredump_fd;
	int				system_event_fd;
};

void qevt_send_to_client(const struct qevt_client * const client, const char *fmt, ...);

void qevt_event_id_cfg_cleanup(struct qevt_client * const client);
int qevt_client_config_event_id(struct qevt_client *client, char *evt_msg);
void qevt_send_event_id_to_client(struct qevt_client * const client,
				char *message, const char * const ifname);

#endif /* _QEVT_SERVER_H_ */
