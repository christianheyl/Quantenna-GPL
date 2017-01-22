/**
 *
 * Copyright (C) Quantenna Communications, 2011
 *
 * Function instrumentation hooks for ep profiling
 */

#include <linux/compiler.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <asm/instrument_hooks.h>

#ifdef PROFILE_LINUX_EP

static void(*__trace_func_enter)(void *this_fn, void *call_site) = NULL;
static void(*__trace_func_exit)(void *this_fn, void *call_site) = NULL;

void notrace __cyg_profile_func_enter(void *this_fn, void *call_site)
{
	if (__trace_func_enter) {
		__trace_func_enter(this_fn, call_site);
	}
}

void notrace __cyg_profile_func_exit(void *this_fn, void *call_site)
{
	if (__trace_func_exit) {
		__trace_func_exit(this_fn, call_site);
	}
}


void notrace set_instrument_enter_func(void(*enter)(void *, void *))
{
	__trace_func_enter = enter;
}

void notrace set_instrument_exit_func(void(*exit)(void *, void *))
{
	__trace_func_exit = exit;
}

void notrace clear_instrument_funcs(void)
{
	__trace_func_enter = NULL;
	__trace_func_exit = NULL;
}

EXPORT_SYMBOL(set_instrument_enter_func);
EXPORT_SYMBOL(set_instrument_exit_func);
EXPORT_SYMBOL(clear_instrument_funcs);

EXPORT_SYMBOL(__cyg_profile_func_enter);
EXPORT_SYMBOL(__cyg_profile_func_exit);

#endif
