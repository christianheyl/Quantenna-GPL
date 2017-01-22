/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <asm-generic/vmlinux.lds.h>
#include <asm/vmlinux.lds.h>
#include <asm/cache.h>
#include <asm/page.h>
#include <asm/thread_info.h>
#include <asm/board/plat_memmap.h>
#include <asm/board/platform.h>

OUTPUT_ARCH(arc)
ENTRY(_stext)

jiffies = jiffies_64;


SECTIONS
{
    . = PHYS_SRAM_OFFSET + CONFIG_ARC_KERNEL_BASE;

    . = ALIGN(PAGE_SIZE); 
    _stext = .;
    .init : {           /* Init code and data       */
        _init_begin = .;

        _sinittext = .;
            INIT_TEXT
        _einittext = .;

            INIT_DATA
        . = ALIGN(16);

        __setup_start = .;
            *(.init.setup) 
        __setup_end = .;

        __initcall_start = .;
            INITCALLS   
        __initcall_end = .;

        __con_initcall_start = .;
            *(.con_initcall.init)
        __con_initcall_end = .;

        __tagtable_begin = .;
            *(.taglist.init)
        __tagtable_end = .;

        . = ALIGN(PAGE_SIZE);
        __per_cpu_start = .;
            *(.data.percpu)
        __per_cpu_end = .;
    }
#ifdef CONFIG_BLK_DEV_INITRD
    .init.ramfs : {
        . = ALIGN(PAGE_SIZE);
        __initramfs_start = .;
            *(.init.ramfs)
        __initramfs_end = .;
    }
#endif

    .init.rodata : {
        *(.init.rodata)
    }

    __sram_load_addr = .;
    .sram RUBY_SRAM_BEGIN + CONFIG_ARC_KERNEL_SRAM_B1_BASE : AT(__sram_load_addr) {
        __sram_start = .;
        __sram_datatext_start = .;

#ifdef CONFIG_ARCH_RUBY_SRAM_IRQ_STACK
        __irq_stack_begin = .;
        . = . + THREAD_SIZE + TOPAZ_CACHE_WAR_OFFSET;
        __irq_stack_end = .;
#endif

        . = ALIGN(0x400);
        __sram_text_start = .;
        _int_vec_base_lds = .;
        *(.vector)
        *(.sram.text)
        *(.text.arcfp)
        __sram_text_end = .;

        . = ALIGN(32);
        __sram_data_start = .;
        *(.sram.data)

        __arc_dccm_base = .;
        *(.data..shared_aligned)
        *(.data.arcfp)
        *(.sram.rodata)
        __sram_data_end = .;

        __sram_datatext_end = .;

        __sram_bss_start = .;
        *(.sram.bss)
#ifdef CONFIG_TOPAZ_PCIE_TARGET
	. = ALIGN(THREAD_SIZE);
	*(.ksoftirqd.stack)
#endif
        __sram_bss_end = .;

        __sram_end = .;
    }
    . = __sram_load_addr + MIN(SIZEOF(.sram), CONFIG_ARC_KERNEL_SRAM_B1_SIZE);


    . = ALIGN(PAGE_SIZE);
    _init_end = .;

    .text : {           /* Real text segment        */
        _text = .;      /* Text and read-only data  */
            TEXT_TEXT
            ARCFP_SDRAM_TEXT
            SCHED_TEXT
            LOCK_TEXT
            KPROBES_TEXT
            *(.fixup)
            *(.gnu.warning)

    }
    EXCEPTION_TABLE(16)
    _etext = .;     /* End of text section      */

    /* writeable */
    _sdata = .;

    RO_DATA_SECTION(PAGE_SIZE)

    /* 1. this is .data essentially
       2. THREAD_SIZE for init.task, must be kernel-stk sz aligned
     */
    RW_DATA_SECTION(L1_CACHE_BYTES, PAGE_SIZE, THREAD_SIZE)

    _edata = .;


    BSS_SECTION(0, 0, 0)
#ifndef CONFIG_TOPAZ_PCIE_TARGET
    . = ALIGN(THREAD_SIZE);
    .ksoftirqd.stack : { *(.ksoftirqd.stack) }
#endif
    . = ALIGN(PAGE_SIZE);
    __start_unwind = .;
    .debug_frame  : { *(.debug_frame) }
    __end_unwind = .;

    . = ALIGN(PAGE_SIZE);
    end_kernel = .;

    _end = . ;

    STABS_DEBUG
    DISCARDS

    .arcextmap 0 : {
        *(.gnu.linkonce.arcextmap.*)
        *(.arcextmap.*)
     }

    .debug_aranges 0 : { *(.debug_aranges) }
    .debug_pubnames 0 : { *(.debug_pubnames) }
    .debug_info 0 : { *(.debug_info) }
    .debug_abbrev 0 : { *(.debug_abbrev) }
    .debug_line 0 : { *(.debug_line) }
    .debug_str 0 : { *(.debug_str) }
    .debug_loc 0 : { *(.debug_loc) }
    .debug_macinfo 0 : { *(.debug_macinfo) }

}
