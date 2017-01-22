/*SH0
*******************************************************************************
**                                                                           **
**         Copyright (c) 2012 Quantenna Communications, Inc.                 **
**                                                                           **
**  File        : monitor_rfenable.c                                         **
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
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <syslog.h>
#include <net/if.h>
#include <call_qcsapi_rpc/client/call_qcsapi_client.h>
#include <call_qcsapi_rpc/generated/call_qcsapi_rpc.h>
#include <qcsapi_rpc_common/client/find_host_addr.h>

#include "qcsapi.h"

static uint8_t gpio_state;

int set_rfenable(int onoff)
{
	const char *host;
	CLIENT *clnt;
	int retval = 0;

	openlog("rfenable", LOG_CONS | LOG_NDELAY, LOG_DAEMON);

	host = client_qcsapi_find_host_addr(NULL, NULL);

	if (host) {
		char *argv[] = { "monitor_rfenable", "-q", "rfenable", onoff ? "1" : "0" };
		clnt = clnt_create(host, CALL_QCSAPI_PROG, CALL_QCSAPI_VERS, "udp");
		if (clnt == NULL) {
			return -1;
		}
		retval = call_qcsapi_rpc_client(clnt, ARRAY_SIZE(argv), argv);
		clnt_destroy(clnt);
	} else {
		syslog(LOG_ALERT, "RF enable: no remote host configured\n");
	}

	syslog(LOG_ALERT, "RF enable state: %d\n", onoff);
	closelog();

	retval = qcsapi_wifi_rfenable(onoff);

	return retval;
}

int get_pin_state(uint8_t rfenable_gpio_pin, uint8_t active_logic)
{
	int rfenable_state;
	int retval;

	retval = qcsapi_led_get(rfenable_gpio_pin, &gpio_state);
	if (retval < 0) {
		fprintf(stderr, "Error getting state for pin %d\n", rfenable_gpio_pin);
		return EXIT_FAILURE;
	}

	if (active_logic) {
		/* Active high */
		rfenable_state = gpio_state;
	} else {
		/* Active low */
		rfenable_state = !gpio_state;
	}

	return set_rfenable(rfenable_state);
}

int main(int argc, char *argv[])
{
	int retval;
	uint8_t rfenable_gpio_pin, active_logic;
	qcsapi_gpio_config rfenable_gpio_config;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <pin> <active_state>", argv[0]);
		fprintf(stderr, "\tParameters:\n"
				"\t\t<pin>               RF enable GPIO pin number\n"
				"\t\t<active_state>      1 for active high, 0 for active low\n");
		return 1;
	}

	rfenable_gpio_pin = (uint8_t) atoi(argv[1]);
	active_logic = (uint8_t) atoi(argv[2]);

	retval = qcsapi_gpio_get_config(rfenable_gpio_pin, &rfenable_gpio_config);

	if (retval < 0) {
		fprintf(stderr,
			"%s: error getting the GPIO configuration for pin %d\n",
			argv[0], rfenable_gpio_pin);
		return EXIT_FAILURE;
	} else if (rfenable_gpio_config == 0) {
		fprintf(stderr, "%s: GPIO pin %d not configured\n",
			argv[0], rfenable_gpio_pin);
		return EXIT_FAILURE;
	}

	while (1) {
		retval = get_pin_state(rfenable_gpio_pin, active_logic);
		if (retval == EXIT_FAILURE) {
			return retval;
		} else if (retval != 0) {
			/* RPC failure, try again */
			sleep(1);
			continue;
		}

		/* Wait for next change */
		retval = qcsapi_gpio_monitor_reset_device(rfenable_gpio_pin, !gpio_state, 1, NULL);

		if (retval < 0) {
			/* Try again */
			sleep(1);
		}
	}

	return 0;
}
