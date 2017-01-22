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

#ifndef _QEVT_EVENT_ID_H_
#define _QEVT_EVENT_ID_H_

#define MAX_EVENT_ID_LEN        4
#define ALLOW_ALL_EVENT		1
#define SUPPRESS_ALL_EVENT	0
#define EVENT_NAME_LEN		32
#define EVENT_SUB_STRING_LEN	32
#define MAX_PARAM_NAME_LEN	32
#define MAX_PARAM_PATTERN_LEN	64
#define MAX_PARAM_VALUE_LEN	64
#define MAX_BUF_SIZE		128
#define MAX_PARAM_LEN		128
#define MAX_OPTIONAL_PARAM_LEN	512
#define MAX_JSON_STRING_LEN	1024
#define QEVT_CONFIG_EVENT_ID	"QEVT_CONFIG_EVENT_ID"
#define QEVT_SERVER_EVT_CNF	"/etc/qevt_server_event.conf"
#define MAX_PARAMS		2

struct qevt_event {
	uint8_t			is_enabled;
	uint32_t		id;    /* 1000, etc */
	struct qevt_event	*next;
};

struct qevt_event_id_config {
	struct qevt_event	*event_list;
	uint32_t		seq_num;
	uint8_t			allow_all_event;
};

struct qevt_tuple {
	char name[MAX_PARAM_NAME_LEN];
	char pattern[MAX_PARAM_PATTERN_LEN];
};

struct qevt_event_id_mapping
{
	uint32_t			id;
	char				name[EVENT_NAME_LEN];
	char				compare_string[EVENT_SUB_STRING_LEN];
	uint8_t				num_of_params;
	struct qevt_tuple		param[MAX_PARAMS];
	struct qevt_event_id_mapping	*next;
};

int qevt_populate_event_id_mapping(void);
void qevt_delete_event_id_mapping(void);

#endif /* _QEVT_EVENT_ID_H_ */
