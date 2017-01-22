/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_ARC_USER_H
#define __ASM_ARC_USER_H

#include <linux/types.h>
#include <asm/ptrace.h>
#include <asm/page.h>

/*
 * Core file format: The core file is written in such a way that gdb
 * can understand it and provide useful information to the user (under
 * linux we use the `trad-core' bfd).  The file contents are as follows:
 *
 *  upage: 1 page consisting of a user struct that tells gdb
 *	what is present in the file.  Directly after this is a
 *	copy of the task_struct, which is currently not used by gdb,
 *	but it may come in handy at some point.  All of the registers
 *	are stored as part of the upage.  The upage should always be
 *	only one page long.
 *  data: The data segment follows next.  We use current->end_text to
 *	current->brk to pick up all of the user variables, plus any memory
 *	that may have been sbrk'ed.  No attempt is made to determine if a
 *	page is demand-zero or if a page is totally unused, we just cover
 *	the entire range.  All of the addresses are rounded in such a way
 *	that an integral number of pages is written.
 *  stack: We need the stack information in order to get a meaningful
 *	backtrace.  We need to write the data from usp to
 *	current->start_stack, so we round each of these in order to be able
 *	to write an integer number of pages.
 */

/* User mode registers, used for core dumps. */
struct user_regs_struct
{
	/* ------- from struct pt_regs --------- */
    long    reserved1;  /* SP unncessarily hops 1 word, in SAVE_ALL_xxx macros
                           No reason to do that, but changing this would
                           require syncing up tools (gdb) so .....
                        */
	long	bta;		/* bta_l1, bta_l2, erbta */
	long	lp_start;
	long	lp_end;
	long	lp_count;
	long	status32;	/* status32_l1, status32_l2, erstatus */
	long	ret;		/* ilink1, ilink2 or eret*/
	long	blink;
	long	fp;
	long	r26;		/* gp */
	long	r12;
	long	r11;
	long	r10;
	long	r9;
	long	r8;
	long	r7;
	long	r6;
	long	r5;
	long	r4;
	long	r3;
	long	r2;
	long	r1;
	long	r0;
	long	orig_r0;
	long 	orig_r8;
	long	sp;	/* user sp or kernel sp, depending on where we came from  */


	/* -------- from struct callee_regs --------- */
	long	reserved2;
	long	r25;
	long	r24;
	long	r23;
	long	r22;
	long	r21;
	long	r20;
	long 	r19;
	long 	r18;
	long	r17;
	long 	r16;
	long	r15;
	long	r14;
	long	r13;

	long 	efa;    /* break pt addr, req for break points in delay slots */
	long 	stop_pc;	/* give dbg stop_pc directly after checking orig_r8 */
};


struct user {
	struct user_regs_struct	regs;		/* entire machine state */
	size_t		u_tsize;		/* text size (pages) */
	size_t		u_dsize;		/* data size (pages) */
	size_t		u_ssize;		/* stack size (pages) */
	unsigned long	start_code;		/* text starting address */
	unsigned long	start_data;		/* data starting address */
	unsigned long	start_stack;		/* stack starting address */
	long int	signal;			/* signal causing core dump */
	struct regs *	u_ar0;			/* help gdb find registers */
	unsigned long	magic;			/* identifies a core file */
	char		u_comm[32];		/* user command name */
};

#define NBPG			PAGE_SIZE
#define UPAGES			1
#define HOST_TEXT_START_ADDR	(u.start_code)
#define HOST_DATA_START_ADDR	(u.start_data)
#define HOST_STACK_END_ADDR	(u.start_stack + u.u_ssize * NBPG)

#endif /* __ASM_ARC_USER_H */
