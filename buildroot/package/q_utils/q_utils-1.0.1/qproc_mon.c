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

static struct qproc_mon_state qproc_mon = {.log_fp = NULL, .conn_sock = -1,
						.launch_fifo_fd = -1, .launch_mqid = -1,
						.active_list = NULL, .pending_list = NULL};

static void _qproc_mon_log_message(int error, const char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);

#ifdef CONFIG_QPROC_MONITOR_LOG
	if (qproc_mon.log_fp) {
		vfprintf(qproc_mon.log_fp, fmt, arg);
		fflush(qproc_mon.log_fp);
	}
#endif

	if (error)
		vfprintf(stderr, fmt, arg);

	va_end(arg);
}

void qproc_mon_sigchild_handler(int sig)
{
	pid_t pid;
	int status;

	while (1) {
		pid = waitpid(-1, &status, WNOHANG);
		if (pid == -1) {
			if (errno == EINTR) {
				continue;
			}
			break;
		}
		else if (pid == 0) {
			break;
		}
	}
}

static int qproc_mon_set_event_listener(int nl_sock, bool enable)
{
	struct __attribute__ ((aligned(NLMSG_ALIGNTO))) {
		struct nlmsghdr nl_hdr;
		struct __attribute__ ((__packed__)) {
			struct cn_msg cn_msg;
			enum proc_cn_mcast_op cn_mcast;
		};
	} nlcn_msg;

	memset(&nlcn_msg, 0, sizeof(nlcn_msg));
	nlcn_msg.nl_hdr.nlmsg_len = sizeof(nlcn_msg);
	nlcn_msg.nl_hdr.nlmsg_pid = getpid();
	nlcn_msg.nl_hdr.nlmsg_type = NLMSG_DONE;

	nlcn_msg.cn_msg.id.idx = CN_IDX_PROC;
	nlcn_msg.cn_msg.id.val = CN_VAL_PROC;
	nlcn_msg.cn_msg.len = sizeof(enum proc_cn_mcast_op);

	nlcn_msg.cn_mcast = enable ? PROC_CN_MCAST_LISTEN : PROC_CN_MCAST_IGNORE;

	if (send(nl_sock, &nlcn_msg, sizeof(nlcn_msg), 0) == -1) {
		log_message(1, "Failed to send netlink connector socket message");
		return -1;
	}

	return 0;
}

static int qproc_mon_process_exists(pid_t pid)
{
	char buf[QPROC_MON_MAX_CMD_LEN];
	struct stat sts;

	snprintf(buf, sizeof(buf), "/proc/%d", pid);
	if (stat(buf, &sts) == 0 && S_ISDIR(sts.st_mode)) {
		return 0;
	}

	return -1;
}

static void qproc_mon_delete_proc_nodes(struct qproc_mon_node **phead)
{
	struct qproc_mon_node *tmp;

	while (*phead != NULL) {
		tmp = *phead;
		*phead = (*phead)->next;
		free(tmp);
	}
}

static int qproc_mon_create_pipe(int *pfd)
{
	int flags;

	if (pipe(pfd) == -1) {
		log_message(1, "Failed to create pipe: %s", strerror(errno));
		return -1;
	}

	flags = fcntl(pfd[1], F_GETFD, 0);
	if (flags < 0) {
		log_message(1, "Failed to get flags using fcntl: %s", strerror(errno));
		close(pfd[0]);
		close(pfd[1]);
		return -1;
	}

	if (fcntl(pfd[1], F_SETFD, flags | FD_CLOEXEC) < 0) {
		log_message(1, "Failed to set flags using fcntl: %s", strerror(errno));
		close(pfd[0]);
		close(pfd[1]);
		return -1;
	}

	return 0;
}

static pid_t qproc_mon_launch_process(char *cmd_args)
{
	int i;
	char *arg;
	char *params[QPROC_MON_MAX_ARGS];
	pid_t pid;
	char cmd_line[QPROC_MON_MAX_CMD_LEN];
	struct stat sts;
	int pfd[2];
	int ret;
	int child_ret = 0;

	strncpy(cmd_line, cmd_args, sizeof(cmd_line) - 1);

	arg = strtok(cmd_line, " ");
	for (i = 0; arg != NULL; i++) {
		if (i >= QPROC_MON_MAX_ARGS - 1) {
			log_message(1, "Number of arguments exceeded");
			return -1;
		}
		params[i] = arg;
		arg = strtok(NULL, " ");
	}

	if (i == 0)
		return -1;

	params[i] = NULL;

	if (!(stat(params[0], &sts) == 0 && sts.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) {
		log_message(1, "Failed to get information of file in stat: %s", strerror(errno));
		return -1;
	}

	if (qproc_mon_create_pipe(pfd) < 0)
		return -1;

	while ((pid = vfork()) == -1)
		usleep(500);

	if (pid == 0) {
		close(qproc_mon.launch_fifo_fd);
		close(pfd[0]);
		ret = execvp(params[0], params);
		write(pfd[1], &ret, sizeof(ret));
		close(pfd[1]);
		_exit(0);
	}

	close(pfd[1]);
	read(pfd[0], &child_ret, sizeof(child_ret));
	close(pfd[0]);
	if (child_ret < 0) {
		log_message(1, "Exec failed");
		return -1;
	}

	return pid;
}

static int qproc_mon_update_persistent_file(void)
{
	struct qproc_mon_node *tmp = qproc_mon.active_list;
	FILE *write_fp;

	write_fp = fopen(QPROC_MON_STATE_FILE_TEMP, "w");
	if (write_fp == NULL) {
		log_message(1, "Failed to open temp persistent file: %s : %s",
					QPROC_MON_STATE_FILE_TEMP, strerror(errno));
		return -1;
	}

	while (tmp != NULL) {
		fprintf(write_fp, "%d:%d:%s\n", tmp->pid,
						tmp->child_pid,
						tmp->cmd_args);
		tmp = tmp->next;
	}

	if (write_fp)
		fclose(write_fp);

	remove(QPROC_MON_STATE_FILE);
	rename(QPROC_MON_STATE_FILE_TEMP, QPROC_MON_STATE_FILE);

	return 0;
}

static int qproc_mon_add_persistent_file(struct qproc_mon_node *proc_node)
{
	FILE *fp;

	fp = fopen(QPROC_MON_STATE_FILE, "a");
	if (fp == NULL) {
		log_message(1, "Failed to open persistent file: %s : %s",
					QPROC_MON_STATE_FILE, strerror(errno));
		return -1;
	}

	fprintf(fp, "%d:%d:%s\n", proc_node->pid, proc_node->child_pid, proc_node->cmd_args);

	fclose(fp);
	return 0;
}

static void qproc_mon_send_event(char *cmd_args, int exit_code)
{
	char process_exit_cmd[QEVT_MAX_MSG_LEN];
	char *process_name;
	char cmd_line[QPROC_MON_MAX_CMD_LEN];

	strncpy(cmd_line, cmd_args, sizeof(cmd_line) - 1);

	process_name = strtok(cmd_line, " ");
	if (process_name == NULL)
		return;

	snprintf(process_exit_cmd, QEVT_MAX_MSG_LEN, QEVT_SEND_EVENT_CMD " \""QEVT_COMMON_PREFIX
					"System process exited %s %d\"", process_name, exit_code);
	if (system(process_exit_cmd) != 0) {
		log_message(1, "Failed to send process exit event message qevt_server");
	}
}

static int qproc_mon_restart_process(struct qproc_mon_node *qprocess, int exit_code)
{
	int ret = 0;
	pid_t pid;

	if (qprocess->child_pid != 0) {
		log_message(0, "Got exit event %d but currently process running"
				" with pid %d, updated new pid",
						qprocess->pid,
						qprocess->child_pid);
		qprocess->pid = qprocess->child_pid;
		qprocess->child_pid = 0;
	} else {
		qproc_mon_send_event(qprocess->cmd_args, exit_code);
		pid = qproc_mon_launch_process(qprocess->cmd_args);
		if (pid > 0) {
			qprocess->pid = pid;
		} else {
			log_message(1, "Failed to restart process: %s", qprocess->cmd_args);
		}
	}

	ret = qproc_mon_update_persistent_file();

	return ret;
}

static int qproc_mon_handle_process_events()
{
	int ret;
	struct qproc_mon_node *tmp = qproc_mon.active_list;
	struct __attribute__ ((aligned(NLMSG_ALIGNTO))) {
		struct nlmsghdr nl_hdr;
		struct __attribute__ ((__packed__)) {
			struct cn_msg cn_msg;
			struct proc_event proc_ev;
		};
	} nlcn_msg;
	struct exit_proc_event *exit_event = &nlcn_msg.proc_ev.event_data.exit;
	struct fork_proc_event *fork_event = &nlcn_msg.proc_ev.event_data.fork;

	ret = recv(qproc_mon.conn_sock, &nlcn_msg, sizeof(nlcn_msg), 0);
	if (ret <= 0) {
		log_message(1, "Failed to recv netlink connector event");
		return ret;
	}

	switch (nlcn_msg.proc_ev.what) {
	case PROC_EVENT_EXIT:
		while (tmp != NULL && tmp->pid != exit_event->process_pid) {
			tmp = tmp->next;
		}

		if (tmp != NULL) {
			log_message(0, "EXIT: pid=%d exit_code=%d exit_signal %d", exit_event->process_pid,
						exit_event->exit_code, exit_event->exit_signal);
			qproc_mon_restart_process(tmp, exit_event->exit_code);
		}
		break;

	case PROC_EVENT_FORK:
		while (tmp != NULL && tmp->pid != fork_event->parent_pid) {
			tmp = tmp->next;
		}

		if (tmp != NULL) {
			log_message(0, "FORK: pid=%d child pid=%d", fork_event->parent_pid,
									fork_event->child_pid);
			tmp->child_pid = fork_event->child_pid;
		}
		break;

	default:
		break;
	}

	return ret;
}

static struct qproc_mon_node *qproc_mon_add_to_list(struct qproc_mon_node **phead,
					pid_t pid, pid_t child_pid, char *cmd_args)
{
	struct qproc_mon_node *tmp;

	tmp = (struct qproc_mon_node *) malloc(sizeof(struct qproc_mon_node));
	if (tmp == NULL) {
		log_message(1, "Failed to allocate memory for process entry: pid %d"
				": child pid %d: cmd %s", pid, child_pid, cmd_args);
		return NULL;
	}
	log_message(0, "Creating node with: %s", cmd_args);

	tmp->pid = pid;
	tmp->child_pid = child_pid;
	strncpy(tmp->cmd_args, cmd_args, sizeof(tmp->cmd_args) - 1);
	tmp->next = *phead;
	*phead = tmp;

	return tmp;
}

static int qproc_mon_process_launch_start_event(char *cmd_buf)
{
	pid_t pid;
	struct qproc_mon_node *tmp;

	pid = qproc_mon_launch_process(cmd_buf);
	if (pid <= 0) {
		log_message(1, "Failed to start process: %s", cmd_buf);
		return -1;
	}

	tmp = qproc_mon_add_to_list(&qproc_mon.active_list, pid, 0, cmd_buf);
	if (tmp == NULL)
		return -1;

	qproc_mon_add_persistent_file(tmp);

	return 0;
}

static int qproc_mon_cmp_process_name(char *name, char *cmd_args)
{
	char *process_name;
	char cmd_line[QPROC_MON_MAX_CMD_LEN];

	strncpy(cmd_line, cmd_args, sizeof(cmd_line) - 1);

	process_name = strtok(cmd_line, " ");
	if (process_name == NULL)
		return 1;

	return strcmp(name, process_name);
}

static int qproc_mon_remove_from_list(struct qproc_mon_node **phead, char *cmd_name)
{
	struct qproc_mon_node *cur = *phead;
	struct qproc_mon_node *prev = NULL;
	struct qproc_mon_node *tmp;
	int update = 0;

	while (cur) {
		if (qproc_mon_cmp_process_name(cmd_name, cur->cmd_args)) {
			prev = cur;
			cur = cur->next;
			continue;
		}

		tmp = cur;
		if (prev == NULL) {
			*phead = cur->next;
		} else {
			prev->next = cur->next;
		}

		cur = cur->next;
		free(tmp);
		update = 1;
	}

	return update;
}

static void qproc_mon_kill_process(char *name)
{
	char *process_name;
	char kill_cmd[QPROC_MON_MAX_CMD_LEN];

	process_name = strrchr(name, '/');
	if (process_name == NULL) {
		process_name = name;
	} else {
		process_name++;
	}

	snprintf(kill_cmd, QPROC_MON_MAX_CMD_LEN, "killall %s", process_name);
	system(kill_cmd);
	/* FIXME To remove zombie entries from process table */
	qproc_mon_sigchild_handler(SIGCHLD);
}

static void qproc_mon_process_launch_stop_event(char *buffer)
{
	if (qproc_mon_remove_from_list(&qproc_mon.active_list, buffer)) {
		qproc_mon_update_persistent_file();
		qproc_mon_kill_process(buffer);
	}

	qproc_mon_remove_from_list(&qproc_mon.pending_list, buffer);
}

static int qproc_mon_handle_launch_events(void)
{
	char buffer[QPROC_MON_MAX_FIFO_MSG_LEN] = {0};
	char *cmd;
	int len;
	unsigned int type;

	len = read(qproc_mon.launch_fifo_fd, buffer, QPROC_MON_MAX_FIFO_MSG_LEN);
	if (len > 0) {
		/* FIFO Message format => "Type:Message" */
		sscanf(buffer, "%d", &type);
		cmd = strchr(buffer, ':');
		if (cmd == NULL)
			return -1;
		cmd++;

		if (type == QPROC_MON_LAUNCH_START_MSG_TYPE) {
			qproc_mon_process_launch_start_event(cmd);
		} else if (type == QPROC_MON_LAUNCH_STOP_MSG_TYPE) {
			qproc_mon_process_launch_stop_event(cmd);
		} else {
			return -1;
		}
	}

	return 0;
}

static int qproc_mon_process_pending_launch_event(void)
{
	pid_t pid;
	struct qproc_mon_node *list = qproc_mon.pending_list;
	struct qproc_mon_node *tmp;

	while (list != NULL) {
		tmp = list;
		list = list->next;

		pid = qproc_mon_launch_process(tmp->cmd_args);
		if (pid <= 0) {
			log_message(1, "Failed to start process: %s", tmp->cmd_args);
			free(tmp);
			continue;
		}

		tmp->pid = pid;
		tmp->next = qproc_mon.active_list;
		qproc_mon.active_list = tmp;
	}

	qproc_mon.pending_list = NULL;

	return qproc_mon_update_persistent_file();
}

static int qproc_mon_wait_for_events(void)
{
	int ret = 0;
	int last_fd = 0;
	fd_set rfds;
	struct timeval tv;

	while (1) {

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		FD_ZERO(&rfds);

		if (qproc_mon.conn_sock >= 0) {
			FD_SET(qproc_mon.conn_sock, &rfds);
			last_fd = qproc_mon.conn_sock;
		}

		if (qproc_mon.launch_fifo_fd >= 0) {
			FD_SET(qproc_mon.launch_fifo_fd, &rfds);
			last_fd = MAX(last_fd, qproc_mon.launch_fifo_fd);
		}

		/* Wait until something happens */
		ret = select(last_fd + 1, &rfds, NULL, NULL, &tv);
		if (ret < 0) {
			if (errno == EAGAIN || errno == EINTR)
				continue;
			log_message(1, "Unhandled select event - exiting");
			break;
		}

		if (ret == 0) {
			continue;
		}

		if (qproc_mon.conn_sock >= 0 && FD_ISSET(qproc_mon.conn_sock, &rfds)) {
				qproc_mon_handle_process_events();
		}

		if (qproc_mon.launch_fifo_fd >= 0 && FD_ISSET(qproc_mon.launch_fifo_fd, &rfds)) {
			qproc_mon_handle_launch_events();
		}

		if (qproc_mon.pending_list != NULL) {
			qproc_mon_process_pending_launch_event();
		}
	}

	return ret;
}

static int qproc_mon_parse_persistent_file(void)
{
	FILE *fp = NULL;
	size_t len = 0;
	char *line = NULL;
	size_t read_l;
	struct qproc_mon_node *tmp = NULL;
	int ret = 0;
	char cmd_args[QPROC_MON_MAX_CMD_LEN];
	pid_t pid;
	pid_t child_pid;

	fp = fopen(QPROC_MON_STATE_FILE, "r");
	if (fp == NULL) {
		log_message(1, "Failed to open process monitor configuration file: %s : %s",
				QPROC_MON_STATE_FILE, strerror(errno));
		return -1;
	}

	log_message(0, "Reading configuration file");

	while ((read_l = getline(&line, &len, fp)) != -1) {
		sscanf(line, "%d", &pid);
		if (qproc_mon_process_exists(pid) < 0) {
			sscanf(line, "%*d:%*d:%[^\n]", cmd_args);
			qproc_mon_add_to_list(&qproc_mon.pending_list, 0, 0, cmd_args);
			continue;
		}

		/* Format for persistent file entry => ProcessID:ChildProcessID:CommandLineArgs */
		sscanf(line, "%d:%d:%[^\n]", &pid, &child_pid, cmd_args);
		tmp = qproc_mon_add_to_list(&qproc_mon.active_list, pid, child_pid, cmd_args);
		if (tmp == NULL) {
			ret = -1;
			goto failure;
		}
		log_message(0, "%s", line);
	}

	ret = qproc_mon_update_persistent_file();

failure:
	if (line)
		free(line);

	fclose(fp);

	return ret;
}

static int qproc_mon_handle_pre_spawn_launch_events(void)
{
	struct qproc_mon_mq_buffer cmd_buf;
	struct msqid_ds stats;
	int len;

	if (msgctl(qproc_mon.launch_mqid, IPC_STAT, &stats) == -1) {
		log_message(1, "Failed msgctl: %s", strerror(errno));
		return -1;
	}

	while (stats.msg_qnum > 0) {
		/* Message type is 0 to receive first message in queue */
		len = msgrcv(qproc_mon.launch_mqid, &cmd_buf, QPROC_MON_MAX_CMD_LEN,
									0, IPC_NOWAIT);
		if (len < 0) {
			log_message(1, "Failed msgrcv: %s", strerror(errno));
			return -1;
		}

		if (cmd_buf.mtype == QPROC_MON_LAUNCH_START_MSG_TYPE) {
			qproc_mon_add_to_list(&qproc_mon.pending_list, 0, 0, cmd_buf.cmd_args);
		} else if (cmd_buf.mtype == QPROC_MON_LAUNCH_STOP_MSG_TYPE) {
			qproc_mon_process_launch_stop_event(cmd_buf.cmd_args);
		}

		stats.msg_qnum--;
	}

	return 0;
}

static int qproc_mon_nl_connector_init(void)
{
	int ret;
	int nl_sock;
	struct sockaddr_nl sa_nl;

	nl_sock = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
	if (nl_sock == -1) {
		log_message(1, "Failed to create netlink connector socket");
		return -1;
	}

	sa_nl.nl_family = AF_NETLINK;
	sa_nl.nl_groups = CN_IDX_PROC;
	sa_nl.nl_pid = getpid();

	ret = bind(nl_sock, (struct sockaddr *) &sa_nl, sizeof(sa_nl));
	if (ret == -1) {
		log_message(1, "Failed to bind netlink connector socket");
		close(nl_sock);
		return ret;
	}

	ret = qproc_mon_set_event_listener(nl_sock, true);
	if (ret == -1) {
		close(nl_sock);
		return ret;
	}

	qproc_mon.conn_sock = nl_sock;

	return 0;
}

static int qproc_mon_launch_recv_init(void)
{
	key_t key;

	key = ftok(QPROC_MON_CONFIG, QPROC_MON_PROJ_ID);
	if (key == -1) {
		log_message(1, "Failed to get key in ftok");
		return -1;
	}

	qproc_mon.launch_mqid = msgget(key, IPC_CREAT | 0666);
	if (qproc_mon.launch_mqid < 0) {
		log_message(1, "Failed to initialize launch message queue: %s", strerror(errno));
		return -1;
	}

	/* TODO: Do we need to set any options for the queue */

	return 0;
}

static int qproc_mon_launch_event_fifo_init(void)
{
	int ret;

	remove(QPROC_MON_FIFO);

	ret = mkfifo(QPROC_MON_FIFO, 0666);
	if (ret < 0) {
		log_message(1, "Failed to create mkfifo: %s", strerror(errno));
		return -1;
	}

	qproc_mon.launch_fifo_fd = open(QPROC_MON_FIFO, O_RDWR | O_NONBLOCK);
	if (qproc_mon.launch_fifo_fd < 0) {
		log_message(1, "Failed to open fifo file: %s", strerror(errno));
		return -1;
	}

	return 0;
}

static void qproc_mon_cleanup(void)
{
	if (qproc_mon.conn_sock != -1) {
		qproc_mon_set_event_listener(qproc_mon.conn_sock, false);
		close(qproc_mon.conn_sock);
	}

	qproc_mon_delete_proc_nodes(&qproc_mon.active_list);
	qproc_mon_delete_proc_nodes(&qproc_mon.pending_list);

	if (qproc_mon.launch_fifo_fd > 0)
		close(qproc_mon.launch_fifo_fd);

#ifdef CONFIG_QPROC_MONITOR_LOG
	if (qproc_mon.log_fp)
		fclose(qproc_mon.log_fp);
#endif
}

static int qproc_mon_init(void)
{
#ifdef CONFIG_QPROC_MONITOR_LOG
	qproc_mon.log_fp = fopen(QPROC_MON_LOGFILE, "a+");
	if (qproc_mon.log_fp == NULL) {
		log_message(1, "Failed to open process monitor log file: %s", QPROC_MON_LOGFILE);
	}
#endif
	log_message(0, "Starting process monitor log messages...");

	if (qproc_mon_nl_connector_init() < 0)
		return -1;

	if (qproc_mon_launch_event_fifo_init() < 0)
		return -1;

	/* Launch message queue receiver initialization */
	if (qproc_mon_launch_recv_init() < 0)
		return -1;

	qproc_mon_parse_persistent_file();

	qproc_mon_handle_pre_spawn_launch_events();

	signal(SIGCHLD, qproc_mon_sigchild_handler);

	return 0;
}

int main(int argc, const char *argv[], const char *env[])
{
	int ret;

	ret = qproc_mon_init();
	if (ret == 0) {
		qproc_mon_wait_for_events();
	}

	qproc_mon_cleanup();

	return ret;
}
