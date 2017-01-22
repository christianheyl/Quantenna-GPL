/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Ashwin Chaugule <ashwin.chaugule@codito.com>
 */

#include <linux/module.h>
#include <linux/string.h>
#include <linux/cryptohash.h>
#include <linux/delay.h>
#include <linux/in6.h>
#include <linux/syscalls.h>
#include <linux/interrupt.h>

#include <asm/checksum.h>
#include <asm/io.h>
#include <asm/system.h>
#include <asm/uaccess.h>

/*
 * libgcc functions - functions that are used internally by the
 * compiler...  (prototypes are not correct though, but that
 * doesn't really matter since they're not versioned).
 */
extern void __ashldi3(void);
extern void __ashrdi3(void);
extern void __divsi3(void);
extern void __divsf3(void);
extern void __lshrdi3(void);
extern void __modsi3(void);
extern void __muldi3(void);
extern void __ucmpdi2(void);
extern void __udivsi3(void);
extern void __umodsi3(void);
extern void __do_div64(void);
extern void __cmpdi2(void);
extern void __fixunsdfsi(void);
extern void __muldf3(void);
extern void __divdf3(void);
extern void __floatunsidf(void);
extern void __floatunsisf(void);

extern void fpundefinstr(void);
extern void fp_enter(void);

extern unsigned long clk_speed;
extern struct sock_addr mac_addr;

	/* gcc lib functions */
EXPORT_SYMBOL(__ashldi3);
EXPORT_SYMBOL(__ashrdi3);
EXPORT_SYMBOL(__divsi3);
EXPORT_SYMBOL(__divsf3);
EXPORT_SYMBOL(__lshrdi3);
EXPORT_SYMBOL(__modsi3);
EXPORT_SYMBOL(__muldi3);
EXPORT_SYMBOL(__ucmpdi2);
EXPORT_SYMBOL(__udivsi3);
EXPORT_SYMBOL(__umodsi3);
EXPORT_SYMBOL(__cmpdi2);

EXPORT_SYMBOL(__fixunsdfsi);
EXPORT_SYMBOL(__muldf3);
EXPORT_SYMBOL(__divdf3);
EXPORT_SYMBOL(__floatunsidf);
EXPORT_SYMBOL(__floatunsisf);

EXPORT_SYMBOL(clk_speed);
EXPORT_SYMBOL(mac_addr);

/* ARC optimised assembler routines */
EXPORT_SYMBOL(memset);
EXPORT_SYMBOL(memcpy);
EXPORT_SYMBOL(memcmp);
EXPORT_SYMBOL(strchr);
EXPORT_SYMBOL(strcpy);
EXPORT_SYMBOL(strcmp);
EXPORT_SYMBOL(strlen);
