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

#include "hapr_eloop.h"
#include "hapr_list.h"
#include "hapr_log.h"
#include "hapr_utils.h"
#include <sys/time.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

struct hapr_eloop_sock
{
	int sock;
	void *user_data;
	hapr_eloop_sock_handler handler;
};

struct hapr_eloop_timeout
{
	struct hapr_list list;
	struct timeval time;
	void *user_data;
	hapr_eloop_timeout_handler handler;
};

struct hapr_eloop_signal
{
	int sig;
	void *user_data;
	hapr_eloop_signal_handler handler;
	int signaled;
};

struct hapr_eloop_sock_table
{
	int count;
	struct hapr_eloop_sock *table;
	int changed;
};

struct hapr_eloop
{
	int max_sock;

	struct hapr_eloop_sock_table readers;

	struct hapr_list timeouts;

	int signal_count;
	struct hapr_eloop_signal *signals;
	int signaled;

	int terminate;

	pthread_mutex_t m;
	int trigger_fds[2];
};

static void hapr_eloop_trigger(struct hapr_eloop *eloop)
{
	char c = '\0';
	int ret;

	do {
		ret = write(eloop->trigger_fds[1], &c, 1);
	} while ((ret < 0) && (errno == EINTR));

	if (ret != 1) {
		HAPR_LOG(ERROR, "trigger write error");
	}
}

static int hapr_eloop_process_trigger(struct hapr_eloop *eloop, fd_set *fds)
{
	int ret;
	char c;

	if (FD_ISSET(eloop->trigger_fds[0], fds)) {
		do {
			ret = read(eloop->trigger_fds[0], &c, 1);
		} while ((ret < 0) && (errno == EINTR));

		if (ret != 1) {
			HAPR_LOG(ERROR, "trigger read error");
		} else {
			return 1;
		}
	}

	return 0;
}

static void hapr_eloop_remove_timeout(struct hapr_eloop_timeout *timeout)
{
	hapr_list_remove(&timeout->list);
	free(timeout);
}

static void hapr_eloop_sock_table_destroy(struct hapr_eloop_sock_table *table)
{
	int i;

	if (!table)
		return;

	for (i = 0; i < table->count && table->table; i++) {
		HAPR_LOG(INFO, "remaining socket: sock=%d user_data=%p handler=%p",
			table->table[i].sock,
			table->table[i].user_data,
			table->table[i].handler);
	}

	free(table->table);
}

static struct hapr_eloop *signal_eloop = NULL;

static void hapr_eloop_handle_signal(int sig)
{
	int i;

	signal_eloop->signaled++;

	for (i = 0; i < signal_eloop->signal_count; i++) {
		if (signal_eloop->signals[i].sig == sig) {
			signal_eloop->signals[i].signaled++;
			break;
		}
	}
}

static void hapr_eloop_process_pending_signals(struct hapr_eloop *eloop)
{
	int i;

	if (eloop->signaled == 0)
		return;

	eloop->signaled = 0;

	for (i = 0; i < eloop->signal_count; i++) {
		if (eloop->signals[i].signaled) {
			eloop->signals[i].signaled = 0;
			eloop->signals[i].handler(eloop->signals[i].sig,
				eloop->signals[i].user_data);
		}
	}
}

static int hapr_eloop_sock_table_dispatch(struct hapr_eloop_sock_table *table,
	fd_set *fds)
{
	int i, num_handled = 0;

	if (table->table == NULL)
		return 0;

	table->changed = 0;

	for (i = 0; i < table->count; i++) {
		if (FD_ISSET(table->table[i].sock, fds)) {
			table->table[i].handler(table->table[i].sock, table->table[i].user_data);
			++num_handled;
			if (table->changed)
				break;
		}
	}

	return num_handled;
}

struct hapr_eloop *hapr_eloop_create(void)
{
	struct hapr_eloop *eloop;

	eloop = malloc(sizeof(*eloop));

	if (eloop == NULL) {
		HAPR_LOG(ERROR, "cannot allocate eloop, no mem");
		goto fail_alloc;
	}

	memset(eloop, 0, sizeof(*eloop));

	hapr_list_init(&eloop->timeouts);

	if (pthread_mutex_init(&eloop->m, NULL) != 0) {
		HAPR_LOG(ERROR, "cannot create mutex");
		goto fail_mtx;
	}

	if (pipe(eloop->trigger_fds) != 0) {
		HAPR_LOG(ERROR, "cannot create pipe");
		goto fail_pipe;
	}

	eloop->max_sock = eloop->trigger_fds[0];

	HAPR_LOG(TRACE, "new eloop %p", eloop);

	return eloop;

fail_pipe:
	pthread_mutex_destroy(&eloop->m);
fail_mtx:
	free(eloop);
fail_alloc:

	return NULL;
}

void hapr_eloop_destroy(struct hapr_eloop *eloop)
{
	struct hapr_eloop_timeout *timeout, *prev;
	struct timeval now;

	gettimeofday(&now, NULL);

	hapr_list_for_each_safe(struct hapr_eloop_timeout, timeout, prev, &eloop->timeouts,
		list) {
		int sec, usec;

		sec = timeout->time.tv_sec - now.tv_sec;
		usec = timeout->time.tv_usec - now.tv_usec;

		if (timeout->time.tv_usec < now.tv_usec) {
			sec--;
			usec += 1000000;
		}

		HAPR_LOG(INFO, "remaining timeout: %d.%06d user_data=%p handler=%p",
			sec, usec, timeout->user_data, timeout->handler);

		hapr_eloop_remove_timeout(timeout);
	}

	hapr_eloop_sock_table_destroy(&eloop->readers);

	close(eloop->trigger_fds[0]);
	close(eloop->trigger_fds[1]);
	pthread_mutex_destroy(&eloop->m);
	free(eloop->signals);

	HAPR_LOG(TRACE, "destroyed eloop %p", eloop);

	free(eloop);
}

void hapr_eloop_run(struct hapr_eloop *eloop)
{
	fd_set rfds;
	int res, i;
	struct timeval tv, now;

	while (!eloop->terminate) {
		struct hapr_eloop_timeout *timeout;
		hapr_eloop_timeout_handler handler = NULL;
		void *user_data = NULL;
		int num_handled = 0;

		pthread_mutex_lock(&eloop->m);

		timeout = hapr_list_first(struct hapr_eloop_timeout, &eloop->timeouts, list);

		if (timeout) {
			gettimeofday(&now, NULL);

			if (hapr_time_before(&now, &timeout->time))
				hapr_time_sub(&timeout->time, &now, &tv);
			else
				tv.tv_sec = tv.tv_usec = 0;
		}

		pthread_mutex_unlock(&eloop->m);

		FD_ZERO(&rfds);

		FD_SET(eloop->trigger_fds[0], &rfds);

		for (i = 0; i < eloop->readers.count; i++)
			FD_SET(eloop->readers.table[i].sock, &rfds);

		res = select(eloop->max_sock + 1, &rfds, NULL, NULL,
			(timeout ? &tv : NULL));

		if ((res < 0) && (errno != EINTR) && (errno != 0)) {
			HAPR_LOG(ERROR, "select: %s", strerror(errno));
			break;
		}

		if (res > 0)
			num_handled += hapr_eloop_process_trigger(eloop, &rfds);

		hapr_eloop_process_pending_signals(eloop);

		pthread_mutex_lock(&eloop->m);

		timeout = hapr_list_first(struct hapr_eloop_timeout, &eloop->timeouts, list);

		if (timeout) {
			gettimeofday(&now, NULL);

			if (!hapr_time_before(&now, &timeout->time)) {
				handler = timeout->handler;
				user_data = timeout->user_data;
				hapr_eloop_remove_timeout(timeout);
			}
		}

		pthread_mutex_unlock(&eloop->m);

		if (handler) {
			handler(user_data);
			++num_handled;
		}

		if (res > 0)
			num_handled += hapr_eloop_sock_table_dispatch(&eloop->readers, &rfds);

		if (num_handled == 0)
			HAPR_LOG(DEBUG, "eloop %p, handled count = 0", eloop);
	}
}

void hapr_eloop_terminate(struct hapr_eloop *eloop)
{
	eloop->terminate = 1;

	hapr_eloop_trigger(eloop);
}

int hapr_eloop_register_read_sock(struct hapr_eloop *eloop, int sock,
	hapr_eloop_sock_handler handler, void *user_data)
{
	struct hapr_eloop_sock *tmp;
	int new_max_sock;

	if (sock > eloop->max_sock)
		new_max_sock = sock;
	else
		new_max_sock = eloop->max_sock;

	tmp = hapr_realloc_array(eloop->readers.table, eloop->readers.count + 1,
		sizeof(struct hapr_eloop_sock));

	if (tmp == NULL) {
		HAPR_LOG(ERROR, "cannot allocate eloop_sock, no mem");
		return -1;
	}

	tmp[eloop->readers.count].sock = sock;
	tmp[eloop->readers.count].user_data = user_data;
	tmp[eloop->readers.count].handler = handler;
	eloop->readers.count++;
	eloop->readers.table = tmp;
	eloop->max_sock = new_max_sock;
	eloop->readers.changed = 1;

	return 0;
}

void hapr_eloop_unregister_read_sock(struct hapr_eloop *eloop, int sock)
{
	int i;

	if ((eloop->readers.table == NULL) || (eloop->readers.count == 0)) {
		return;
	}

	for (i = 0; i < eloop->readers.count; i++) {
		if (eloop->readers.table[i].sock == sock)
			break;
	}

	if (i == eloop->readers.count) {
		return;
	}

	if (i != (eloop->readers.count - 1)) {
		memmove(&eloop->readers.table[i], &eloop->readers.table[i + 1],
			(eloop->readers.count - i - 1) *
			sizeof(struct hapr_eloop_sock));
	}

	eloop->readers.count--;
	eloop->readers.changed = 1;
}

int hapr_eloop_register_timeout(struct hapr_eloop *eloop,
	hapr_eloop_timeout_handler handler,
	void *user_data,
	uint32_t timeout_ms)
{
	struct hapr_eloop_timeout *timeout, *tmp;
	time_t now_sec;
	uint32_t secs = timeout_ms / 1000;
	uint32_t usecs = (timeout_ms % 1000) * 1000;

	timeout = malloc(sizeof(*timeout));

	if (timeout == NULL) {
		HAPR_LOG(ERROR, "cannot allocate eloop_timeout, no mem");
		return -1;
	}

	memset(timeout, 0, sizeof(*timeout));

	gettimeofday(&timeout->time, NULL);

	now_sec = timeout->time.tv_sec;
	timeout->time.tv_sec += secs;

	if (timeout->time.tv_sec < now_sec) {
		/*
		 * Integer overflow - assume long enough timeout to be assumed
		 * to be infinite, i.e., the timeout would never happen.
		 */
		HAPR_LOG(INFO, "too long timeout (secs = %u) to ever happen - ignore it", secs);
		free(timeout);
		return 0;
	}

	timeout->time.tv_usec += usecs;

	while (timeout->time.tv_usec >= 1000000) {
		timeout->time.tv_sec++;
		timeout->time.tv_usec -= 1000000;
	}

	timeout->user_data = user_data;
	timeout->handler = handler;

	pthread_mutex_lock(&eloop->m);

	hapr_list_for_each(struct hapr_eloop_timeout, tmp, &eloop->timeouts, list) {
		if (hapr_time_before(&timeout->time, &tmp->time)) {
			hapr_list_add(tmp->list.prev, &timeout->list);

			pthread_mutex_unlock(&eloop->m);

			hapr_eloop_trigger(eloop);

			return 0;
		}
	}

	hapr_list_add_tail(&eloop->timeouts, &timeout->list);

	pthread_mutex_unlock(&eloop->m);

	hapr_eloop_trigger(eloop);

	return 0;
}

int hapr_eloop_is_timeout_registered(struct hapr_eloop *eloop, hapr_eloop_timeout_handler handler,
	void *user_data)
{
	struct hapr_eloop_timeout *tmp;

	pthread_mutex_lock(&eloop->m);

	hapr_list_for_each(struct hapr_eloop_timeout, tmp, &eloop->timeouts, list) {
		if ((tmp->handler == handler) && (tmp->user_data == user_data)) {
			pthread_mutex_unlock(&eloop->m);
			return 1;
		}
	}

	pthread_mutex_unlock(&eloop->m);

	return 0;
}

int hapr_eloop_cancel_timeout(struct hapr_eloop *eloop, hapr_eloop_timeout_handler handler,
	void *user_data)
{
	struct hapr_eloop_timeout *timeout, *prev;
	int removed = 0;

	pthread_mutex_lock(&eloop->m);

	hapr_list_for_each_safe(struct hapr_eloop_timeout, timeout, prev, &eloop->timeouts, list) {
		if ((timeout->handler == handler) && (timeout->user_data == user_data)) {
			hapr_eloop_remove_timeout(timeout);
			removed++;
		}
	}

	pthread_mutex_unlock(&eloop->m);

	return removed;
}

int hapr_eloop_register_signal(struct hapr_eloop *eloop, int sig,
	hapr_eloop_signal_handler handler, void *user_data)
{
	struct hapr_eloop_signal *tmp;

	if (signal_eloop && (signal_eloop != eloop)) {
		HAPR_LOG(ERROR, "signal processing eloop cannot be changed");
		return -1;
	}

	tmp = hapr_realloc_array(eloop->signals, eloop->signal_count + 1,
		sizeof(struct hapr_eloop_signal));

	if (tmp == NULL) {
		HAPR_LOG(ERROR, "cannot allocate eloop_signal, no mem");
		return -1;
	}

	signal_eloop = eloop;

	tmp[eloop->signal_count].sig = sig;
	tmp[eloop->signal_count].user_data = user_data;
	tmp[eloop->signal_count].handler = handler;
	tmp[eloop->signal_count].signaled = 0;
	eloop->signal_count++;
	eloop->signals = tmp;
	signal(sig, hapr_eloop_handle_signal);

	return 0;
}
