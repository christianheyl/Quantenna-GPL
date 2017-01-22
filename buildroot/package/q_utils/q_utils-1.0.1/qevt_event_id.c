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

#include "qevt_server.h"

extern struct qevt_server_config qevt_server_cfg;

/*
 * Search the msg for the list of strings in qevt_event_id_mapping struct
 * and returns corresponding node
 */
static struct qevt_event_id_mapping *qevt_find_event_id_mapping(char *msg)
{
	struct qevt_event_id_mapping *ptr = qevt_server_cfg.event_id_mapping;

	while (ptr != NULL) {
		if (strstr(msg, ptr->compare_string)) {
			break;
		}
		ptr = ptr->next;
	}

	return ptr;
}

/* Function looks for the given event id from qevt_event_id_mapping list */
static int qevt_is_event_id_supported(const int id)
{
	struct qevt_event_id_mapping *ptr = qevt_server_cfg.event_id_mapping;

	while (ptr != NULL) {
		if (id == ptr->id) {
			return 1;
		}
		ptr = ptr->next;
	}

	return 0;
}

static int qevt_read_event_id(char *str, uint32_t *event_id)
{
	char num_s[MAX_EVENT_ID_LEN + 1];
	int i = 0;

	while (str[i] != '\0' && isdigit(str[i])) {
		num_s[i] = str[i];
		i++;
		if (i > MAX_EVENT_ID_LEN)
			return -1;
	}

	num_s[i] = '\0';
	*event_id = (uint32_t) atoi(num_s);

	return i; /* Return no of characters in event_id */
}

static int qevt_parse_conf_file(char *str)
{
	char *data;
	uint16_t opt = 1;
	struct qevt_event_id_mapping *tmp = NULL;

	tmp = (struct qevt_event_id_mapping *) malloc(sizeof(struct qevt_event_id_mapping));
	if (!tmp) {
		perror("qevt_parse_conf_file: malloc failed");
		return EXIT_FAILURE;
	}

	data = strtok (str, ",");
	tmp->num_of_params = 0;

	while (data != NULL)
	{
		switch (opt++) {
		case 1:
			if (qevt_read_event_id(data, &tmp->id) <= 0)
				goto failure;
			break;
		case 2:
			if (strlen(data) >= EVENT_NAME_LEN)
				goto failure;

			strncpy(tmp->name, data, EVENT_NAME_LEN - 1);
			break;
		case 3:
			if (strlen(data) >= EVENT_SUB_STRING_LEN)
				goto failure;

			strncpy(tmp->compare_string, data, EVENT_SUB_STRING_LEN - 1);
			break;
		default:
			if (tmp->num_of_params >= MAX_PARAMS)
				goto failure;

			if (strlen(data) >= MAX_PARAM_NAME_LEN)
				goto failure;

			strncpy(tmp->param[tmp->num_of_params].name, data, MAX_PARAM_NAME_LEN - 1);
			/* Get pattern for the param */
			data = strtok (NULL, ",");
			if (data == NULL)
				goto failure;

			if (strlen(data) >= MAX_PARAM_PATTERN_LEN)
				goto failure;

			strncpy(tmp->param[tmp->num_of_params].pattern, data, MAX_PARAM_PATTERN_LEN - 1);
			tmp->num_of_params++;
			break;
		}
		data = strtok (NULL, ",");
	}

	tmp->next = qevt_server_cfg.event_id_mapping;
	qevt_server_cfg.event_id_mapping = tmp;

	return EXIT_SUCCESS;

failure:
	qevt_delete_event_id_mapping();
	free(tmp);
	return EXIT_FAILURE;
}

/* Function to populate the struct qevt_event_id_mapping from the config file */
int qevt_populate_event_id_mapping(void)
{
	FILE *file = fopen (QEVT_SERVER_EVT_CNF, "r");
	size_t len = 0;
	char *line = NULL;
	size_t read_l;
	int ret_val = EXIT_FAILURE;

	if (file != NULL) {
		while ((read_l = getline(&line, &len, file)) != -1) {
			if (line[read_l - 1] == '\n')
				line[read_l - 1] = '\0';

			ret_val = qevt_parse_conf_file(line);
			if (ret_val == EXIT_FAILURE)
				break;
		}

		if (line)
			free(line);

		fclose(file);
	} else {
		perror("qevt_populate_event_id_mapping:" QEVT_SERVER_EVT_CNF "could not be opened");
	}

	return ret_val;
}

void qevt_delete_event_id_mapping(void)
{
	struct qevt_event_id_mapping *ptr, *head;

	head = qevt_server_cfg.event_id_mapping;
	while (head != NULL) {
		ptr = head->next;
		free(head);
		head = ptr;
	}

	qevt_server_cfg.event_id_mapping = NULL;
}

/* Function to update the received event id and is_enabled flag to the qevt_event structure */
static void qevt_add_to_client_event_list(struct qevt_client *client, uint32_t id, uint8_t flag)
{
	struct qevt_event *node = (struct qevt_event *) malloc (sizeof(struct qevt_event));
	if (!node) {
		perror ("qevt_add_to_client_event_list: malloc failed");
		return;
	}

	node->id = id;
	node->is_enabled = !flag;
	node->next = NULL;

	node->next = client->u.event_id_config.event_list;
	client->u.event_id_config.event_list = node;
}

/*
 * Parse the recieved event_id(s), update them to the qevt_event structure
 *	Example input formats from qevt client:
 *	[2000:2005]:+4002:+
 *	2000:-3000:-4000:-
 *	2000:+3001:+4004:+
 *	2000:+2005:+[4000:4003]:+
 */
static int qevt_parse_event_id(struct qevt_client *client, char *event_id_list,
							   uint8_t event_id_flag)
{
	uint32_t start = 0;
	uint32_t end = 0;
	int i = 0;
	int len = 0;

	while (event_id_list[i] != '\0') {
		/* Eg: [3000:3005]:- */
		if (event_id_list[i] == '[') {
			i++;	/* Points to the id */

			len = qevt_read_event_id(&event_id_list[i], &start);
			if (len <= 0)
				goto failure;

			i += len; /* Points to colon */
			if (event_id_list[i++] != ':')
				goto failure;

			len = qevt_read_event_id(&event_id_list[i], &end);
			if (len <= 0)
				goto failure;

			/* Make sure the start value is not greater that the end value */
			if (start >= end)
				goto failure;

			i += len; /* Points to ']' */
			if (event_id_list[i++] != ']')
				goto failure;
		} else {
			/* Eg: 2000:-4003:- */
			len = qevt_read_event_id(&event_id_list[i], &start);
			if (len <= 0)
				goto failure;

			end = start;
			i += len;
		}

		/* Points to : in 2000:-, validate :, and sign must be opposite to default sign */
		if ((event_id_list[i] != ':') || (event_id_list[i + 1] != (event_id_flag ? '-' : '+'))) {
			goto failure;
		}
		i += 2; /* Move to the next event_id */

		while (start <= end) {
			if (qevt_is_event_id_supported(start)) {
				qevt_add_to_client_event_list(client, start, event_id_flag);
			}
			start++;
		}
	}

	return 0;

failure:
	qevt_event_id_cfg_cleanup(client);
	return 1;
}

static void qevt_remove_space(char *str)
{
	char *p1 = str, *p2 = str;

	do {
		while (isspace(*p2))
			p2++;
	} while ((*p1++ = *p2++));

	return;
}

/* Validate the client input string */
int qevt_client_config_event_id(struct qevt_client *client, char * evt_msg)
{
	char *default_msg = NULL;;
	int event_id_flag = ALLOW_ALL_EVENT;

	client->u.event_id_config.event_list = NULL;
	client->u.event_id_config.seq_num = 0;

	qevt_remove_space(evt_msg);

	default_msg = strstr(evt_msg, "default:");
	if (default_msg == evt_msg) {
		default_msg = (default_msg + (sizeof("default:") - 1));

		if (default_msg[0] == '+') {
			event_id_flag = ALLOW_ALL_EVENT;
		} else if (default_msg[0] == '-') {
			event_id_flag = SUPPRESS_ALL_EVENT;
		} else {
			return 1;
		}

		default_msg++;
	} else {
		default_msg = evt_msg;
	}

	client->u.event_id_config.allow_all_event = event_id_flag;
	return qevt_parse_event_id(client, default_msg, event_id_flag);
}

/*
 * Look for given event id in the qevt_event list
 * Return value at variable 'is_enabled' if event id found (or)
 * Return default value for the event id
 */
static int is_event_enabled(struct qevt_client *client, uint32_t event_id)
{
	struct qevt_event *ptr;

	ptr =  client->u.event_id_config.event_list;
	while (ptr != NULL) {
		if (event_id == ptr->id) {
			return ptr->is_enabled;
		}
		ptr = ptr->next;
	}

	return client->u.event_id_config.allow_all_event;
}

void qevt_event_id_cfg_cleanup(struct qevt_client * const client)
{
	struct qevt_event *phead = client->u.event_id_config.event_list;
	struct qevt_event *ptr;

	while (phead != NULL) {
		ptr = phead->next;
		free(phead);
		phead = ptr;
	}

	client->u.event_id_config.event_list = NULL;
}

static void qevt_send_json_string(struct qevt_client *client, struct qevt_event_id_mapping *event,
				   const char *msg, const char * const ifname)
{
	char *json_string;
	int len;
	char param_str[MAX_OPTIONAL_PARAM_LEN];
	char value[MAX_PARAM_VALUE_LEN];
	char buffer[MAX_BUF_SIZE];
	char *temp_param_str = param_str;
	char *temp_msg = NULL;
	int i = 0;

	memset(param_str, 0, MAX_OPTIONAL_PARAM_LEN);
	len = snprintf(temp_param_str, MAX_PARAM_LEN, "\"params\":[");
	temp_param_str += len;
	for (i = 0; i < event->num_of_params; i++) {
		sprintf(buffer, "%s %s", event->compare_string, event->param[i].pattern);
		temp_msg = strstr(msg, event->compare_string);
		if (temp_msg == NULL)
			return ;

		sscanf(temp_msg, buffer, value);
		len = snprintf(temp_param_str, MAX_PARAM_LEN, "{\"%s\":\"%s\"},",
							event->param[i].name, value);
		if (len < 0)
			return;

		temp_param_str += len;
	}
	snprintf(temp_param_str, MAX_PARAM_LEN, "]");

	json_string = (char *) malloc(MAX_JSON_STRING_LEN + 1);
	if (!json_string) {
		perror ("qevt_send_json_string: malloc failed");
		return;
	}

	/* JSON format
		{"event":{
			"id":event->id,
			"name":event->name,
			"interface":ifname,
			"seq":client->u.event_id_config.seq_num++,
			"params":[
				"param1":msg,
				"param2":msg,
				]
			},
		}
	 */

	len = snprintf(json_string, MAX_JSON_STRING_LEN,
		"{\"event\":{\"id\":%d,\"name\":%s,\"interface\":%s,\"seq\":%d,"
		"%s},}", event->id, event->name, ifname ? ifname : "NULL",
		client->u.event_id_config.seq_num++,
		(event->num_of_params > 0) ? param_str : "");

	if (len > MAX_JSON_STRING_LEN) {
		printf("The JSON string constructed is too long\n");
		len = 0;
	}

	if (len)
		qevt_send_to_client(client, "%s\n", json_string);

	free(json_string);
}

void qevt_send_event_id_to_client(struct qevt_client * const client,
				char *message, const char * const ifname)
{
	struct qevt_event_id_mapping *event;

	event = qevt_find_event_id_mapping(message);
	if (!event)
		return;

	if (!(is_event_enabled(client, event->id)))
		return;

	qevt_send_json_string(client, event, message, ifname);
}
