/****************************************************************************
*
* Copyright (c) 2015  Quantenna Communications, Inc.
*
* Permission to use, copy, modify, and/or distribute this software for any
* purpose with or without fee is hereby granted, provided that the above
* copyright notice and this permission notice appear in all copies.
*
* THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
* WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
* MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
* SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
* RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
* NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
* USE OR PERFORMANCE OF THIS SOFTWARE.
*
*****************************************************************************/

#include <stdio.h>
#include <syslog.h>
#include <unistd.h>
#include <stdlib.h>

#include "qtn/qcsapi.h"
#include "wfa_main.h"

int gxcSockfd = -1;

int wfa_ca_main(int argc, char *argv[]);

static void print_usage(const char *programm)
{
	fprintf(stderr, "Usage: %s [OPTIONS]\n"
		"\t-i <control agent interface>\n"
		"\t-p <control agent port>\n"
		"\t-I <native control agent IP>\n"
		"\t-P <native control agent port>\n"
		"\t-l <log level>, 0 is EMERG, 5 is NOTICE (default), 7 is DEBUG\n", programm);
}

int main(int argc, char **argv)
{
	enum { CMD_NAME = 0, CA_IF = 1, CA_PORT = 2, DUT_IP = 3, DUT_PORT = 4 };
	char *wfa_args[5] = { 0 };

	int log_level = LOG_NOTICE;
	int opt;

	wfa_args[CMD_NAME] = strdup(argv[0]);

	while ((opt = getopt(argc, argv, "hi:p:I:P:l:")) != -1) {
		switch (opt) {
		case 'h':
			print_usage(argv[0]);
			return 0;
		case 'i':
			free(wfa_args[CA_IF]);
			wfa_args[CA_IF] = strdup(optarg);
			break;

		case 'p':
			free(wfa_args[CA_PORT]);
			wfa_args[CA_PORT] = strdup(optarg);
			break;

		case 'P':
			free(wfa_args[DUT_PORT]);
			wfa_args[DUT_PORT] = strdup(optarg);
			break;

		case 'I':
			free(wfa_args[DUT_IP]);
			wfa_args[DUT_IP] = strdup(optarg);
			break;

		case 'l':
			sscanf(optarg, "%d", &log_level);
			break;
		}
	}

	setlogmask(LOG_UPTO(log_level));
	openlog(argv[0], LOG_CONS | LOG_PID | LOG_NDELAY, LOG_DAEMON);

	int error = wfa_ca_main(sizeof(wfa_args) / sizeof(*wfa_args), wfa_args);

	for (int i = 0; i < sizeof(wfa_args) / sizeof(*wfa_args); ++i) {
		free(wfa_args[i]);
	}

	closelog();

	return error;
}
