/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASMARCTANGENT_ELF_H
#define __ASMARCTANGENT_ELF_H

/*
 * ELF register definitions..
 */

#include <asm/ptrace.h>
#include <asm/user.h>

#define  R_ARC_32		0x4
#define  R_ARC_32_ME		0x1B
#define  R_ARC_S25H_PCREL	0x10
#define  R_ARC_S25W_PCREL	0x11

typedef unsigned long elf_greg_t;

#define EM_ARCTANGENT	0x5D

#ifdef CONFIG_ARC_ELF_CORE_FIX
/* Backported from 3.14.24 kernel to fix core dump format */
#define ELF_NGREG	(sizeof(struct user_regs_struct) / sizeof(elf_greg_t))
#else
/* core dump regs is in the order pt_regs, callee_regs, stop_pc (for gdb's sake) */
#define ELF_NGREG ((sizeof (struct pt_regs)+ sizeof (struct callee_regs) + sizeof(unsigned long))/ sizeof(elf_greg_t))
#endif

typedef elf_greg_t elf_gregset_t[ELF_NGREG];

/* A placeholder, ARC does not have any fp regs */
typedef unsigned long elf_fpregset_t;

/*
 * This is used to ensure we don't load something for the wrong architecture.
 */
#define elf_check_arch(x) ((x)->e_machine == EM_ARCTANGENT)

/*
 * These are used to set parameters in the core dumps.
 */
#define ELF_CLASS	ELFCLASS32
#define ELF_DATA	ELFDATA2LSB
#define ELF_ARCH	EM_ARCTANGENT

#ifdef __KERNEL__

#define USE_ELF_CORE_DUMP
#define CORE_DUMP_USE_REGSET
#define ELF_EXEC_PAGESIZE PAGE_SIZE

/* This is the location that an ET_DYN program is loaded if exec'ed.  Typical
   use of this is to invoke "./ld.so someprog" to test out a new version of
   the loader.  We need to make sure that it is out of the way of the program
   that it will "exec", and that there is sufficient room for the brk.  */

#define ELF_ET_DYN_BASE	(2 * TASK_SIZE / 3)

/* When the program starts, a1 contains a pointer to a function to be
   registered with atexit, as per the SVR4 ABI.  A value of 0 means we
   have no such handler.  */
#define ELF_PLAT_INIT(_r, load_addr)	(_r)->r0 = 0

extern void arc_elf_core_copy_regs(elf_gregset_t *,struct pt_regs *);

#define ELF_CORE_COPY_REGS(elf_reg, regs)       \
                arc_elf_core_copy_regs(&(elf_reg),regs);

/* This is needed to dump the non-fatal threads in case of multi-threaded
 * program
 */
extern int arc_elf_core_copy_tsk_regs(struct task_struct *, elf_gregset_t *);

#define ELF_CORE_COPY_TASK_REGS(tsk, elf_regs)  \
                arc_elf_core_copy_tsk_regs(tsk, elf_regs)

/* This yields a mask that user programs can use to figure out what
   instruction set this cpu supports. */

#define ELF_HWCAP	(0)

/* This yields a string that ld.so will use to load implementation
   specific libraries for optimization.  This is more specific in
   intent than poking at uname or /proc/cpuinfo. */

#define ELF_PLATFORM	(NULL)

#define SET_PERSONALITY(ex) set_personality(PER_LINUX)
#endif

#endif
