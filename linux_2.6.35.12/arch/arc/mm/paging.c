/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/sched.h>
#include <linux/mm.h>
#include <asm/cacheflush.h> /* for flush_dcache_page_virt */
#include <linux/module.h>

#ifndef NONINLINE_MEMSET

/* page functions */

void copy_page(void *to, void *from)
{
	unsigned long reg1, reg2, reg3, reg4;

	__asm__ __volatile__(
			     "mov lp_count,%6\n"
                 "lp 1f\n"
			     "ld.ab  %0, [%5, 4]\n\t"
			     "ld.ab  %1, [%5, 4]\n\t"
			     "ld.ab  %2, [%5, 4]\n\t"
			     "ld.ab  %3, [%5, 4]\n\t"
			     "st.ab  %0, [%4, 4]\n\t"
			     "st.ab  %1, [%4, 4]\n\t"
			     "st.ab  %2, [%4, 4]\n\t"
			     "st.ab  %3, [%4, 4]\n\t"
                 "1:\n"
                 :"=&r"(reg1), "=&r"(reg2), "=&r"(reg3), "=&r"(reg4)
			     :"r"(to), "r"(from), "ir"(PAGE_SIZE/4/4)
                 :"lp_count"
	    );

}
EXPORT_SYMBOL(copy_page);

/* Initialize the new pgd with invalid ptes */

void pgd_init(unsigned long page)
{
	int zero = 0;
	unsigned long dummy1, dummy2;
	__asm__ __volatile__("sub   %0, %0, 4\n\t"
			     "1:\n\t"
			     "st.a  %2, [%0, 4]\n\t"
			     "st.a  %2, [%0, 4]\n\t"
			     "st.a  %2, [%0, 4]\n\t"
			     "st.a  %2, [%0, 4]\n\t"
			     "st.a  %2, [%0, 4]\n\t"
			     "st.a  %2, [%0, 4]\n\t"
			     "st.a  %2, [%0, 4]\n\t"
			     "st.a  %2, [%0, 4]\n\t"
			     "sub.f %1, %1, 1\n\t"
			     "nop \n\t"
			     "bnz   1b\n\t":"=r"(dummy1), "=r"(dummy2)
			     :"r"(zero), "0"(page), "1"(USER_PTRS_PER_PGD / 8)
	    );

}

#else

void copy_page(void *to, void *from)
{
    memcpy(to, from, PAGE_SIZE);
}

void pgd_init(unsigned long page)
{
    memzero((void *)page, USER_PTRS_PER_PGD*4);
}
#endif

void clear_user_page(void *addr, unsigned long vaddr, struct page *page)
{
    clear_page(addr);

    if (cpuinfo_arc700[0].dcache.has_aliasing)
        flush_dcache_page_virt((unsigned long *)page);

}

void copy_user_page(void *vto, void *vfrom, unsigned long vaddr, struct page *to)
{
    copy_page(vto,vfrom);

    if (cpuinfo_arc700[0].dcache.has_aliasing)
        flush_dcache_page_virt((unsigned long*)vto);
}

