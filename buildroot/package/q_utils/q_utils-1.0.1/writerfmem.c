/**
 * Copyright (c) 2009-2010 Quantenna Communications, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define QDRV_CONTROL	"/sys/devices/qdrv/control"

static unsigned int
atox( const char *hexval )
{
	int		iter = 0;
	unsigned int	retval = 0;

	if (strncmp( hexval, "0x", 2 ) == 0 || strncmp( hexval, "0X", 2 ) == 0)
	  hexval += 2;

	while (isxdigit( *hexval ) && iter < 8)
	{
		int	hexdigit = *hexval;

		if (isdigit( hexdigit ))
		  hexdigit = hexdigit - '0';
		else if (islower( hexdigit ))
		  hexdigit = hexdigit - 'a' + 10;
		else
		  hexdigit = hexdigit - 'A' + 10;

		retval = retval * 16 + hexdigit;

		hexval++;
		iter++;
	}

	return( retval );
}

int
main( int argc, char *argv[] )
{
	int		qdrv_fd = -1;
	int		bb_index, rf_reg;
	unsigned int	rf_val, rf_val1, rf_val2, rf_val3;
	char		qdrv_message[ 122 ];

	if (argc < 4)
	{
		fprintf( stderr, "Usage: %s <baseband index> <register offset> <register value>\n", argv[ 0 ] );
    		fprintf( stderr, "where <register offset> is in the range 64 - 255" );
		return( 1 );
	}
	else if ((qdrv_fd = open( QDRV_CONTROL, O_WRONLY )) < 0)
	{
		fprintf( stderr, "Cannot access %s\n", QDRV_CONTROL );
		fprintf( stderr, "(Has the system been started?)\n" );
		return( 1 );
	}

	bb_index = atoi( argv[ 1 ] );
	rf_reg = atoi( argv[ 2 ] );
	rf_val = atox( argv[ 3 ] );

	rf_val1 = rf_val & 0xff;
	rf_val = rf_val >> 8;
	rf_val2 = rf_val & 0xff;
	rf_val = rf_val >> 8;
	rf_val3 = rf_val & 0xff;

	snprintf(
		&qdrv_message[ 0 ], sizeof( qdrv_message ),
		"calcmd 34 0 14 0 1 %d 2 %d 3 %u 4 %u 5 %u",
		 bb_index, rf_reg, rf_val3, rf_val2, rf_val1
	);

	write( qdrv_fd, &qdrv_message[ 0 ], strlen( qdrv_message ) + 1 );
	close( qdrv_fd );

	return( 0 );
}
