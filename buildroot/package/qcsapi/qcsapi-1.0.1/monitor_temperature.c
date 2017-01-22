/*SH0
*******************************************************************************
**                                                                           **
**         Copyright (c) 2009 - 2015 Quantenna Communications Inc            **
**                                                                           **
**  File        : monitor_temperature.c                                      **
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
#include "qcsapi.h"

#include <sys/ioctl.h>
#include <net/if.h>
#include <net80211/ieee80211_ioctl.h>
#include "wireless.h"

#ifndef IW_CUSTOM_MAX
#define IW_CUSTOM_MAX 256
#endif

#define TEMP_MON "MONITOR_TEMP: "
#define GPIO_NONDEF  0xFF

int
main(int argc, char *argv[])
{
	int		retval = 0;
	uint8_t		temp_warn_gpio_pin = GPIO_NONDEF;
	int		active_logic = 1;
	float		temp_warn_bbic;
	float		temp_warn_rfic;
	int		temperature_normal = 1;
	char		ifname[IFNAMSIZ];
	int		ioctl_sock = 0;

	qcsapi_gpio_config	temp_warn_gpio_config;

	if (argc < 3 || argc == 4 || argc > 5) {
		fprintf(stderr, "syntax\n%s <BB temp threshold> <RF temp threshold> "
			"[<GPIO number> <GPIO output level normal>]\n\n"
			"<BB temp threshold>          baseband warning temperature (Celsius)\n"
			"<RF temp threshold>          radio IC warning temperature (Celsius)\n"
			"<GPIO number>                GPIO output pin number\n"
			"<GPIO output level normal>   GPIO output level when temperature normal "
			"(converse will be output when temperature above warning threshold)\n",
			argv[0]);
		return(EXIT_FAILURE);
	}

	temp_warn_bbic = atof(argv[1]);
	temp_warn_rfic = atof(argv[2]);

	if (argc == 5) {
		/* (optional) GPIO config parameters */
		temp_warn_gpio_pin = (uint8_t) atoi(argv[3]);
		active_logic = atoi(argv[4]);

		if (active_logic != 0 && active_logic != 1) {
			fprintf(stderr, TEMP_MON"GPIO output logic level must be 0 or 1\n");
			return(EXIT_FAILURE);
		}
	}

	if (temp_warn_gpio_pin != GPIO_NONDEF) {
		retval = qcsapi_gpio_get_config(temp_warn_gpio_pin, &temp_warn_gpio_config);

		if (retval < 0) {
			fprintf(stderr, TEMP_MON"error getting the GPIO configuration for "
				 "pin %d\n", temp_warn_gpio_pin );
			return(EXIT_FAILURE);
		} else if (temp_warn_gpio_config != qcsapi_gpio_output) {
			fprintf(stderr, TEMP_MON"GPIO pin %d not configured as output\n",
				temp_warn_gpio_pin);
			return(EXIT_FAILURE);
		}
	}

	ioctl_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (ioctl_sock < 0) {
		fprintf(stderr, TEMP_MON"unable to open socket (%d)\n", ioctl_sock);
		return(EXIT_FAILURE);
	}

	retval = qcsapi_get_primary_interface(ifname, IFNAMSIZ);

	/*
	* Disconnect from the controlling terminal.
	* For debugging recommend removing the call to this API.
	*/
	qcsapi_console_disconnect();

	/*
	* Set output GPIO to "normal" - even if temperature is too high at the very start,
	* we need to do a transition from "normal" to "bad", so have to at least set normal
	*/
	if (retval >= 0 && temp_warn_gpio_pin != GPIO_NONDEF) {
		retval = qcsapi_led_set(temp_warn_gpio_pin, active_logic ? 1 : 0);
		sleep(1);
	}

	/*
	 * This program continues until some function call fails (which it should not...)
	 */
	while (retval >= 0) {
		int temp_scaled_ext;
		int temp_scaled_int;
		int temp_scaled_bb_int;

		retval = qcsapi_get_temperature_info(&temp_scaled_ext, &temp_scaled_int,
						     &temp_scaled_bb_int);

		if (retval >= 0) {
			float temp_int = temp_scaled_int / 1000000.0;
			float temp_bb_int = temp_scaled_bb_int / 1000000.0;

			if (temp_bb_int >= temp_warn_bbic || temp_int >= temp_warn_rfic) {
				/* temperature_normal = 0 is sticky and will never be reset to 1 */
				temperature_normal = 0;
			}

			if (temperature_normal) {
				/* temperature below threshold */
				if (temp_warn_gpio_pin != GPIO_NONDEF) {
					retval = qcsapi_led_set(temp_warn_gpio_pin,
								active_logic ? 1 : 0);
				}

				/* 20 seconds monitoring interval */
				sleep(20);
			} else {
				char message[IW_CUSTOM_MAX];

				snprintf(message, IW_CUSTOM_MAX,
					 "TEMPERATURE THRESHOLD WAS REACHED "
					 "bb temp %.1f, bb temp threshold %.1f / "
					 "rf temp %.1f, rf temp threshold %.1f\n",
					 temp_bb_int, temp_warn_bbic,
					 temp_int, temp_warn_rfic);

				if (ioctl_sock >= 0) {
					struct iwreq iwr;

					memset(&iwr, 0, sizeof(iwr));
					strncpy(iwr.ifr_ifrn.ifrn_name, ifname, IFNAMSIZ);
					iwr.u.data.pointer = message;
					iwr.u.data.length = strlen(message);
					ioctl(ioctl_sock, IEEE80211_IOCTL_POSTEVENT, &iwr);
				}

				/* temperature above threshold */
				openlog(NULL, LOG_CONS | LOG_NDELAY, LOG_DAEMON);
				syslog(LOG_ERR, TEMP_MON"%s", message);
				closelog();

				/*
				 * turn-off wifi service - if STA are associated, this will
				 * result in disassociation, and host will be informed as
				 * consequence if wakeup-wireless-lan is configured.
				 * Don't care about retval return status for this, as no other
				 * recovery is possible.
				 */
				qcsapi_wifi_rfenable(0);

				/*
				 * allow brief time for rfenable 0 to complete and give delay to
				 * loop
				 */
				sleep(2);

				if (temp_warn_gpio_pin != GPIO_NONDEF) {
					/*
					 * Transition GPIO. Don't care about retval return status
					 * for this, as no other recovery is possible.
					 */
					qcsapi_led_set(temp_warn_gpio_pin, active_logic ? 0 : 1);
				}

				/* repeat forever.... */
			}
		}
	}
	openlog(NULL, LOG_CONS | LOG_NDELAY, LOG_DAEMON);
	syslog(LOG_ERR, TEMP_MON"exit due to error %d\n", retval);
	closelog();

	if (ioctl_sock >= 0) {
		close(ioctl_sock);
	}
	/*
	 * If control come here, an error occurred.  Exit with error status.
	 */
	return(EXIT_FAILURE);
}
