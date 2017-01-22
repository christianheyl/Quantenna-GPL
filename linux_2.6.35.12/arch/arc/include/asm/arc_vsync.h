/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARC_VSYNC_H__
#define __ARC_VSYNC_H__

#include <linux/fb.h>
#include <linux/fs.h>

typedef int (*frame_switch_cb_t)(int buf);

void arc_vsync_init(frame_switch_cb_t fun);

void arc_vsync_uninit(void);

int arc_vsync_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg);
#endif

