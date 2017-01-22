/*SH0
*******************************************************************************
**                                                                           **
**         Copyright (c) 2009 - 2011 Quantenna Communications Inc            **
**                                                                           **
**  File        : show_traffic_rates.c                                       **
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
#include <stdio.h>
#include <string.h>

#include "qcsapi.h"

#define WIFI_INTERFACE	"wifi0"

int
main( int argc, char *argv[] )
{
	u_int64_t	 tx_bytes_1, tx_bytes_2, rx_bytes_1, rx_bytes_2;
	unsigned int	 limit = 1;
	unsigned int	 iter;
	unsigned int	 association_index = 0;
	int		 qcsapi_retval;
	int		 stop_monitoring = 0;
	char		*base_file_name = strrchr( argv[ 0 ], '/' );

	if (base_file_name == NULL)
	  base_file_name = argv[ 0 ];

	if (strcmp( "monitor_traffic_rates", base_file_name ) == 0)
	  limit = 2147483647;

	qcsapi_retval = qcsapi_wifi_get_tx_bytes_per_association( WIFI_INTERFACE, association_index, &tx_bytes_1 );
	if (qcsapi_retval < 0)
	{
		fprintf( stderr, "Cannot get count of TX bytes (is the WiFi device in association?)\n" );
		return( 1 );
	}

	qcsapi_retval = qcsapi_wifi_get_rx_bytes_per_association( WIFI_INTERFACE, association_index, &rx_bytes_1 );
	if (qcsapi_retval < 0)
	{
		fprintf( stderr, "Cannot get count of RX bytes (is the WiFi device in association?)\n" );
		return( 1 );
	}

	for (iter = 0; iter < limit && stop_monitoring == 0; iter++)
	{
		sleep( 1 );

		qcsapi_retval = qcsapi_wifi_get_tx_bytes_per_association( WIFI_INTERFACE, association_index, &tx_bytes_2 );
		if (qcsapi_retval < 0)
		{
			fprintf( stderr, "Cannot get TX byte count update\n" );
			stop_monitoring = 1;
		}

		if (stop_monitoring == 0)
		{
			qcsapi_retval = qcsapi_wifi_get_rx_bytes_per_association( WIFI_INTERFACE, association_index, &rx_bytes_2 );
			if (qcsapi_retval < 0)
			{
				fprintf( stderr, "Cannot get RX byte count update\n" );
				stop_monitoring = 1;
			}
		}

		if (stop_monitoring == 0)
		{
			u_int64_t	tx_diff = tx_bytes_2 - tx_bytes_1;
			u_int64_t	rx_diff = rx_bytes_2 - rx_bytes_1;
		  /*
		   * Interval between updates is 1 second - so the difference represents the current traffic rates
		   * - but in bytes / sec.  Convert bytes to bits.
		   */
			tx_diff = tx_diff * 8;
			rx_diff = rx_diff * 8;

			printf( "TX rate: %llu, RX rate: %llu\n", tx_diff, rx_diff );

			rx_bytes_1 = rx_bytes_2;
			tx_bytes_1 = tx_bytes_2;
		}
	}

	return( 0 );
}
