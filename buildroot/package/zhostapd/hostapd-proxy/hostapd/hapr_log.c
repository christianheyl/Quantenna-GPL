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

#include "hapr_log.h"
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <syslog.h>

static int log_syslog = 0;
static pthread_mutex_t log_m = PTHREAD_MUTEX_INITIALIZER;
static hapr_log_level log_level = HAPR_LOG_ERROR;

static const char *level_str[] = {
	"INFO",
	"ERROR",
	"WARN",
	"DEBUG",
	"TRACE",
};

static int level_priority[] = {
	LOG_INFO,
	LOG_ERR,
	LOG_WARNING,
	LOG_DEBUG,
	LOG_DEBUG,
};

void hapr_log_init(int use_syslog)
{
	log_syslog = use_syslog;
	if (log_syslog) {
		openlog("hapr", LOG_PID, LOG_DAEMON);
	}
}

void hapr_log_cleanup(void)
{
	if (log_syslog) {
		closelog();
	}
}

void hapr_log_set_debug(int debug)
{
	if (debug <= 0)
		log_level = HAPR_LOG_ERROR;
	else if (debug >= (HAPR_LOG_TRACE - 1))
		log_level = HAPR_LOG_TRACE;
	else
		log_level = (hapr_log_level)(debug + 1);
}

void hapr_log(hapr_log_level level, const char *fmt, ...)
{
	va_list args;

	if (log_syslog) {
		va_start(args, fmt);
		vsyslog(level_priority[level], fmt, args);
		va_end(args);
	} else {
		pthread_mutex_lock(&log_m);
		printf("%-5s ", level_str[level]);
		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
		printf("\n");
		pthread_mutex_unlock(&log_m);
	}
}

int hapr_log_is_enabled(hapr_log_level level)
{
	return (level <= log_level);
}
