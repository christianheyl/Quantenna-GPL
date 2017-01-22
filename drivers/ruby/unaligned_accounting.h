/**
 * Copyright (c) 2008-2012 Quantenna Communications, Inc.
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
#ifndef __UNALIGNED_ACCOUNTING_H
#define __UNALIGNED_ACCOUNTING_H

#ifdef CONFIG_ARC_MISALIGNED_ACCESS

#define UNALIGNED_INST_OPCODES		32
#define UNALIGNED_INSTPTR_BUFSIZE	2048

struct unaligned_access_accounting {
	unsigned long user;
	unsigned long kernel;
	unsigned long read;
	unsigned long write;
	unsigned long inst_16;
	unsigned long inst_32;
	unsigned long skipped;
	unsigned long half;
	unsigned long word;
	unsigned long inst[UNALIGNED_INST_OPCODES];		// count each kind of instruction opcode
	unsigned long kernel_iptr[UNALIGNED_INSTPTR_BUFSIZE];
};

extern struct unaligned_access_accounting unaligned_access_stats;

#endif 

#endif // __UNALIGNED_ACCOUNTING_H

