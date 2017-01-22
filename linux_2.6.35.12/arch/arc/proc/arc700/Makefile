#
# Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
#
obj-y += entry.o process.o signal.o traps.o

#----------------------------------------------------------------------
# for unwinding from __switch_to, we need to write it in "C"
# otherwise hand written assembler is a more "natural" way to code it
#
ifdef CONFIG_ARC_STACK_UNWIND
CFLAGS_ctx_sw.o += -fno-omit-frame-pointer
obj-y += ctx_sw.o
else
obj-y += ctx_sw_asm.o
endif
#---------------------------------------------------------------------

ifdef CONFIG_SMP
obj-y += smp.o idu.o
endif

extra-y += head.o

head.o: head.S

entry.o: entry.S
