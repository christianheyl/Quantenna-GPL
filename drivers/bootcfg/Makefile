#
# Quantenna Communications Inc. Driver Makefile
#
# Author: Jim Wood
#

EXTRA_CFLAGS	+= -Wall -Werror -I../drivers -I../include
EXTRA_CFLAGS    += -mlong-calls
EXTRA_CFLAGS += -DQTN_DEBUG
ifneq ($(KERNELRELEASE),)
bootcfg-objs += bootcfg_drv.o \
	bootcfg_mtd.o \
	bootcfg_eeprom.o \
	bootcfg_store_init.o \
	bootcfg_compress.o \
	bootcfg_file.o

obj-m   += bootcfg.o
else
KERNELDIR	?= ../../linux-2.6.20.1
INSTALL		= INSTALL_MOD_PATH=../linux/modules
CROSS		= ARCH=arm CROSS_COMPILE=../buildroot/build_arm/staging_dir/bin/arm-linux-
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
