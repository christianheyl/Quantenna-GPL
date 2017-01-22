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

#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define  CONFIG_DATA_END   "config_data_end"
#define  BOOTCFG_FILE_SYS  "/proc/bootcfg"
#define  BOOTCFG_ENV_PATH  "/proc/bootcfg/env"
#define  BOOTCFG_ENV_SIZE  4096
#define  MAX_NUMBER_VARS   2048

typedef struct variable_entry
{
	char	*name;
	char	*value;
} variable_entry;


static variable_entry	orig_env_table[ MAX_NUMBER_VARS ];
static variable_entry	update_env_table[ MAX_NUMBER_VARS ];

static char		orig_env_buffer[ BOOTCFG_ENV_SIZE ];
static char		update_env_buffer[ BOOTCFG_ENV_SIZE ];


static int
is_value_file_reference( const char *env_value )
{
	int		 retval = 1;
	int		 iter;
	const char	*tmpaddr = env_value;

	if (strncmp( "cfg ", env_value, 4 ) != 0 && strncmp( "cfg\t", env_value, 4 ) != 0)
	  retval = 0;

	if (retval != 0)
	{
		tmpaddr += 3;

		for (iter = 0; iter < 2 && retval != 0; iter++)
		{
			while (*tmpaddr == ' ' || *tmpaddr == '\t')
			  tmpaddr++;

			if (*tmpaddr != '\0')
			{
				unsigned int	uval = 0;
				int		ival = sscanf( tmpaddr, "%x", &uval );

				if (ival != 1)
				  retval = 0;
			}
			else
			  retval = 0;

			if (retval != 0)
			{
				while (*tmpaddr != ' ' && *tmpaddr != '\t' && *tmpaddr != '\0')
				  tmpaddr++;
			}
		}
	}

	return( retval );
}

static int
locate_variable_entry( const char *variable_name, const variable_entry *entry_table, int table_size )
{
	int	retval = -1;
	int	iter;

	if (table_size > 0)
	{
		for (iter = 0; iter < table_size && retval < 0; iter++)
		{
			if (strcmp( entry_table[ iter ].name, variable_name ) == 0)
			{
				retval = iter;
			}
		}
	}

	return( retval );
}

static int
load_env_file( const char *env_path, variable_entry *entry_table, char *buffer_base )
{
	char	 bootcfg_env_line[ 256 ];
	FILE	*fh;
	int	 env_index = 0;
	char	*buffer_addr = buffer_base;

	fh = fopen( env_path, "r" );
	if (fh == NULL)
	{
		fprintf( stderr, "Cannot access %s\n", env_path );
		return( -1 );
	}

	while (fgets( &bootcfg_env_line[ 0 ], sizeof( bootcfg_env_line ), fh ) != NULL)
	{
		char	*addr_value = strchr( &bootcfg_env_line[ 0 ], '\n' );

		if (addr_value != NULL)
		  *addr_value = '\0';

		addr_value = strchr( &bootcfg_env_line[ 0 ], '=' );

		if (addr_value != NULL)
		{
			int	length_name, length_value;

			*addr_value = '\0';
			addr_value++;

			length_name = strlen( &bootcfg_env_line[ 0 ] );
			length_value = strlen( addr_value );

			if ((buffer_addr - buffer_base) + length_name + length_value + 2 >= BOOTCFG_ENV_SIZE)
			{
				fprintf( stderr, "Size of environment from %s greater than %d\n", env_path, BOOTCFG_ENV_SIZE );
				return( -1 );
			}

			strcpy( buffer_addr, &bootcfg_env_line[ 0 ] );
			entry_table[ env_index ].name = buffer_addr;
			buffer_addr += (length_name + 1);

			strcpy( buffer_addr, addr_value );
			entry_table[ env_index ].value = buffer_addr;
			buffer_addr += (length_value + 1);

			env_index++;

			if (env_index >= MAX_NUMBER_VARS)
			{
				fprintf( stderr, "More than %d entries in %s!\n", MAX_NUMBER_VARS, env_path );
				return( -1 );
			}
		}
	}

	fclose( fh );

	return( env_index );
}

static int
update_bootcfg_env(
	const char *variable_name,
	const char *variable_value,
	const variable_entry *orig_table,
	int size_orig_table
)
{
	int	retval = 0;
  /*
   * Never update "config_data_end"; it is managed by the bootcfg file system.
   */
	if (strcmp( variable_name, CONFIG_DATA_END ) != 0)
	{
		if (is_value_file_reference( variable_value ) != 0)
		{
			char	bootcfg_file[ 48 ];

			strcpy( &bootcfg_file[ 0 ], BOOTCFG_FILE_SYS );
			strcat( &bootcfg_file[ 0 ], "/" );
			strcat( &bootcfg_file[ 0 ], variable_name );

			if (access( &bootcfg_file[ 0 ], F_OK ) != 0)
			{
				printf( "Missing %s file %s\n", BOOTCFG_FILE_SYS, variable_name );
			}
		}
		else
		{
			int	index_orig = locate_variable_entry( variable_name, orig_table, size_orig_table );
			int	update_entry = 0;

			if (index_orig >= 0)
			{
				if (strcmp( orig_table[ index_orig ].value, variable_value ) != 0)
				  update_entry = 1;
			}
			else
			  update_entry = 1;

			if (update_entry != 0)
			{
				FILE	*fh = fopen( BOOTCFG_ENV_PATH, "w" );

				if (fh != NULL)
				{
					printf( "Updating %s with value %s\n", variable_name, variable_value );
					fprintf( fh, "%s %s", variable_name, variable_value );

					fclose( fh );
				}
				else
				  retval = -1;
			}
		}
	}

	return( retval );
}

int
main( int argc, char *argv[] )
{
	int	iter;
	int	count_update, count_orig;
	
	if (argc < 2)
	{
		fprintf( stderr, "usage: %s <path to backup of boot cfg env file>\n", argv[ 0 ] );
		return( 1 );
	}

	count_orig = load_env_file( BOOTCFG_ENV_PATH, &orig_env_table[ 0 ], &orig_env_buffer[ 0 ] );
	count_update = load_env_file( argv[ 1 ], &update_env_table[ 0 ], &update_env_buffer[ 0 ] );

	for (iter = 0; iter < count_update; iter++)
	{
		update_bootcfg_env(
			 update_env_table[ iter ].name, 
			 update_env_table[ iter ].value, 
			&orig_env_table[ 0 ],
			 count_orig
		);
	}

	return( 0 );
}
