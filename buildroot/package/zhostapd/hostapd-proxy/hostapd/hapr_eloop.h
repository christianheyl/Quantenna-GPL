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

#ifndef _HAPR_ELOOP_H_
#define _HAPR_ELOOP_H_

#include "hapr_types.h"

struct hapr_eloop;

typedef void (*hapr_eloop_sock_handler)(int sock, void *user_data);

typedef void (*hapr_eloop_timeout_handler)(void *user_data);

typedef void (*hapr_eloop_signal_handler)(int sig, void *user_data);

struct hapr_eloop *hapr_eloop_create(void);

void hapr_eloop_destroy(struct hapr_eloop *eloop);

void hapr_eloop_run(struct hapr_eloop *eloop);

int hapr_eloop_register_read_sock(struct hapr_eloop *eloop, int sock,
	hapr_eloop_sock_handler handler, void *user_data);

void hapr_eloop_unregister_read_sock(struct hapr_eloop *eloop, int sock);

int hapr_eloop_register_signal(struct hapr_eloop *eloop, int sig,
	hapr_eloop_signal_handler handler, void *user_data);

/*
 * Below functions are thread-safe.
 */

void hapr_eloop_terminate(struct hapr_eloop *eloop);

int hapr_eloop_register_timeout(struct hapr_eloop *eloop,
	hapr_eloop_timeout_handler handler,
	void *user_data,
	uint32_t timeout_ms);

int hapr_eloop_is_timeout_registered(struct hapr_eloop *eloop, hapr_eloop_timeout_handler handler,
	void *user_data);

int hapr_eloop_cancel_timeout(struct hapr_eloop *eloop, hapr_eloop_timeout_handler handler,
	void *user_data);

#endif
