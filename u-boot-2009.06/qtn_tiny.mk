#
#  (C) Copyright 2015 Quantenna Communications Inc.
#
# Makefile for building the U-Boot Tiny (first stage bootloader)
#

include helper.mk
include ../Make.toolchain

CFLAGS =
CC = $(CROSS_COMPILE)gcc
CPP = $(CROSS_COMPILE)gcc -E
LD = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

TARGET=tiny

default: u-boot-$(TARGET) u-boot-$(TARGET).bin

BUILD_DIR=./build_$(TARGET)

-include $(shell if [ -d $(BUILD_DIR) ]; then \
                     find $(BUILD_DIR)/ -name \*.d; \
                 fi)

gccincdir = $(shell $(CC) -print-file-name=include)
UBOOT_TINY_TEXT_BASE_OFFSET = 0x14000

CFLAGS +=	-Wall -Werror -Wno-unused-parameter \
		-Os \
		-D__KERNEL__ -D__ARC__ -DTOPAZ_TINY_UBOOT -DRUBY_MINI\
		-mA7 \
		-fno-builtin -ffreestanding -fomit-frame-pointer \
		-nostdinc \
		-mno-sdata -mvolatile-cache -mno-millicode \
		-pipe \
		-Iboard/ruby/ -Iinclude/ -I../common/ -I../include/qtn/ \
		-isystem $(gccincdir) \
		-DTEXT_BASE_OFFSET=$(UBOOT_TINY_TEXT_BASE_OFFSET) \

AFLAGS = -D__ASSEMBLY__ $(CFLAGS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $< -c -o $@ -MP -MD -MF $@.d

$(BUILD_DIR)/%.o: %.S
	@mkdir -p $(@D)
	$(CC) $(AFLAGS) $< -c -o $@ -MP -MD -MF $@.d

$(BUILD_DIR)/%.s: %.S
	@mkdir -p $(@D)
	$(CPP) $(AFLAGS) $< -o $@ -MP -MD -MF $@.d


OBJS =	board/ruby/start.o			\
	board/ruby/flip.o			\
	cpu/arc/cache.o				\
	board/ruby/gpio.o			\
	board/ruby/timer.o			\
	board/ruby/serial.o			\
	board/ruby/reset.o			\
	board/ruby/spi_api.o			\
	board/ruby/newlib_io_copy.o		\
	board/ruby/spi_flash.o			\
	lib_generic/ctype.o			\
	lib_generic/vsprintf.o			\
	lib_generic/string.o			\
	lib_generic/crc32.o			\
	board/ruby_mini/ruby_mini_common.o	\
	board/ruby_mini/ruby_tiny.o

LDFLAGS = -L $(tools_path)../lib/gcc/arc-linux-uclibc/4.2.1/ -lgcc

LDMAP = $(BUILD_DIR)/u-boot-$(TARGET).lds

RUBY_TINY_OBJS = $(OBJS:%=$(BUILD_DIR)/%)

u-boot-$(TARGET): $(RUBY_TINY_OBJS) $(LDMAP)
	$(LD) -Bstatic -T $(LDMAP) $(RUBY_TINY_OBJS) -Map $@.map -o $@ $(LDFLAGS)

%.bin: %
	$(OBJCOPY) -O binary $< $@

$(LDMAP): $(RUBY_MINI_LDMAP_SRC)
	$(call build-mini-ldmap,$(CFLAGS))
