#
#
# This file is subject to the terms and conditions of the GNU General Public
# License.  See the file "COPYING" in the main directory of this archive
# for more details.
#
# Copyright (C) 2003 ARC International
#


SYSTEM	=$(srctree)/vmlinux

#obj-y += initrd.o

MKIMAGE =mkimage
MKIMAGEFLAGS =-A arc -O linux -C none  -T kernel -a 0x80000000 -e 0x80002000 -n "ARC700 Linux kernel" -d Image
OBJCOPYFLAGS =-O binary -R .note -R .comment -S vmlinux

# From Beta's doc:
# ZTEXTADDR - Address where zImage is located by the bootloader
# ZRELADDR - Address where the zImage will be relocated
# PARAMS_PHYS - Address where tagged parameters are to be found
# INITRD_PHYS - Physical Address of the Ramdisk
# ZBSSADDR - Address where the real kernel should execute from

#not very sure as to what to fill here...

ZRELADDR	 = 0x00000000 # where to relocate it... hmmm.... 
ZTEXTADDR	 = 0x00000000 # we'll put the kernel image here for now...
#endif


#
# If you don't define ZRELADDR above,
# then it defaults to ZTEXTADDR
#
ifeq ($(ZRELADDR),)
ZRELADDR	= $(ZTEXTADDR)
endif

export	SYSTEM ZTEXTADDR ZBSSADDR ZRELADDR 

targets := uImage 

#arch/arc/boot/bootpImage: $(SYSTEM) 
#	cp -f $(SYSTEM) $@

$(obj)/Image: vmlinux
	$(OBJCOPY) $(OBJCOPYFLAGS) $@

Image: bootpImage
	$(OBJCOPY) $(OBJCOPYFLAGS) $@			

uImage:	Image 
	$(MKIMAGE) $(MKIMAGEFLAGS) $@
clean:
	$(RM) Image zImage bootpImage initrd.c jffsimg.c
	#@$(MAKE) -C compressed clean


dep:
