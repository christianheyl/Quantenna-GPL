/**
* Copyright (c) 2015 Quantenna Communications, Inc.
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

#include "qevt_server.h"

struct qevt_server_config qevt_server_cfg = {.if_cache = NULL, .if_wpa = NULL, .running = 0,
	.wpa_mutex = PTHREAD_MUTEX_INITIALIZER, .netlink_socket = -1, .server_socket = -1 ,
	.event_id_mapping = NULL, .coredump_fd = -1, .system_event_fd= -1};

char* qevt_nondefined_prefix = "nondefined";
char* qevt_default_prefix = "default";

unsigned int qevt_seq_number = 0;

/* these are built-in initial settings - used to initialise qevt_nondef & qevt_default */
static const struct qevt_config_flags qevt_nondef_flags_init = {
	.seen = 0,
	.disabled = 1,
	.prfx = 1,
	.interface = 0,
	.timestamp = 0,
	.message = 0,
	.newline = 0,
	.seqnum = 0
};
static const struct qevt_config_flags qevt_default_flags_init = {
	.seen = 0,
	.disabled = 0,
	.prfx = 1,
	.interface = 1,
	.timestamp = 0,
	.message = 1,
	.newline = 1,
	.seqnum = 0
};

static void qevt_iwevent_action(const struct iw_event * const event, const char * const ifname);
static void qevt_wpa_close(struct qevt_wpa_iface * const if_wpa);

/*
 * qevt_prefix function takes a input line and searches for a "prefix" within that. A prefix
 * is a consecutive sequence of (non-space) characters, immediately followed by a colon (":")
 * For example ThisIsAPrefix:
 * A special exclusion is that a MAC address is specifically tested, to avoid being mistaken as
 * a prefix.
 * Also, the text immediately from the prefix is returned (if pointer is provided for that).
 */
static const char * qevt_prefix(const char *msg, char ** const text)
{
	char *colon;
	char *space;

	/* search out prefix: */
	while ((colon = strstr(msg, ":")) && (space = strstr(msg, " ")) && (space < colon)) {
		msg = space + 1;
	}

	/* is this a free-form MAC address mistaken as a prefix ? */
	if (colon && (colon >= msg + 2)) {
		unsigned int mac_dummy;

		/* check if we identify all 6 MAC address elements - if so, reject */
		if (sscanf(colon - 2, "%02x:%02x:%02x:%02x:%02x:%02x", &mac_dummy, &mac_dummy,
				&mac_dummy, &mac_dummy, &mac_dummy, &mac_dummy) == 6) {
			return NULL;
		}
	}

	/* prefix cannot be longer than QEVT_MAX_PREFIX_LEN and must be alphanumeric */
	if (colon && (msg < colon) && ((msg + QEVT_MAX_PREFIX_LEN)  >= colon)) {
		const char *check = msg;
		int  alphacount = 0;
		int  numbercount = 0;

		/* check all prefix characters are only alphanumeric (or underscore) */
		while (check < colon) {
			char test = *check;

			if (isalpha(test) || test == '_') {
				alphacount++;
			} else if (isdigit(test)) {
				numbercount++;
			} else {
				return NULL;
			}
			check++;
		}

		/* there has to be at least 1 non-numeric character and no more than 2 numeric */
		if (alphacount == 0 || numbercount > 2) {
			return NULL;
		}

		/* check only a single colon */
		if (colon[1] == ':') {
			return NULL;
		}

		if (text) {
			*text = colon;
		}
		return msg;
	}
	return NULL;
}

static struct qevt_msg * qevt_msgcfg_find(struct qevt_msg ** const phead,
						const char * const prefix)
{
	struct qevt_msg * msg_cfg = phead ? *phead : NULL;

	while (msg_cfg) {
		if (!strcmp(msg_cfg->prefix, prefix)) {
			return msg_cfg;
		}
		msg_cfg = msg_cfg->next;
	}
	return NULL;
}

static struct qevt_msg * qevt_msgcfg_add(struct qevt_msg ** const phead,
					const char * const prefix, uint32_t * const msg_count)
{
	struct qevt_msg *msg_cfg;
	struct qevt_config_flags   flags_default;
	char *data;

	/* avoid generating excessive error messages (just do it once) - use bit 31 to mark this */
	if (*msg_count >= QEVT_MAX_MSG_CONFIGS) {
		if (!(*msg_count & (1<<31))) {
			fprintf(stderr, QEVT_TXT_PREFIX"maximum number (%d) of message prefix "
				"types reached\n", QEVT_MAX_MSG_CONFIGS);
			*msg_count |= (1<<31);
		}
		return NULL;
	}

	/* handle built in pseudo-type */
	msg_cfg = qevt_msgcfg_find(phead, qevt_default_prefix);

	if (msg_cfg != NULL) {
		flags_default = msg_cfg->flags;
	} else if (!strcmp(prefix, qevt_nondefined_prefix)) {
		flags_default = qevt_nondef_flags_init;
	} else {
		flags_default = qevt_default_flags_init;
	}

	msg_cfg = malloc(sizeof(*msg_cfg));
	data = msg_cfg ? malloc(strlen(prefix) + 1) : NULL;

	if (!msg_cfg || !data) {
		fprintf(stderr, QEVT_TXT_PREFIX"malloc failed\n");

		if (msg_cfg) {
			free(msg_cfg);
		}
		return NULL;
	}
	(*msg_count)++;

	memset(msg_cfg, 0, sizeof(*msg_cfg));

	msg_cfg->prefix = data;
	msg_cfg->flags = flags_default;
	msg_cfg->flags.dynamic = 1;

	strcpy(msg_cfg->prefix, prefix);

	if (*phead) {
		struct qevt_msg *head = *phead;

		while (head && head->next) {
			head = head->next;
		}
		head->next = msg_cfg;
	} else {
		*phead = msg_cfg;
	}

	return msg_cfg;
}

static void qevt_msgcfg_update_all_clients(const char * const prefix)
{
	int i;

	for (i = 0; i < QEVT_MAX_CLIENT_CONNECTIONS; i++) {
		struct qevt_client *client = &qevt_server_cfg.client[i];
		if (client->config_type == CONFIG_TYPE_MSG) {
			if (client->client_socket >= 0 && !qevt_msgcfg_find(&client->u.msg_config.msg,
						prefix)) {
				qevt_msgcfg_add(&client->u.msg_config.msg, prefix, &client->u.msg_config.msg_count);
			}
		}
	}
}


static void qevt_msgcfg_cleanup(struct qevt_client * const client)
{
	if (client != NULL) {
		struct qevt_msg *head = client->u.msg_config.msg;

		/* delete all the dynamically allocated list entries */
		while (head) {
			struct qevt_msg *next = head->next;

			if (head->flags.dynamic) {
				if (head->prefix) {
					free(head->prefix);
				}
				free(head);
			}
			head = next;
		}
		client->u.msg_config.msg_count = 0;
		client->u.msg_config.msg = NULL;
	}
}

static void qevt_cache_free(void)
{
	struct qevt_wifi_iface *curr = qevt_server_cfg.if_cache;
	struct qevt_wifi_iface *prev;

	while (curr) {
		prev = curr;
		curr = curr->next;
		if (prev) {
			free(prev);
		}
	}
}

static void qevt_wpa_cleanup()
{
	struct qevt_wpa_iface *head = qevt_server_cfg.if_wpa;

	qevt_server_cfg.running = 0;
	pthread_join(qevt_server_cfg.wpa_thread, NULL);

	while (head) {
		struct qevt_wpa_iface *next = head->next;

		qevt_wpa_close(head);
		free(head);
		head = next;
	}
	qevt_server_cfg.if_wpa = NULL;
}

static void qevt_client_close(struct qevt_client * const client)
{
	if (client && client->client_socket >= 0) {
		close(client->client_socket);
		client->client_socket = -1;
		qevt_server_cfg.num_client_connections--;
	}
	if (client->config_type == CONFIG_TYPE_EVENT_ID) {
		qevt_event_id_cfg_cleanup(client);
	} else {
		qevt_msgcfg_cleanup(client);
	}
}

static void qevt_cleanup()
{
	int i;

	for (i = 0; i < QEVT_MAX_CLIENT_CONNECTIONS; i++ ) {
		qevt_client_close(&qevt_server_cfg.client[i]);
	}
	if (qevt_server_cfg.server_socket >= 0) {
		close(qevt_server_cfg.server_socket);
	}
	qevt_server_cfg.server_socket = -1;

	if (qevt_server_cfg.netlink_socket >= 0) {
		close(qevt_server_cfg.netlink_socket);
	}
	qevt_server_cfg.netlink_socket = -1;

	if (qevt_server_cfg.coredump_fd >= 0) {
		close(qevt_server_cfg.coredump_fd);
	}
	qevt_server_cfg.coredump_fd = -1;

	if (qevt_server_cfg.system_event_fd >= 0) {
		unlink(QEVT_SYSTEM_EVENT_SOCKET_PATH);
		close(qevt_server_cfg.system_event_fd);
	}
	qevt_server_cfg.system_event_fd = -1;

	qevt_delete_event_id_mapping();
	qevt_cache_free();
	qevt_wpa_cleanup();
}

static void qevt_msgcfg_report(struct qevt_msg ** const phead, char *buffer,
			       uint32_t len, const char * const prefix)
{
	const struct qevt_msg *head;
	int output;

	head = phead ? *phead : NULL;

	if (prefix) {
		output = snprintf(buffer, len - 1, "%s", prefix);

		if (output < len) {
			len -= output;
			buffer += output;
		} else {
			len = 0;
		}
	}

	while (head && len) {
		struct qevt_config_flags flags = head->flags;

		output = snprintf(buffer, len - 1, " %s:%s%s%s%s%s%s%s", head->prefix,
				  flags.disabled ? "-" : "+",
				  flags.prfx ? "P" : "",
				  flags.interface ? "I" : "",
				  flags.timestamp ? "T" : "",
				  flags.message ? "M" : "",
				  flags.newline ? "N" : "",
				  flags.seqnum ? "S" : ""
				 );

		if (output < len) {
			len -= output;
			buffer += output;
		} else {
			len = 0;
		}
		head = head->next;
	}
}


/*
 * Get name of interface based on interface index
 */
static int qevt_index2name(const int skfd, const int ifindex, char *const name, const int name_len)
{
	struct ifreq if_req;

	memset(name, 0, name_len);

	/*
	 * Get interface name.
	 * For some reasons, ifi_index in the RTNETLINK message has bit 17 set.
	 */
	if_req.ifr_ifindex = ifindex & 0xFF;

	if (ioctl(skfd, SIOCGIFNAME, &if_req) < 0)
		return -1;

	strncpy(name, if_req.ifr_name, MIN(IFNAMSIZ, name_len));

	return 0;
}

/*
 * Get interface data from cache.  Create the cache entry if it doesn't already exist.
 */
static struct qevt_wifi_iface *qevt_get_interface_data(const int ifindex)
{
	struct qevt_wifi_iface *curr = qevt_server_cfg.if_cache;
	int skfd;

	/* Search for it in the cache first */
	while (curr) {
		if (curr->ifindex == ifindex) {
			return curr;
		}
		curr = curr->next;
	}

	skfd = iw_sockets_open();
	if (skfd < 0) {
		perror(QEVT_TXT_PREFIX"iw_sockets_open");
		return NULL;
	}

	/* Create new entry */
	curr = calloc(1, sizeof(struct qevt_wifi_iface));
	if (!curr) {
		fprintf(stderr, QEVT_TXT_PREFIX"malloc failed\n");
		iw_sockets_close(skfd);
		return NULL;
	}

	curr->ifindex = ifindex;

	/* Extract static data */
	if (qevt_index2name(skfd, ifindex, curr->ifname, sizeof(curr->ifname)) < 0) {
		perror(QEVT_TXT_PREFIX"qevt_index2name");
		iw_sockets_close(skfd);
		free(curr);
		return NULL;
	}

	curr->has_range = (iw_get_range_info(skfd, curr->ifname, &curr->range) >= 0);

	iw_sockets_close(skfd);

	/* Link it */
	curr->next = qevt_server_cfg.if_cache;
	qevt_server_cfg.if_cache = curr;

	return curr;
}

#define QEVT_TIMESTAMP_BUFFER 64
const char * qevt_timestamp()
{
	static char buffer[QEVT_TIMESTAMP_BUFFER];
	struct timeval time_now;

	uint32_t days;
	uint32_t hours;
	uint32_t minutes;
	uint32_t seconds;
	uint32_t milliseconds;

	gettimeofday(&time_now, NULL);

	seconds = time_now.tv_sec;
	minutes = seconds / 60;
	seconds -= minutes * 60;
	hours = minutes / 60;
	minutes -= hours * 60;
	days = hours / 24;
	hours -= days * 24;
	milliseconds = time_now.tv_usec / 1000;

	snprintf(buffer, QEVT_TIMESTAMP_BUFFER, "(%u/%02u:%02u:%02u.%03u) ",
		 days, hours, minutes, seconds, milliseconds);

	return buffer;
}

void qevt_timedwait(const uint32_t microsecond_delay)
{
	struct timespec delay_end;
	struct timeval time_now;
	uint32_t delay_secs;
	uint32_t delay_usecs;

	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

	gettimeofday(&time_now, NULL);

	delay_usecs = time_now.tv_usec + microsecond_delay;
	delay_secs = delay_usecs / MICROSECONDS_IN_ONE_SECOND;
	delay_usecs -= delay_secs * MICROSECONDS_IN_ONE_SECOND;
	delay_secs += time_now.tv_sec;

	delay_end.tv_sec = delay_secs;
	delay_end.tv_nsec = delay_usecs * NANOSECONDS_IN_ONE_MICROSECOND;

	pthread_mutex_lock(&mutex);
	pthread_cond_timedwait(&cond, &mutex, &delay_end);
	pthread_mutex_unlock(&mutex);
}

static void qevt_wpa_open(struct qevt_wpa_iface * const if_wpa)
{
	int size;
	char *file;

	size = strlen(if_wpa->ifname) + MAX(sizeof(QEVT_HOSTAPD), sizeof(QEVT_WPA));

	file = malloc(size);

	if (file == NULL) {
		fprintf(stderr, QEVT_TXT_PREFIX"malloc failed\n");
		return;
	}

	/* we don't know which between hostapd or wpa_supplicant are present - try both */
	if (if_wpa->wpa_event == NULL) {
		strcpy(file, QEVT_HOSTAPD);
		strcat(file, if_wpa->ifname);
		if_wpa->wpa_event = wpa_ctrl_open(file);
	}
	if (if_wpa->wpa_event != NULL  && if_wpa->wpa_control == NULL) {
		if_wpa->wpa_control = wpa_ctrl_open(file);
	}

	if (if_wpa->wpa_event == NULL) {
		strcpy(file, QEVT_WPA);
		strcat(file, if_wpa->ifname);
		if_wpa->wpa_event = wpa_ctrl_open(file);
	}

	if (if_wpa->wpa_event != NULL && if_wpa->wpa_control == NULL) {
		if_wpa->wpa_control = wpa_ctrl_open(file);
	}

	free(file);

	if (if_wpa->wpa_event) {
		if_wpa->fd = wpa_ctrl_get_fd(if_wpa->wpa_event);
		wpa_ctrl_attach(if_wpa->wpa_event);
	}
	else {
		if_wpa->fd  = 0;
	}
}

static void qevt_wpa_close(struct qevt_wpa_iface * const if_wpa)
{
	if (if_wpa->wpa_control) {
		wpa_ctrl_close(if_wpa->wpa_control);
	}
	if (if_wpa->wpa_event) {
		wpa_ctrl_detach(if_wpa->wpa_event);
		wpa_ctrl_close(if_wpa->wpa_event);
	}
	if_wpa->wpa_event = NULL;
	if_wpa->wpa_control = NULL;
	if_wpa->fd = 0;
	if_wpa->event_check = 0;
}

static void qevt_wpa_add_iface(struct qevt_wpa_iface **if_wpa_head, const char * const iface)
{
	struct qevt_wpa_iface *if_wpa = malloc(sizeof(*if_wpa));

	if (if_wpa == NULL) {
		fprintf(stderr, QEVT_TXT_PREFIX"malloc failed\n");
		return;
	}

	memset(if_wpa, 0, sizeof(*if_wpa));
	strncpy(if_wpa->ifname, iface, IFNAMSIZ);

	if_wpa->ifname[IFNAMSIZ] = 0;

	qevt_wpa_open(if_wpa);

	if_wpa->next = *if_wpa_head;
	*if_wpa_head = if_wpa;
}


#define WPA_CTRL_REQ_BUFFER 32
static int qevt_wpa_ping(const struct qevt_wpa_iface * const if_wpa)
{
	if (if_wpa->wpa_event && if_wpa->wpa_control) {
		char buffer[WPA_CTRL_REQ_BUFFER];
		size_t buffer_len = WPA_CTRL_REQ_BUFFER - 1;

		buffer[0] = 0;

		if (wpa_ctrl_request(if_wpa->wpa_control, "PING", sizeof("PING") - 1, buffer,
				     &buffer_len, NULL) == 0) {

			buffer[MIN(buffer_len, WPA_CTRL_REQ_BUFFER - 1)] = 0;

			if (!strcmp(buffer, "PONG\n")) {
				/* wpa/hostapd connection alive! */
				return 1;
			} else {
				fprintf(stderr, QEVT_TXT_PREFIX"unexpected wpa reply on '%s': %s\n",
					if_wpa->ifname, buffer);
			}
		}
	}
	return 0;
}

static int qevt_wpa_scan(struct qevt_server_config * const config)
{
	DIR *d;
	struct dirent *dir;
	struct qevt_wpa_iface *if_wpa_head;

	/* this gives us a list of all network interfaces */
	d = opendir("/sys/class/net");
	if (!d) {
		fprintf(stderr, QEVT_TXT_PREFIX"unable to open /sys/class/net\n");
		return -1;
	}

	/* we may be changing the wpa connection data, so lock it */
	pthread_mutex_lock(&config->wpa_mutex);

	if_wpa_head = config->if_wpa;

	/* start by assuming all are dead, and then confirm alive */
	while (if_wpa_head) {
		if_wpa_head->dead = 1;
		if_wpa_head = if_wpa_head->next;
	}

	/* check for all wifi interfaces */
	while ((dir = readdir(d)) != NULL)
	{
		char *wifi;

		wifi = strstr(dir->d_name, QEVT_WIFI_INTERFACE_NAME);
		if (wifi) {
			int found = 0;

			if_wpa_head = config->if_wpa;
			while (if_wpa_head) {
				if (!strcmp(wifi, if_wpa_head->ifname)) {
					found = 1;
					if_wpa_head->dead = 0;
					break;
				}
				if_wpa_head = if_wpa_head->next;
			}

			if (found) {
				/* in case wpa not connected yet */
				if (!if_wpa_head->wpa_event || !if_wpa_head->wpa_control) {
					qevt_wpa_open(if_wpa_head);
				}

				/* check the wpa socket is alive and well */
				if (!qevt_wpa_ping(if_wpa_head)) {
					qevt_wpa_close(if_wpa_head);
				}
			} else {
				qevt_wpa_add_iface(&config->if_wpa, wifi);
			}
		}
	}
	closedir(d);

	if_wpa_head = config->if_wpa;

	/* look for dead wifi interfaces (no longer listed in /sys/class/net) */
	while (if_wpa_head) {
		if (if_wpa_head->dead) {
			qevt_wpa_close(if_wpa_head);
		}
		if_wpa_head = if_wpa_head->next;
	}
	pthread_mutex_unlock(&config->wpa_mutex);

	return 0;
}

/*
 * This 'qevt_wpa_monitor_thread' is a secondary pthread. The purpose is to keep check on
 * the socket that connects to WPA supplicant or hostapd, because WPA/hostapd may be quit and
 * launched independently.
 * This thread is mostly always sleeping (interval 1 second). It then calls 'qevt_wpa_scan'.
 * This looks for changes in wifi network interfaces and whether there is a working WPA/hostapd
 * socket connection.
 * This thread is not involved in any messaging, it is only checking connectivity. The main thread
 * handles all the message forwarding, including messages from WPA/hostpapd.
 */
static void *qevt_wpa_monitor_thread(void *arg)
{
	struct qevt_server_config * const config =  (struct qevt_server_config * const) arg;
	int status = 0;

	while (config->running) {
		status = qevt_wpa_scan(config);

		if (status == 0) {
			/* wait 1 second before checking wpa connectivity again */
			qevt_timedwait(1 * MICROSECONDS_IN_ONE_SECOND);
		} else {
			config->running = 0;
			fprintf(stderr, QEVT_TXT_PREFIX"%s exit\n", __FUNCTION__);
		}
	}
	pthread_exit(NULL);
	return (void*)status;
}

static int qevt_server_wpa_init(void)
{
	if (pthread_create(&qevt_server_cfg.wpa_thread, NULL, qevt_wpa_monitor_thread,
			   &qevt_server_cfg)) {
		return -1;
	}
	return 0;
}

static void qevt_wpa_event(const struct qevt_wpa_iface * const wpa)
{
	static char buffer[IW_CUSTOM_MAX + 1];
	size_t reply_len;
	struct iw_event event;
	int wpa_prefix_len;
	char *wpa_event_msg;

	snprintf(buffer, IW_CUSTOM_MAX, QEVT_WPA_PREFIX":");
	wpa_prefix_len = strlen(buffer);

	reply_len = IW_CUSTOM_MAX - wpa_prefix_len;
	wpa_event_msg = buffer + wpa_prefix_len;
	wpa_ctrl_recv(wpa->wpa_event, wpa_event_msg, &reply_len);

	wpa_event_msg[MIN(IW_CUSTOM_MAX - wpa_prefix_len, reply_len)] = 0;

	/*
	 * look for wpa message level eg <3> and reformat message to give individual prefix types
	 * per message level eg WPACTRL3: etc
	 */
	if ((reply_len > 3) && (wpa_event_msg[0] == '<') && (wpa_event_msg[2] == '>')) {
		snprintf(buffer, IW_CUSTOM_MAX, QEVT_WPA_PREFIX"%c:", wpa_event_msg[1]);
		wpa_prefix_len = strlen(buffer);
		memmove(buffer + wpa_prefix_len, wpa_event_msg + 3, strlen(wpa_event_msg + 3) + 1);
	}
	event.cmd = IWEVCUSTOM;
	event.u.data.pointer = buffer;
	event.u.data.length = strlen(buffer);

	qevt_iwevent_action(&event, wpa->ifname);
}

static int qevt_netlink_open(void)
{
	qevt_server_cfg.netlink_socket = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);

	if (qevt_server_cfg.netlink_socket < 0) {
		qevt_server_cfg.netlink_socket = -1;
		perror(QEVT_TXT_PREFIX"cannot open netlink socket");
		return -1;
	}

	memset(&qevt_server_cfg.netlink_dst, 0, sizeof(qevt_server_cfg.netlink_dst));

	qevt_server_cfg.netlink_dst.nl_family = AF_NETLINK;
	qevt_server_cfg.netlink_dst.nl_groups = RTMGRP_LINK;

	if (bind(qevt_server_cfg.netlink_socket, (struct sockaddr *)&qevt_server_cfg.netlink_dst,
			sizeof(qevt_server_cfg.netlink_dst)) < 0) {
		close(qevt_server_cfg.netlink_socket);
		qevt_server_cfg.netlink_socket = -1;
		perror(QEVT_TXT_PREFIX"cannot bind netlink socket");
		return -1;
	}

	return 0;
}

/* Ignore the return value for this API */
static int qevt_core_dump_watch_init(void)
{
	int fd;
	int ret;

	fd = inotify_init();
	if (fd == -1) {
		return -1;
	}

	ret = inotify_add_watch(fd, DEFAULT_CORE_FILE_PATH, IN_CREATE);
	if (ret == -1) {
		perror("inotify_add_watch");
		close(fd);
		return -1;
	}

	qevt_server_cfg.coredump_fd = fd;

	return 0;
}

static int qevt_system_event_init(void)
{
	struct sockaddr_un srv_addr;
	int sock_fd;

	sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (sock_fd < 0) {
		perror(QEVT_TXT_PREFIX"cannot open unix domain socket");
		return -1;
	}

	memset(&srv_addr, 0, sizeof(struct sockaddr_un));

	srv_addr.sun_family = AF_UNIX;
	strncpy(srv_addr.sun_path, QEVT_SYSTEM_EVENT_SOCKET_PATH,
				sizeof(srv_addr.sun_path) - 1);

	/* Remove the file, if already exists */
	if (access(srv_addr.sun_path, F_OK) == 0) {
		unlink(srv_addr.sun_path);
	}

	if (bind(sock_fd, (struct sockaddr *) &srv_addr,
				sizeof(struct sockaddr_un)) < 0) {
		perror(QEVT_TXT_PREFIX"cannot bind unix domain socket");
		close(sock_fd);
		return -1;
	}

	return sock_fd;
}

static int qevt_server_socket_init(void)
{
	int server_socket;
	int optval = 1;
	struct sockaddr_in srv_addr;

	server_socket = socket(AF_INET, SOCK_STREAM, 0);

	if (server_socket < 0) {
		return -1;
	}

	if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
		close(server_socket);
		return -1;
	}

	memset(&srv_addr, 0, sizeof(srv_addr));
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	srv_addr.sin_port = htons(qevt_server_cfg.port);

	if (bind(server_socket, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0) {
		close(server_socket);
		return -1;
	}

	if (listen(server_socket, 1) < 0) {
		close(server_socket);
		return -1;
	}

	return server_socket;
}

static int qevt_server_init(void)
{
	int i;

	qevt_server_cfg.running = 1;

	for (i = 0; i < QEVT_MAX_CLIENT_CONNECTIONS; i++) {
		qevt_server_cfg.client[i].client_socket = -1;
	}

	if ((qevt_server_cfg.server_socket = qevt_server_socket_init()) < 0) {
		perror(QEVT_TXT_PREFIX"failed to initialise server socket");
		qevt_server_cfg.server_socket = -1;
		return -1;
	}

	if (qevt_netlink_open() < 0) {
		return -1;
	}

	if (qevt_server_wpa_init() < 0) {
		return -1;
	}

	(void) qevt_core_dump_watch_init();

	if ((qevt_server_cfg.system_event_fd = qevt_system_event_init()) < 0) {
		perror(QEVT_TXT_PREFIX"Failed to initialise system events socket");
		qevt_server_cfg.system_event_fd = -1;
		return -1;
	}

	return 0;
}

static void qevt_client_init(struct qevt_client * const client)
{
	/* initialise built-in pseduo types (do default init last) */
	qevt_msgcfg_add(&client->u.msg_config.msg, qevt_nondefined_prefix, &client->u.msg_config.msg_count);
	qevt_msgcfg_add(&client->u.msg_config.msg, qevt_default_prefix, &client->u.msg_config.msg_count);
	client->config_type = CONFIG_TYPE_MSG;
}

void qevt_send_to_client(const struct qevt_client * const client, const char *fmt, ...)
{
	va_list ap;
	static char loc_buf[QEVT_TX_BUF_SIZE];
	int sent_bytes = 0;
	int ret;

	if ((client == NULL) || (client->client_socket < 0)) {
		return;
	}

	va_start(ap, fmt);
	vsnprintf(loc_buf, sizeof(loc_buf), fmt, ap);
	va_end(ap);

	do {
		ret = send(client->client_socket, loc_buf + sent_bytes,
			   strlen(loc_buf) - sent_bytes, 0);
		if (ret >= 0)
			sent_bytes += ret;
		else if (errno == EINTR)
			continue;
		else
			break;
	} while (sent_bytes < strlen(loc_buf));

	if (ret < 0) {
		fprintf(stderr, QEVT_TXT_PREFIX"failure sending a message to client\n");
	}
}

struct qevt_client* qevt_accept_client_connection(void)
{
	int i;
	struct qevt_client temp_client = { .client_socket = -1, .config_type = CONFIG_TYPE_MSG,
					   .u.msg_config.msg = NULL, .u.msg_config.msg_count = 0};

	temp_client.client_socket = accept(qevt_server_cfg.server_socket, NULL, NULL);

	if (temp_client.client_socket < 0) {
		perror(QEVT_TXT_PREFIX"accept");
		return NULL;
	}
	qevt_server_cfg.num_client_connections++;

	for (i = 0; i < QEVT_MAX_CLIENT_CONNECTIONS; i++) {
		if (qevt_server_cfg.client[i].client_socket < 0) {
			qevt_server_cfg.client[i] = temp_client;

			return &qevt_server_cfg.client[i];
		}
	}
	qevt_send_to_client(&temp_client, QEVT_TXT_PREFIX"cannot accept new connection (max %d)\n",
			    QEVT_MAX_CLIENT_CONNECTIONS);

	qevt_client_close(&temp_client);
	return NULL;
}


/*
 * QEVT_CONFIG is an input command from the client, to configure the host event
 * message filtering and output format.
 * Host event messages have a prefix following by colon eg 'MessagePrefix:'
 *
 * example config: QEVT_CONFIG MessagePrefix:[+/-][P][I][M][N][T][S]
 *
 * the + or - immediately after the prefix select whether to enable or disable
 * if not specified, the state is unchanged
 *
 * the P, I, M, N, T, S are single character options. They can be specified in
 * any order. If present, the option is on; if not present, the option is off.
 *
 * P = prefix; the event message will indicate the prefix
 * I = interface; the event message will indicate the interface name
 * M = message; the event message will indicate the message (!)
 * N = newline; the event message will be terminated by newline
 * T = timestamp; the event message will have timestamp
 * S = sequence number; the event message will have a (global) sequence number
 *
 * A naked QEVT_CONFIG (with no parameters) will just return back the current
 * settings.
 */
static void qevt_client_config(struct qevt_client * const client, const char * const config)
{
	const char *prefix = NULL;
	char *params = NULL;
	char *item;
	struct qevt_msg *msg_cfg;

	if (config[sizeof(QEVT_CONFIG) - 1] == ' ') {
		prefix = qevt_prefix(config + sizeof(QEVT_CONFIG), &params);
	}

	while (prefix) {
		if (!params) {
			/* some problem */
			return;
		}
		/* params points to colon at the end of prefix */
		*params++ = 0;

		msg_cfg = qevt_msgcfg_find(&client->u.msg_config.msg, prefix);

		if (!msg_cfg) {
			msg_cfg = qevt_msgcfg_add(&client->u.msg_config.msg, prefix,
						  &client->u.msg_config.msg_count);
		}

		if (!msg_cfg) {
			/* some problem */
			return;
		}

		item = strstr(params, " ");
		if (item) {
			*item = 0;
		}

		if (params[0] == '+') {
			msg_cfg->flags.disabled = 0;
		} else if (params[0] == '-') {
			msg_cfg->flags.disabled = 1;
		}

		msg_cfg->flags.prfx = strstr(params, "P") ? 1 : 0;
		msg_cfg->flags.interface = strstr(params, "I") ? 1 : 0;
		msg_cfg->flags.message = strstr(params, "M") ?  1 : 0;
		msg_cfg->flags.newline = strstr(params, "N") ? 1: 0;
		msg_cfg->flags.timestamp = strstr(params, "T") ? 1: 0;
		msg_cfg->flags.seqnum = strstr(params, "S") ? 1: 0;

		if (item) {
			prefix = qevt_prefix(item + 1, &params);
		} else {
			prefix = NULL;
		}
	}
}

static void qevt_client_input(struct qevt_client * const client, char * const input,
			      const int len)
{
	char *command;

	/* QEVT_VERSION is a input request from the client, to request the server version */
	command = strstr(input, QEVT_VERSION);
	if (command && command[sizeof(QEVT_VERSION) - 1] <= ' ') {
		/* Client v1.00 and earlier do not send their version number,
		 * while clients starting from v1.10 send their version number
		 */
		if (command[sizeof(QEVT_VERSION) - 1] == ' ') {
			qevt_send_to_client(client, "%s\n", QEVT_VERSION" "QEVT_CONFIG_VERSION);
			return;
		}

		qevt_send_to_client(client, "%s\n", QEVT_VERSION" "QEVT_CONFIG_OLD_VERSION);
		return;
	}

	/* QEVT_CONFIG is an input command from the client, to configure the server */
	command = strstr(input, QEVT_CONFIG);
	if (command && command[sizeof(QEVT_CONFIG) - 1] <= ' ') {
		qevt_client_init(client);
		qevt_client_config(client, command);

		/* report back the total configuration as confirmation (reuse input buffer) */
		qevt_msgcfg_report(&client->u.msg_config.msg, input, len, QEVT_CONFIG);
		qevt_send_to_client(client, "%s\n", input);
		return;
	}

	command = strstr(input, QEVT_CONFIG_EVENT_ID);
	if (command && command[sizeof(QEVT_CONFIG_EVENT_ID) - 1] <= ' ') {
		client->config_type = CONFIG_TYPE_EVENT_ID;
		if (!qevt_client_config_event_id(client, command + sizeof(QEVT_CONFIG_EVENT_ID))) {
			qevt_send_to_client(client, "%s\n", input);
		} else {
			/* When validation on input string fails,
			 * we will send NULL to client indicating
			 * an invalid configuration and close connection */
			qevt_send_to_client(client, "%s\n", NULL);
		}

		return;
	}

	/* QEVT_CONFIG_RESET reverts back to default configuration (loses learned entries) */
	command = strstr(input, QEVT_CONFIG_RESET);
	if (command && command[sizeof(QEVT_CONFIG_RESET) - 1] <= ' ') {
		qevt_msgcfg_cleanup(client);
		qevt_client_init(client);

		/* report back the total configuration as confirmation (reuse input buffer) */
		qevt_msgcfg_report(&client->u.msg_config.msg, input, len, QEVT_CONFIG);
		qevt_send_to_client(client, "%s\n", input);
		return;
	}
	/* all other input is ignored */
}

static int qevt_client_connected(struct qevt_client * const client)
{
	static char buffer[QEVT_CONFIG_RX_BUF];
	int count = recv(client->client_socket, buffer, sizeof(buffer), MSG_DONTWAIT);

	if (count > 0) {
		buffer[MIN(sizeof(buffer) - 1, count)] = 0;
		qevt_client_input(client, buffer, sizeof(buffer));
	}
	return count;
}


static void qevt_send_event_to_client(struct qevt_client * const client, const char * const prefix, const char *message,
				       const char * const ifname)
{
	char seq_string[16];

	snprintf(seq_string, sizeof(seq_string), "<%u> ", qevt_seq_number++);

	struct qevt_msg *msg_cfg;
	struct qevt_config_flags flags;

	msg_cfg = qevt_msgcfg_find(&client->u.msg_config.msg, prefix);

	if (msg_cfg != NULL) {
		msg_cfg->flags.seen = 1;
		flags = msg_cfg->flags;
	} else {
		flags = qevt_default_flags_init;
	}

	if (!flags.disabled) {
		char *newline = strstr(message, "\n");
		const char * const timestamp = flags.timestamp ? qevt_timestamp() : "";

		do {
			if (newline) {
				*newline = 0;
			}
			qevt_send_to_client(client, "%s%s%s%s%s%s%s%s%c",
					flags.seqnum ? seq_string : "",
					flags.prfx ? prefix : "",
					flags.prfx ? ": " : "",
					flags.interface ? "[" : "",
					(flags.interface && ifname) ? ifname : "NULL",
					flags.interface ? "] " : "",
					timestamp,
					flags.message ? message : "",
					flags.newline ? '\n' : 0);

			if (newline) {
				message = newline + 1;
				newline = strstr(message, "\n");
			}
		} while (newline);
	}
}

static void qevt_iwevent_action(const struct iw_event * const event, const char * const ifname)
{
	static char custom[IW_CUSTOM_MAX + 1];
	const char *prefix = NULL;
	char *message = NULL;
	char *data = NULL;
	int chars_to_copy = 0;
	int i;

	switch (event->cmd) {
	case IWEVCUSTOM:
		if (event->u.data.pointer && event->u.data.length) {
			chars_to_copy = event->u.data.length;
			data = event->u.data.pointer;
		}
		break;
	case SIOCGIWSCAN:
		data = QEVT_COMMON_PREFIX"notify scan done";
		chars_to_copy = strlen(data);
		break;
	default:
		return;
	}

	if (data == NULL || chars_to_copy == 0) {
		return;
	}
	chars_to_copy = MIN(chars_to_copy, IW_CUSTOM_MAX);
	memcpy(custom, data, chars_to_copy);
	custom[chars_to_copy] = '\0';

	for (i = 0; i < QEVT_MAX_CLIENT_CONNECTIONS; i++) {
		struct qevt_client *client = &qevt_server_cfg.client[i];
		if (client->client_socket < 0) {
			continue;
		}

		if (client->config_type == CONFIG_TYPE_EVENT_ID) {
			qevt_send_event_id_to_client(client, custom, ifname);
		} else {
			prefix = qevt_prefix(custom, &message);
			/* only recognise prefix at the start of line */
			if (prefix && (prefix == custom)) {
				/* message points to the colon at the end of prefix */
				*message++ = 0;
				qevt_msgcfg_update_all_clients(prefix);
			} else {
				message = custom;
				prefix = qevt_nondefined_prefix;
			}

			if (prefix) {
				qevt_send_event_to_client(client, prefix, message, ifname);
			}
		}
	}
}

static void qevt_iwevent_stream_parse(const int ifindex, char *data, const int len)
{
	struct stream_descr stream;
	struct iw_event event;
	struct qevt_wifi_iface *wireless_data;

	/* Get data from cache */
	wireless_data = qevt_get_interface_data(ifindex);

	if (!wireless_data)
		return;

	iw_init_event_stream(&stream, data, len);

	while (iw_extract_event_stream(&stream, &event,
				       wireless_data->range.we_version_compiled) > 0) {
		qevt_iwevent_action(&event, wireless_data->ifname);
	}
}

static void qevt_recv_interface_status(struct ifinfomsg *ifim)
{
	char *message;
	struct iw_event event;
	struct qevt_wifi_iface *iface_data;

	/* Get data from cache */
	iface_data = qevt_get_interface_data(ifim->ifi_index);

	if (!iface_data)
		return;

	if (ifim->ifi_flags & (IFF_RUNNING | IFF_UP)) {
		message = QEVT_COMMON_PREFIX"Interface status change TRUE";
	} else {
		message = QEVT_COMMON_PREFIX"Interface status change FALSE";
	}

	event.cmd = IWEVCUSTOM;
	event.u.data.pointer = message;
	event.u.data.length = strlen(message);
	event.len = sizeof(event);
	qevt_iwevent_action(&event, iface_data->ifname);
}

/* Respond to a single RTM_NEWLINK event from the rtnetlink socket. */
static void qevt_rtnetlink_parse(const struct nlmsghdr * const nlh)
{
	struct ifinfomsg *ifim = (struct ifinfomsg *)NLMSG_DATA(nlh);
	struct rtattr *rta = IFLA_RTA(ifim);
	size_t rtasize = IFLA_PAYLOAD(nlh);

	/* Only keep add/change events */
	if (nlh->nlmsg_type != RTM_NEWLINK)
		return;

	while (RTA_OK(rta, rtasize)) {
		if (rta->rta_type == IFLA_WIRELESS) {
			qevt_iwevent_stream_parse(ifim->ifi_index,
						  RTA_DATA(rta), RTA_PAYLOAD(rta));
		}

		if ((rta->rta_type == IFA_LOCAL) || (rta->rta_type == IFA_ADDRESS)) {
			qevt_recv_interface_status(ifim);
		}

		rta = RTA_NEXT(rta, rtasize);
	}
}

static int qevt_netlink_read(void)
{
	int len;
	static char buf[QEVT_RX_BUF_SIZE];
	struct iovec iov = {buf, sizeof(buf)};
	struct msghdr msg;
	struct nlmsghdr *nlh;

	msg = (struct msghdr){NULL, 0, &iov, 1, NULL, 0, 0};

	do {
		len = recvmsg(qevt_server_cfg.netlink_socket, &msg, 0);
	} while (len < 0 && errno == EINTR);

	if (len <= 0) {
		perror(QEVT_TXT_PREFIX"error reading netlink");
		return -1;
	}

	for (nlh = (struct nlmsghdr *)buf; NLMSG_OK(nlh, len);
			nlh = NLMSG_NEXT(nlh, len)) {
		/* The end of multipart message. */
		if (nlh->nlmsg_type == NLMSG_DONE)
			break;

		if (nlh->nlmsg_type == RTM_NEWLINK)
			qevt_rtnetlink_parse(nlh);
	}

	return 0;
}

#define MAX_PROCESS_NAME	128
#define BUF_LEN			(10 * (sizeof(struct inotify_event) + NAME_MAX + 1))
static int qevt_read_core_dump_event(void)
{
	ssize_t ret;
	int coredump_id;
	int len;
	char buf[BUF_LEN];
	char *ptr;
	char message[IW_CUSTOM_MAX + 1];
	char process_name[MAX_PROCESS_NAME];
	struct inotify_event *core_ievt;
	struct iw_event event;

	ret = read(qevt_server_cfg.coredump_fd, buf, BUF_LEN);
	if (ret <= 0)
		return -1;

	memset(message, 0, sizeof(message));

	ptr = buf;
	while (ptr < buf + ret) {
		core_ievt = (struct inotify_event *) ptr;

		if (core_ievt->mask & IN_CREATE) {

			sscanf(core_ievt->name, "core_t%d_%s", &coredump_id, process_name);
			len = snprintf(message, sizeof(message), "System process coredumped %s %d",
								process_name, coredump_id);
			event.cmd = IWEVCUSTOM;
			event.u.data.pointer = message;
			event.u.data.length = len;
			event.len = sizeof(event);

			qevt_iwevent_action(&event, NULL);
		}

		ptr += sizeof(struct inotify_event) + core_ievt->len;
	}

	return 0;
}

static int qevt_read_system_event(void)
{
	static char buffer[QEVT_CONFIG_RX_BUF];
	struct iw_event event;
	size_t bytes;

	memset(buffer, 0, sizeof(buffer));
	bytes = recv(qevt_server_cfg.system_event_fd, buffer,
				sizeof(buffer), MSG_DONTWAIT);
	if (bytes <= 0) {
		if (bytes == 0)
			return -1;
		return -errno;
	}

	event.cmd = IWEVCUSTOM;
	event.u.data.pointer = buffer;
	event.u.data.length = bytes;
	event.len = sizeof(event);

	qevt_iwevent_action(&event, NULL);

	return 0;
}

static void qevt_wait_for_event(void)
{
	int ret;

	while (qevt_server_cfg.running) {
		struct qevt_wpa_iface *wpa;
		struct timeval tv;
		int wpa_event_check = 1;
		int i;
		int last_fd = 0;
		fd_set rfds;

		/* Re-generate rfds each time */
		FD_ZERO(&rfds);

		if (qevt_server_cfg.netlink_socket >= 0) {
			FD_SET(qevt_server_cfg.netlink_socket, &rfds);
			last_fd = qevt_server_cfg.netlink_socket;
		};

		if (qevt_server_cfg.server_socket < 0 &&
			qevt_server_cfg.num_client_connections < QEVT_MAX_CLIENT_CONNECTIONS) {

			qevt_server_cfg.server_socket = qevt_server_socket_init();

			if (qevt_server_cfg.server_socket < 0) {
				qevt_server_cfg.server_socket = -1;
			}
		}

		if (qevt_server_cfg.server_socket >= 0) {
			FD_SET(qevt_server_cfg.server_socket, &rfds);
			last_fd = MAX(last_fd, qevt_server_cfg.server_socket);
		}

		for (i = 0; i < QEVT_MAX_CLIENT_CONNECTIONS; i++) {
			int client_socket = qevt_server_cfg.client[i].client_socket;

			if (client_socket >= 0) {
				FD_SET(client_socket, &rfds);
				last_fd = MAX(last_fd, client_socket);
			}
		}

		if (pthread_mutex_trylock(&qevt_server_cfg.wpa_mutex) != 0) {
			/*
			 * The wpa_monitor thread is updating the internal wpa state
			 * (a wpa socket might be going down or coming up), so wait 100ms.
			 */
			qevt_timedwait(100 * MICROSECONDS_IN_ONE_MILLISECOND);

			if (pthread_mutex_trylock(&qevt_server_cfg.wpa_mutex) != 0) {
				/* wpa_monitor is momentarily stuck, so skip it */
				wpa_event_check = 0;
			}
		}
		wpa = qevt_server_cfg.if_wpa;
		while (wpa) {
			if (wpa->wpa_event && wpa->fd && wpa_event_check) {
				FD_SET(wpa->fd, &rfds);
				last_fd = MAX(last_fd, wpa->fd);
				/*
				 * in case wpa state changes (ie a new connection comes up later),
				 * it would not be registered here but the wpa->event_check would
				 * be 0 for that
				 */
				wpa->event_check = 1;
			} else {
				wpa->event_check = 0;
			}
			wpa = wpa->next;
		}
		if (wpa_event_check) {
			pthread_mutex_unlock(&qevt_server_cfg.wpa_mutex);
		}

		if (qevt_server_cfg.coredump_fd >= 0) {
			FD_SET(qevt_server_cfg.coredump_fd, &rfds);
			last_fd = MAX(last_fd, qevt_server_cfg.coredump_fd);
		};

		if (qevt_server_cfg.system_event_fd >= 0) {
			FD_SET(qevt_server_cfg.system_event_fd, &rfds);
			last_fd = MAX(last_fd, qevt_server_cfg.system_event_fd);
		};

		/* timeout after 1 second in case wpa monitor finds something new */
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		/* Wait until something happens or timeout */
		ret = select(last_fd + 1, &rfds, NULL, NULL, &tv);

		/* Check if there was an error */
		if (ret < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			fprintf(stderr, QEVT_TXT_PREFIX"unhandled signal - exiting\n");
			break;
		}

		/* Check if there was a timeout */
		if (ret == 0) {
			continue;
		}

		/* new client connection ? */
		if (qevt_server_cfg.server_socket >= 0 && FD_ISSET(qevt_server_cfg.server_socket,
									&rfds)) {
			struct qevt_client* client = qevt_accept_client_connection();

			if (client != NULL) {
				if (qevt_server_cfg.num_client_connections >=
							QEVT_MAX_CLIENT_CONNECTIONS) {
					close(qevt_server_cfg.server_socket);
					qevt_server_cfg.server_socket = -1;
				}
			}
		}

		for (i = 0; i < QEVT_MAX_CLIENT_CONNECTIONS; i++) {
			int client_socket = qevt_server_cfg.client[i].client_socket;

			if (client_socket >= 0 && FD_ISSET(client_socket, &rfds)) {
				struct qevt_client* client = &qevt_server_cfg.client[i];

				if (qevt_client_connected(client) == 0) {
					qevt_client_close(client);
				}
			}
		}

		/* Check for interface discovery events. */
		if (qevt_server_cfg.netlink_socket >= 0 && FD_ISSET(qevt_server_cfg.netlink_socket,
									&rfds)) {
			qevt_netlink_read();
		}

		wpa = qevt_server_cfg.if_wpa;
		while (wpa) {
			if (wpa->event_check && wpa->fd && FD_ISSET(wpa->fd, &rfds)) {
				qevt_wpa_event(wpa);
			}
			wpa = wpa->next;
		}

		/* Check for system events  */
		if (qevt_server_cfg.system_event_fd >= 0
				&& FD_ISSET(qevt_server_cfg.system_event_fd, &rfds)) {
			qevt_read_system_event();
		}

		/* Check for coredump event */
		if (qevt_server_cfg.coredump_fd >= 0
				&& FD_ISSET(qevt_server_cfg.coredump_fd, &rfds)) {
			qevt_read_core_dump_event();
		}
	}
}

int main(int argc, char *argv[])
{
	if (argc != 1 && argc != 2) {
		fprintf(stderr, "Usage: %s {server IP port}\n", argv[0]);
		return EXIT_FAILURE;
	}

	if (argc == 2) {
		qevt_server_cfg.port = atoi(argv[1]);
	} else {
		qevt_server_cfg.port = QEVT_DEFAULT_PORT;
	}

	if (qevt_populate_event_id_mapping() == EXIT_FAILURE) {
		fprintf(stderr, "Not able to parse config file");
		return EXIT_FAILURE;
	}

	if (qevt_server_init() >= 0) {
		/* qevt_wait_for_event does not return except for error */
		qevt_wait_for_event();
	}
	qevt_server_cfg.running = 0;
	qevt_cleanup();

	return EXIT_FAILURE;
}

