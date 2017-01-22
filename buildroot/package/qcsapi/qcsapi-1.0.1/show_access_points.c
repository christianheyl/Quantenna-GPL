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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "qcsapi.h"


static const char	*protocol_name_table[] =
{
	"None",
	"WPA",
	"WPA2",
	"WPA WPA2"
};

static const char	*encryption_name_table[] =
{
	"None",
	"TKIP",
	"CCMP",
	"TKIP CCMP"
};

static const char	*authentication_name_table[] =
{
	"None",
	"PSK",
	"EAP"
};

static int		 verbose_flag = 0;
static const char	*program_name = NULL;

static const char *
get_name_by_value( const int qcsapi_value, const char **qcsapi_lookup_table, const size_t lookup_table_size )
{
	const char	*retaddr = NULL;

	if (qcsapi_value >= 0 && qcsapi_value < (int) lookup_table_size)
	  retaddr = qcsapi_lookup_table[ qcsapi_value ];

	return( retaddr );
}

static void
local_snprint_bitrate(char *buffer, int len, unsigned int bitrate)
{
	int i=0;
	char ch[]={' ','k','M','G'};
	int remainder = 0;
	int val;

	for (i = 0; i < 4 && bitrate >= 1000; i++)  {
		val = bitrate / 1000;
		remainder = (bitrate % 1000);
		bitrate = val;
	}
	if (remainder) {
		snprintf(buffer, len, "%d.%1.1d %cb/s", bitrate, remainder, ch[i]);
	} else {
		snprintf(buffer, len, "%d %cb/s", bitrate, ch[i]);
	}
}

static void
show_ap_properties( const qcsapi_unsigned_int index_ap, const qcsapi_ap_properties *p_ap_properties )
{
	char	 mac_addr_string[ 24 ];
	char buffer[32];

	printf( "AP %d:\n", index_ap );
	printf( "\tSSID: %s\n", p_ap_properties->ap_name_SSID );
	sprintf( &mac_addr_string[ 0 ], MACFILTERINGMACFMT,
		  p_ap_properties->ap_mac_addr[ 0 ],
		  p_ap_properties->ap_mac_addr[ 1 ],
		  p_ap_properties->ap_mac_addr[ 2 ],
		  p_ap_properties->ap_mac_addr[ 3 ],
		  p_ap_properties->ap_mac_addr[ 4 ],
		  p_ap_properties->ap_mac_addr[ 5 ]
	);
	printf( "\tMAC address: %s\n", &mac_addr_string[ 0 ] );
	printf( "\tChannel: %d\n", p_ap_properties->ap_channel );
	printf( "\tBandwidth: %d\n", p_ap_properties->ap_bw );
	printf( "\tRSSI: %d\n", p_ap_properties->ap_RSSI );
	printf( "\tBeacon Interval: %d\n", p_ap_properties->ap_beacon_interval);
	printf( "\tDTIM period: %d\n", p_ap_properties->ap_dtim_interval);
	printf( "\tOperating Mode: %s\n", p_ap_properties->ap_is_ess?"Infrastrcutre":"Ad-Hoc");
	local_snprint_bitrate(buffer, sizeof(buffer), p_ap_properties->ap_best_data_rate);
	printf("\tBest Data Rate: %s\n", buffer);

	if ((p_ap_properties->ap_flags & qcsapi_ap_security_enabled) != 0)
	{
		const char	*value_name = NULL;

		printf( "\tsecurity enabled\n" );

		value_name = get_name_by_value(
				p_ap_properties->ap_protocol,
				protocol_name_table,
				TABLE_SIZE( protocol_name_table ) );
		if (value_name == NULL)
		  value_name = "(unknown)";
		printf( "\tprotocol: %s\n", value_name );

		if (verbose_flag > 0)
		{
			value_name = get_name_by_value(
					p_ap_properties->ap_authentication_mode,
					authentication_name_table,
					TABLE_SIZE( authentication_name_table )
			);
			if (value_name == NULL)
			  value_name = "(unknown)";
			printf( "\tauthentication mode: %s\n", value_name );

			value_name = get_name_by_value(
					p_ap_properties->ap_encryption_modes,
					encryption_name_table,
					TABLE_SIZE( encryption_name_table )
			);
			if (value_name == NULL)
			  value_name = "(unknown)";
			printf( "\tencryption modes: %s\n", value_name );
		}
	}
	else
	  printf( "\tsecurity disabled\n" );

	printf( "\n" );
}

static void
provide_help( void )
{
	printf( "usage: show_access_points <WiFi interface device>\n" );
}

static int
process_options( int argc, char *argv[] )
{
	int	local_index = 0;

	while (local_index < argc && *(argv[ local_index ]) == '-')
	{
		char		*option_arg = argv[ local_index ];
		unsigned int	 length_option = strlen( option_arg );

		if (length_option > 1)
		{
			char	option_letter = option_arg[ 1 ];

			if (option_letter == 'v')
			{
				unsigned int	index_2 = 1;

				while (option_arg[ index_2 ] == 'v')
				{
					verbose_flag++;
					index_2++;
				}
			}
			else if (option_letter == 'h')
			{
				provide_help();
				exit( 0 );
			}
			else
			{
				printf( "unrecognized option '%c'\n", option_letter );
			}
		}
	  /*
 	   * Control would take the non-existent else clause if the argument were just "-".
 	   */
		local_index++;
	}

	return( local_index );
}

int
main( int argc, char *argv[] )
{
	int			 errorval = 0;
	char			*ifname = NULL;
	int			 ival;
	qcsapi_unsigned_int	 count_APs = 0;

	program_name = argv[ 0 ];

	if (argc < 2)
	{
		provide_help();
		return( 1 );
	}

	argc--;
	argv++;

	ival = process_options( argc, argv );

	argc = argc - ival;
	argv += ival;

	if (argc < 1)
	{
		printf( "Missing WiFi interface on the command line.\n" );
		provide_help();
		return( 1 );
	}
	else
	  ifname = argv[ 0 ];

	errorval = qcsapi_wifi_get_results_AP_scan( ifname, &count_APs );
	if (errorval >= 0)
	{
		qcsapi_unsigned_int	iter;
		qcsapi_ap_properties	ap_properties;

		for (iter = 0; iter < count_APs && errorval >= 0; iter++)
		{
			errorval = qcsapi_wifi_get_properties_AP( ifname, iter, &ap_properties );
			if (errorval >= 0)
			  show_ap_properties( iter + 1, &ap_properties );
		}
	}
	else
	{
		char	error_message[ 122 ] = { '\0' };

		qcsapi_errno_get_message( errorval, &error_message[ 0 ], sizeof( error_message ) );
		printf( "%s, error getting the AP scan results for %s:\n", program_name, ifname );
		printf( "%s\n", &error_message[ 0 ] );
	}

	return( 0 );
}
