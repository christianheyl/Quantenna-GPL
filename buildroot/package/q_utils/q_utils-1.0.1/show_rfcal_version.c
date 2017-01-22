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
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>


typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

typedef struct {
	u16 magic_num;
	u16 vers_num;
	u32	part_id;
	u32 data_size;
	u16 data_offset;
	u16 num_elems;
	u32 chksum;
}rmgr_hdr_t;

typedef struct {
	u16 elem_id;
	u16 elem_length;
	u32 elem_offset;
}rmgr_rec_info_t;

typedef union {
	u16	as_u16;
	u8	as_u8[ 2 ];
} u16_and_u8s;

static int
get_file_size( int fd )
{
	struct stat	the_file_stat;
	int		ival = fstat( fd, &the_file_stat );
	int		retval = 0;

	if (ival >= 0)
	  retval = -1;
	else
	  retval = (int) (the_file_stat.st_size);

	return( retval );
}

int
main( int argc, char *argv[] )
{
	rmgr_hdr_t	rf_calib_header;
	int		fd, ival;
	int		actual_size;
	unsigned int	expected_size = 0;
	u16_and_u8s	version_num;

	if (argc < 2)
	{
		fprintf( stderr, "%s requires at least one parameter, the name of a RF calibration file\n", argv[ 0 ] );
		return( EXIT_FAILURE );
	}

	fd = open( argv[ 1 ], O_RDONLY );
	if (fd < 0)
	{
		fprintf( stderr, "%s: cannot access %s\n", argv[ 0 ], argv[ 1 ] );
		return( EXIT_FAILURE );
	}

	actual_size = get_file_size( fd );

	ival = read( fd, &rf_calib_header, sizeof( rf_calib_header ) );
	if (ival != sizeof( rf_calib_header ))
	{
		fprintf( stderr, "%s: cannot read the RF calibration header from %s\n", argv[ 0 ], argv[ 1 ] );
		return( EXIT_FAILURE );
	}

	close( fd );

	expected_size = sizeof( rmgr_hdr_t ) + rf_calib_header.num_elems * sizeof( rmgr_rec_info_t ) + rf_calib_header.data_size;
	if (expected_size > actual_size )
	{
		printf( "Actual size of %s does not match expected size of %u\n", argv[ 1 ], expected_size );
		return( EXIT_FAILURE );
	}

	version_num.as_u16 = rf_calib_header.vers_num;
	printf( "File: %s, RF calibration version: %u.%u\n", argv[ 1 ], version_num.as_u8[ 1 ], version_num.as_u8[ 0 ] );

	return( EXIT_SUCCESS );
}
