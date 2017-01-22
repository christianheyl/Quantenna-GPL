How to use ARC U-BOOT

Configuring u-boot
==================

See the file include/configs/arcangel_arc700_ram.h

Right near the top of the file you'll see an include of config_cmd_default
this causes the build to include all the u-boot default commands.
The following lines either add to or subtract from that configuration.



Building u-boot
================

For a RAM based build :

$ make arcangel_arc700_ram_config
$ make

The resulting u-boot ELF file can be loaded with a suitable debugger / loader

To make a flash based image you'll need to edit the linker script and locate the image
correctly for your memory layout.

Look in board/arcangel/u-boot.lds for a working example.



Booting large files.  
====================

u-boot by default starts at TEXT_BASE specified in board/arcangel/config.mk
which which is mapped to 0x80000000 + TEXT_BASE in the memory (in untranslated
space).  u-boot uses memory region from TEXT_BASE - CONFIG_SYS_MALLOC_LEN to
TEXT_BASE for malloc. 

When an image is loaded (either via network or IDE), the address where the image
is loaded is specified in the boot command and its elf header specifies the 
address where the image has to be copied to. You should make sure that their is
no overlap of u-boot image with the memory where image it is loaded to and with 
the memory where it is copied to.

By default TEXT_BASE is 0x3000000 and CONFIG_SYS_MALLOC_LEN is 0x8000. If
0x4000000 is specified as load address and the image is copied to 0x80000000,
then you  will clobber the loader code if the image size is bigger than
47.96875MB, which is TEXT_BASE - CONFIG_SYS_MALLOC_LEN.

You have to modify TEXT_BASE in board/arcangel/config.mk to avoid the overlaps
or to change uboot starting address.

Network booting
===============

For TFTP boot the following has been tested.

ArcAngel4 # tftpboot 0x4000000 vmlinux
Using ARC EMAC device
TFTP from server 172.16.18.73; our IP address is 172.16.18.98
Filename 'vmlinux'.
Load address: 0x4000000
Loading: #################################################################
     #################################################################
     #################################################################
     #####################
done
Bytes transferred = 3170437 (306085 hex)
ArcAngel4 #
ArcAngel4 # bootelf
Loading .vector @ 0x80000000 (504 bytes)
Loading .init @ 0x80002000 (73728 bytes)
Loading .text @ 0x80014000 (2016384 bytes)
Loading __ksymtab @ 0x80201000 (14408 bytes)
Loading __ksymtab_gpl @ 0x80204848 (2808 bytes)
Loading __ksymtab_strings @ 0x80205340 (38956 bytes)
Loading __param @ 0x8020eb6c (400 bytes)
Loading .data @ 0x80210000 (76728 bytes)
Clearing .bss @ 0x80224000 (97576 bytes)
copying linux command line into BSS: arcclk=65 
Loading .init.ramfs @ 0x8023c000 (133 bytes)
## Starting application at 0x80002000 ...
EMAC : RESET
LXT970A : RESET
Linux version 2.6.19-04142008 (simonrs@jasper) (gcc version 4.2.1 (ARC_2.0)) #3 PREEMPT Mon Jun 9 12:17:34 PDT 2008
Parsing ATAG parameters

etc.

NFS Booting has also been tested.

# ArcAngel4 # nfs 0x4000000 172.16.18.73:/shared/simons_aa4/simon/vmlinux

ARC Angel 4 IDE Booting.
========================

Although highly dependent on ARC Angel 4 hardware the u-boot port also supports
booting from an IDE disk.  This will likely form the basis of your own loader should 
you wish to support booting from an IDE type disk (flash card etc)


 

