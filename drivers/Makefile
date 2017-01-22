
QTN_EXTERNAL_MODULES=y
export QTN_EXTERNAL_MODULES

ifneq ($(KERNELRELEASE),)
ifeq ($(CONFIG_QUANTENNA_RUBY),y)
obj-m += ruby/
endif
ifeq ($(CONFIG_QUANTENNA_TOPAZ),y)
obj-m += topaz/
obj-m += fwt/
endif

ifeq ($(filter topaz_host_config,$(board_config)),)
obj-m += qdrv/
obj-m += wlan/
obj-m += i2cbus/
obj-m += se95sensor/
obj-m += pm_interval/
endif
obj-m += bootcfg/
ifeq ($(CONFIG_TOPAZ_PCIE_HOST), y)
obj-m += pcie2/host/quantenna/
endif
else
KERNELDIR	= ../linux
CROSS		= ARCH=arc
INSTALL		= INSTALL_MOD_PATH=../linux/modules
MDIR		= ../drivers/
EXTRA_CFLAGS	+= -Wall -Werror -I../include -I$(KERNELDIR)/include

default: force
	$(MAKE) -C $(KERNELDIR) $(CROSS) M=$(MDIR) modules KBUILD_EXTRA_SYMBOLS=$(MDIR)/extra_kos.symvers

install: force
	$(MAKE) -C $(KERNELDIR) $(CROSS) $(INSTALL) M=$(MDIR) modules_install

clean: force
	$(MAKE) -C $(KERNELDIR) $(CROSS) M=$(MDIR) clean

force:

endif
