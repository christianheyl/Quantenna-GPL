/*SH0
*******************************************************************************
**                                                                           **
**         Copyright (c) 2014 Quantenna Communications, Inc.                 **
**         All rights reserved.                                              **
**                                                                           **
*******************************************************************************
EH0*/

#ifndef __DFS_MON_H_
#define __DFS_MON_H_

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define MY_NAME "dfs_daemon"
#define DEFAULT_INTERFACE "wifi0"
#define DEFAULT_CONFIG "/etc/dfs_daemon.conf"

#ifdef REMOTE_QCSAPI
#include "qcsapi_rpc/client/qcsapi_rpc_client.h"
#include "qcsapi_rpc/generated/qcsapi_rpc.h"
#include <errno.h>
void connect_client(int force);
#endif

#define DEBUG_NONE	0
#define DEBUG_ERROR	0
#define DEBUG_WARNING	1
#define DEBUG_NOTICE	2
#define DEBUG_INFO	3

#ifndef NOQCSAPI
#ifndef REMOTE_QCSAPI
#define CALL_QCSAPI(api, ret, errroutine, ...) \
	do { \
		(ret) = qcsapi_##api(__VA_ARGS__); \
		if (ret<0) { \
			DBG_ERROR("qcsapi[%s] failed, error code = %d", #api, (ret));\
			errroutine; \
		} \
	} while(0)
#else
#define RETRY_MAX 3
static CLIENT *g_client = NULL;
void connect_client(int force)
{
	int retry;

	if ((force == 0) && (g_client != NULL)) {
		return;
	}
	do {
		if (g_client != NULL) {
			clnt_destroy(g_client);
		}
		g_client =
		    clnt_create(g_dfs_daemon_cfg.remote, QCSAPI_PROG,
				QCSAPI_VERS, "udp");
		if (g_client == NULL) {
			DBG_ERROR("cannot connect to %s",
				  g_dfs_daemon_cfg.remote);
			sleep(1);
			continue;
		} else {
			DBG_NOTICE("got connected with %s",
				   g_dfs_daemon_cfg.remote);
			client_qcsapi_set_rpcclient(g_client);
			return;
		}
	} while (retry++ < RETRY_MAX);
}

#define CALL_QCSAPI(api, ret, errroutine, ...) \
	do { \
		connect_client(0);\
		(ret) = qcsapi_##api(__VA_ARGS__); \
		if (ret<0) { \
			DBG_ERROR("qcsapi[%s] failed, error code = %d", #api, (ret));\
			if (ret==-ENOLINK) {\
				DBG_INFO("try connect again");\
				connect_client(1);\
				(ret) = qcsapi_##api(__VA_ARGS__); \
				if (ret<0) {\
					DBG_ERROR("qcsapi2[%s] failed, error code = %d", #api, (ret));\
				}\
			}\
			errroutine; \
		} \
	} while(0)
#endif				/* REMOTE_QCSAPI */
#else
#define CALL_QCSAPI(api, ret, errroutine, ...)
#endif				/* NOQCSAPI */

#ifdef USE_SYSLOG

#include <syslog.h>
#define DBG_ERROR(fmt,...) \
	do { \
			if (g_dfs_daemon_cfg.debug >= DEBUG_ERROR) { \
				if (g_dfs_daemon_cfg.console_print) { \
					printf("dfs_daemon[error]: " fmt "\n" , ##__VA_ARGS__);\
				} else if (g_dfs_daemon_cfg.syslog) { \
					syslog(LOG_ERR, fmt, ##__VA_ARGS__);\
				} \
			} \
		} while(0)

#define DBG_WARNING(fmt,...) \
	do { \
			if (g_dfs_daemon_cfg.debug >= DEBUG_WARNING) { \
				if (g_dfs_daemon_cfg.console_print) { \
					printf("dfs_daemon[warning]: " fmt "\n" , ##__VA_ARGS__);\
				} else if (g_dfs_daemon_cfg.syslog) { \
					syslog(LOG_WARNING, fmt, ##__VA_ARGS__);\
				} \
			} \
		} while(0)

#define DBG_NOTICE(fmt,...) \
	do { \
		if (g_dfs_daemon_cfg.debug >= DEBUG_NOTICE) { \
			if (g_dfs_daemon_cfg.console_print) { \
				printf("dfs_daemon[notice]: " fmt "\n" , ##__VA_ARGS__);\
			} else if (g_dfs_daemon_cfg.syslog) { \
				syslog(LOG_NOTICE, fmt, ##__VA_ARGS__);\
			} \
		} \
	} while(0)

#define DBG_INFO(fmt,...) \
	do { \
			if (g_dfs_daemon_cfg.debug >= DEBUG_INFO) { \
				if (g_dfs_daemon_cfg.console_print) { \
					printf("dfs_daemon[info]: " fmt "\n" , ##__VA_ARGS__);\
				} else if (g_dfs_daemon_cfg.syslog) { \
					syslog(LOG_INFO, fmt, ##__VA_ARGS__);\
				} \
			} \
		} while(0)

#else

#define DBG_ERROR(fmt,...) do { printf("dfs_daemon[error]: " fmt "\n" , ##__VA_ARGS__);} while(0)
#define DBG_WARNING(fmt,...) do { printf("dfs_daemon[warning]: " fmt "\n" , ##__VA_ARGS__);} while(0)
#define DBG_NOTICE(fmt,...) do { printf("dfs_daemon[notice]: " fmt "\n" , ##__VA_ARGS__);} while(0)
#define DBG_INFO(fmt,...) do { printf("dfs_daemon[info]: " fmt "\n" , ##__VA_ARGS__);} while(0)

#endif

typedef struct {
	char *iface;		//e.g. wifi0
	int interval;		//in sec
	int second_interval;	//in sec
	int delay;		//in sec
	int start_hour;		//0 - 23, in hour
	int end_hour;		//0 - 23, in hour
	int scs_rxtx_threshold;	//0 - 1000, in msec
	int scs_tx_threshold;	//0 - 1000, in msec
	int vsp_stream_threshold;	//in Kbps
	int target_count;	//e.g. 3 means threshold hit for 3 consecutive intervals
	int curr_count;		//keep track of current hit count
	int time_offset;	//time difference relative to GMT, e.g. -8 for PST
	int score_threshold;	// threshold of max score of DFS channel better than max score of non-DFS channel
} dfs_reentry_cfg;

typedef struct {
	int debug;		//no debug, 0 - 4, max debug
	int syslog;		//0 or 1
	int console_print;	//0 or 1
	char *region;		//e.g. us
	char *conf_file;	//e.g. /etc/dfs_daemon.conf
	char *remote;		//currently not used

	dfs_reentry_cfg reentry;
} dfs_daemon_cfg;

#endif
