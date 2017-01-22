#
# linux/arch/arc/drivers/Makefile
#
#


#obj-$(CONFIG_ARC_SERIAL) += arc700_serial.obsolete.o
obj-$(CONFIG_ARC700_SERIAL) += arc_uart.o
#obj-$(CONFIG_ARC_PS2) += arc_ps2.o
obj-$(CONFIG_ARCTANGENT_EMAC) += arctangent_emac.o
obj-$(CONFIG_ARC_IDE) += arc_ide.o
obj-$(CONFIG_ARCPGU) += arc_pgufb.o arc_vsync.o
obj-$(CONFIG_SOUND_ARC_AC97)    += arc_ac97.o
obj-$(CONFIG_VFB_SIM) += arc_vfb.o
obj-$(CONFIG_XILINX_TEMAC) += xtemac.o
#obj-$(CONFIG_ISS_MAC) += iss_enet.o
# if AA4 + if XBF has PCI
ifeq ($(PLATFORM),AA4)
obj-$(CONFIG_ARC_AHB_PCI_BRIDGE) += arc700_pci.o
endif


