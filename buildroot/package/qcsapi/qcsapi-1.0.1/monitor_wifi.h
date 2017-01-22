/*SH0
*******************************************************************************
**                                                                           **
**         Copyright (c) 2011 Quantenna Communications Inc                   **
**                                                                           **
**  File        : monitor_wifi.h                                             **
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

#ifndef _MONITOR_WIFI_H
#define _MONITOR_WIFI_H

#define  MONITOR_WIFI_DEFAULT_VAP	"wifi0"

#ifndef TOPAZ_AMBER_IP_APP
#define  MONITOR_WIFI_WPS_LED_NUMBER	3

#if defined(CUSTOMER_SPEC_BBIC4_RGMII) || defined(CUSTOMER_SPEC_VB)
#define  MONITOR_WIFI_WLAN_LED_NUMBER	1
#else
#define  MONITOR_WIFI_WLAN_LED_NUMBER	2
#endif

#define  MONITOR_WIFI_WPS_ERR_LED_NUMBER  13
#define  MONITOR_WIFI_MODE_LED_NUMBER	6
#define	 MONITOR_WIFI_QUAL_LED_NUMBER	16

#else
#define  MONITOR_WIFI_MODE_LED_NUMBER		-1
#define  MONITOR_WIFI_WPS_LED_NUMBER		11
#define  MONITOR_WIFI_WPS_ERR_LED_NUMBER	12
#define  MONITOR_WIFI_WLAN_LED_NUMBER		13
#define  MONITOR_WIFI_ACTIVITY_LED_NUMBER	14
#define	 MONITOR_WIFI_QUAL_LED_NUMBER		15
#define	 MONITOR_WIFI_QUAL2_LED_NUMBER		16
#define	 MONITOR_WIFI_QUAL3_LED_NUMBER		17
#endif

#define  MONITOR_WIFI_ACTIVE_LOW	0
#define  MONITOR_WIFI_ACTIVE_HIGH	1

#define  MONITOR_WIFI_MAX_GPIO_PIN	32

#define  MONITOR_WIFI_SLOW_TIME_SEC	0
#define  MONITOR_WIFI_SLOW_TIME_USEC	500000
#define  MONITOR_WIFI_FAST_TIME_SEC	0
#define  MONITOR_WIFI_FAST_TIME_USEC	250000

#define  MONITOR_WIFI_WLAN_BLINK_INTERVAL_SEC	0
#define  MONITOR_WIFI_WLAN_BLINK_INTERVAL_USEC	500000
#define  MONITOR_WIFI_WLAN_HANDSHAKING_BLINK_INTERVAL_SEC	0
#define  MONITOR_WIFI_WLAN_HANDSHAKING_BLINK_INTERVAL_USEC	250000
#define  MONITOR_WIFI_WPS_BLINK_INTERVAL_SEC	0
#define  MONITOR_WIFI_WPS_BLINK_INTERVAL_USEC	500000
#define  MONITOR_WIFI_MODE_BLINK_INTERVAL_SEC	0
#define  MONITOR_WIFI_MODE_BLINK_INTERVAL_USEC	500000
#define	 MONITOR_WIFI_QUAL_BLINK_INTERVAL_SEC	0
#define	 MONITOR_WIFI_QUAL_BLINK_INTERVAL_USEC	500000

#ifdef TOPAZ_AMBER_IP_APP
#define	 MONITOR_WIFI_ACTIVITY_BLINK_INTERVAL_SEC	0
#define	 MONITOR_WIFI_ACTIVITY_BLINK_INTERVAL_USEC	500000
#endif

typedef enum {
	MONITOR_WIFI_LED_OFF = 0,
	MONITOR_WIFI_LED_BLINK,
	MONITOR_WIFI_LED_ON,
	MONITOR_WIFI_LED_NOT_DEFINED = -1
} monitor_wifi_led_state;

enum {
	MONITOR_WIFI_WPS_INITIAL = 0,
	MONITOR_WIFI_WPS_IN_PROGRESS = 1,
	MONITOR_WIFI_WPS_SUCCESS = 2,
	MONITOR_WIFI_WPS_MESSAGE_ERROR = 3,
	MONITOR_WIFI_WPS_TIMEOUT = 4,
	MONITOR_WIFI_WPS_OVERLAP = 5,
	MONITOR_WIFI_WPS_UNKNOWN = -1
};

#define  MONITOR_WIFI_LED_STATE_NOT_ASSOCIATED	MONITOR_WIFI_LED_BLINK
#define  MONITOR_WIFI_LED_STATE_ASSOCIATED	MONITOR_WIFI_LED_ON
#define  MONITOR_WIFI_LED_STATE_TRANSFER	MONITOR_WIFI_LED_BLINK
#define  MONITOR_WIFI_LED_STATE_NO_TRANSFER	MONITOR_WIFI_LED_ON

#define  MONITOR_WPS_LED_STATE_INITIAL		MONITOR_WIFI_LED_OFF
#define  MONITOR_WPS_LED_STATE_STARTED		MONITOR_WIFI_LED_BLINK
#define  MONITOR_WPS_LED_STATE_ERROR		MONITOR_WIFI_LED_OFF
#define  MONITOR_WPS_LED_STATE_TIMEOUT		MONITOR_WIFI_LED_OFF
#define  MONITOR_WPS_LED_STATE_OVERLAP		MONITOR_WIFI_LED_OFF
#define  MONITOR_WPS_LED_STATE_UNKNOWN		MONITOR_WIFI_LED_OFF
#define  MONITOR_WPS_LED_STATE_SUCCESS		MONITOR_WIFI_LED_ON
#define  MONITOR_WPS_ERR_LED_ON			MONITOR_WIFI_LED_ON
#define  MONITOR_WPS_ERR_LED_OFF		MONITOR_WIFI_LED_OFF
#define  MONITOR_WIFI_QUAL_LED_ON		MONITOR_WIFI_LED_ON
#define  MONITOR_WIFI_QUAL_LED_BLINK		MONITOR_WIFI_LED_BLINK
#define  MONITOR_WIFI_QUAL_LED_OFF		MONITOR_WIFI_LED_OFF

#ifdef TOPAZ_AMBER_IP_APP
#define  MONITOR_WIFI_QUAL1_LED		MONITOR_WIFI_QUAL_LED_ON
#define  MONITOR_WIFI_QUAL2_LED		MONITOR_WIFI_QUAL_LED_BLINK
#define  MONITOR_WIFI_QUAL3_LED		MONITOR_WIFI_QUAL_LED_OFF
#endif

#endif /* _MONITOR_WIFI_H */
