/**
 *
 * Copyright (C) Quantenna Communications, 2011
 *
 * Function instrumentation hooks for leaving u-boot crumbs
 */

#include <linux/compiler.h>
#include <linux/interrupt.h>
#include <linux/module.h>

#define PROLOG_PRINT_FN	0

#if PROLOG_PRINT_FN
volatile int print_prologs = 0;
EXPORT_SYMBOL(print_prologs);
#endif

void notrace __cyg_profile_func_enter(void *this_fn, void *call_site)
{
	struct ruby_crumbs_percore *crumbs = &((struct ruby_crumbs*)RUBY_CRUMBS_ADDR)->lhost;
	register unsigned long sp asm ("sp");
	register unsigned long blink asm ("blink");
	unsigned long status32 = read_new_aux_reg(ARC_REG_STATUS32);

	arc_write_uncached_32(&crumbs->blink, blink);
	arc_write_uncached_32(&crumbs->status32, status32);
	arc_write_uncached_32(&crumbs->sp, sp);

#if PROLOG_PRINT_FN
	if (print_prologs) {
		printk(". 0x%x\n", blink);
	}
#endif
}

void notrace __cyg_profile_func_exit(void *this_fn, void *call_site)
{
}

EXPORT_SYMBOL(__cyg_profile_func_enter);
EXPORT_SYMBOL(__cyg_profile_func_exit);

