#
# Quantenna Communications Inc. Driver Makefile
#
# Author: Mats Aretun
# 
#
EXTRA_CFLAGS	+= -Wall -Werror -I../drivers -I../include
EXTRA_CFLAGS	+= -mlong-calls
ifneq ($(KERNELRELEASE),)
                       #---------------------------------#
                       # Call from kernel build system   #
                       #---------------------------------#

#EXTRA_CFLAGS	+= -Wall -Werror -DQDRV_DEBUG -DQDRV -DQDRV_FEATURE_WGB -I../drivers -I../include
qtsens-objs		+=	temp_sensor.o
obj-m			+=	qtsens.o

else
                       #---------------------------------#
                       # Call from external build system #
                       #---------------------------------#
#KERNELDIR	?= /lib/modules/$(shell uname -r)/build
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
