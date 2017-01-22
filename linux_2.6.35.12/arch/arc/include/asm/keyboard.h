/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ASM_KEYBOARD_H
#define _ASM_KEYBOARD_H

#ifdef __KERNEL__

#include <linux/config.h>

extern int kbd_setkeycode(unsigned int scancode, unsigned int keycode);
extern int kbd_getkeycode(unsigned int scancode);
extern int kbd_translate(unsigned char scancode, unsigned char *keycode,
	char raw_mode);
extern char kbd_unexpected_up(unsigned char keycode);
extern void kbd_leds(unsigned char leds);
extern void kbd_init_hw(void);

#endif /* __KERNEL */

#endif /* _ASM_KEYBOARD_H */
