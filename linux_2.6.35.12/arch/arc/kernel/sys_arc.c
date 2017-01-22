/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Vineetg: July 2009
 *    -kernel_execve inline asm optimised to prevent un-needed reg shuffling
 *     Now have 6 instructions in  generated code as opposed to 16.
 */

#include <linux/errno.h>
#include <linux/msg.h>
#include <linux/shm.h>
#include <linux/syscalls.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>


struct sel_arg_struct {
    unsigned long n;
    fd_set *inp, *outp, *exp;
    struct timeval *tvp;
};

asmlinkage int old_select(struct sel_arg_struct *arg)
{
    struct sel_arg_struct a;

    if (copy_from_user(&a, arg, sizeof(a)))
        return -EFAULT;
    /* sys_select() does the appropriate kernel locking */
    return sys_select(a.n, a.inp, a.outp, a.exp, a.tvp);
}

int kernel_execve(const char *filename, char *const argv[], char *const envp[])
{
    /* Although the arguments (order, number) to this function are
     * same as sys call, we don't need to setup args in regs again.
     * However in case mainline kernel changes the order of args to
     * kernel_execve, that assumtion will break.
     * So to be safe, let gcc know the args for sys call.
     * If they match no extra code will be generated
     */
    register int arg2 asm ("r1") = (int)argv;
    register int arg3 asm ("r2") = (int)envp;

    register int filenm_n_ret asm ("r0") = (int)filename;

    __asm__ __volatile__(
                 "mov   r8, %1\n\t"
                 "trap0 \n\t"
                 :"+r"(filenm_n_ret)
                 :"i"(__NR_execve),  "r"(arg2), "r"(arg3)
                 :"r8","memory");

    return filenm_n_ret;
}
EXPORT_SYMBOL(kernel_execve);
