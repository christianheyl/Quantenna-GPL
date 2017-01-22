#
#  Copyright Codito Technologies (www.codito.com)
#
#  cpu/arc/Makefile
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

LIB	= lib$(CPU).a

START	= start.o
OBJS	= cpu.o interrupts.o

all:	.depend $(START) $(LIB)

$(LIB):	$(OBJS)
	$(AR) crv $@ $(OBJS)

#########################################################################

.depend:	Makefile $(START:.o=.S) $(OBJS:.o=.c)
		$(CC) -M $(CFLAGS) $(START:.o=.S) $(OBJS:.o=.c) > $@

sinclude .depend

#########################################################################
