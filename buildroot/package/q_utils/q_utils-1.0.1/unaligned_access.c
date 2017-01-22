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
#include <stdio.h>
#include "../../../../drivers/unaligned_access/unaligned_access.h"

void test(int a) 
{
	printf("unaligned access: %d = ", a);
	printf("%d\n", do_unaligned_access(a));
}

int main(int argc, char **argv) 
{
	test(7);
	return 0;
}
