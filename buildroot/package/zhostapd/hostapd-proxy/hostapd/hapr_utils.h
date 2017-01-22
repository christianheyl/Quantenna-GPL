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

#ifndef _HAPR_UTILS_H_
#define _HAPR_UTILS_H_

#include "hapr_types.h"
#include <stdlib.h>

#define MADWIFI_CMD_BUF_SIZE 128

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

static inline void *hapr_realloc_array(void *ptr, size_t nmemb, size_t size)
{
	if (size && nmemb > (~(size_t) 0) / size)
		return NULL;
	return realloc(ptr, nmemb * size);
}

static inline int hapr_time_before(const struct timeval *a, const struct timeval *b)
{
	return (a->tv_sec < b->tv_sec) || ((a->tv_sec == b->tv_sec) && (a->tv_usec < b->tv_usec));
}

static inline void hapr_time_sub(struct timeval *a, struct timeval *b, struct timeval *res)
{
	res->tv_sec = a->tv_sec - b->tv_sec;
	res->tv_usec = a->tv_usec - b->tv_usec;

	if (res->tv_usec < 0) {
		res->tv_sec--;
		res->tv_usec += 1000000;
	}
}

int hapr_linux_set_iface_flags(int sock, const char *ifname, int dev_up);

int hapr_linux_br_get(char *brname, const char *ifname);

int hapr_linux_br_del_if(int sock, const char *brname, const char *ifname);

int hapr_linux_br_add_if(int sock, const char *brname, const char *ifname);

int hapr_linux_iface_up(int sock, const char *ifname);

int hapr_qdrv_vap_start(const uint8_t *bssid, const char *ifname);

int hapr_qdrv_vap_stop(const char *ifname);

int hapr_qdrv_write_control(const char *buf);

char *hapr_read_config_file(const char *path);

int hapr_write_config_file(const char *buff, const char *path, const char *tmp_path);

void hapr_signal_daemon_parent(int fd, int value);

#endif
