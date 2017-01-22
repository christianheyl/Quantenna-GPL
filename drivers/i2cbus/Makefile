#
# Quantenna Communications Inc. Driver Makefile
#
# Author: Mats Aretun
# 
#

EXTRA_CFLAGS	+= -Wall -Werror

ifneq ($(KERNELRELEASE),)

i2cbus-objs	+= i2c-qtn-platdrv.o
i2cbus-objs	+= i2c-qtn-core.o
obj-m	+= i2cbus.o

else
KERNELDIR	?= ../../linux
INSTALL		= INSTALL_MOD_PATH=../linux/modules
CROSS		= ARCH=arc CROSS_COMPILE=/usr/local/ARC/gcc/bin/arc-linux-uclibc-
PWD			:= $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) $(CROSS) M=$(PWD) modules

install:
	$(MAKE) -C $(KERNELDIR) $(CROSS) $(INSTALL) M=$(PWD) modules_install

endif

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions

depend .depend dep:
	$(CC) $(CFLAGS) -M *.c > .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif
