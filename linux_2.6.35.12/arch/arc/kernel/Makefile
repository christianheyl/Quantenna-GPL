#
# Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.

obj-$(CONFIG_MODULES)   += arcksyms.o module.o

# Pass UTS_MACHINE for user_regset definition
CFLAGS_ptrace.o		+= -DUTS_MACHINE='"$(UTS_MACHINE)"'

obj-y		+= arcksyms.o ptrace.o sys_arc.o setup.o
obj-y		+= troubleshoot.o stacktrace.o event-log.o
obj-$(CONFIG_ARCH_ARC_HLINK)		+= arc_hostlink.o
obj-$(CONFIG_XILINX_TEMAC) += platform.o
obj-$(CONFIG_ISS_MAC) += platform.o
ifdef CONFIG_SMP
obj-y		+= atomic_lock.o bitops_lock.o
endif

obj-$(CONFIG_KERNEL_TEXT_SNAPSHOTS)	+= kdump.o

ifeq ($(CONFIG_GENERIC_CLOCKEVENTS),y)
    obj-y += time.o
else
    obj-y += time-jiff.o
endif

ifeq ($(PROFILE_LINUX_EP), 1)
    obj-y += instrument_hooks.o
endif

ifeq ($(LHOST_PROLOG_CRUMBS), 1)
    obj-y += ruby_crumbs.o
endif

obj-m		:=
obj-n		:=
obj-		:=

ifdef CONFIG_ARC_STACK_UNWIND
obj-y		+= unwind.o
endif

ifdef CONFIG_KPROBES
obj-y       += kprobes.o
endif

CFLAGS_fpu.o   += -mdpfp
obj-$(CONFIG_ARCH_ARC_FPU) += fpu.o

extra-y := init_task.o vmlinux.lds
