#
# Quantenna Communications Inc. Driver Makefile
#
EXTRA_CFLAGS	+= -Wall -Werror -I../drivers -I../include -I../drivers/include/shared -I../drivers/include/kernel
EXTRA_CFLAGS    += -mlong-calls

ifneq ($(KERNELRELEASE),)
pm_interval-objs += pm_interval_drv.o
obj-m   += pm_interval.o
else

INSTALL	= INSTALL_MOD_PATH=../linux/modules
PWD	:= $(shell pwd)

default:
	$(MAKE) -C $(KERNELDIR) $(CROSS) M=$(PWD) modules

install:
	$(MAKE) -C $(KERNELDIR) $(CROSS) $(INSTALL) M=$(PWD) modules_install

endif

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions

