EXTRA_CFLAGS +=	-Wall -Werror \
		-I../include \
		-I../common \
		-I../drivers/include/shared \
		-I../drivers/include/kernel

obj-y += \
	board_config.o \
	irq.o \
	gpio.o\
	machine.o \
	spi_flash.o \
	spi_api.o \
	clock.o \
	soc.o \
	i2c_bus.o \
	skb_recycle.o \
	pm.o \
	iputil.o \
	dmautil.o \
	health.o

ifeq ($(RUBY_SBM_TEST),1)
	obj-y += ../topaz/hbm.o
endif

ifeq ($(QTN_EXTERNAL_MODULES),y)
emaclib-objs += emac_lib.o ar823x.o mv88e6071.o
obj-$(CONFIG_ARCH_RUBY_EMAC_LIB) += emaclib.o
emac-objs += arasan_emac_ahb.o
obj-$(CONFIG_ARCH_RUBY_EMAC) += emac.o
endif

obj-$(CONFIG_EARLY_PRINTK) += early_printk.o

obj-$(CONFIG_ARC_AHB_PCI_BRIDGE) += pcibios.o
obj-$(CONFIG_ARC_AHB_PCI_BRIDGE) += pcibios_sysfs.o
obj-$(CONFIG_PCI_MSI) += pci_msi.o
obj-$(CONFIG_SERIAL_8250) += serial.o

