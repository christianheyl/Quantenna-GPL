#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <netinet/ether.h>

typedef enum {
	e_qwe_param_specific = 1,
	e_qwe_param_macaddr,
	e_qwe_param_index,
	e_qwe_param_wpspin,
	e_qwe_param_non_special,
	e_qwe_param_variable,
	e_qwe_param_none
} qwe_param_type;

typedef enum {
	e_qwe_output_string = 1,
	e_qwe_output_none
} qwe_output_type;

typedef enum {
	e_qwe_handle_qweconfig = 1,
	e_qwe_handle_qweaction,
	e_qwe_handle_ap_num,
	e_qwe_handle_ap_info,
	e_qwe_handle_sta_num,
	e_qwe_handle_sta_info
} qwe_handle_type;

typedef struct qwe_param {
	qwe_param_type type;
	const char *param;
} qwe_param;

typedef struct qwe_cmd_entry{
	const char *cmd;
	const qwe_param param;
	const qwe_param sub_param;
	const qwe_handle_type handle;
	const qwe_output_type output_type;
} qwe_cmd_entry;

static const qwe_cmd_entry qweconfig_cmd_list[] = {
	{"get", {e_qwe_param_non_special, "<name>"}, {e_qwe_param_none, ""}, e_qwe_handle_qweconfig, e_qwe_output_string},
	{"set", {e_qwe_param_non_special, "<name>"}, {e_qwe_param_variable, "<value>"}, e_qwe_handle_qweconfig, e_qwe_output_none},
	{"deny_mac", {e_qwe_param_macaddr, "<mac>"}, {e_qwe_param_none, ""}, e_qwe_handle_qweconfig, e_qwe_output_none},
	{"allow_mac", {e_qwe_param_macaddr, "<mac>"}, {e_qwe_param_none, ""}, e_qwe_handle_qweconfig, e_qwe_output_none},
	{"default", {e_qwe_param_none, ""}, {e_qwe_param_none, ""}, e_qwe_handle_qweconfig, e_qwe_output_none},
	{NULL, {e_qwe_param_none, NULL}, {e_qwe_param_none, NULL}, e_qwe_handle_qweconfig, e_qwe_output_none},
};

static const qwe_cmd_entry qweaction_cmd_list[] = {
	{"commit", {e_qwe_param_none, ""}, {e_qwe_param_none, ""}, e_qwe_handle_qweaction, e_qwe_output_none},
	{"down", {e_qwe_param_none, ""}, {e_qwe_param_none, ""}, e_qwe_handle_qweaction, e_qwe_output_none},
	{"up", {e_qwe_param_none, ""}, {e_qwe_param_none, ""}, e_qwe_handle_qweaction, e_qwe_output_none},
	{"ap_num", {e_qwe_param_none, ""}, {e_qwe_param_none, ""}, e_qwe_handle_ap_num, e_qwe_output_string},
	{"ap_info", {e_qwe_param_index, "<index>"}, {e_qwe_param_none, ""}, e_qwe_handle_ap_info, e_qwe_output_string},
	{"status", {e_qwe_param_specific, "version"}, {e_qwe_param_none, ""}, e_qwe_handle_qweaction, e_qwe_output_string},
	{"status", {e_qwe_param_specific, "channel"}, {e_qwe_param_none, ""}, e_qwe_handle_qweaction, e_qwe_output_string},
	{"status", {e_qwe_param_specific, "bandwidth"}, {e_qwe_param_none, ""}, e_qwe_handle_qweaction, e_qwe_output_string},
	{"status", {e_qwe_param_specific, "wps"}, {e_qwe_param_none, ""}, e_qwe_handle_qweaction, e_qwe_output_string},
	{"status", {e_qwe_param_specific, "rootap"}, {e_qwe_param_none, ""}, e_qwe_handle_qweaction, e_qwe_output_string},
	{"status", {e_qwe_param_non_special, "<interface>"}, {e_qwe_param_none, ""}, e_qwe_handle_qweaction, e_qwe_output_string},
	{"wps", {e_qwe_param_specific, "pbc"}, {e_qwe_param_none, ""}, e_qwe_handle_qweaction, e_qwe_output_none},
	{"wps", {e_qwe_param_specific, "stop"}, {e_qwe_param_none, ""}, e_qwe_handle_qweaction, e_qwe_output_none},
	{"wps", {e_qwe_param_specific, "get_state"}, {e_qwe_param_none, ""}, e_qwe_handle_qweaction, e_qwe_output_string},
	{"wps", {e_qwe_param_wpspin, "<pin>"}, {e_qwe_param_none, ""}, e_qwe_handle_qweaction, e_qwe_output_none},
	{"list", {e_qwe_param_specific, "capability"}, {e_qwe_param_none, ""}, e_qwe_handle_qweaction, e_qwe_output_string},
	{"list", {e_qwe_param_specific, "allchannels"}, {e_qwe_param_none, ""}, e_qwe_handle_qweaction, e_qwe_output_string},
	{"sta_num", {e_qwe_param_non_special, "<interface>"}, {e_qwe_param_none, ""}, e_qwe_handle_sta_num, e_qwe_output_string},
	{"sta_info", {e_qwe_param_index, "<index>"}, {e_qwe_param_none, ""}, e_qwe_handle_sta_info, e_qwe_output_string},
	{"stats", {e_qwe_param_non_special, "<interface>"}, {e_qwe_param_none, ""}, e_qwe_handle_qweaction, e_qwe_output_string},
	{"showhwaddr", {e_qwe_param_non_special, "<interface>"}, {e_qwe_param_none, ""}, e_qwe_handle_qweaction, e_qwe_output_string},
	{NULL, {e_qwe_param_none, NULL}, {e_qwe_param_none, NULL}, e_qwe_output_none},
};

const static char qweconfig_usage[] = "\
qweconfig <command> [<param>] [<value>]\n\
Commands:\n\
	set <param> <value>	: set the value to param\n\
	get <param>		: get the value of param\n\
	deny_mac <mac>		: deny one device\n\
	allow_mac <mac>		: allow one device\n\
	default			: set all of the config to default";

const static char qweaction_usage[] = "\
qweaction <device> <command> [<argument>]\n\
Devices:\n\
	wlan0|wlan1		: wlan device\n\
Commands:\n\
	down			: down wlan\n\
	up			: up wlan\n\
	commit			: take effect settings\n\
	ap_num			: scan and show num of AP detected\n\
	ap_info <index>		: show info of specified AP\n\
	status <argument>	: show current status\n\
		version		:: show version\n\
		channel		:: show channel\n\
		bandwidth	:: show bandwidth\n\
		wps		:: show WPS process status\n\
		rootap		:: show info of root AP\n\
		<interface>	:: show interface status\n\
	wps <argument>		: wps action\n\
		pbc		:: start WPS PBC method\n\
		<pinnumber>	:: start WPS PIN method\n\
		stop		:: stop WPS process\n\
		get_state	:: get runtime WPS state\n\
	list <argument>		: list more info\n\
		capability	:: list capability\n\
		allchannels	:: list all channels\n\
	sta_num <interface>	: show number of assocaited station\n\
	sta_info <index>	: show info of specified station\n\
	stats <interface>	: show statistic\n\
	showhwaddr <interface>	: show HW address of interface";

static inline int str_printable(const char *str)
{
	char c;

	if (!str)
		return 1;
	while ((c = *str++) != '\0')
		if (!isprint(c))
			return 0;
	return 1;
}

static inline int str_non_special(const char *str)
{
	char c;

	if (!str)
		return 1;
	while ((c = *str++) != '\0')
		if (!isalnum(c) && c != '.' && c != '_')
			return 0;
	return 1;
}

static inline int validate_pin_num(unsigned long pin)
{
	unsigned long accum = 0;

	accum += 3 * ((pin / 10000000) % 10);
	accum += 1 * ((pin / 1000000) % 10);
	accum += 3 * ((pin / 100000) % 10);
	accum += 1 * ((pin / 10000) % 10);
	accum += 3 * ((pin / 1000) % 10);
	accum += 1 * ((pin / 100) % 10);
	accum += 3 * ((pin / 10) % 10);
	accum += 1 * ((pin / 1) % 10);

	return (0 == (accum % 10));
}

static inline int validate_pin_str(const char *str_pin)
{
	unsigned long pin;
	char *end;

	if (!str_pin || strlen(str_pin) != 8)
		return 0;

	pin = strtoul(str_pin, &end, 10);
	if (!end != '\0')
		return 0;

	return validate_pin_num(pin);
}

/* return 1 if the the input param match the required param, return 0 for other case */
static int match_param(const char *param, const qwe_param *require)
{
	const char *p;

	switch (require->type) {
	case e_qwe_param_specific:
		return (param != NULL && !strcasecmp(require->param, param));
	case e_qwe_param_macaddr:
		return (param != NULL && strlen(param) == 17 && ether_aton(param) != NULL);
	case e_qwe_param_index:
		if (param == NULL || param[0] == '\0')
			return 0;
		for (p = param; isdigit(*p) && *p != '\0'; p++)
			;
		return *p == '\0';
	case e_qwe_param_wpspin:
		return validate_pin_str(param);
	case e_qwe_param_non_special:
		return (param != NULL && str_non_special(param));
	case e_qwe_param_variable:
		return (param != NULL && param[0] != '\0');
	case e_qwe_param_none:
		return (param == NULL);
	default:
		return 0;
	}
	return 0;
}

#define CATCH_STDOUT 0x1
#define CATCH_STDERR 0x2
/* Advanced system function that can get the output and return status of the command */
static int system_adv(const char *cmd, const int catch, char *output, const unsigned int max_len)
{
	int fd[2];
	int fd_stdout_bak = 0, fd_stdout_new = 0;
	int fd_stderr_bak = 0, fd_stderr_new = 0;
	int result, len;
	char *tail;

	if (!output || catch == 0)
		return system(cmd);

	if (pipe(fd))
		return -1;

	if (catch & CATCH_STDOUT) {
		fd_stdout_bak = dup(STDOUT_FILENO);
		fd_stdout_new = dup2(fd[1], STDOUT_FILENO);
	}
	if (catch & CATCH_STDERR) {
		fd_stderr_bak = dup(STDERR_FILENO);
		fd_stderr_new = dup2(fd[1], STDERR_FILENO);
	}

	result = system(cmd);
	/* print a newline for the case that reading pipe hungs when the cmd don't have any output */
	if (catch & CATCH_STDOUT)
		printf("\n");
	else if (catch & CATCH_STDERR)
		fprintf(stderr, "\n");
	len = read(fd[0], output, max_len - 1);

	if (catch & CATCH_STDOUT)
		dup2(fd_stdout_bak, fd_stdout_new);
	if (catch & CATCH_STDERR)
		dup2(fd_stderr_bak, fd_stderr_new);

	if (len <= 0) {
		output[0] = '\0';
		return result;
	} else
		output[len] = '\0';

	/* remove '\n' from the tail */
	tail = output + len - 1;
	while (tail >= output && *tail == '\n')
		*tail-- = '\0';

	return result;
}

static int qwe_handle_config_and_action(const char *cmd, const char *param1, const char *param2, const char *param3, char *output, const unsigned int max_len, qwe_output_type output_type)
{
	char qwecmd[256];
	int result;
	char safe_param3[128];
	char *p = safe_param3;
	char c;

	if (param3 != NULL) {
		*p++ = '"';
		while ((c = *param3++) != '\0' && (p - safe_param3 < sizeof(safe_param3) - 4)) {
			if (c == '$' || c == '`' || c == '"')
				*p++ = '\\';
			*p++ = c;
		}
		if (c != '\0')
			return -EFAULT;
		*p++ = '"';
		*p = '\0';
	}

	snprintf(qwecmd, sizeof(qwecmd), "%s%s%s%s%s%s%s", cmd,
		 param1 == NULL ? "" : " ", param1 == NULL ? "" : param1,
		 param2 == NULL ? "" : " ", param2 == NULL ? "" : param2,
		 param3 == NULL ? "" : " ", param3 == NULL ? "" : safe_param3);

	result = system_adv(qwecmd, CATCH_STDOUT | CATCH_STDERR, output, max_len);
	if (output) {
		/* put "complete" to the output if command is performed successed and the command does not output useful message */
		if (result == 0 && output_type == e_qwe_output_none)
			snprintf(output, max_len, "complete");
		if (result != 0) {
			/* tell user something if command is performed unsuccessfully without any output */
			if (output[0] == '\0')
				snprintf(output, max_len, "Fail to execute \"%s\"", qwecmd);
			result = 1;
		} else if (output_type != e_qwe_output_none && output[0] == '\0') {
			/* tell user something if command expect output but get nothing */
			snprintf(output, max_len, "Don't catch any output for command \"%s\"", qwecmd);
			result = 1;
		}
		return result;
	} else if (result == 0 && output_type == e_qwe_output_none)
		return 0;

	return -EINVAL;
}

#define AP_INFO_FILE "/tmp/.qweapinfo"
#define STA_INFO_FILE "/tmp/.qwestainfo"
static int qwe_handle_ap_sta_num(const qwe_handle_type handle, const char *device, const char *interface, char *output, const unsigned int max_len)
{
	char qwecmd[128];
	char cmd[128];
	char buf[128];
	int result = 0;
	int num = 0;
	char *filename;
	FILE *fp;

	if (handle == e_qwe_handle_ap_num) {
		filename = AP_INFO_FILE;
		snprintf(qwecmd, sizeof(qwecmd), "qweaction %s scan", device);
	} else if (handle == e_qwe_handle_sta_num) {
		filename = STA_INFO_FILE;
		snprintf(qwecmd, sizeof(qwecmd), "qweaction %s stainfo %s", device, interface);
	} else {
		snprintf(output, max_len, "Function %s get unknow handle", __FUNCTION__);
		return 1;
	}

	snprintf(cmd, sizeof(cmd), "echo %lu > %s", time(NULL), filename);
	system(cmd);

	snprintf(cmd, sizeof(cmd), "%s >> %s", qwecmd, filename);

	result = system_adv(cmd, CATCH_STDERR, output, max_len);
	if (result == 0) {
		fp = fopen(filename, "r");
		if (!fp) {
			snprintf(output, max_len, "Fail to open %s", filename);
			return 1;
		}
		while (fgets(buf, sizeof(buf), fp) != NULL)
			if (strncmp(buf, "mac", 3) == 0)
				num++;
		fclose(fp);
		snprintf(output, max_len, "%d", num);
	} else {
		unlink(filename);
		if (output[0] == '\0') {
			/* tell user something if command is performed unsuccessfully without any output */
			snprintf(output, max_len, "Fail to execute \"%s\"", qwecmd);
		}
		result = 1;
	}

	return result;
}

static int qwe_handle_ap_sta_info(const qwe_handle_type handle, const char *device, const char *index, char *output, const unsigned int max_len)
{
#define INFO_EXPIRE_TIME 300
	char qwecmd[128];
	char buf[128];
	time_t timestamp;
	int index_i, num, line;
	int cur_len = 0;
	FILE *fp;
	char *filename;

	if (handle == e_qwe_handle_ap_info) {
		filename = AP_INFO_FILE;
		snprintf(qwecmd, sizeof(qwecmd), "qweaction %s scan", device);
	} else if (handle == e_qwe_handle_sta_info) {
		filename = STA_INFO_FILE;
		snprintf(qwecmd, sizeof(qwecmd), "qweaction %s stainfo <interface>", device);
	} else {
		snprintf(output, max_len, "Function %s get unknow handle", __FUNCTION__);
		return 1;
	}

	fp = fopen(filename, "r");
	if (!fp) {
		snprintf(output, max_len, "Please execute \"%s\" first", qwecmd);
		return 1;
	}

	fscanf(fp, "%lu\n", &timestamp);

	if (time(NULL) > timestamp + INFO_EXPIRE_TIME) {
		snprintf(output, max_len, "The \"%s\" already was operated over %d seconds, please execute again", qwecmd, INFO_EXPIRE_TIME);
		fclose(fp);
		return 1;
	}

	index_i = atoi(index);
	if (index_i <= 0) {
		snprintf(output, max_len, "The index should start from 1");
		fclose(fp);
		return 1;
	}

	for (num = 1, line = 1; num < index_i;) {
		if (fgets(buf, sizeof(buf), fp) == NULL)
			break;
		++line;
		if (strcmp(buf, "\n") == 0)
			++num;
	}
	if (feof(fp)) {
		if (line == 1)
			snprintf(output, max_len, "No object is found in previous, you can execute \"%s\" to try again", qwecmd);
		else
			snprintf(output, max_len, "Only %d objects found in previous, the index you specified is %d", num, index_i);
		fclose(fp);
		return 1;
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if (strcmp(buf, "\n") == 0)
			break;
		cur_len += snprintf(output + cur_len, max_len - cur_len, "%s", buf);
		if (cur_len >= max_len)
			break;
	}
	if (output[cur_len - 1] == '\n')
		output[cur_len - 1] = '\0';

	fclose(fp);
	return 0;
}

int qwe_command(const char *cmd, const char *param1, const char *param2, const char *param3, char *output, const unsigned int max_len)
{
	int index = 0;
	int miss_essential_param = 0;
	const qwe_cmd_entry *entry = NULL;

	if (!cmd)
		return -EINVAL;

	if (!str_printable(cmd) || !str_printable(param1) || !str_printable(param2) || !str_printable(param3))
		return -EINVAL;

	if (!strcasecmp(cmd, "qweconfig")) {
		if (param1 == NULL)
			miss_essential_param = 1;
		while (!miss_essential_param) {
			entry = &qweconfig_cmd_list[index++];
			if (entry->cmd == NULL)
				break;
			if (!strcasecmp(param1, entry->cmd) && match_param(param2, &entry->param) && match_param(param3, &entry->sub_param))
				break;
		}
		if (entry == NULL || entry->cmd == NULL) {
			if (!output)
				return -EINVAL;
			snprintf(output, max_len, "Don't support command \"%s%s%s%s%s%s%s\" or argument error\nUsage: %s", cmd,
				 param1 == NULL ? "" : " ", param1 == NULL ? "" : param1,
				 param2 == NULL ? "" : " ", param2 == NULL ? "" : param2,
				 param3 == NULL ? "" : " ", param3 == NULL ? "" : param3,
				 qweconfig_usage);
			return 1;
		}

	} else if (!strcasecmp(cmd, "qweaction")) {
		if (param1 == NULL || !str_non_special(param1) || param2 == NULL)
			miss_essential_param = 1;
		while (!miss_essential_param) {
			entry = &qweaction_cmd_list[index++];
			if (entry->cmd == NULL)
				break;
			if (!strcasecmp(param2, entry->cmd) && match_param(param3, &entry->param) && match_param(NULL, &entry->sub_param))
				break;
		}
		if (entry == NULL || entry->cmd == NULL) {
			if (!output)
				return -EINVAL;
			snprintf(output, max_len, "Don't support command \"%s%s%s%s%s%s%s\" or argument error\nUsage: %s", cmd,
				 param1 == NULL ? "" : " ", param1 == NULL ? "" : param1,
				 param2 == NULL ? "" : " ", param2 == NULL ? "" : param2,
				 param3 == NULL ? "" : " ", param3 == NULL ? "" : param3,
				 qweaction_usage);
			return 1;
		}
	} else if (!strcasecmp(cmd, "help")) {
		if (!output)
			return -EINVAL;
		snprintf(output, max_len, "qweconfig <command> [<param>] [<value>]\nqweaction <device> <command> [<argument>]");
		return 0;
	} else {
		if (output)
			return -EINVAL;
		snprintf(output, max_len, "Don't support command %s, please use help command to get help", cmd);
		return 1;
	}

	if ((!output || max_len <= 0) && entry->output_type != e_qwe_output_none)
		return -EINVAL;

	switch (entry->handle) {
	case e_qwe_handle_qweconfig:
	case e_qwe_handle_qweaction:
		return qwe_handle_config_and_action(cmd, param1, param2, param3, output, max_len, entry->output_type);
	case e_qwe_handle_ap_num:
	case e_qwe_handle_sta_num:
		return qwe_handle_ap_sta_num(entry->handle, param1, param3, output, max_len);
	case e_qwe_handle_ap_info:
	case e_qwe_handle_sta_info:
		return qwe_handle_ap_sta_info(entry->handle, param1, param3, output, max_len);
	default:
		return -EINVAL;
	}
}

