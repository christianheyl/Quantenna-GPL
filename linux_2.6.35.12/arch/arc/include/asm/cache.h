/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARC_ASM_CACHE_H
#define __ARC_ASM_CACHE_H

/* Hardware Config Specific Items */
#define L1_CACHE_SHIFT      5
#define L1_CACHE_BYTES      ( 1 << L1_CACHE_SHIFT )

#define ICACHE_COMPILE_WAY_NUM      2
#define DCACHE_COMPILE_WAY_NUM      4

/* Helpers */
#define ICACHE_COMPILE_LINE_LEN     L1_CACHE_BYTES
#define DCACHE_COMPILE_LINE_LEN     L1_CACHE_BYTES

/* define the below 2 macros for PCIe */
#define ARC_ICACHE_LINE_LEN	ICACHE_COMPILE_LINE_LEN
#define ARC_DCACHE_LINE_LEN	DCACHE_COMPILE_LINE_LEN

#define ICACHE_LINE_MASK     (~(ICACHE_COMPILE_LINE_LEN - 1))
#define DCACHE_LINE_MASK     (~(DCACHE_COMPILE_LINE_LEN - 1))

#if ICACHE_COMPILE_LINE_LEN != DCACHE_COMPILE_LINE_LEN
#error "Need to fix some code as I/D cache lines not same"
#else
#define is_not_cache_aligned(p) ((unsigned long)p & (~DCACHE_LINE_MASK))
#endif

#define L1_CACHE_ALIGN(x)   ((((unsigned int)(x))+(L1_CACHE_BYTES-1)) & \
                                ~(L1_CACHE_BYTES-1))

#ifndef __ASSEMBLY__

/* Uncached access macros */
#define arc_read_uncached_8(ptr)					\
	({								\
	 uint8_t __ret;							\
	 __asm__ __volatile__ ("ldb.di %0, [%1]":"=r"(__ret):"r"(ptr));	\
	 __ret;								\
	 })

#define arc_write_uncached_8(ptr, data)					\
	({								\
	 __asm__ __volatile__ ("stb.di %0, [%1]"::"r"(data), "r"(ptr));	\
	 })

#define arc_read_uncached_16(ptr)					\
	({								\
	 uint16_t __ret;						\
	 __asm__ __volatile__ ("ldw.di %0, [%1]":"=r"(__ret):"r"(ptr));	\
	 __ret;								\
	 })

#define arc_write_uncached_16(ptr, data)				\
	({								\
	 __asm__ __volatile__ ("stw.di %0, [%1]"::"r"(data), "r"(ptr));	\
	 })

#define arc_read_uncached_32(ptr)					\
	({								\
	 uint32_t __ret;						\
	 __asm__ __volatile__ ("ld.di %0, [%1]":"=r"(__ret):"r"(ptr));	\
	 __ret;								\
	 })

#define arc_write_uncached_32(ptr, data)				\
	({								\
	 __asm__ __volatile__ ("st.di %0, [%1]"::"r"(data), "r"(ptr));	\
	 })

/* used to give SHMLBA a value to avoid Cache Aliasing */
extern unsigned int ARC_shmlba ;


#endif

#endif /* _ASM_CACHE_H */
