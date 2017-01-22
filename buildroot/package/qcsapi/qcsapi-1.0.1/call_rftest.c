/*SH0
*******************************************************************************
**                                                                           **
**         Copyright (c) 2009 - 2009 Quantenna Communications Inc            **
**                            All Rights Reserved                            **
**                                                                           **
**  File        : call_rftest.c                                              **
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
#include <unistd.h>
#include <stdio.h>

#include "qcsapi_rftest.h"

#define MAX_COUNT_RFTESTS	10

typedef struct rftest_interface_params
{
	qcsapi_rftest_operation	rftest_operations[ MAX_COUNT_RFTESTS ];
	qcsapi_dump_format	dump_format;
	unsigned int		index_operations;
} rftest_interface_params;


static const struct
{
	const char		*operation_name;
	qcsapi_rftest_operation	 operation_enum;
} rftest_operation_table[] =
{
	{ "dump_params",	e_qcsapi_rftest_dump_params },
	{ "dump_counters",	e_qcsapi_rftest_dump_counters },
	{ "show_counters",	e_qcsapi_rftest_dump_counters },
	{ "show_packets",	e_qcsapi_rftest_dump_counters },
	{ "setup",		e_qcsapi_rftest_setup },
	{ "start_rx_pkt",	e_qcsapi_rftest_start_packet_receive },
	{ "start_rx_packet",	e_qcsapi_rftest_start_packet_receive },
	{ "start_tx_pkt",	e_qcsapi_rftest_start_packet_xmit },
	{ "start_tx_packet",	e_qcsapi_rftest_start_packet_xmit },
	{ "stop_pkt",		e_qcsapi_rftest_stop_packet_test },
	{ "stop_packet",	e_qcsapi_rftest_stop_packet_test },
	{ "get_counters",	e_qcsapi_rftest_get_pkt_counters },
	{ "get_packets",	e_qcsapi_rftest_get_pkt_counters },
	{ "start_cw",		e_qcsapi_rftest_start_send_cw },
	{ "send_cw",		e_qcsapi_rftest_start_send_cw },
	{ "start_send_cw",	e_qcsapi_rftest_start_send_cw },
	{ "stop_cw",		e_qcsapi_rftest_stop_send_cw },
	{ "stop_send_cw",	e_qcsapi_rftest_stop_send_cw },
	{ "stop_RF_tests",	e_qcsapi_rftest_stop_RF_tests },
	{  NULL,		e_qcsapi_nosuch_rftest }
};


static int	verbose_flag = 0;


static FILE *
open_console( void )
{
	return( fopen( "/dev/console", "w" ) );
}

static qcsapi_rftest_operation
name_to_rftest_operation_enum( const char *rftest_param )
{
	qcsapi_rftest_operation	retval = e_qcsapi_nosuch_rftest;
	unsigned int		iter;

	for (iter = 0; retval == e_qcsapi_nosuch_rftest && rftest_operation_table[ iter ].operation_name != NULL ; iter++)
	{
	  /*
	   * Unlike the RF test parameters, here an exact match (up to case compare) is required.
	   * No additional characters following the name in the text.
	   */
		if (strcasecmp( rftest_param, rftest_operation_table[ iter ].operation_name ) == 0)
		{
			retval = rftest_operation_table[ iter ].operation_enum;
		}
	}

	return( retval );
}

static int
update_rftest_type( int argc, char *argv[], rftest_interface_params *p_interface_params )
{
	int			iter = 0;
	int			finished = 0;
	qcsapi_rftest_operation	current_operation = e_qcsapi_nosuch_rftest;

	while (iter < argc && finished == 0)
	{
		current_operation = name_to_rftest_operation_enum( argv[ iter ] );
		if (current_operation != e_qcsapi_nosuch_rftest && p_interface_params->index_operations + 1 < MAX_COUNT_RFTESTS)
		{
			p_interface_params->rftest_operations[ p_interface_params->index_operations ] = current_operation;
			iter++;
			p_interface_params->index_operations++;
		}
		else
		  finished = 1;
	}

	return( iter );
}

static void
provide_help( void )
{
	unsigned int	iter;

	printf( "RF test operations (more than one name can refer to the same operation):\n" );
	for (iter = 0; rftest_operation_table[ iter ].operation_name != NULL; iter++)
	{
		printf( "\t%s\n", rftest_operation_table[ iter ].operation_name );
	}
}

static int
process_rftest_command_line( int argc, char *argv[], rftest_interface_params *p_interface_params, qcsapi_rftest_params *p_rftest_params )
{
	int	 retval = 0;

	while (argc > 0)			/* This program processes ALL the command line arguments */
	{
		int	 found_error = 0;
		int	 count_args_processed = 0;
		char	*value_addr = NULL;

		if (strcasecmp( argv[ 0 ], "help" ) == 0 || strcmp( argv[ 0 ], "-h") == 0)
		{
			provide_help();
			exit( 0 );
		}
		else if (strcmp( argv[ 0 ], "-v") == 0)
		{
			count_args_processed = 1;
			verbose_flag++;
		}
		else if (strncasecmp( argv[ 0 ], "format", 6 ) == 0)
		{
			count_args_processed = 1;
			if ((value_addr = strchr( argv[ 0 ] + 6, '=' )) != NULL)
			{
				value_addr++;

				if (strcasecmp( value_addr, "http" ) == 0 ||
				    strcasecmp( value_addr, "post" ) == 0)
				{
					p_interface_params->dump_format = qcsapi_dump_HTTP_POST;
				}
				else if (strcasecmp( value_addr, "ascii" ) == 0 ||
					 strcasecmp( value_addr, "text" ) == 0)
				{
					p_interface_params->dump_format = qcsapi_dump_ascii_text;
				}
				else
				{
					found_error = 1;
				}
			}
			else
			{
				found_error = 1;
			}
		}
		else
		{
			if ((count_args_processed = update_rftest_type( argc, argv, p_interface_params )) <= 0)
			  count_args_processed = qcsapi_rftest_update_params( p_rftest_params, argc, argv );
		}

		if (count_args_processed <= 0)
		{
			fprintf( stderr, "Unrecognized argument %s.\n", argv[ 0 ] );
			count_args_processed = 1;
		}
		else if (found_error)
		{
			fprintf( stderr, "Format for argument %s not unrecognized.\n", argv[ 0 ] );
		}

		argc = argc - count_args_processed;
		argv = argv + count_args_processed;
	}

	return( retval );
}

int
main( int argc, char *argv[] )
{
	qcsapi_rftest_params		rftest_params;
	rftest_interface_params		interface_params = { { e_qcsapi_nosuch_rftest }, qcsapi_dump_ascii_text, 0 };
	qcsapi_rftest_packet_report	packet_report = { { 0, 0 }, { 0, 0 } };
	unsigned int			iter;
	int				produced_output = 0;
	int				got_pkt_counters = 0;
	int				showed_pkt_counters = 0;

	qcsapi_rftest_init_params( &rftest_params );

	if (argc > 1)
	{
		argc--;
		argv++;
		process_rftest_command_line( argc, argv, &interface_params, &rftest_params );
	}

	for (iter = 0; iter < interface_params.index_operations; iter++)
	{
		switch( interface_params.rftest_operations[ iter ] )
		{
		  case e_qcsapi_rftest_dump_params:
			qcsapi_rftest_dump_params( &rftest_params, NULL, interface_params.dump_format );
			produced_output = 1;
			break;

		  case e_qcsapi_rftest_dump_counters:
			if (got_pkt_counters == 0)
			{
				qcsapi_rftest_get_pkt_counters( &rftest_params, packet_report );
				got_pkt_counters = 1;
			}
			if (showed_pkt_counters == 0)
			{
				qcsapi_rftest_dump_counters( packet_report, NULL, interface_params.dump_format );
				showed_pkt_counters = 1;
				produced_output = 1;
			}
			break;

		  case e_qcsapi_rftest_setup:
			qcsapi_rftest_setup( &rftest_params );
			break;

		  case e_qcsapi_rftest_start_packet_receive:
			qcsapi_rftest_setup( &rftest_params );
			qcsapi_rftest_start_packet_receive( &rftest_params );
			break;

		  case e_qcsapi_rftest_start_packet_xmit:
			qcsapi_rftest_setup( &rftest_params );
			qcsapi_rftest_start_packet_xmit( &rftest_params );
			break;

		  case e_qcsapi_rftest_stop_packet_test:
			qcsapi_rftest_stop_packet_test( &rftest_params );
			break;
		/*
		 * Always get packet counters if specifically requested.
		 * If "get_counters" appears on the command line twice,
		 * qcsapi_rftest_get_pkt_counters will be called twice.
		 */
		  case e_qcsapi_rftest_get_pkt_counters:
			qcsapi_rftest_get_pkt_counters( &rftest_params, packet_report );
			got_pkt_counters = 1;
			break;

		  case e_qcsapi_rftest_start_send_cw:
			qcsapi_rftest_setup( &rftest_params );
			qcsapi_rftest_start_send_cw( &rftest_params );
			break;

		  case e_qcsapi_rftest_stop_send_cw:
			qcsapi_rftest_stop_send_cw( &rftest_params );
			break;

		  case e_qcsapi_rftest_stop_RF_tests:
			qcsapi_rftest_stop_RF_tests( &rftest_params );
			break;

		  case e_qcsapi_nosuch_rftest:
		  default:
			fprintf( stderr, "Error: no RF test operation selected\n" );
			break;
		}
	}

	if (produced_output == 0 && verbose_flag >= 0)
	{
		printf( "complete\n" );
	}

// debug only

	if (verbose_flag > 0)
	{
		FILE	*console_h = open_console();

		if (console_h != NULL)
		{
			for (iter = 0; iter < argc; iter++)
			  fprintf( console_h, "%u: %s\n", iter, argv[ iter ] );
			qcsapi_rftest_dump_params( &rftest_params, console_h, qcsapi_dump_ascii_text );
		}

		fclose( console_h );
	}

	return( 0 );
}
