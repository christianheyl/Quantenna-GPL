/*
 *  host/bin2ums.c
 *
 *  Copyright (c) Quantenna Communications Incorporated 2007.
 *  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * An application for converting binary data into the UMS serial download
 * format with an appropriate load address.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "umsdl.h"

static void usage(void)
{
	fprintf(stderr,
		"Usage1: cat <file> | bin2ums <address> > <outfile>\n"
		"         to encode a data file for download\n"
		"Usage2: bin2ums -r <address> > <outfile>\n"
		"         to generate frame to jump to an execution address\n"
		"Usage3: bin2ums -r <address> [bwl] <data> > <outfile>\n"
		"         generates a byte/word/long write of data to address.\n"
		"         Words are written as two byte writes, longs are\n"
		"         written with a single write on the target.\n");
}

int main(int argc, char *argv[])
{
	unsigned long addr, val;
	int rc, len;

	if (argc < 2) {
		usage();
		return -1;
	} else if (argc == 2) {
		addr = strtoul(argv[1], NULL, 0);
	} else if (((argc == 3) || (argc == 5)) && !strcmp(argv[1], "-r")) {
		addr = strtoul(argv[2], NULL, 0);
		if (argc == 3) {
			rc = ums_exec(stdout, addr);
		} else {
			val = strtoul(argv[4], NULL, 0);
			if (*argv[3] == 'b') {
				len = 1;
			} else if (*argv[3] == 'w') {
				len = 2;
			} else if (*argv[3] == 'l') {
				len = 4;
				if (addr & 3) {
					fprintf(stderr, "Bad address alignment\n");
					return -1;
				}
			} else {
				usage();
				return -1;
			}
			rc = ums_single_write(stdout, addr, len, val);
		}
		return !rc;
	} else {
		usage();
		return -1;
	}

	return !bin2ums(stdin, stdout, addr);
}
