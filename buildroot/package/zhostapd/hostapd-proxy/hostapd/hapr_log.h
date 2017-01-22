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

#ifndef _HAPR_LOG_H_
#define _HAPR_LOG_H_

#include "hapr_types.h"
#include "hapr_utils.h"

typedef enum
{
	HAPR_LOG_INFO = 0,
	HAPR_LOG_ERROR,
	HAPR_LOG_WARN,
	HAPR_LOG_DEBUG,
	HAPR_LOG_TRACE,
} hapr_log_level;

void hapr_log_init(int use_syslog);

void hapr_log_cleanup(void);

void hapr_log_set_debug(int debug);

void hapr_log(hapr_log_level level, const char *fmt, ...);

int hapr_log_is_enabled(hapr_log_level level);

#define HAPR_LOG(level, format, ...) \
	do { \
		if (hapr_log_is_enabled(HAPR_LOG_##level)) { \
			hapr_log(HAPR_LOG_##level, "%s:%d - " format, __FUNCTION__, __LINE__,##__VA_ARGS__); \
		} \
	} while(0)

#endif
