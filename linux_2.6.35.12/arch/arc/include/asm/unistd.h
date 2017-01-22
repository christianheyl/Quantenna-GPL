/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vineetg: Dec 1st 2008:
 *  -waitpid no longer needed in syscall Table as uClibc implements it
 *   using wait4 syscall. As a result we no loner need __ARCH_WANT_SYS_WAITPID
 *
 * Amit Bhor: Codito Technologies 2004
 */

#ifndef _ASM_ARC_UNISTD_H
#define _ASM_ARC_UNISTD_H

/* IRQ used fir system calls, make sure that that this is a level 1 interrupt.
 * 6 and 7 default to level 2 so dont use them. Dont forget to change vector
 * table in entry.S if you change this.
 */
#define SYSCALL_IRQ	0x8

#ifndef __ASSEMBLY__
/*
 * This file contains the system call numbers.
 */
#define __NR_exit		    1
#define __NR_fork		    2
#define __NR_read		    3
#define __NR_write		    4
#define __NR_open		    5
#define __NR_close		    6
//#define __NR_waitpid	    7
#define __NR_creat		    8
#define __NR_link		    9
#define __NR_unlink		    10
#define __NR_execve		    11
#define __NR_chdir		    12
#define __NR_time		    13
#define __NR_mknod		    14
#define __NR_chmod		    15
#define __NR_chown		    16
#define __NR_break		    17
#define __NR_oldstat	    18
#define __NR_lseek		    19
#define __NR_getpid		    20
#define __NR_mount		    21
#define __NR_umount		    22
#define __NR_setuid		    23
#define __NR_getuid		    24
#define __NR_stime		    25
#define __NR_ptrace		    26
#define __NR_alarm		    27
#define __NR_oldfstat	    28
#define __NR_pause		    29
#define __NR_utime		    30
#define __NR_stty		    31
#define __NR_gtty		    32
#define __NR_access		    33
#define __NR_nice		    34
#define __NR_ftime		    35
#define __NR_sync		    36
#define __NR_kill		    37
#define __NR_rename		    38
#define __NR_mkdir		    39
#define __NR_rmdir		    40
#define __NR_dup		    41
#define __NR_pipe		    42
#define __NR_times		    43
#define __NR_prof		    44
#define __NR_brk		    45
#define __NR_setgid		    46
#define __NR_getgid		    47
#define __NR_signal		    48
#define __NR_geteuid	    49
#define __NR_getegid	    50
#define __NR_acct		    51
#define __NR_umount2	    52
#define __NR_lock		    53
#define __NR_ioctl		    54
#define __NR_fcntl		    55
#define __NR_mpx		    56
#define __NR_setpgid	    57
#define __NR_ulimit		    58
#define __NR_oldolduname    59
#define __NR_umask		    60
#define __NR_chroot		    61
#define __NR_ustat		    62
#define __NR_dup2		    63
#define __NR_getppid	    64
#define __NR_getpgrp	    65
#define __NR_setsid		    66
#define __NR_sigaction		67
        /* __NR_sgetmask	68 obsolete, supersseded by sigprocmask */
        /* __NR_ssetmask	69 obsolete */
#define __NR_setreuid		70
#define __NR_setregid		71
#define __NR_sigsuspend		72
#define __NR_sigpending		73
#define __NR_sethostname	74
#define __NR_setrlimit		75
#define __NR_old_getrlimit	76
#define __NR_getrusage		77
#define __NR_gettimeofday	78
#define __NR_settimeofday	79
#define __NR_getgroups		80
#define __NR_setgroups		81
#define __NR_select		    82
#define __NR_symlink		83
#define __NR_oldlstat		84
#define __NR_readlink		85
#define __NR_uselib		    86
#define __NR_swapon		    87
#define __NR_reboot		    88
#define __NR_readdir		89
#define __NR_mmap		    90
#define __NR_munmap		    91
#define __NR_truncate		92
#define __NR_ftruncate		93
#define __NR_fchmod		    94
#define __NR_fchown		    95
#define __NR_getpriority	96
#define __NR_setpriority	97
#define __NR_profil		    98
#define __NR_statfs		    99
#define __NR_fstatfs		100
#define __NR_ioperm		    101
#define __NR_socketcall		102
#define __NR_syslog		    103
#define __NR_setitimer		104
#define __NR_getitimer		105
#define __NR_stat		    106
#define __NR_lstat		    107
#define __NR_fstat		    108
#define __NR_olduname		109
#define __NR_iopl		    110  /* not supported */
#define __NR_vhangup		111
#define __NR_idle		    112 /* Obsolete */
        /* __NR_vm86	    113  not supported */
#define __NR_wait4		    114
#define __NR_swapoff		115
#define __NR_sysinfo		116
#define __NR_ipc		    117
#define __NR_fsync		    118
#define __NR_sigreturn		119
#define __NR_clone		    120
#define __NR_setdomainname	121
#define __NR_uname		    122
#define __NR_cacheflush		123
#define __NR_adjtimex		124
#define __NR_mprotect		125
#define __NR_sigprocmask	126
#define __NR_create_module	127
#define __NR_init_module	128
#define __NR_delete_module	129
#define __NR_get_kernel_syms	130
#define __NR_quotactl		    131
#define __NR_getpgid		    132
#define __NR_fchdir		        133
#define __NR_bdflush		    134
#define __NR_sysfs		        135
#define __NR_personality	    136
#define __NR_afs_syscall	    137 /* Syscall for Andrew File System */
#define __NR_setfsuid		    138
#define __NR_setfsgid		    139
#define __NR__llseek		    140
#define __NR_getdents		    141
#define __NR__newselect		    142
#define __NR_flock		        143
#define __NR_msync		        144
#define __NR_readv		        145
#define __NR_writev		        146
#define __NR_getsid		        147
#define __NR_fdatasync		    148
#define __NR__sysctl		    149
#define __NR_mlock		        150
#define __NR_munlock		    151
#define __NR_mlockall		    152
#define __NR_munlockall		    153
#define __NR_sched_setparam	        154
#define __NR_sched_getparam	        155
#define __NR_sched_setscheduler		156
#define __NR_sched_getscheduler		157
#define __NR_sched_yield		    158
#define __NR_sched_get_priority_max	159
#define __NR_sched_get_priority_min	160
#define __NR_sched_rr_get_interval	161
#define __NR_nanosleep		        162
#define __NR_mremap		            163
#define __NR_setresuid		        164
#define __NR_getresuid		    165
#define __NR_query_module	    167
#define __NR_poll		        168
#define __NR_nfsservctl		    169
#define __NR_setresgid		    170
#define __NR_getresgid		    171
#define __NR_prctl		        172
#define __NR_rt_sigreturn	    173
#define __NR_rt_sigaction	    174
#define __NR_rt_sigprocmask	    175
#define __NR_rt_sigpending	    176
#define __NR_rt_sigtimedwait	177
#define __NR_rt_sigqueueinfo	178
#define __NR_rt_sigsuspend	    179
#define __NR_pread		        180
#define __NR_pwrite		        181
#define __NR_lchown		        182
#define __NR_getcwd		        183
#define __NR_capget		        184
#define __NR_capset		        185
#define __NR_sigaltstack	    186
#define __NR_sendfile		    187
#define __NR_getpmsg		    188	/* some people actually want streams */
#define __NR_putpmsg		    189	/* some people actually want streams */
#define __NR_vfork		        190
#define __NR_getrlimit	    	191
#define __NR_mmap2		        192
#define __NR_truncate64		    193
#define __NR_ftruncate64	    194
#define __NR_stat64		        195
#define __NR_lstat64		    196
#define __NR_fstat64		    197
#define __NR_chown32		    198
#define __NR_getuid32		    199
#define __NR_getgid32		    200
#define __NR_geteuid32		    201
#define __NR_getegid32		    202
#define __NR_setreuid32		    203
#define __NR_setregid32		    204
#define __NR_getgroups32	    205
#define __NR_setgroups32	    206
#define __NR_fchown32		    207
#define __NR_setresuid32	    208
#define __NR_getresuid32	    209
#define __NR_setresgid32	    210
#define __NR_getresgid32	    211
#define __NR_lchown32		    212
#define __NR_setuid32		    213
#define __NR_setgid32		    214
#define __NR_setfsuid32		    215
#define __NR_setfsgid32		    216
#define __NR_pivot_root		    217
#define __NR_getdents64		    220
#define __NR_fcntl64            221
#define __NR_gettid		        224
#define __NR_lookup_dcookie     225
#define __NR_statfs64           226
#define __NR_waitpid            227     /* This is no loger supported */
#define __NR_mq_open            228
#define __NR_mq_unlink          229
#define __NR_mq_timedreceive    230
#define __NR_mq_notify          231
#define __NR_mq_getsetattr      232
#define __NR_mq_timedsend       233
#define __NR_timer_create       234
#define __NR_timer_settime      235
#define __NR_timer_gettime      236
#define __NR_timer_getoverrun   237
#define __NR_timer_delete       238
#define __NR_clock_settime      239
#define __NR_clock_gettime      240
#define __NR_clock_getres       241
#define __NR_clock_nanosleep    242
#define __NR_sched_setaffinity  243
#define __NR_sched_getaffinity  244
#define __NR_waitid             245
#define __NR_restart_syscall    246
#define __NR_pread64            247
#define __NR_pwrite64           248
#define __NR_mincore            249
#define __NR_madvise            250
#define __NR_readahead          251
#define __NR_setxattr           252
#define __NR_lsetxattr          253
#define __NR_fsetxattr          254
#define __NR_getxattr           255
#define __NR_lgetxattr          256
#define __NR_fgetxattr          257
#define __NR_listxattr          258
#define __NR_llistxattr         259
#define __NR_flistxattr         260
#define __NR_removexattr        261
#define __NR_lremovexattr       262
#define __NR_fremovexattr       263
#define __NR_tkill              264
#define __NR_sendfile64         265
#define __NR_futex              266
#define __NR_io_setup           267
#define __NR_io_destroy         268
#define __NR_io_getevents       269
#define __NR_io_submit          270
#define __NR_io_cancel          271
#define __NR_fadvise64          272
#define __NR_exit_group         273
#define __NR_epoll_create       274
#define __NR_epoll_ctl          275
#define __NR_epoll_wait         276
#define __NR_remap_file_pages   277
#define __NR_set_tid_address    278
#define __NR_fstatfs64          279
#define __NR_tgkill             280
#define __NR_utimes             281
#define __NR_fadvise64_64       282
#define __NR_mbind              283
#define __NR_get_mempolicy      284
#define __NR_set_mempolicy      285
#define __NR_kexec_load         286
#define __NR_add_key            287
#define __NR_request_key        288
#define __NR_keyctl             289
#define __NR_ioprio_set         290
#define __NR_ioprio_get         291
#define __NR_inotify_init       292
#define __NR_inotify_add_watch  293
#define __NR_inotify_rm_watch   294
#define __NR_migrate_pages      295
#define __NR_openat             296
#define __NR_mkdirat            297
#define __NR_mknodat            298
#define __NR_fchownat           299
#define __NR_futimesat          300
#define __NR_fstatat64          301
#define __NR_unlinkat           302
#define __NR_renameat           303
#define __NR_linkat             304
#define __NR_symlinkat          305
#define __NR_readlinkat         306
#define __NR_fchmodat           307
#define __NR_faccessat          308
#define __NR_pselect6           309
#define __NR_ppoll              310
#define __NR_unshare            311
#define __NR_set_robust_list    312
#define __NR_get_robust_list    313
#define __NR_splice             314
#define __NR_sync_file_range    315
#define __NR_tee                316
#define __NR_vmsplice           317
#define __NR_move_pages         318
#define __NR_getcpu             319
#define __NR_epoll_pwait        320
#define __NR_utimensat          321
#define __NR_signalfd           322
#define __NR_timerfd_create     323
#define __NR_eventfd            324
#define __NR_fallocate          325
#define __NR_timerfd_settime    326
#define __NR_timerfd_gettime    327
#define __NR_signalfd4          328
#define __NR_eventfd2           329
#define __NR_epoll_create1      330
#define __NR_dup3               331
#define __NR_pipe2              332
#define __NR_inotify_init1       333
#define __NR_preadv             334
#define __NR_pwritev            335

#define __NR_socket			    336
#define __NR_bind			    337
#define __NR_connect		    338
#define __NR_listen			    339
#define __NR_accept			    340
#define __NR_getsockname	    341
#define __NR_getpeername	    342
#define __NR_socketpair		    343
#define __NR_send			    344
#define __NR_sendto			    345
#define __NR_recv			    346
#define __NR_recvfrom		    347
#define __NR_shutdown		    348
#define __NR_setsockopt		    349
#define __NR_getsockopt		    350
#define __NR_sendmsg		    351
#define __NR_recvmsg		    352
#define __NR_arc_settls		    353
#define __NR_arc_gettls		    354

#endif	/* ! __ASSEMBLY__ */

#define NR_syscalls             354



#define _syscall0(type, name)	                \
type name(void)				        \
{					        \
  long __res;					\
  __asm__ __volatile__ ("mov	r8, %1\n\t"	\
  			"trap0 \n\t"	\
			"nop    \n\t"\
                        "nop    \n\t"\
	                "mov	%0, r0"		\
			: "=r" (__res)		\
			: "i" (__NR_##name)	\
			: "cc", "r0", "r8");		\
  if ((unsigned long)(__res) >= (unsigned long)(-125)) {\
    errno = -__res;					\
    __res = -1;						\
  }							\
  return (type)__res;					\
}

#define _syscall1(type, name, atype, a)	\
type name(atype a)				\
{										\
  long __res;									\
  __asm__ __volatile__ ("mov	r0, %2\n\t"					\
			"mov	r8, %1\n\t"					\
  			"trap0 \n\t"					\
			"nop    \n\t"\
                        "nop    \n\t"\
	                "mov	%0, r0"					        \
			: "=r" (__res)						\
			: "i" (__NR_##name),					\
			  "r" ((long)a)					\
			: "cc", "r0", "r8");					\
  if ((unsigned long)(__res) >= (unsigned long)(-125)) {			\
    errno = -__res;								\
    __res = -1;									\
  }										\
  return (type)__res;								\
}

#define _syscall2(type, name, atype, a, btype, b)	\
type name(atype a, btype b)				\
{										\
  long __res;									\
  __asm__ __volatile__ ("mov	r1, %3\n\t"					\
  			"mov	r0, %2\n\t"					\
			"mov	r8, %1\n\t"					\
  			"trap0 \n\t"					\
  			"nop    \n\t"\
                        "nop    \n\t"\
	                "mov	%0, r0"					        \
			: "=r" (__res)						\
			: "i" (__NR_##name),					\
			  "r" ((long)a),					\
			  "r" ((long)b)					\
			: "cc", "r0", "r1", "r8");				\
  if ((unsigned long)(__res) >= (unsigned long)(-125)) {			\
    errno = -__res;								\
    __res = -1;									\
  }										\
  return (type)__res;								\
}
#define _syscall3(type, name, atype, a, btype, b, ctype, c)	\
type name(atype a, btype b, ctype c)				\
{										\
  long __res;									\
  __asm__ __volatile__ (						\
			"ld	r2, %4\n\t"					\
			"mov	r1, %3\n\t"					\
  			"mov	r0, %2\n\t"					\
			"mov	r8, %1\n\t"					\
  			"trap0 \n\t"					\
			"nop    \n\t"\
                        "nop    \n\t"\
	                "mov	%0, r0"					        \
			: "=r" (__res)						\
			: "i" (__NR_##name),					\
			  "r" ((long)a),					\
			  "r" ((long)b),					\
			  "m" ((long)c)					\
			: "cc", "r0", "r1", "r2", "r8");			\
  if ((unsigned long)(__res) >= (unsigned long)(-125)) {			\
    errno = -__res;								\
    __res = -1;									\
  }										\
  return (type)__res;								\
}

#define _syscall4(type, name, atype, a, btype, b, ctype, c, dtype, d)	\
type name(atype a, btype b, ctype c, dtype d)				\
{										\
  long __res;									\
  __asm__ __volatile__ ("mov	r3, %5\n\t"					\
			"mov	r2, %4\n\t"					\
			"mov	r1, %3\n\t"					\
  			"mov	r0, %2\n\t"					\
			"mov	r8, %1\n\t"					\
  			"trap0 \n\t"					\
                        "nop    \n\t"\
                        "nop    \n\t"\
	                "mov	%0, r0"					        \
			: "=r" (__res)						\
			: "i" (__NR_##name),					\
			  "r" ((long)a),					\
			  "r" ((long)b),					\
			  "r" ((long)c),					\
			  "r" ((long)d)					\
			: "cc", "r0", "r1", "r2", "r3",			        \
			  "r8");					        \
  if ((unsigned long)(__res) >= (unsigned long)(-125)) {			\
    errno = -__res;								\
    __res = -1;									\
  }										\
  return (type)__res;								\
}

#define _syscall5(type, name, atype, a, btype, b, ctype, c, dtype, d, etype, e)	\
type name(atype a, btype b, ctype c, dtype d, etype e)				\
{										\
  long __res;									\
  __asm__ __volatile__ ("mov	r4, %6\n\t"					\
			"mov	r3, %5\n\t"					\
			"mov	r2, %4\n\t"					\
			"mov	r1, %3\n\t"					\
  			"mov	r0, %2\n\t"					\
			"mov	r8, %1\n\t"					\
  			"trap0 \n\t"					\
                        "nop    \n\t"\
                        "nop    \n\t"\
			"mov	%0, r0"					        \
			: "=r" (__res)						\
			: "i" (__NR_##name),					\
			  "r" ((long)a),					\
			  "r" ((long)b),					\
			  "r" ((long)c),					\
			  "r" ((long)d),					\
			  "r" ((long)e)						\
			: "cc", "r0", "r1", "r2", "r3",			        \
			  "r4", "r8");					        \
  if ((unsigned long)(__res) >= (unsigned long)(-125)) {			\
    errno = -__res;								\
    __res = -1;									\
  }										\
  return (type)__res;								\
}

#define __ARCH_WANT_SYS_TIME
#define __ARCH_WANT_SYS_OLDUMOUNT
#define __ARCH_WANT_SYS_ALARM
#define __ARCH_WANT_SYS_PAUSE
#define __ARCH_WANT_SYS_UTIME
#define __ARCH_WANT_SYS_NICE
#define __ARCH_WANT_SYS_SIGPENDING
#define __ARCH_WANT_SYS_OLD_GETRLIMIT
#define __ARCH_WANT_OLD_READDIR
#define __ARCH_WANT_SYS_LLSEEK
#define __ARCH_WANT_SYS_RT_SIGACTION
#define __ARCH_WANT_SYS_SIGPROCMASK
#define __ARCH_WANT_SYS_RT_SIGSUSPEND
#define __ARCH_WANT_COMPAT_SYS_RT_SIGSUSPEND
#define __ARCH_WANT_SYS_GETPGRP
#define __ARCH_WANT_STAT64
#define __ARCH_WANT_IPC_PARSE_VERSION
#define __ARCH_WANT_SYS_SOCKETCALL
#define __ARCH_WANT_SYS_IPC

// ARC uclibc implements waitpid() using wait4 so dont need this anymore
//#define __ARCH_WANT_SYS_WAITPID

/*
 * "Conditional" syscalls
 *
 * What we want is __attribute__((weak,alias("sys_ni_syscall"))),
 * but it doesn't work on all toolchains, so we just do it by hand
 */

#define cond_syscall(x) asm(".weak\t" #x "\n\t.set\t" #x ",sys_ni_syscall")


#endif	/* _ASM_ARC_UNISTD_H */
