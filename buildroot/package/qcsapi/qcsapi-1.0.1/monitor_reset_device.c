/*SH0
*******************************************************************************
**                                                                           **
**         Copyright (c) 2009 - 2011 Quantenna Communications Inc            **
**                                                                           **
**  File        : monitor_reset_device.c                                     **
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


typedef enum {
	e_reset_no_action = 1,
	e_reset_reboot,
	e_reset_default_config,
	e_no_such_reset_action = 0
} reset_device_action;

enum {
	count_no_action = 1,
	count_just_reboot = 5,
	count_proceed_now = 6
};


static FILE *
open_console( void )
{
	return( fopen( "/dev/console", "w" ) );
}
 
static void
respond_reset_device( uint8_t reset_device_pin, uint8_t original_level )
{
	reset_device_action	 current_action = e_no_such_reset_action;
	unsigned int		 cycle_count = 0;
	uint8_t			 current_level = original_level;
	int			 ival = 0, complete = 0;
	FILE			*console_fh = open_console();

	openlog("reset device", LOG_CONS | LOG_NDELAY, LOG_DAEMON);

	do
	{
		sleep( 1 );

		ival = qcsapi_led_get( reset_device_pin, &current_level );
	  /*
	   * Sanity check - the LED / GPIO pin should be valid
	   */
		if (ival < 0)
		{
			complete = 1;
		}
		else if (current_level != original_level || cycle_count > count_proceed_now)
		{
			complete = 1;

			if (cycle_count < count_no_action)
			  current_action = e_reset_no_action; 
			else if (cycle_count < count_just_reboot)
			  current_action = e_reset_reboot;
			else
			  current_action = e_reset_default_config;
		}
		else
		{
			cycle_count++;
		}
	}
	while (complete == 0);

	switch (current_action)
	{
	  case e_reset_default_config:
		syslog(LOG_ALERT, "Reset to default configuration\n" );
		if (console_fh)
			fprintf( console_fh, "Monitor reset device: reset to default configuration\n" );
		system( RESTORE_DEFAULT_CONFIG );
	    /*
	     *  Fall thru.
	     */

	  case e_reset_reboot:
		syslog(LOG_ALERT, "Rebooting the device\n" );
		closelog();

		if (console_fh)
			fprintf( console_fh, "Monitor reset device: rebooting the device\n" );
		fclose( console_fh );
	    /*
	     * This call to system should never return ...
	     *
	     * When you look into the C-callable API that reboots Linux,
	     * you will most likely agree it is easier to effect the reboot
	     * thru a child process.
	     */
		system( "reboot" );
		break;

	  case e_reset_no_action:
	  default:
		syslog(LOG_ALERT, "No action\n" );
		if (console_fh)
			fprintf( console_fh, "Monitor reset device: no action\n" );
		break;
	}

	closelog();
	fclose( console_fh );
}

int
main( int argc, char *argv[] )
{
	int			retval;
	uint8_t			reset_device_gpio_pin, active_logic;
	qcsapi_gpio_config	reset_device_gpio_config;

	if (argc < 3)
	{
		fprintf( stderr, "%s requires a parameter, the Reset Device GPIO pin number\n", argv[ 0 ] );
		fprintf( stderr, "and a parameter to specify whether it is active high (1) or active low (0)\n" );
		return( 1 );
	}

	reset_device_gpio_pin = (uint8_t) atoi( argv[ 1 ] );
	active_logic = (uint8_t) atoi( argv[ 2 ] );

	retval = qcsapi_gpio_get_config( reset_device_gpio_pin, &reset_device_gpio_config );
	if (retval < 0)
	{
		fprintf( stderr, "Error getting the GPIO configuration for pin %d\n", reset_device_gpio_pin );
		return( EXIT_FAILURE );
	}
	else if (reset_device_gpio_config == 0)
	{
		fprintf( stderr, "GPIO pin %d not configured\n", reset_device_gpio_pin );
		return( EXIT_FAILURE );
	}

	/*
	 * Disconnect from the controlling terminal.
	 * For debugging recommend removing the call to this API.
	 */

	qcsapi_console_disconnect();
	
	/*
	 * This program continues until qcsapi_gpio_monitor_reset_device fails.
	 */
	while (retval >= 0)
	{
		retval = qcsapi_gpio_monitor_reset_device(
			reset_device_gpio_pin,
			active_logic,
			1,				/* always block until the reset button is pressed */
			respond_reset_device
		);

		if (retval < 0)
		{
			openlog("monitor reset device", LOG_CONS | LOG_NDELAY, LOG_DAEMON);
			syslog(LOG_ERR, "Monitor reset device API returned error code %d\n", retval );
			closelog();
		}
		else
		{
			sleep( 1 );
		}
	}
  /*
   * If control come here, an error occurred.  Exit with error status.
   */
	return( EXIT_FAILURE );
}
