#
# linux/arch/arcnommu/tools/Makefile
# Copyright (C) 1996 David S. Miller (dm@engr.sgi.com)
# Copyright (C) 1997 Ralf Baechle (ralf@gnu.ai.mit.edu)
#
TARGET	:= $(srctree)/include/asm-$(ARCH)/offset.h

USE_STANDARD_AS_RULE := true


$(TARGET): offset.h
	cmp -s $^ $@ || (cp $^ $(TARGET).new && mv $(TARGET).new $(TARGET))

#offset.h: offset.s
#	sed -n '/^@@@/s///p' $^ >$@

offset.h: offset.s
#	cp ./offset.h $(srctree)/include/asm-$(ARCH)/offset.h

offset.s: offset.c $(srctree)/include/linux/autoconf.h

#obj-y += offset.o

dep : 

clean:
#	rm -f offset.[hs] $(TARGET).new
#	rm -f $(TARGET)

mrproper: clean
#	rm -f $(TARGET)

#include $(TOPDIR)/Rules.make
