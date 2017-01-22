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
#include "hapr_eloop.h"
#include "hapr_server.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>

#define QLINK_CLIENT_CONNECT_TIMEOUT 30000

struct hapr_eloop *main_eloop = NULL;

static int my_daemon_parent(int fd)
{
	fd_set rfds;
	struct timeval tv_start, tv_cur, tv;
	uint32_t passed_ms = 0, remain_ms;
	int ret;
	char c = 0;

	/*
	 * wait for the child to write a byte to the pipe to indicate that client has connected
	 * or that an error occurred.
	 */

	gettimeofday(&tv_start, NULL);

	while (passed_ms <= QLINK_CLIENT_CONNECT_TIMEOUT) {
		remain_ms = QLINK_CLIENT_CONNECT_TIMEOUT - passed_ms;

		FD_ZERO(&rfds);
		FD_SET(fd, &rfds);
		tv.tv_sec = remain_ms / 1000;
		tv.tv_usec = (remain_ms % 1000) * 1000;

		do {
			ret = select(fd + 1, &rfds, NULL, NULL, &tv);
		} while ((ret < 0) && (errno == EINTR));

		if (ret < 0) {
			HAPR_LOG(ERROR, "cannot select daemon pipe: %s", strerror(errno));
			return 1;
		}

		if (FD_ISSET(fd, &rfds)) {
			while (read(fd, &c, sizeof(c)) == -1) {
				if (errno != EINTR) {
					HAPR_LOG(ERROR, "cannot read from daemon pipe: %s",
						strerror(errno));
					return 1;
				}
			}

			if (c == 0)
				HAPR_LOG(INFO, "client connected");
			else
				HAPR_LOG(ERROR, "error starting daemon");

			return ((c == 0) ? 0 : 1);
		}

		gettimeofday(&tv_cur, NULL);

		timersub(&tv_cur, &tv_start, &tv);

		passed_ms = (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
	}

	/*
	 * client didn't connect within the specified time interval, his
	 * problems, just go on.
	 */

	HAPR_LOG(INFO, "client connect timeout");

	return 0;
}

static int my_daemon(int nochdir, int noclose, int *parent_fd)
{
	int fds[2];
	int ret;
	pid_t pid;

        ret = pipe(fds);

        if (ret != 0) {
        	HAPR_LOG(ERROR, "cannot create daemon pipe: %s", strerror(errno));
        	return -1;
        }

	pid = fork();

	if (pid < 0) {
		HAPR_LOG(ERROR, "cannot fork: %s", strerror(errno));
		return -1;
	}

	if (pid != 0) {
		close(fds[1]);
		_exit(my_daemon_parent(fds[0]));
	}

	close(fds[0]);

	if (setsid() == -1) {
		hapr_signal_daemon_parent(fds[1], -1);
		return -1;
	}

	if (!nochdir)
		chdir("/");

	if (!noclose) {
		struct stat st;
		int fd;

		if (((fd = open("/dev/null", O_RDWR, 0)) != -1) && (fstat(fd, &st) == 0)) {
			if (S_ISCHR(st.st_mode) != 0) {
				dup2(fd, STDIN_FILENO);
				dup2(fd, STDOUT_FILENO);
				dup2(fd, STDERR_FILENO);
				if (fd > 2)
					close(fd);
			} else {
				close(fd);
				hapr_signal_daemon_parent(fds[1], -1);
				return -1;
			}
		} else {
			close(fd);
			hapr_signal_daemon_parent(fds[1], -1);
			return -1;
		}
	}

	*parent_fd = fds[1];

	return 0;
}

static char *rel2abs_path(const char *rel_path)
{
	char *buf = NULL, *cwd, *ret;
	size_t len = 128, cwd_len, rel_len, ret_len;
	int last_errno;

	if (!rel_path)
		return NULL;

	if (rel_path[0] == '/')
		return strdup(rel_path);

	for (;;) {
		buf = malloc(len);
		if (buf == NULL)
			return NULL;
		cwd = getcwd(buf, len);
		if (cwd == NULL) {
			last_errno = errno;
			free(buf);
			if (last_errno != ERANGE)
				return NULL;
			len *= 2;
			if (len > 2000)
				return NULL;
		} else {
			buf[len - 1] = '\0';
			break;
		}
	}

	cwd_len = strlen(cwd);
	rel_len = strlen(rel_path);
	ret_len = cwd_len + 1 + rel_len + 1;
	ret = malloc(ret_len);
	if (ret) {
		memcpy(ret, cwd, cwd_len);
		ret[cwd_len] = '/';
		memcpy(ret + cwd_len + 1, rel_path, rel_len);
		ret[ret_len - 1] = '\0';
	}
	free(buf);
	return ret;
}

static void handle_term(int sig, void *user_data)
{
	HAPR_LOG(DEBUG, "signal %d received - terminating", sig);

	hapr_eloop_terminate(main_eloop);
}

static void handle_reload(int sig, void *user_data)
{
	HAPR_LOG(DEBUG, "signal %d received - reloading configuration", sig);
}

static int global_init(void)
{
	HAPR_LOG(TRACE, "enter");

	main_eloop = hapr_eloop_create();

	if (main_eloop == NULL) {
		HAPR_LOG(TRACE, "exit");
		return -1;
	}

	HAPR_LOG(TRACE, "exit");

	return 0;
}

static void global_deinit(const char *pid_file)
{
	HAPR_LOG(TRACE, "enter: %s", pid_file);

	hapr_eloop_destroy(main_eloop);

	if (pid_file)
		unlink(pid_file);

	HAPR_LOG(TRACE, "exit");
}

static int global_daemonize(const char *pid_file, int *parent_fd)
{
	if (my_daemon(0, 0, parent_fd)) {
		perror("daemon");
		return -1;
	}

	if (pid_file) {
		FILE *f = fopen(pid_file, "w");
		if (f) {
			fprintf(f, "%u\n", getpid());
			fclose(f);
		}
	}

	return 0;
}

static int global_run(int daemonize, const char *pid_file)
{
	int parent_fd = -1;

	HAPR_LOG(TRACE, "enter: %d, %s", daemonize, pid_file);

	if (daemonize && global_daemonize(pid_file, &parent_fd)) {
		HAPR_LOG(TRACE, "exit");
		return -1;
	}

	hapr_eloop_register_signal(main_eloop, SIGINT, &handle_term, NULL);
	hapr_eloop_register_signal(main_eloop, SIGTERM, &handle_term, NULL);
	hapr_eloop_register_signal(main_eloop, SIGHUP, &handle_reload, NULL);

	hapr_server_run(parent_fd);

	hapr_eloop_run(main_eloop);

	HAPR_LOG(TRACE, "exit");

	return 0;
}

static void show_version(void)
{
	fprintf(stderr,
		"hostapd-proxy\n"
		"Copyright (c) 2008 - 2015 Quantenna Communications Inc\n");
}

static void usage(void)
{
	show_version();
	fprintf(stderr,
		"\n"
		"usage: hostapd [-hdBv] [-P <PID file>] <configuration file>\n"
		"\n"
		"options:\n"
		"   -h   show this usage\n"
		"   -d   show more debug messages\n"
		"   -B   run daemon in the background\n"
		"   -P   PID file\n"
		"   -I   interface to bind to\n"
		"   -v   show hostapd version\n");

	exit(1);
}

int main(int argc, char *argv[])
{
	int ret = 1;
	int c, daemonize = 0, debug = 0;
	char *pid_file = NULL;
	char *bind_iface = NULL;
	struct qlink_server *qs;

	for (;;) {
		c = getopt(argc, argv, "BdhP:I:v");
		if (c < 0)
			break;
		switch (c) {
		case 'h':
			usage();
			break;
		case 'd':
			debug++;
			break;
		case 'B':
			daemonize++;
			break;
		case 'P':
			free(pid_file);
			pid_file = rel2abs_path(optarg);
			break;
		case 'I':
			bind_iface = optarg;
			break;
		case 'v':
			show_version();
			exit(1);
			break;
		default:
			usage();
			break;
		}
	}

	hapr_log_init(daemonize);

	hapr_log_set_debug(debug);

	if (argc != (optind + 1))
		usage();

	if (global_init()) {
		goto out;
	}

	qs = hapr_server_init(argv[optind], bind_iface);

	if (qs == NULL) {
		global_deinit(pid_file);
		goto out;
	}

	if (global_run(daemonize, pid_file) == 0) {
		ret = 0;
	}

 	hapr_server_deinit();
	global_deinit(pid_file);

out:
	free(pid_file);
	hapr_log_cleanup();

	return ret;
}
