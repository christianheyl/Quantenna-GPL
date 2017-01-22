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

#include <sys/un.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>

#include "qevt_server.h"

int main(int argc, char *argv[])
{
	struct sockaddr_un serv_addr;
	int sock_fd;
	size_t bytes;
	size_t len;

	if (argc != 2) {
		printf("Usage: qevt_send_event <message>\n");
		return -1;
	}

	if ((sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
		perror("Failed to create unix domain socket");
		return -1;
	}

	memset(&serv_addr, 0, sizeof(struct sockaddr_un));
	serv_addr.sun_family = AF_UNIX;
	strncpy(serv_addr.sun_path, QEVT_SYSTEM_EVENT_SOCKET_PATH,
				sizeof(serv_addr.sun_path) - 1);

	len = strlen(argv[1]);
	bytes =	sendto(sock_fd, argv[1], len, 0,
				(struct sockaddr *) &serv_addr,
				sizeof(struct sockaddr_un));
	if (bytes != len) {
		perror("Failed to send event\n");
		close(sock_fd);
		return -1;
	}

	close(sock_fd);

	return 0;
}

