/*SH0
*******************************************************************************
**                                                                           **
**         Copyright (c) 2008 - 2009 Quantenna Communications Inc            **
**                            All Rights Reserved                            **
**                                                                           **
**  Author      : Mats Aretun                                                **
**  Date        : 03/13/09                                                   **
**  File        : wifigun.c                                                  **
**  Description :                                                            **
**                                                                           **
*******************************************************************************
**                                                                           **
**  Redistribution and use in source and binary forms, with or without       **
**  modification, are permitted provided that the following conditions       **
**  are met:                                                                 **
**  1. Redistributions of source code must retain the above copyright        **
**     notice, this list of conditions and the following disclaimer.         **
**  2. Redistributions in binary form must reproduce the above copyright     **
**     notice, this list of conditions and the following disclaimer in the   **
**     documentation and/or other materials provided with the distribution.  **
**  3. The name of the author may not be used to endorse or promote products **
**     derived from this software without specific prior written permission. **
**                                                                           **
**  Alternatively, this software may be distributed under the terms of the   **
**  GNU General Public License ("GPL") version 2, or (at your option) any    **
**  later version as published by the Free Software Foundation.              **
**                                                                           **
**  In the case this software is distributed under the GPL license,          **
**  you should have received a copy of the GNU General Public License        **
**  along with this software; if not, write to the Free Software             **
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA  **
**                                                                           **
**  THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR       **
**  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES**
**  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  **
**  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,         **
**  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT **
**  NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,**
**  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY    **
**  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT      **
**  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF **
**  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.        **
**                                                                           **
*******************************************************************************
EH0*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/time.h>

char *g_myname;

static int s_print_only = 0;

#define MSWAIT(t) \
	{														\
		struct timeval tv;									\
		tv.tv_sec = (t) / 1000;								\
		tv.tv_usec = ((t) - (((t) / 1000) * 1000)) * 1000;	\
		select(0, NULL, NULL, NULL, &tv);					\
	}

static char *s_iwlist_option_table[] =
{
	"scanning",
	"frequency",
	"rate",
	"keys",
	"power",
	"txpower",
	"retry",
	"event",
	"auth",
	"wpakeys",
	"genie",
	"modulation",
};

#define IWLIST_TABLE_SIZE (sizeof(s_iwlist_option_table) / sizeof(char *))

static char *s_iwconfig_option_table[] =
{
	"essid",
	"nwid",
	"mode",
	"freq",
	"channel",
	"sens",
	"ap",
	"nick",
	"rate",
	"rts",
	"frag",
	"txpower",
	"enc",
	"key",
	"power",
	"retry",
	"modu",
};

#define IWCONFIG_TABLE_SIZE (sizeof(s_iwconfig_option_table) / sizeof(char *))

static char *s_iwpriv_option_table[] =
{
	"setoptie",
	"getoptie",
	"setkey",
	"delkey",
	"setmlme",
	"setbssid",
	"delmac",
	"kickmac",
	"wds_add",
	"wds_del",
	"setchanlist",
	"getchanlist",
	"getchaninfo",
	"mode",
	"get_mode",
	"setwmmparams",
	"getwmmparams",
	"cwmin",
	"get_cwmin",
	"cwmax",
	"get_cwmax",
	"aifs",
	"get_aifs",
	"txoplimit",
	"get_txoplimit",
	"acm",
	"get_acm",
	"noackpolicy",
	"get_noackpolicy",
	"setparam",
	"getparam",
	"authmode",
	"get_authmode",
	"protmode",
	"get_protmode",
	"mcastcipher",
	"get_mcastcipher",
	"mcastkeylen",
	"get_mcastkeylen",
	"ucastciphers",
	"get_uciphers",
	"ucastcipher",
	"get_ucastcipher",
	"ucastkeylen",
	"get_ucastkeylen",
	"keymgtalgs",
	"get_keymgtalgs",
	"rsncaps",
	"get_rsncaps",
	"hostroaming",
	"get_hostroaming",
	"privacy",
	"get_privacy",
	"countermeasures",
	"get_countermeas",
	"dropunencrypted",
	"get_dropunencry",
	"wpa",
	"get_wpa",
	"driver_caps",
	"get_driver_caps",
	"maccmd",
	"wmm",
	"get_wmm",
	"hide_ssid",
	"get_hide_ssid",
	"ap_bridge",
	"get_ap_bridge",
	"inact",
	"get_inact",
	"inact_auth",
	"get_inact_auth",
	"inact_init",
	"get_inact_init",
	"abolt",
	"get_abolt",
	"dtim_period",
	"get_dtim_period",
	"bintval",
	"get_bintval",
	"doth",
	"get_doth",
	"doth_pwrtgt",
	"get_doth_pwrtgt",
	"doth_reassoc",
	"doth_chanswitch",
	"pureg",
	"get_pureg",
	"ar",
	"get_ar",
	"wds",
	"get_wds",
	"bgscan",
	"get_bgscan",
	"bgscanidle",
	"get_bgscanidle",
	"bgscanintvl",
	"get_bgscanintvl",
	"mcast_rate",
	"get_mcast_rate",
	"coverageclass",
	"get_coveragecls",
	"countryie",
	"get_countryie",
	"scanvalid",
	"get_scanvalid",
	"regclass",
	"get_regclass",
	"dropunenceapol",
	"get_dropunencea",
	"shpreamble",
	"get_shpreamble",
	"rssi11a",
	"get_rssi11a",
	"rssi11b",
	"get_rssi11b",
	"rssi11g",
	"get_rssi11g",
	"rate11a",
	"get_rate11a",
	"rate11b",
	"get_rate11b",
	"rate11g",
	"get_rate11g",
	"uapsd",
	"get_uapsd",
	"sleep",
	"get_sleep",
	"qosnull",
	"pspoll",
	"eospdrop",
	"get_eospdrop",
	"markdfs",
	"get_markdfs",
	"setiebuf",
	"getiebuf",
	"setfilter",
	"fixedtxrate",
	"get_fixedtxrate",
	"mimomode",
	"get_mimomode",
	"aggregation",
	"get_aggregation",
	"retrycount",
	"get_retrycount",
	"setchannal",
	"getchannel",
	"setcalib",
	"get_calib",
	"expmattype",
	"get_expmattype",
	"bwselect",
	"get_bwselect",
	"rgselect",
	"get_rgselect",
	"bwselect_muc",
	"get_bwselect_muc",
	"ackpolicy",
	"get_ackpolicy",
	"legacyselect",
	"get_legacyselect",
	"max_aggsubfr",
	"get_maxaggsubfr",
	"txbf_ctrl",
	"get_txbfctrl",
	"txbf_period",
	"get_txbfperiod",
	"htba_seq",
	"get_htba_seq",
	"htba_size",
	"get_htba_size",
	"htba_time",
	"get_htba_time",
	"htba_addba",
	"get_htba_addba",
	"htba_delba",
	"get_htba_delba",
	"fake_chan",
	"get_fake_chan",
	"muc_profile",
	"muc_profile",
	"muc_set_phystat",
	"muc_get_phystat",
	"muc_set_partnum",
	"muc_get_partnum",
	"ena_gain_adapt",
	"get_gain_adapt",
};

#define IWPRIV_TABLE_SIZE (sizeof(s_iwpriv_option_table) / sizeof(char *))

static int usage(char *name)
{
	printf("\nUsage: %s [OPTIONS] ... \n", name);
	printf("\n\tOPTIONS are some of the following:\n\n");
	printf("\t-n \"<num>\"              Number of concurrent executions (default 1).\n");
	printf("\t-i \"<num>\"              Number of iterations (default 1).\n");
	printf("\t-p                      Print commands only (don't execute).\n");
	printf("\t-h                      Show this message.\n");
	printf("\n\n");

	return(0);
}

static char random_char(void)
{
	unsigned char c;

	do
	{
		c = 32 + (random() % 91);
	}
	while(!isalnum(c));
	
	return((char) c);
}

static char random_digit(void)
{
	return((char) (48 + (random() % 10)));
}

static char random_xdigit(void)
{
	unsigned char c;

	do
	{
		c = 32 + (random() % 73);
	}
	while(!isxdigit(c));
	
	return((char) c);
}

static void random_string(char *buff, int len)
{
	int i;

	for(i = 0; i < (len - 1); i++)
	{
		buff[i] = random_char();
	}
	buff[len - 1] = '\0';

	return;
}

static void random_number(char *buff, int len)
{
	int i;

	for(i = 0; i < (len - 1); i++)
	{
		buff[i] = random_digit();
	}
	buff[len - 1] = '\0';

	return;
}

static void random_hexnumber(char *buff, int len)
{
	int i;
	int n = 0;

	if(len >= 3)
	{
		buff[0] = '0';
		buff[1] = 'x';
		n = 2;
	}

	for(i = 0 + n; i < (len - 1); i++)
	{
		buff[i] = random_xdigit();
	}
	buff[len - 1] = '\0';

	return;
}

static void do_iwconfig_command(void)
{
	int real;
	int cmd_num;
	int argtype;
	char *cmd;
	char cmd_buff[32];
	char *val;
	char val_buff[16];
	int len;

	real = random() % 100;

	if(real < 20)
	{
		len = (random() % sizeof(cmd_buff)) + 1;
		random_string(cmd_buff, len);
		cmd = &cmd_buff[0];
	}
	else
	{
		cmd_num = random() % IWCONFIG_TABLE_SIZE;
		cmd = s_iwconfig_option_table[cmd_num];
	}

	argtype = random() % 3;
	switch(argtype)
	{
		case 0:
			len = (random() % 10) + 2;
			random_number(val_buff, len);
			break;
		case 1:
			len = (random() % 8) + 4;
			random_hexnumber(val_buff, len);
			break;
		case 2:
			len = (random() % sizeof(val_buff)) + 1;
			random_string(val_buff, len);
			break;
		default:
			strcpy(val_buff, "keehhh");
			break;
	}
	val = &val_buff[0];

	if(s_print_only != 0)
	{
		printf("iwconfig wifi0 %s %s\n", cmd, val);
	}
	else
	{
		char buff[256];
		printf("iwconfig wifi0 %s %s\n", cmd, val);
		sprintf(buff, "iwconfig wifi0 %s %s\n", cmd, val);
		system(buff);
	}

	return;
}

static void do_iwpriv_command(void)
{
	int real;
	int cmd_num;
	int set;
	char *cmd;
	char cmd_buff[32];
	char *val;
	char val_buff[16];
	int len;
	int argtype;

	real = random() % 100;

	if(real < 20)
	{
		len = (random() % sizeof(cmd_buff)) + 1;
		random_string(cmd_buff, len);
		cmd = &cmd_buff[0];
	}
	else
	{
		cmd_num = random() % IWPRIV_TABLE_SIZE;
		cmd = s_iwpriv_option_table[cmd_num];
	}

	set = random() & 1;
	/* 2 or 3 arguments */
	if(set == 0)
	{
		if(s_print_only != 0)
		{
			printf("iwpriv wifi0 %s\n", cmd);
		}
		else
		{
			char buff[256];
			printf("iwpriv wifi0 %s\n", cmd);
			sprintf(buff, "iwpriv wifi0 %s\n", cmd);
			system(buff);
		}
	}
	else
	{
		argtype = random() % 2;
		switch(argtype)
		{
			case 0:
				len = (random() % 10) + 2;
				random_number(val_buff, len);
				break;
			case 1:
				len = (random() % 8) + 4;
				random_hexnumber(val_buff, len);
				break;
			default:
				strcpy(val_buff, "keehhh");
				break;
		}
		val = &val_buff[0];
		if(s_print_only != 0)
		{
			printf("iwpriv wifi0 %s %s\n", cmd, val);
		}
		else
		{
			char buff[256];
			printf("iwpriv wifi0 %s %s\n", cmd, val);
			sprintf(buff, "iwpriv wifi0 %s %s\n", cmd, val);
			system(buff);
		}
	}

	return;
}

static void do_iwlist_command(void)
{
	int real;
	int cmd_num;
	char *cmd;
	char cmd_buff[32];
	int len;

	real = random() % 100;

	if(real < 20)
	{
		len = (random() % sizeof(cmd_buff)) + 1;
		random_string(cmd_buff, len);
		cmd = &cmd_buff[0];
	}
	else
	{
		cmd_num = random() % IWLIST_TABLE_SIZE;
		cmd = s_iwlist_option_table[cmd_num];
	}

	if(s_print_only != 0)
	{
		printf("iwlist wifi0 %s\n", cmd);
	}
	else
	{
		char buff[256];
		printf("iwlist wifi0 %s\n", cmd);
		sprintf(buff, "iwlist wifi0 %s\n", cmd);
		system(buff);
	}

	return;
}

static void (*s_command_table[])(void) =
{
	do_iwconfig_command,
	do_iwlist_command,
	do_iwpriv_command,
};

#define NUM_COMMANDS	(sizeof(s_command_table) / sizeof(void (*)(void)))

int do_tests(int iterations)
{
	int wait;
	int i;
	int cmd;

	printf("Starting pid %d\n", getpid());
	for(i = 0; i < iterations; i++)
	{
		wait = random() % 1000;
		cmd = random() % NUM_COMMANDS;
		(*s_command_table[cmd])();
		MSWAIT(wait);
	}
	printf("Ending pid %d\n", getpid());

	return(0);
}

int main(int argc, char *argv[])
{
	/* Stuff used by getopt */
	int c;
	extern char *optarg;
	extern int optind, opterr;

	/* Our own local variables */
	int error_flag = 0;
	struct timeval tv;
	int status = 0;
	int num = 1;
	int i;
	int pid;
	int *wait_on_me;
	int iterations = 1;
	int seed_wait;

	/* Get our name */
	if((g_myname = strrchr(argv[0], '/')) == NULL)
	{
		g_myname = argv[0];
	}
	else
	{
		/* Skip the "/" */
		g_myname++;
	}

	opterr = 0;     /* Don't want getopt() writing to stderr */
	while((c = getopt(argc, argv, "n:i:ph")) != EOF)
	{
		switch(c)
		{
			case 'n':
					num = atoi(optarg);
					break;
			case 'i':
					iterations = atoi(optarg);
	 				break;
			case 'p':
					s_print_only = 1;
	 				break;
			case 'h':
			case '?':
			default:
					error_flag++;
					break;
		}
	}

	if(error_flag > 0)
	{
		usage(g_myname);
		exit(-1);
	}

	/* Initialize random generator */
	srandom(time(NULL));

	/* Allocate an array for our PIDs */
	if((wait_on_me = calloc(num, sizeof(int))) == NULL)
	{
		fprintf(stderr, "Failed to allocated PID array.\n");
		exit(-1);
	}

	/* Spin of workers */
	for(i = 0; i < num; i++)
	{
		pid = fork();

		switch(pid)
		{
			case -1:
				/* What the f*@# ... */
				perror("fork()");
				status = -1;
				break;
			case 0:
				/* In child */
				/* Wait a random time to get a descent seed for the random */
				/* generator.                                              */
				seed_wait = random() % 1000;	/* Max 1000 ms */
				/* Wait for it ... wait for it ... wait for it */
				MSWAIT(seed_wait);
				/* Get the new time */
				gettimeofday(&tv, NULL);
				/* Re-initialize random generator */
				srandom(tv.tv_usec);
				/* Run the tests */
				status = do_tests(iterations);
				/* Bye bye */
				exit(status);
				break;
			default:
				/* In parent */
				wait_on_me[i] = pid;
				break;
		}
	}

	/* Wait for them to complete */
	for(i = 0; i < num; i++)
	{
		int status;
		if(waitpid(wait_on_me[i], &status, 0) < 0)
		{
			/* Just print something and keep on waiting on the other guys */
			perror("waitpid()");
		}
		printf("Process %d exited with %d.\n", i, WEXITSTATUS(status));
	}

	exit(status);
}
