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

#include "qproc_mon.h"

int main(int argc, char *argv[])
{
	int ret;
	int mqid;
	key_t mq_key;
	size_t len;
	size_t buf_len;
	struct qproc_mon_mq_buffer mq_cmd;
	int fd;
	unsigned int type;
	char buffer[QPROC_MON_MAX_FIFO_MSG_LEN];

	if (argc != 3) {
		fprintf(stderr, "Usage: qproc_mon_send_event <start> <\"app_name app_arguments\">\n"
				"       qproc_mon_send_event <stop> <\"app_name\">\n"
				"NOTE: app_name should be with absolute path\n");
		return -1;
	}

	if (!strcmp(argv[1], "start")) {
		type = QPROC_MON_LAUNCH_START_MSG_TYPE;
	} else if (!strcmp(argv[1], "stop")) {
		type = QPROC_MON_LAUNCH_STOP_MSG_TYPE;
	} else {
		return -1;
	}

	len = strlen(argv[2]) + 1;
	if (len == 1 || len >= QPROC_MON_MAX_CMD_LEN)
		return -1;

	fd = open(QPROC_MON_FIFO, O_WRONLY | O_NONBLOCK);
	if (fd <= 0) {
		fprintf(stderr, "Failed to open fifo file: %s : %s : %s\n",
					argv[1], argv[2], strerror(errno));
	} else {
		/* FIFO Message format => "Type:Message" */
		buf_len = snprintf(buffer, QPROC_MON_MAX_FIFO_MSG_LEN, "%d:%s", type, argv[2]);
		if (buf_len > 0) {
			buf_len += 1;
			ret = write(fd, buffer, buf_len);
			close(fd);
			if (ret == buf_len) {
				return 0;
			}
		} else {
			close(fd);
		}
	}

	mq_key = ftok(QPROC_MON_CONFIG, QPROC_MON_PROJ_ID);
	if (mq_key == -1)
		return -1;

	mqid = msgget(mq_key, IPC_CREAT | 0666);
	if (mqid < 0) {
		fprintf(stderr, "Failed to get message queue: %s : %s : %s\n",
					argv[1], argv[2], strerror(errno));
		return -1;
	}

	mq_cmd.mtype = type;
	strncpy(mq_cmd.cmd_args, argv[2], sizeof(mq_cmd.cmd_args) - 1);
	ret = msgsnd(mqid, &mq_cmd, len, IPC_NOWAIT);
	if (ret < 0) {
		fprintf(stderr, "Failed to send launch event: %s : %s : %s\n",
					argv[1], argv[2], strerror(errno));
		return -1;
	}

	/*
	 * NB: Don't remove/close the mqid, since it will remove the queue.
	 * Message queue is expected be present until qproc_mon process
	 * reads the messages
	 */

	return 0;
}
