/*SH0
*******************************************************************************
**                                                                           **
**         Copyright (c) 2014 Quantenna Communications, Inc.                 **
**         All rights reserved.                                              **
**                                                                           **
*******************************************************************************
EH0*/

#include <sys/time.h>
#include <signal.h>
#include "../../qcsapi/qcsapi-1.0.1/qcsapi.h"
#include "ini.h"
#include "dfs_daemon.h"

#define _STR(s)     #s
#define STR(s)      _STR(s)
#ifndef VERSION
#error No version defined
#else
#define __VERSION STR(VERSION)
#endif

static dfs_daemon_cfg g_dfs_daemon_cfg = { 0 };

static int ini_handler(void *user, const char *section, const char *name,
		       const char *value)
{
#define MATCH(field, value) (strcmp(field, value)==0)
	dfs_daemon_cfg *config = (dfs_daemon_cfg *) user;

	if (value == NULL || strlen(value) == 0) {
		DBG_NOTICE
		    ("Skip NULL value for parameter %s, default will be used",
		     name);
		return 0;
	}

	if (MATCH(section, "dfs_daemon")) {
		if (MATCH(name, "debug")) {
			config->debug = atoi(value);
		} else if (MATCH(name, "syslog")) {
			config->syslog = atoi(value);
		} else if (MATCH(name, "console_print")) {
			config->console_print = atoi(value);
		} else if (MATCH(name, "remote")) {
			if (config->remote)
				free(config->remote);
			config->remote = strdup(value);
		} else
			DBG_ERROR("Skip unknown parameter %s in section %s",
				  name, "dfs_daemon");
	} else if (MATCH(section, "dfs_reentry")) {
		dfs_reentry_cfg *reentry_cfg = &(config->reentry);
		if (MATCH(name, "interface")) {
			if (reentry_cfg->iface)
				free(reentry_cfg->iface);
			reentry_cfg->iface = strdup(value);
		} else if (MATCH(name, "interval")) {
			reentry_cfg->interval = atoi(value);
		} else if (MATCH(name, "second_interval")) {
			reentry_cfg->second_interval = atoi(value);
		} else if (MATCH(name, "start_hour")) {
			reentry_cfg->start_hour = atoi(value);
		} else if (MATCH(name, "end_hour")) {
			reentry_cfg->end_hour = atoi(value);
		} else if (MATCH(name, "scs_rxtx_threshold")) {
			reentry_cfg->scs_rxtx_threshold = atoi(value);
		} else if (MATCH(name, "scs_tx_threshold")) {
			reentry_cfg->scs_tx_threshold = atoi(value);
		} else if (MATCH(name, "target_count")) {
			reentry_cfg->target_count = atoi(value);
		} else if (MATCH(name, "time_offset")) {
			reentry_cfg->time_offset = atoi(value);
		} else if (MATCH(name, "score_threshold")) {
			reentry_cfg->score_threshold = atoi(value);
		} else
			DBG_ERROR("Skip unknown parameter %s in section %s",
				  name, "dfs_reentry");
	} else
		DBG_ERROR("Skip unknown section %s", section);

	return 0;
}

static void signal_handler(int sig)
{
	if (g_dfs_daemon_cfg.reentry.iface)
		free(g_dfs_daemon_cfg.reentry.iface);
	if (g_dfs_daemon_cfg.region)
		free(g_dfs_daemon_cfg.region);
	if (g_dfs_daemon_cfg.conf_file)
		free(g_dfs_daemon_cfg.conf_file);
	if (g_dfs_daemon_cfg.remote)
		free(g_dfs_daemon_cfg.remote);

	exit(0);
}

static int dfs_daemon_init_config()
{
	int qret;
	char region[10];
	qcsapi_unsigned_int scs_enabled;

	g_dfs_daemon_cfg.remote = NULL;
	g_dfs_daemon_cfg.debug = 3;
	g_dfs_daemon_cfg.syslog = 1;
	g_dfs_daemon_cfg.console_print = 1;

	g_dfs_daemon_cfg.reentry.iface = strdup(DEFAULT_INTERFACE);
	g_dfs_daemon_cfg.reentry.interval = 900;
	g_dfs_daemon_cfg.reentry.second_interval = 60;
	g_dfs_daemon_cfg.reentry.start_hour = 1;
	g_dfs_daemon_cfg.reentry.end_hour = 4;
	g_dfs_daemon_cfg.reentry.scs_rxtx_threshold = 100;
	g_dfs_daemon_cfg.reentry.scs_tx_threshold = 50;
	g_dfs_daemon_cfg.reentry.target_count = 5;
	g_dfs_daemon_cfg.reentry.curr_count = 0;
	g_dfs_daemon_cfg.reentry.time_offset = 0;
	g_dfs_daemon_cfg.reentry.score_threshold = 50;

	if (g_dfs_daemon_cfg.conf_file == NULL) {
		g_dfs_daemon_cfg.conf_file = strdup(DEFAULT_CONFIG);
	}
	if (ini_parse
	    (g_dfs_daemon_cfg.conf_file, ini_handler, &g_dfs_daemon_cfg) < 0) {
		DBG_WARNING("cannot load '%s', use default settings",
			    g_dfs_daemon_cfg.conf_file);
	}
	g_dfs_daemon_cfg.reentry.delay = g_dfs_daemon_cfg.reentry.interval;

	CALL_QCSAPI(wifi_get_regulatory_region, qret, goto bail,
		    g_dfs_daemon_cfg.reentry.iface, region);
	g_dfs_daemon_cfg.region = strdup(region);

	CALL_QCSAPI(wifi_get_scs_status, qret, goto bail,
		    g_dfs_daemon_cfg.reentry.iface, &scs_enabled);
	DBG_INFO("scs_enabled = %d", scs_enabled);

	if (!scs_enabled) {
		DBG_ERROR("SCS not enabled, DFS Re-entry Monitor stopped!");
		qret = -1;
		goto bail;
	}

	DBG_INFO("debug = %d", g_dfs_daemon_cfg.debug);
	DBG_INFO("syslog = %d", g_dfs_daemon_cfg.syslog);
	DBG_INFO("console_print = %d", g_dfs_daemon_cfg.console_print);
	DBG_INFO("region = %s", g_dfs_daemon_cfg.region);
	DBG_INFO("config_file = %s", g_dfs_daemon_cfg.conf_file);
	if (g_dfs_daemon_cfg.remote != NULL) {
		DBG_INFO("remote = %s", g_dfs_daemon_cfg.remote);
	}

	DBG_INFO("interface = %s", g_dfs_daemon_cfg.reentry.iface);
	DBG_INFO("interval = %d", g_dfs_daemon_cfg.reentry.interval);
	DBG_INFO("second_interval = %d",
		 g_dfs_daemon_cfg.reentry.second_interval);
	DBG_INFO("delay = %d", g_dfs_daemon_cfg.reentry.delay);
	DBG_INFO("start_hour = %d", g_dfs_daemon_cfg.reentry.start_hour);
	DBG_INFO("end_hour = %d", g_dfs_daemon_cfg.reentry.end_hour);
	DBG_INFO("scs_rxtx_threshold = %d",
		 g_dfs_daemon_cfg.reentry.scs_rxtx_threshold);
	DBG_INFO("scs_tx_threshold = %d",
		 g_dfs_daemon_cfg.reentry.scs_tx_threshold);
	DBG_INFO("target_count = %d", g_dfs_daemon_cfg.reentry.target_count);
	DBG_INFO("curr_count = %d", g_dfs_daemon_cfg.reentry.curr_count);
	DBG_INFO("time_offset = %d", g_dfs_daemon_cfg.reentry.time_offset);
	DBG_INFO("score_threshold = %d",
		 g_dfs_daemon_cfg.reentry.score_threshold);

 bail:
	return qret;
}

static int dfs_daemon_parse_args(int argc, char *argv[])
{
	int ret = 0;
	int n = 0;

	while (++n < argc) {
		if (strncmp(argv[n], "-c", 2) == 0) {
			g_dfs_daemon_cfg.conf_file = strdup(argv[n] + 2);
		} else {
			ret = -1;
		}
	}

	return ret;
}

static void dfs_daemon_print_usage()
{
	printf("(dfs_daemon v%s) Usage: dfs_daemon [-cCONF_FILE]\n", __VERSION);
}

#define start_dfsreentry() \
do { \
	g_dfs_daemon_cfg.reentry.curr_count = 0; \
	g_dfs_daemon_cfg.reentry.delay = g_dfs_daemon_cfg.reentry.interval; \
	DBG_NOTICE("start_dfsreentry called"); \
	CALL_QCSAPI(wifi_start_dfs_reentry, qret, goto bail, g_dfs_daemon_cfg.reentry.iface); \
} while(0)

static int in_maint_wnd()
{
	char buf[30];
	int curr_hour;
	time_t rawtime;
	struct tm *p_timeinfo;

	time(&rawtime);
	p_timeinfo =
	    g_dfs_daemon_cfg.reentry.time_offset ==
	    0 ? localtime(&rawtime) : gmtime(&rawtime);
	if (p_timeinfo == NULL) {
		DBG_ERROR("local(gm)time returned error");
		return -1;
	}
	strftime(buf, 30, "%Y-%m-%d %H:%M:%S", p_timeinfo);
	DBG_INFO("current time is %s", buf);

	curr_hour = p_timeinfo->tm_hour + g_dfs_daemon_cfg.reentry.time_offset;
	if (curr_hour < 0)
		curr_hour += 24;
	curr_hour %= 24;
	if (curr_hour >= g_dfs_daemon_cfg.reentry.start_hour
	    && curr_hour <= g_dfs_daemon_cfg.reentry.end_hour)
		return 1;
	else
		return 0;
}

static int dfs_daemon_handle_reenty()
{
	int qret = 0, is_dfs;
	qcsapi_unsigned_int channel, reentry_request;
	struct qcsapi_scs_currchan_rpt scs_report;

	/* get current channel and check whether it is DFS channel */
	CALL_QCSAPI(wifi_get_channel, qret, goto bail,
		    g_dfs_daemon_cfg.reentry.iface, &channel);
	CALL_QCSAPI(wifi_is_channel_DFS, qret, goto bail,
		    g_dfs_daemon_cfg.region, channel, &is_dfs);
	if (!is_dfs) {		/* current channel is non-DFS channel, check whether DFS Re-entry needed. */

		/* Check if non-DFS channel all poor, if so trigger DFS Re-entry immediately */
		CALL_QCSAPI(wifi_get_scs_dfs_reentry_request, qret, goto bail,
			    g_dfs_daemon_cfg.reentry.iface, &reentry_request);
		if (reentry_request == 1) {
			DBG_INFO
			    ("All non-DFS channels have interference, start DFS-Reentry immediately\n");
			start_dfsreentry();
			return 0;
		}

		/* check if current time in configured maintenance window? */
		qret = in_maint_wnd();
		if (qret < 0)
			goto bail;
		else if (qret == 1) {	/* current time in configured maintenance window */
			int i, max_nondfs_score = 0, max_dfs_score = 0;
			qcsapi_scs_score_rpt score_rpt;

			/* get score for all channels, bigger score better channel */
			CALL_QCSAPI(wifi_get_scs_score_report, qret, goto bail,
				    g_dfs_daemon_cfg.reentry.iface, &score_rpt);
			for (i = 0; i < score_rpt.num; i++) {
				CALL_QCSAPI(wifi_is_channel_DFS, qret,
					    goto bail, g_dfs_daemon_cfg.region,
					    score_rpt.chan[i], &is_dfs);
				//DBG_INFO("%0d %0d %d", score_rpt.chan[i], score_rpt.score[i], is_dfs);
				if (is_dfs) {
					if (score_rpt.score[i] >= max_dfs_score)
						max_dfs_score =
						    score_rpt.score[i];
				} else {
					if (score_rpt.score[i] >=
					    max_nondfs_score)
						max_nondfs_score =
						    score_rpt.score[i];
				}
			}
			DBG_INFO("max_dfs_score=%d max_nondfs_score=%d",
				 max_dfs_score, max_nondfs_score);

			/* check whether max score of DFS channel better than max score of non-DFS channel over threshold */
			if ((max_dfs_score - max_nondfs_score) >=
			    g_dfs_daemon_cfg.reentry.score_threshold) {
				/* get statistics for the current channel */
				CALL_QCSAPI(wifi_get_scs_currchan_report, qret,
					    goto bail,
					    g_dfs_daemon_cfg.reentry.iface,
					    &scs_report);
				DBG_INFO("current: tx_ms(%d), rx_ms(%d)",
					 scs_report.tx_ms, scs_report.rx_ms);
				if ((scs_report.tx_ms + scs_report.rx_ms <
				     g_dfs_daemon_cfg.reentry.
				     scs_rxtx_threshold)
				    && (scs_report.tx_ms <
					g_dfs_daemon_cfg.reentry.
					scs_tx_threshold)) {
					g_dfs_daemon_cfg.reentry.curr_count++;
					g_dfs_daemon_cfg.reentry.delay =
					    g_dfs_daemon_cfg.reentry.
					    second_interval;
					DBG_INFO
					    ("no-video threshold met: tx_ms(%d), rx_ms(%d), scs_rxtx_threshold(%d), scs_tx_threshold(%d)",
					     scs_report.tx_ms, scs_report.rx_ms,
					     g_dfs_daemon_cfg.reentry.
					     scs_rxtx_threshold,
					     g_dfs_daemon_cfg.reentry.
					     scs_tx_threshold);
					DBG_INFO
					    ("curr_count(%d), target_count(%d)",
					     g_dfs_daemon_cfg.reentry.
					     curr_count,
					     g_dfs_daemon_cfg.reentry.
					     target_count);

					if (g_dfs_daemon_cfg.reentry.
					    curr_count >=
					    g_dfs_daemon_cfg.reentry.
					    target_count) {
						start_dfsreentry();
					}
					return 0;
				}
			}
		}
	} else {
		DBG_INFO("current channel %d is DFS channel", channel);
	}

 bail:
	if (g_dfs_daemon_cfg.reentry.curr_count != 0) {
		DBG_INFO("curr_count(%d) reset to 0",
			 g_dfs_daemon_cfg.reentry.curr_count);
		g_dfs_daemon_cfg.reentry.curr_count = 0;
		g_dfs_daemon_cfg.reentry.delay =
		    g_dfs_daemon_cfg.reentry.interval;
	}
	return qret;
}

int main(int argc, char *argv[])
{
	int ret;

	signal(SIGINT, signal_handler);
	signal(SIGHUP, signal_handler);
	signal(SIGKILL, signal_handler);
	signal(SIGSEGV, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGABRT, signal_handler);

#ifdef USE_SYSLOG
	openlog(MY_NAME, LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
#endif
	if ((ret = dfs_daemon_parse_args(argc, argv)) != 0) {
		dfs_daemon_print_usage();
		goto bail;
	}

	if ((ret = dfs_daemon_init_config()) != 0) {
		goto bail;
	}

	DBG_NOTICE("DFS Daemon v%s started, interval set to %d seconds",
		   __VERSION, g_dfs_daemon_cfg.reentry.interval);

	while (1) {
		dfs_daemon_handle_reenty();
		DBG_INFO("sleep for %d seconds",
			 g_dfs_daemon_cfg.reentry.delay);
		sleep(g_dfs_daemon_cfg.reentry.delay);
	}

 bail:
	DBG_NOTICE("dfs_daemon terminated, exit code %d", ret);

	if (g_dfs_daemon_cfg.reentry.iface)
		free(g_dfs_daemon_cfg.reentry.iface);
	if (g_dfs_daemon_cfg.region)
		free(g_dfs_daemon_cfg.region);
	if (g_dfs_daemon_cfg.conf_file)
		free(g_dfs_daemon_cfg.conf_file);
	if (g_dfs_daemon_cfg.remote)
		free(g_dfs_daemon_cfg.remote);

#ifdef USE_SYSLOG
	closelog();
#endif
	return ret;
}
