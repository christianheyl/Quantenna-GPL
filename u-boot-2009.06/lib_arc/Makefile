#
# Copyright Codito Technologies (www.codito.com)
#
#  lib_arc/Makefile
#
#  Copyright (C) 
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
# 
# Authors : Sandeep Patil (sandeep.patil@codito.com) 
# 			Pradeep Sawlani (pradeep.sawlani@codito.com) 
#


include $(TOPDIR)/config.mk

LIB	= lib$(ARCH).a

AOBJS	=

COBJS	= board.o arclinux.o

OBJS	= $(AOBJS) $(COBJS)

$(LIB):	.depend $(OBJS)
	$(AR) crv $@ $(OBJS)

#########################################################################

.depend:	Makefile $(AOBJS:.o=.S) $(COBJS:.o=.c)
		$(CC) -M $(CFLAGS) $(AOBJS:.o=.S) $(COBJS:.o=.c) > $@

sinclude .depend

#########################################################################
