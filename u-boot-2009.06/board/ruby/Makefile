#########################################################################

include $(TOPDIR)/config.mk

CFLAGS += -Wall -Werror

RTL8367B_SRC=./rtl8367b

LIB	= lib$(BOARD).a

OBJS	:= \
	ruby.o \
	ddr.o \
	board_cfg.o \
	timer.o \
	serial.o \
	progress.o \
	gpio.o \
	reset.o \
	intr.o \
	arasan-emac-ahb.o \
	pcie.o \
	ar8236.o \
	ar8237.o \
	info.o \
	spi_flash.o \
	spi_api.o \
	cmd_qtnboot.o \
	cmd_qtnboard.o \
	uc.o \
	cmd_memtest.o \
	tftp_bootp_loop.o \
	pcie2.o \
	$(RTL8367B_SRC)/rtl8367b_init.o \
	$(RTL8367B_SRC)/rtl8367b_api.o \
	$(RTL8367B_SRC)/rtl8367b_smi.o

ifneq ($(filter topaz_amber_medium_config, $(board_config)),)
OBJS += amber_uboot.o
endif

SOBJS	:= \
	vectors.o \
	flip.o \
	start.o \
	newlib_memcpy-700.o \
	newlib_memcmp.o \
	newlib_memset.o \
	newlib_io_copy.o

UC_OBJS	:= \
	muc.o \
	dsp.o \
	uc_start.o \
	muc_start.o \
	dsp_start.o

$(LIB):	$(OBJS) $(SOBJS) uc_objs
	$(AR) crv $@ $(OBJS) $(SOBJS) $(UC_OBJS)

clean:
	rm -f $(SOBJS) $(OBJS) $(UC_OBJS)

distclean:	clean
	rm -f $(LIB) core *.bak .depend

$(obj)u-boot.lds: u-boot.lds.S force
	$(CPP) -I../../../ -I../../ $(CPPFLAGS) -D__ASSEMBLY__ -P $< > $@

.PHONY: force

#########################################################################

# TODO: implement dependency to MuC/DSP code (now it is compiled each time make run)
UC_C_FLAGS = $(PLATFORM_CPPFLAGS_PATH) -I$(TOPDIR)/../include/qtn -I$(TOPDIR)/include -fno-builtin -ffreestanding -nostdinc -isystem $(gccincdir) -D__KERNEL__ -Os -mno-volatile-cache -mno-sdata
UC_ASM_FLAGS = $(UC_C_FLAGS) -D__ASSEMBLY__
MUC_FLAGS = -mA7
DSP_FLAGS = -mA6

uc_objs:
	$(CC) $(UC_ASM_FLAGS) $(DSP_FLAGS) -c dsp_start.S
	$(CC) $(UC_ASM_FLAGS) $(MUC_FLAGS) -c muc_start.S
	$(CC) $(UC_ASM_FLAGS) -c uc_start.S
	$(CC) $(UC_C_FLAGS) $(DSP_FLAFS) -c dsp.c
	$(CC) $(UC_C_FLAGS) $(MUC_FLAGS) -c muc.c

#########################################################################

.depend:	Makefile $(SOBJS:.o=.S) $(OBJS:.o=.c)
		$(CC) -M $(CPPFLAGS) $(SOBJS:.o=.S) $(OBJS:.o=.c) > $@

-include .depend

#########################################################################
