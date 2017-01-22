/*SH0
*******************************************************************************
**                                                                           **
**         Copyright (c) 2011-2014 Quantenna Communications Inc              **
**                                                                           **
**  File        : monitor_wifi.c                                             **
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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>

#include "qcsapi.h"

#include "monitor_wifi.h"

typedef struct {
	struct timeval	blink_interval;
	struct timeval		countdown_value;
	monitor_wifi_led_state	led_current_state;
	const uint8_t		led_number;
	const uint8_t		led_active_state;
	uint8_t			gpio_current_state;
} monitor_wifi_led_entry;

static monitor_wifi_led_entry	monitor_wifi_led_table[] = {
	/* WPS LED */
	{	{ MONITOR_WIFI_WPS_BLINK_INTERVAL_SEC, MONITOR_WIFI_WPS_BLINK_INTERVAL_USEC },
		{ 0, 0 },
		MONITOR_WIFI_LED_NOT_DEFINED,
		MONITOR_WIFI_WPS_LED_NUMBER,
		MONITOR_WIFI_ACTIVE_HIGH,
		0 },
	/* WLAN association LED */
	{	{ MONITOR_WIFI_WLAN_BLINK_INTERVAL_SEC, MONITOR_WIFI_WLAN_BLINK_INTERVAL_USEC },
		{ 0, 0 },
		MONITOR_WIFI_LED_NOT_DEFINED,
		MONITOR_WIFI_WLAN_LED_NUMBER,
		MONITOR_WIFI_ACTIVE_HIGH,
		0 },
	/* Mode AP/STA LED */
	{	{ MONITOR_WIFI_MODE_BLINK_INTERVAL_SEC, MONITOR_WIFI_MODE_BLINK_INTERVAL_USEC },
		{ 0, 0 },
		MONITOR_WIFI_LED_NOT_DEFINED,
		MONITOR_WIFI_MODE_LED_NUMBER,
		MONITOR_WIFI_ACTIVE_HIGH,
		0 },
	/* WPS error LED */
	{	{ MONITOR_WIFI_WPS_BLINK_INTERVAL_SEC, MONITOR_WIFI_WPS_BLINK_INTERVAL_USEC },
		{ 0, 0 },
		MONITOR_WIFI_LED_NOT_DEFINED,
		MONITOR_WIFI_WPS_ERR_LED_NUMBER,
		MONITOR_WIFI_ACTIVE_HIGH,
		0 },
	/* Quality LED */
	{	{ MONITOR_WIFI_QUAL_BLINK_INTERVAL_SEC, MONITOR_WIFI_QUAL_BLINK_INTERVAL_USEC},
		{ 0, 0 },
		MONITOR_WIFI_LED_NOT_DEFINED,
		MONITOR_WIFI_QUAL_LED_NUMBER,
		MONITOR_WIFI_ACTIVE_HIGH,
		0 },
#ifdef TOPAZ_AMBER_IP_APP
	/* Activity LED */
	{	{ MONITOR_WIFI_ACTIVITY_BLINK_INTERVAL_SEC, MONITOR_WIFI_ACTIVITY_BLINK_INTERVAL_USEC },
		{ 0, 0 },
		MONITOR_WIFI_LED_NOT_DEFINED,
		MONITOR_WIFI_ACTIVITY_LED_NUMBER,
		MONITOR_WIFI_ACTIVE_HIGH,
		0 },
	/* Medium Quality LED */
	{	{ MONITOR_WIFI_QUAL_BLINK_INTERVAL_SEC, MONITOR_WIFI_QUAL_BLINK_INTERVAL_USEC},
		{ 0, 0 },
		MONITOR_WIFI_LED_NOT_DEFINED,
		MONITOR_WIFI_QUAL2_LED_NUMBER,
		MONITOR_WIFI_ACTIVE_HIGH,
		0 },
	/* Low Quality LED */
	{	{ MONITOR_WIFI_QUAL_BLINK_INTERVAL_SEC, MONITOR_WIFI_QUAL_BLINK_INTERVAL_USEC},
		{ 0, 0 },
		MONITOR_WIFI_LED_NOT_DEFINED,
		MONITOR_WIFI_QUAL3_LED_NUMBER,
		MONITOR_WIFI_ACTIVE_HIGH,
		0 },
#endif
};

struct monitor_wifi_quality {
	const struct timeval update_interval;
	struct timeval countdown_value;
	int quality_state;
};

static struct monitor_wifi_quality current_link_quality = {
	.update_interval = {1, 0},
	.quality_state = -1
};

enum {
	MONITOR_WIFI_WPS_INDEX = 0,
	MONITOR_WIFI_MIN_INDEX = MONITOR_WIFI_WPS_INDEX,
	MONITOR_WIFI_WLAN_INDEX = 1,
	MONITOR_WIFI_MODE_INDEX = 2,
	MONITOR_WIFI_WPS_ERR_INDEX = 3,
	MONITOR_WIFI_QUAL_INDEX = 4,
#ifndef TOPAZ_AMBER_IP_APP
	MONITOR_WIFI_MAX_INDEX = MONITOR_WIFI_QUAL_INDEX
#else
	MONITOR_WIFI_ACTIVITY_INDEX = 5,
	MONITOR_WIFI_QUAL2_INDEX = 6,
	MONITOR_WIFI_QUAL3_INDEX = 7,
	MONITOR_WIFI_MAX_INDEX = MONITOR_WIFI_QUAL3_INDEX
#endif
};

static uint8_t
get_led_number_by_index( int led_index )
{
	uint8_t	retval = (uint8_t) -1;

	if (led_index >= MONITOR_WIFI_MIN_INDEX && led_index <= MONITOR_WIFI_MAX_INDEX) {
		retval = monitor_wifi_led_table[ led_index ].led_number;
	}

	return( retval );
}

static uint8_t
get_led_active_by_index( int led_index )
{
	uint8_t	retval = (uint8_t) -1;

	if (led_index >= MONITOR_WIFI_MIN_INDEX && led_index <= MONITOR_WIFI_MAX_INDEX) {
		retval = monitor_wifi_led_table[ led_index ].led_active_state;
	}

	return( retval );
}

static int
set_led_current_state( int led_index, monitor_wifi_led_state new_state )
{
	int	retval = -1;

	if (led_index >= MONITOR_WIFI_MIN_INDEX && led_index <= MONITOR_WIFI_MAX_INDEX) {
		monitor_wifi_led_table[ led_index ].led_current_state = new_state;
		retval = 0;
	}

	return( retval );
}

static struct timeval *
get_led_countdown_value( int led_index )
{
	struct timeval	*retaddr = NULL;

	if (led_index >= MONITOR_WIFI_MIN_INDEX && led_index <= MONITOR_WIFI_MAX_INDEX) {
		retaddr = &(monitor_wifi_led_table[ led_index ].countdown_value);
	}

	return( retaddr );
}

static int
set_led_countdown_value( int led_index, const struct timeval *p_new_countdown_value )
{
	int	retval = -1;

	if (led_index >= MONITOR_WIFI_MIN_INDEX && led_index <= MONITOR_WIFI_MAX_INDEX) {
		monitor_wifi_led_table[ led_index ].countdown_value = *p_new_countdown_value;
		retval = 0;
	}

	return( retval );
}

static const struct timeval *
get_led_blink_interval( int led_index )
{
	const struct timeval	*retaddr = NULL;

	if (led_index >= MONITOR_WIFI_MIN_INDEX && led_index <= MONITOR_WIFI_MAX_INDEX) {
		retaddr = &(monitor_wifi_led_table[ led_index ].blink_interval);
	}

	return( retaddr );
}

static uint8_t
get_gpio_current_state( int led_index )
{
	uint8_t	retval = (uint8_t) -1;

	if (led_index >= MONITOR_WIFI_MIN_INDEX && led_index <= MONITOR_WIFI_MAX_INDEX) {
		retval = monitor_wifi_led_table[ led_index ].gpio_current_state;
	}

	return( retval );
}

static int
set_gpio_current_state( int led_index, uint8_t new_gpio_state )
{
	int	retval = -1;

	if (led_index >= MONITOR_WIFI_MIN_INDEX && led_index <= MONITOR_WIFI_MAX_INDEX) {
		monitor_wifi_led_table[ led_index ].gpio_current_state = new_gpio_state;
		retval = 0;
	}

	return( retval );
}

static const struct timeval	fast_interval = { MONITOR_WIFI_FAST_TIME_SEC, MONITOR_WIFI_FAST_TIME_USEC };
static const struct timeval	slow_interval = { MONITOR_WIFI_SLOW_TIME_SEC, MONITOR_WIFI_SLOW_TIME_USEC };

#define  MONITOR_WIFI_LENGTH_ERROR_MESSAGE	60

static int
monitor_wifi_init_leds( void )
{
	int			retval = 0;
	qcsapi_gpio_config	gpio_config = qcsapi_nosuch_gpio_config;
	uint8_t			wps_led_number = get_led_number_by_index( MONITOR_WIFI_WPS_INDEX );
	uint8_t			wlan_led_number = get_led_number_by_index( MONITOR_WIFI_WLAN_INDEX );
#ifndef CUSTOMER_SPEC_RGMII
	uint8_t			mode_led_number = get_led_number_by_index( MONITOR_WIFI_MODE_INDEX );
#endif
	uint8_t			wps_led_active = 0;
	uint8_t			wlan_led_active = 0;
	uint8_t			gpio_state = 0;
	monitor_wifi_led_state	the_led_state =	MONITOR_WIFI_LED_NOT_DEFINED;
	char			qcsapi_error_message[ MONITOR_WIFI_LENGTH_ERROR_MESSAGE ];

	int	status_from_api = qcsapi_gpio_get_config( wps_led_number, &gpio_config );

	if (status_from_api < 0) {
		qcsapi_errno_get_message( status_from_api,
					 &qcsapi_error_message[ 0 ],
				          sizeof( qcsapi_error_message ) );
		fprintf( stderr, "Error accessing LED for WPS %d: %s\n", wps_led_number, &qcsapi_error_message[ 0 ] );
		retval = status_from_api;
	} else if (gpio_config != qcsapi_gpio_output) {
		fprintf( stderr, "LED for WPS %d is not configured for output\n", wps_led_number );
		retval = -qcsapi_configuration_error;
	}

	status_from_api = qcsapi_gpio_get_config( wlan_led_number, &gpio_config );

	if (status_from_api < 0) {
		qcsapi_errno_get_message( status_from_api,
					 &qcsapi_error_message[ 0 ],
				          sizeof( qcsapi_error_message ) );
		fprintf( stderr, "Error accessing WLAN LED %d: %s\n", wlan_led_number, &qcsapi_error_message[ 0 ] );
		retval = status_from_api;
	} else if (gpio_config != qcsapi_gpio_output) {
		fprintf( stderr, "WLAN LED %d is not configured for output\n", wlan_led_number );
		retval = -qcsapi_configuration_error;
	}

#ifndef CUSTOMER_SPEC_RGMII
	status_from_api = qcsapi_gpio_get_config( mode_led_number, &gpio_config );

	if (status_from_api < 0) {
		qcsapi_errno_get_message( status_from_api,
					 &qcsapi_error_message[ 0 ],
				          sizeof( qcsapi_error_message ) );
		fprintf( stderr, "Error accessing Wifi mode LED %d: %s\n", mode_led_number, &qcsapi_error_message[ 0 ] );
		retval = status_from_api;
	} else if (gpio_config != qcsapi_gpio_output) {
		fprintf( stderr, "Wifi mode LED %d is not configured for output\n", mode_led_number );
		retval = -qcsapi_configuration_error;
	}
#endif

	if (retval < 0) {
		return( retval );
	}

	/* Ignore errors from these APIs - should definitely be OK */

	qcsapi_led_get( wps_led_number, &gpio_state );
	set_gpio_current_state(MONITOR_WIFI_WPS_INDEX, gpio_state);

	wps_led_active = get_led_active_by_index(MONITOR_WIFI_WPS_INDEX);

	the_led_state = (gpio_state == wps_led_active) ? MONITOR_WIFI_LED_ON : MONITOR_WIFI_LED_OFF;

	set_led_current_state( MONITOR_WIFI_WPS_INDEX, the_led_state );

	qcsapi_led_get( wlan_led_number, &gpio_state );
	set_gpio_current_state( MONITOR_WIFI_WLAN_INDEX, gpio_state );

	wlan_led_active = get_led_active_by_index(MONITOR_WIFI_WLAN_INDEX);

	the_led_state = (gpio_state == wlan_led_active) ? MONITOR_WIFI_LED_ON : MONITOR_WIFI_LED_OFF;

	set_led_current_state( MONITOR_WIFI_WLAN_INDEX, the_led_state );

	return( retval );
}

static int
monitor_wifi_invert_led( const int led_index, const uint8_t led_number )
{
	int	retval = 0;
	uint8_t	current_gpio_state = get_gpio_current_state( led_index );

	if (current_gpio_state == (uint8_t) -1) {
		return -qcsapi_programming_error;
	}

	if (retval >= 0) {
		current_gpio_state = !current_gpio_state;
		retval = qcsapi_led_set( led_number, current_gpio_state );
		set_gpio_current_state( led_index, current_gpio_state );
	}

	return( retval );
}

static int
monitor_wifi_blink_led( const int led_index,
			const uint8_t led_number,
			const struct timeval *p_time_decrement )
{
	int			 retval = 0;
	int			 change_led = 0;
	const struct timeval	*p_blink_interval = get_led_blink_interval( led_index );
	struct timeval		*p_current_countdown = get_led_countdown_value( led_index );
	struct timeval		 new_countdown_value = { 0, 0 };
	/*
	 * Sanity check - LED index should be valid ...
	 */
	if (p_blink_interval == NULL || p_current_countdown == NULL) {
		fprintf( stderr, "Address of a timeval is NULL in blink LED\n" );
		return( -qcsapi_programming_error );
	} else if (!timerisset( p_blink_interval )) {
		fprintf( stderr, "LED with index %d (GPIO pin %d) not configured for blinking\n", led_index, led_number );
		return( -qcsapi_programming_error );
	}
	/*
	 * from /usr/include/sys/timer.h:
	 * NOTE: `timercmp' does not work for >= or <=.
	 */
	if (timercmp( p_current_countdown, p_time_decrement, == ) ||
	    timercmp( p_current_countdown, p_time_decrement, < )) {
		change_led = 1;
	}

	if (change_led) {
		retval = monitor_wifi_invert_led( led_index, led_number );
		new_countdown_value = *p_blink_interval;
	} else {
		timersub( p_current_countdown, p_time_decrement, &new_countdown_value );
	}

	set_led_countdown_value( led_index, &new_countdown_value );

	return( retval );
}

/*
 * Turn LED on or off ...
 */
static int
monitor_wifi_switch_led( const int led_index, const uint8_t led_number, const monitor_wifi_led_state new_led_state )
{
	int	retval = 0;
	uint8_t	current_gpio_state = get_gpio_current_state( led_index );
	uint8_t	led_active_state = get_led_active_by_index( led_index );

	if (current_gpio_state == (uint8_t) -1) {
		return -qcsapi_programming_error;
	}

	if (new_led_state == MONITOR_WIFI_LED_OFF) {
		if (current_gpio_state == led_active_state) {
			retval = monitor_wifi_invert_led(led_index, led_number);
		}
	}
	else if (new_led_state == MONITOR_WIFI_LED_ON) {
		if (current_gpio_state != led_active_state) {
			retval = monitor_wifi_invert_led(led_index, led_number);
		}
	}
	else {
		fprintf( stderr, "New LED state is neither off nor on in switch LED\n" );
		retval = -qcsapi_programming_error;
	}

	if (retval >= 0) {
		struct timeval	timer_is_zero = { 0, 0 };

		retval = set_led_countdown_value( led_index, &timer_is_zero );
	}

	return (retval );
}

static int
monitor_wifi_update_led( const int led_index,
			 const monitor_wifi_led_state new_led_state,
			 const struct timeval *p_time_decrement)
{
	int	retval = 0;
	uint8_t	led_number = get_led_number_by_index( led_index );

	if (led_number == (uint8_t) -1) {
		return -qcsapi_programming_error;
	}

	switch (new_led_state) {
	  case MONITOR_WIFI_LED_BLINK:
		retval = monitor_wifi_blink_led( led_index, led_number, p_time_decrement );
		break;

	  case MONITOR_WIFI_LED_OFF:
	  case MONITOR_WIFI_LED_ON:
		retval = monitor_wifi_switch_led( led_index, led_number, new_led_state );
		break;

	  case MONITOR_WIFI_LED_NOT_DEFINED:
	  default:
		retval = -qcsapi_programming_error;
		break;
	}

	return retval;
}

static void
monitor_wifi_sigalrm_handler( int signal_num, siginfo_t *p_siginfo, void *p_ucontext )
{
	(void) signal_num;
	(void) p_siginfo;
	(void) p_ucontext;
}

static int
monitor_wifi_set_slow_timer()
{
	int			ival;
	struct itimerval	slow_timer;

	slow_timer.it_interval.tv_sec = 0;
	slow_timer.it_interval.tv_usec = 0;
	slow_timer.it_value = slow_interval;

	ival = setitimer(ITIMER_REAL, &slow_timer, NULL);

	return( ival );
}

static int
monitor_wifi_set_fast_timer()
{
	int			ival;
	struct itimerval	fast_timer;

	fast_timer.it_interval.tv_sec = 0;
	fast_timer.it_interval.tv_usec = 0;
	fast_timer.it_value = fast_interval;

	ival = setitimer(ITIMER_REAL, &fast_timer, NULL);

	return( ival );
}

#define LOCAL_WPS_STATE_LEN	20
#define LOCAL_WPA_STATUS_LEN 20
#define ZERO_MAC_ADDR "00:00:00:00:00:00"

static int
monitor_wifi_update_wlan( const char *vap,
			  const qcsapi_wifi_mode current_wifi_mode,
			  const struct timeval *p_decrement)
{
	int	retval = 0;
	int	in_association = 0;
#ifdef CUSTOMER_SPEC_RGMII
	char			wps_state_str[ LOCAL_WPS_STATE_LEN ];
	int			wps_state_value = MONITOR_WIFI_WPS_UNKNOWN;
	char			wpa_status_str[ LOCAL_WPA_STATUS_LEN ];
	qcsapi_unsigned_int		wlan_rx_bytes, wlan_tx_bytes, wlan_bytes_total;
	static	qcsapi_unsigned_int	wlan_bytes_total_old = 0;
	monitor_wifi_led_state		new_wlan_state = MONITOR_WIFI_LED_STATE_NO_TRANSFER;
#endif

	if (current_wifi_mode == qcsapi_access_point) {
		qcsapi_unsigned_int	association_count = 0;

		retval = qcsapi_wifi_get_count_associations( vap, &association_count );

		if (retval >= 0) {
			in_association = (association_count > 0 );
		}
	} else {
		qcsapi_mac_addr	all_zeros = { 0, 0, 0, 0, 0, 0 };
		qcsapi_mac_addr	current_bssid = { 0, 0, 0, 0, 0, 0 };

		retval = qcsapi_wifi_get_BSSID( vap, current_bssid );

		if (retval >= 0) {
			in_association = (memcmp( all_zeros, current_bssid, sizeof( current_bssid ) ) != 0);
		}
	}
#ifndef CUSTOMER_SPEC_RGMII
	if (retval >= 0) {
		monitor_wifi_led_state	new_wlan_state = MONITOR_WIFI_LED_STATE_NOT_ASSOCIATED;

		if (in_association) {
			new_wlan_state = MONITOR_WIFI_LED_STATE_ASSOCIATED;
		}

		retval = monitor_wifi_update_led( MONITOR_WIFI_WLAN_INDEX, new_wlan_state, p_decrement );
		if (retval < 0) {
			fprintf( stderr, "Cannot update the WLAN LED\n" );
		}
	}
#else
	if (retval >= 0) {
		if (!in_association) {
			/*
			 * If no station connects to AP, the WPS handshaking and WPA handshaking will
			 * be checked here, and the wlan led will blink according to the related status.
			 */
			retval = qcsapi_wps_get_state( vap, &wps_state_str[ 0 ], sizeof( wps_state_str ) );
			if (retval >= 0) {
				wps_state_value = atoi( &wps_state_str[ 0 ] );
				if (wps_state_value == MONITOR_WIFI_WPS_IN_PROGRESS) {
					new_wlan_state = MONITOR_WIFI_LED_STATE_TRANSFER;
					monitor_wifi_led_table[MONITOR_WIFI_WLAN_INDEX].blink_interval.tv_usec =
						MONITOR_WIFI_WLAN_HANDSHAKING_BLINK_INTERVAL_USEC;
					goto wlan_led_blink;
				}
			} else {
				fprintf(stderr, "Cannot get wps state, returns %d\n", retval);
				return retval;
			}

			if (current_wifi_mode == qcsapi_access_point) {
				retval = qcsapi_wifi_get_wpa_status( vap, &wpa_status_str[ 0 ], ZERO_MAC_ADDR,
						sizeof( wpa_status_str ));
				if (retval >= 0){
					if (strcmp( wpa_status_str, "WPA_HANDSHAKING") == 0 ) {
						monitor_wifi_led_table[MONITOR_WIFI_WLAN_INDEX].blink_interval.tv_usec =
							MONITOR_WIFI_WLAN_HANDSHAKING_BLINK_INTERVAL_USEC;
						new_wlan_state = MONITOR_WIFI_LED_STATE_TRANSFER;
					}
				} else {
					fprintf(stderr, "Cannot get wpa status, returns %d\n", retval);
					return retval;
				}
			}
		} else {

			retval = qcsapi_interface_get_counter( vap, QCSAPI_TOTAL_BYTES_SENT, &wlan_tx_bytes );
			if (retval >= 0) {
				retval = qcsapi_interface_get_counter( vap, QCSAPI_TOTAL_BYTES_RECEIVED, &wlan_rx_bytes );
			}

			if (retval >= 0) {
				wlan_bytes_total = wlan_rx_bytes + wlan_tx_bytes;
				if (wlan_bytes_total != wlan_bytes_total_old) {
					new_wlan_state = MONITOR_WIFI_LED_STATE_TRANSFER;
					wlan_bytes_total_old = wlan_bytes_total;
					monitor_wifi_led_table[MONITOR_WIFI_WLAN_INDEX].blink_interval.tv_usec =
							MONITOR_WIFI_WLAN_BLINK_INTERVAL_USEC;
				}
			} else {
				fprintf( stderr, "Cannot get interface counter\n" );
			}
		}

wlan_led_blink:
		retval = monitor_wifi_update_led( MONITOR_WIFI_WLAN_INDEX, new_wlan_state, p_decrement );
		if (retval < 0) {
			fprintf( stderr, "Cannot update the WLAN LED\n" );
		}
	}

#endif

	return( retval );
}

#ifdef TOPAZ_AMBER_IP_APP
static int
monitor_wifi_update_activity( const char *vap,
			  const qcsapi_wifi_mode current_wifi_mode,
			  const struct timeval *p_decrement)
{
	int	retval = 0;
	int	in_association = 0;
	qcsapi_unsigned_int		wlan_rx_bytes, wlan_tx_bytes, wlan_bytes_total;
	static	qcsapi_unsigned_int	wlan_bytes_total_old = 0;
	monitor_wifi_led_state		new_act_state = MONITOR_WIFI_LED_STATE_NO_TRANSFER;

	if (current_wifi_mode == qcsapi_access_point) {
		qcsapi_unsigned_int	association_count = 0;

		retval = qcsapi_wifi_get_count_associations( vap, &association_count );

		if (retval >= 0) {
			in_association = (association_count > 0 );
		}
	} else {
		qcsapi_mac_addr	all_zeros = { 0, 0, 0, 0, 0, 0 };
		qcsapi_mac_addr	current_bssid = { 0, 0, 0, 0, 0, 0 };

		retval = qcsapi_wifi_get_BSSID( vap, current_bssid );

		if (retval >= 0) {
			in_association = (memcmp( all_zeros, current_bssid, sizeof( current_bssid ) ) != 0);
		}
	}

	if (retval >= 0) {
		if (in_association) {
			retval = qcsapi_interface_get_counter( vap, QCSAPI_TOTAL_BYTES_SENT, &wlan_tx_bytes );
			if (retval >= 0) {
				retval = qcsapi_interface_get_counter( vap, QCSAPI_TOTAL_BYTES_RECEIVED, &wlan_rx_bytes );
			}

			if (retval >= 0) {
				wlan_bytes_total = wlan_rx_bytes + wlan_tx_bytes;
				if (wlan_bytes_total != wlan_bytes_total_old) {
					new_act_state = MONITOR_WIFI_LED_STATE_TRANSFER;
					wlan_bytes_total_old = wlan_bytes_total;
					monitor_wifi_led_table[MONITOR_WIFI_ACTIVITY_INDEX].blink_interval.tv_usec =
							MONITOR_WIFI_ACTIVITY_BLINK_INTERVAL_USEC;
				}
			} else {
				fprintf( stderr, "Cannot get interface counter\n" );
			}
		}

		retval = monitor_wifi_update_led( MONITOR_WIFI_ACTIVITY_INDEX, new_act_state, p_decrement );
		if (retval < 0) {
			fprintf( stderr, "Cannot update the WLAN LED\n" );
		}
	}

	return( retval );
}
#endif

#ifndef TOPAZ_AMBER_IP_APP
#define WPS_ERR_LED_TIMER	80
#else
#define WPS_ERR_LED_TIMER	20
#endif
static int
monitor_wifi_update_wps( const char *vap,
			 const qcsapi_wifi_mode current_wifi_mode,
			 const struct timeval *p_decrement)
{
	char			wps_state_str[ LOCAL_WPS_STATE_LEN ];
	char			wps_enable_str[ LOCAL_WPS_STATE_LEN ];
	int			retval=0;
	int			wps_state_value = (int)MONITOR_WIFI_WPS_UNKNOWN;
	monitor_wifi_led_state	new_wps_state = MONITOR_WIFI_LED_OFF;
	monitor_wifi_led_state	new_wps_err_state = MONITOR_WIFI_LED_OFF;
	static	uint		wps_err_counter = 0;
	static	int		wps_err_timeout_20s = 0;

	if (current_wifi_mode == qcsapi_access_point) {
		retval = qcsapi_wps_get_configured_state( vap, &wps_enable_str[ 0 ], sizeof( wps_state_str ) );
		if (retval < 0 || strcmp(wps_enable_str, "configured")) {
			goto wps_led_end;
		}
	}

	retval = qcsapi_wps_get_state( vap, &wps_state_str[ 0 ], sizeof( wps_state_str ) );
	if (retval >= 0) {
		wps_state_value = atoi( &wps_state_str[ 0 ] );

		switch( wps_state_value ) {
		  case MONITOR_WIFI_WPS_INITIAL:
			new_wps_state = MONITOR_WPS_LED_STATE_INITIAL;
			new_wps_err_state = MONITOR_WPS_ERR_LED_OFF;
			wps_err_counter = 0;
			wps_err_timeout_20s = 0;
			break;

		  case MONITOR_WIFI_WPS_IN_PROGRESS:
			new_wps_state = MONITOR_WPS_LED_STATE_STARTED;
			new_wps_err_state = MONITOR_WPS_ERR_LED_OFF;
			wps_err_counter = 0;
			wps_err_timeout_20s = 0;
			break;

		  case MONITOR_WIFI_WPS_SUCCESS:
			new_wps_state = MONITOR_WPS_LED_STATE_SUCCESS;
			new_wps_err_state = MONITOR_WPS_ERR_LED_OFF;
			wps_err_counter = 0;
			wps_err_timeout_20s = 0;
			break;

		  case MONITOR_WIFI_WPS_MESSAGE_ERROR:
			new_wps_state = MONITOR_WPS_LED_STATE_ERROR;
			new_wps_err_state = MONITOR_WPS_ERR_LED_ON;
			break;

		  case MONITOR_WIFI_WPS_OVERLAP:
			new_wps_state = MONITOR_WPS_LED_STATE_OVERLAP;
			new_wps_err_state = MONITOR_WPS_ERR_LED_ON;
			break;

		  case MONITOR_WIFI_WPS_TIMEOUT:
			new_wps_state = MONITOR_WPS_LED_STATE_TIMEOUT;
			new_wps_err_state = MONITOR_WPS_ERR_LED_ON;
			break;

		  case MONITOR_WIFI_WPS_UNKNOWN:
			new_wps_state = MONITOR_WPS_LED_STATE_UNKNOWN;
			new_wps_err_state = MONITOR_WPS_ERR_LED_ON;
			break;

		  default:
			fprintf( stderr, "Unexpected WPS state %d in update WPS\n", wps_state_value );
			retval = -qcsapi_programming_error;
		}
	} else if (retval == -qcsapi_daemon_socket_error) {
		/*
		 * Ignore failed to contact the Supplicant / Host APD error.
		 */
		retval = 0;
		new_wps_state = MONITOR_WPS_LED_STATE_INITIAL;
	}

	if (new_wps_err_state == MONITOR_WPS_ERR_LED_ON) {
		wps_err_counter ++;
		if (wps_err_counter >= WPS_ERR_LED_TIMER) {
			wps_err_timeout_20s = 1;
		}

	}

	if (wps_err_timeout_20s && new_wps_err_state == MONITOR_WPS_ERR_LED_ON) {
		new_wps_err_state = MONITOR_WPS_ERR_LED_OFF;
		new_wps_state = MONITOR_WIFI_LED_OFF;
	}

wps_led_end:
	if (retval >= 0) {
		retval = monitor_wifi_update_led( MONITOR_WIFI_WPS_INDEX, new_wps_state, p_decrement );
		if (retval < 0) {
			fprintf( stderr, "Cannot update the WPS LED\n" );
		}
#ifdef CUSTOMER_SPEC_RGMII
		retval = monitor_wifi_update_led( MONITOR_WIFI_WPS_ERR_INDEX, new_wps_err_state, p_decrement );
		if (retval < 0) {
			fprintf( stderr, "Cannot update the WPS ERR LED\n" );
		}
#endif
	}

	return( retval );
}

static int
monitor_wifi_get_link_quality_max(const char *vap)
{
	qcsapi_unsigned_int max_quality;

	if (qcsapi_wifi_get_link_quality_max(vap, &max_quality) < 0) {
		max_quality = 0;
	}

	return max_quality;
}

static int
monitor_wifi_update_quality( const char *vap,
			 const qcsapi_wifi_mode current_wifi_mode,
			 struct monitor_wifi_quality *link_quality,
			 const struct timeval *p_decrement)
{
	int		quality_state;
	int             rf_chipid;

	if (link_quality->quality_state == -1 ||
			timercmp(&link_quality->countdown_value, p_decrement, == ) ||
			timercmp(&link_quality->countdown_value, p_decrement, < )) {

		uint32_t link_quality_max = monitor_wifi_get_link_quality_max(vap);
		if (link_quality_max >= 100) {
			quality_state = MONITOR_WIFI_QUAL_LED_ON;
		} else if (link_quality_max >= 30) {
			quality_state = MONITOR_WIFI_QUAL_LED_BLINK;
		} else {
			quality_state = MONITOR_WIFI_QUAL_LED_OFF;
		}

		link_quality->quality_state = quality_state;
		link_quality->countdown_value = link_quality->update_interval;
	} else {
		struct timeval new_countdown = { 0, 0 };
		timersub(&link_quality->countdown_value, p_decrement, &new_countdown);
		link_quality->countdown_value = new_countdown;
	}

	local_wifi_get_rf_chipid(&rf_chipid);
	if (rf_chipid != CHIPID_DUAL) {
#ifndef TOPAZ_AMBER_IP_APP
		monitor_wifi_update_led(
			MONITOR_WIFI_QUAL_INDEX, link_quality->quality_state, p_decrement);
#else
		switch (link_quality->quality_state){
		case MONITOR_WIFI_QUAL1_LED:
			monitor_wifi_update_led(MONITOR_WIFI_QUAL_INDEX, MONITOR_WIFI_QUAL_LED_ON, p_decrement);
			monitor_wifi_update_led(MONITOR_WIFI_QUAL2_INDEX, MONITOR_WIFI_QUAL_LED_OFF, p_decrement);
			monitor_wifi_update_led(MONITOR_WIFI_QUAL3_INDEX, MONITOR_WIFI_QUAL_LED_OFF, p_decrement);
			break;
		case MONITOR_WIFI_QUAL2_LED:
			monitor_wifi_update_led(MONITOR_WIFI_QUAL_INDEX, MONITOR_WIFI_QUAL_LED_OFF, p_decrement);
			monitor_wifi_update_led(MONITOR_WIFI_QUAL2_INDEX, MONITOR_WIFI_QUAL_LED_ON, p_decrement);
			monitor_wifi_update_led(MONITOR_WIFI_QUAL3_INDEX, MONITOR_WIFI_QUAL_LED_OFF, p_decrement);
			break;
		case MONITOR_WIFI_QUAL3_LED:
			monitor_wifi_update_led(MONITOR_WIFI_QUAL_INDEX, MONITOR_WIFI_QUAL_LED_OFF, p_decrement);
			monitor_wifi_update_led(MONITOR_WIFI_QUAL2_INDEX, MONITOR_WIFI_QUAL_LED_OFF, p_decrement);
			monitor_wifi_update_led(MONITOR_WIFI_QUAL3_INDEX, MONITOR_WIFI_QUAL_LED_ON, p_decrement);
			break;
		default:
			break;
		}
#endif
	}

	return 0;
}

#if !defined(CUSTOMER_SPEC_RGMII) && !defined(TOPAZ_AMBER_IP_APP)
static int
monitor_wifi_update_mode( const qcsapi_wifi_mode current_wifi_mode, const struct timeval *p_decrement )
{
	int			retval = 0;
	monitor_wifi_led_state	new_mode_led_state = MONITOR_WIFI_LED_NOT_DEFINED;
	uint8_t			led_number = get_led_number_by_index( MONITOR_WIFI_MODE_INDEX );

	if (led_number >= MONITOR_WIFI_MAX_GPIO_PIN) {
		return( 0 );
	}

	switch( current_wifi_mode ) {
	  case qcsapi_access_point:
		new_mode_led_state = MONITOR_WIFI_LED_ON;
		break;

	  case qcsapi_station:
		new_mode_led_state = MONITOR_WIFI_LED_OFF;
		break;

	  default:
		new_mode_led_state = MONITOR_WIFI_LED_BLINK;
		break;

	}

	retval = monitor_wifi_update_led( MONITOR_WIFI_MODE_INDEX, new_mode_led_state, p_decrement );

	return( retval );
}
#endif

static int
monitor_wifi_show_device_off(qcsapi_wifi_mode wifi_mode)
{
	int rf_chipid = 0;

#if !defined(CUSTOMER_SPEC_RGMII) && !defined(TOPAZ_AMBER_IP_APP)
	monitor_wifi_update_mode(wifi_mode, &slow_interval);
#endif
	monitor_wifi_update_led(MONITOR_WIFI_WLAN_INDEX, MONITOR_WIFI_LED_OFF, &slow_interval);
	monitor_wifi_update_led(MONITOR_WIFI_WPS_INDEX, MONITOR_WIFI_LED_OFF, &slow_interval);
	local_wifi_get_rf_chipid(&rf_chipid);
	if (rf_chipid != CHIPID_DUAL) {
#ifndef TOPAZ_AMBER_IP_APP
		monitor_wifi_update_led(MONITOR_WIFI_QUAL_INDEX, MONITOR_WIFI_LED_OFF, &slow_interval);
#else
		monitor_wifi_update_led(MONITOR_WIFI_QUAL_INDEX, MONITOR_WIFI_LED_OFF, &slow_interval);
		monitor_wifi_update_led(MONITOR_WIFI_QUAL2_INDEX, MONITOR_WIFI_LED_OFF, &slow_interval);
		monitor_wifi_update_led(MONITOR_WIFI_QUAL3_INDEX, MONITOR_WIFI_LED_OFF, &slow_interval);
#endif
	}

	return monitor_wifi_set_slow_timer();
}

static int
monitor_wifi_update_leds( void )
{
	int	retval = 0;
	int	qcsapi_retval = 0;
	char	vap_status[ 10 ] = "";
	const char* vapname = MONITOR_WIFI_DEFAULT_VAP;
	qcsapi_wifi_mode wifi_mode = qcsapi_nosuch_mode;

	qcsapi_retval = qcsapi_wifi_get_mode(vapname, &wifi_mode);
	if (qcsapi_retval < 0 || wifi_mode == qcsapi_mode_not_defined) {
		return monitor_wifi_show_device_off(qcsapi_mode_not_defined);
	}

	qcsapi_retval = qcsapi_interface_get_status(vapname, vap_status);
	if (qcsapi_retval < 0 || strcasecmp(vap_status, "Disabled") == 0) {
		retval = monitor_wifi_show_device_off(wifi_mode);

		if (wifi_mode == qcsapi_access_point)
			return retval;
	}
#if !defined(CUSTOMER_SPEC_RGMII) && !defined(TOPAZ_AMBER_IP_APP)
	monitor_wifi_update_mode(wifi_mode, &slow_interval);
#endif

	monitor_wifi_update_wlan(MONITOR_WIFI_DEFAULT_VAP, wifi_mode, &fast_interval);
	monitor_wifi_update_wps(MONITOR_WIFI_DEFAULT_VAP, wifi_mode, &fast_interval);
	monitor_wifi_update_quality(MONITOR_WIFI_DEFAULT_VAP, wifi_mode, &current_link_quality, &fast_interval);

#ifdef TOPAZ_AMBER_IP_APP
	monitor_wifi_update_activity(MONITOR_WIFI_DEFAULT_VAP, wifi_mode, &fast_interval);
#endif

	monitor_wifi_set_fast_timer();

	return 0;
}

int
main( int argc, char *argv[] )
{
	struct sigaction	action_for_sigalrm;
	int			monitor_wifi_status = 0;
	char			qcsapi_error_message[ 60 ] = "";

	if (monitor_wifi_init_leds() < 0) {
		fprintf( stderr, "Problem with the LED configuration; correct before restarting\n" );
		return( 1 );
	}

	memset( (void *) &action_for_sigalrm, 0, sizeof( action_for_sigalrm ) );
	action_for_sigalrm.sa_flags = SA_SIGINFO;
	action_for_sigalrm.sa_sigaction = monitor_wifi_sigalrm_handler;
	sigemptyset( &action_for_sigalrm.sa_mask );

	if (sigaction( SIGALRM, &action_for_sigalrm, NULL ) < 0) {
		fprintf( stderr, "Problem registering a callback for timer expiration\n" );
		return( 1 );
	}

	qcsapi_console_disconnect();

	monitor_wifi_status = monitor_wifi_update_leds();

	while (monitor_wifi_status == 0) {
		pause();
		monitor_wifi_status = monitor_wifi_update_leds();
	}

	qcsapi_errno_get_message( monitor_wifi_status,
				 &qcsapi_error_message[ 0 ],
			          sizeof( qcsapi_error_message ) );
	fprintf( stderr, "Error monitoring WiFi: %d (%s)\n", monitor_wifi_status, &qcsapi_error_message[ 0 ] );

	return( 1 );
}
