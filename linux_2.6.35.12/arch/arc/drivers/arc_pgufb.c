/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *	ARC Pixel Graphics Unit framebuffer driver.
 */

/* The ARC PGU allows the user to send two video outputs to one display
 * simultaneously. From the hardware perspective, there is a main display
 * and a secondary overlay display. The main display can be run in RGB555 or
 * YUV 4:2:0 (aka YV12) modes, the overlay display in RGBA4444, RGBA5551 and
 * RGB555 with colour keying. Each display has a distinct buffer space. When
 * both displays are running, the contents of the overlay buffer are alpha
 * blended in hardware onto the contents of the primary buffer as the display
 * is rendered.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

#include <asm/uaccess.h>

#include <asm/arc_pgu.h>
#include <asm/arc_vsync.h>

/*--------------------------------------------------------------------------------*/
#define DRIVER_NAME "arc_pgu"
#define DRIVER_DESC "ARC Pixel Graphic Unit driver"
#define ID "ARC PGU FB"

#define PGU_REG_ID_OLD (0x1df20)
#define PGU_REG_ID     (0x1df21)

#define VCLK_MASK    7
#define VLCK_NOTIFY  8

#define VESA_60HZ 0
#define VESA_75HZ 2
#define VESA_CVT 4
#define VESA_DMT 0
#define VIDEO_PLL_ENABLE 1

#define CMAPSIZE 16

#define BASIC_ALIGNMENT 8
#define DEFAULT_ALIGN_SIZE 32
#define BUF_ALIGN(x, alignment) ((x) & ~alignment)

#define VOLATILE_MEMSET(s,c,n) memset((void *) s, c, n)

/*--------------------------------------------------------------------------------*/

DECLARE_MUTEX(pgu_sem);

struct arc_pgu_devdata {
        struct fb_info info;                    // Framebuffer driver info
        struct known_displays *display;         // Display connected to this device
        arc_pgu *regs;                  // PGU registers
        unsigned long fb_virt_start;
        size_t fb_size;
        unsigned long fb_phys;
        unsigned long yuv_max_size;
        unsigned long rgb_size;
        struct yuv_info yuv;
        struct yuv_info default_yuv;
        unsigned long yuv_base[CONFIG_ARCPGU_YUVBUFS];
};

/*--------------------------------------------------------------------------------*/
struct known_displays displays[] =
{
        { /* 0: Toshiba 640x480 TFT panel - AA4 colour display */
                640, /* xres */
                480, /* yres */
                16,      /* bpp  */

                "Toshiba LTA065A041F 640x480 colour LCD",
                /* control */
                ( STATCTRL_HPOL_MASK | STATCTRL_VPOL_MASK | STATCTRL_DPOL_MASK ),

                /* clkcontrol */
                ( 0x1 ),

                /* hsync */
                ( DMT_75_HSYNC ),

                /* vsync */
                ( DMT_75_VSYNC ),

                /* bl_active_high */
                ( 0 ),

                /* max_freq */
                ( 15000000 )
        },

        { /* 1: Hitachi 320x240 TFT panel - display on some of the obsolete AA3/3200's */
                320, /* xres */
                240, /* yres */
                16,      /* bpp  */

                "Hitachi TX14D11VM1CBA 320x240 colour LCD",
                /* mode_control */
                ( 0 ),

                /* clkcontrol */
                ( 0x00010002 ),

                /* hsync */
                ( 0x04220180 ),

                /* vsync */
                ( 0x010500fc ),

                /* bl_active_high */
                ( 1 ),

                /* max_freq */
                ( 12000000 )
        }
};


static struct fb_fix_screeninfo arc_pgu_fix __initdata = {
        .id =           "arc_pgu",
        .type =         FB_TYPE_PACKED_PIXELS,
        .visual =       FB_VISUAL_PSEUDOCOLOR,
        .xpanstep =     1,
        .ypanstep =     1,
        .ywrapstep =    0,
        .accel =        FB_ACCEL_NONE,
};

static struct fb_var_screeninfo arc_pgu_var __initdata = {
        .xres           = 640,
        .yres           = 480,
        .xres_virtual   = 640,
        .yres_virtual   = 480,
        .bits_per_pixel = 16,
};

/*--------------------------------------------------------------------------------*/

static int display_index = 0;
static int nohwcursor = 0;
struct arc_pgu_devdata fb_devdata;
static struct arc_pgu_par current_par;
static struct yuv_info tmpyuv;

unsigned int arcpgu_yuv_xres = CONFIG_ARCPGU_XRES;
unsigned int arcpgu_yuv_yres = CONFIG_ARCPGU_YRES;
unsigned int enable_video_pll = 1;
unsigned int cpld_controls_pll = 1;


struct fb_fix_screeninfo *fix;

/*--------------------------------------------------------------------------------*/

void arc_pgu_setup(char *options);
static int arc_pgu_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg);

static int arc_pgu_setcolreg(unsigned regno, unsigned red, unsigned green,
                         unsigned blue, unsigned transp,
                         struct fb_info *info);
/*--------------------------------------------------------------------------------*/
static struct fb_ops arc_pgu_ops = {
        .owner                  = THIS_MODULE,
        .fb_setcolreg           = arc_pgu_setcolreg,
        .fb_fillrect            = cfb_fillrect,
        .fb_copyarea            = cfb_copyarea,
        .fb_imageblit           = cfb_imageblit,
        .fb_ioctl               = arc_pgu_ioctl,
};

static void set_vlck(unsigned int newval)
{
#ifdef CONFIG_ARCPGU_VCLK
	newval &= VCLK_MASK;
	*((unsigned long*) VLCK_ADDR) = (newval | VLCK_NOTIFY);
	__udelay(10);
	*((unsigned long*) VLCK_ADDR) = newval;
	__udelay(10);
#endif
}

/*--------------------------------------------------------------------------------*/

static void reset_video_pll(unsigned int enable)
{
	fb_devdata.regs->clk_cfg = 0x1;
	if (enable)
	{
		set_vlck(VESA_75HZ | VESA_DMT | VIDEO_PLL_ENABLE);
	}
	else
		set_vlck(VESA_75HZ | VESA_DMT);
}

/*--------------------------------------------------------------------------------*/

static void stop_arcpgu(void)
{
	int i;

	// Stop PGU
	fb_devdata.regs->statctrl &= ~STATCTRL_REG_DISP_EN_MASK;

	// Wait for the PGU to stop running
	while ((fb_devdata.regs->statctrl & STATCTRL_DISP_BUSY_MASK) && (fb_devdata.regs->statctrl & STATCTRL_BU_BUSY_MASK)) {}
	// And a bit more for good measure
	for (i = 0; i < 10000; i++) {}
}

/*--------------------------------------------------------------------------------*/

static int calculate_yuv_offsets(struct yuv_info *info)
{
	unsigned long tmpsize;
	int i;

	/* Check requested size - error if larger than the display */
	if ((arcpgu_yuv_yres < info->height) ||	 (arcpgu_yuv_xres < info->width))
		return -1;

	/* Check if we can fit the requested number of buffers in the available area */
	tmpsize = info->num_buffers * ((info->height * info->width) + ((info->width * info->height)/2) + (4 * info->alignment));
	//if (tmpsize > fb_devdata.yuv_max_size)
	//	return -1;

	/* Now we're motoring. Use this for U,V size now. */
	tmpsize = (info->width/2 * info->height/2);

	info->u_offset = BUF_ALIGN(((info->width * info->height) + info->alignment),info->alignment);
	info->v_offset = BUF_ALIGN((info->u_offset + tmpsize + info->alignment),info->alignment);

	/* This now contains the length of the buffer */
	tmpsize += info->v_offset;

	// Work out base offsts
	fb_devdata.yuv_base[0] = BUF_ALIGN((info->start_offset + info->alignment),
		info->alignment);
	for (i = 1; i < info->num_buffers; i++)
	{
		fb_devdata.yuv_base[i] = BUF_ALIGN((fb_devdata.yuv_base[i-1] + tmpsize +
			    info->alignment), info->alignment);
	}

	info->y_stride = info->width;
	info->u_stride = info->width/2;
	info->v_stride = info->width/2;

	return 0;
}

static void blank_yuv(struct yuv_info *info)
{
	int i;

	for (i = 0; i < info->num_buffers; i++)
	{
		VOLATILE_MEMSET(((void*)fb_devdata.fb_virt_start) +
			fb_devdata.yuv_base[i], 0, (info->width*info->height));
		VOLATILE_MEMSET(((void*)fb_devdata.fb_virt_start) +
			fb_devdata.yuv_base[i] + (info->width*info->height), 0x80,
			(info->width*info->height)/2);
	}
}

static void arc_pgu_setdims(struct yuv_info *info)
{
/*	printk(KERN_INFO "Setting dims to: width %d, height %d, stride %d, u stride %d, v stride %d, u offset %d, v offset %d\n", */
/*				 info->width, info->height,fb_devdata.yuv.y_stride,fb_devdata.yuv.u_stride,fb_devdata.yuv.v_stride,fb_devdata.yuv.u_offset,fb_devdata.yuv.v_offset); */

	unsigned long width, height;

	/* Clip frame dimensions to size of display */
	width = (info->width > fb_devdata.display->xres) ? fb_devdata.display->xres-1 : info->width-1;
	height = (info->height > fb_devdata.display->yres) ? fb_devdata.display->yres-2 : info->height-2; /* FIXME Last line is hidden to hide the green stripe */
  fb_devdata.regs->frm_dim = ENCODE_PGU_DIMS(width, height);

	fb_devdata.regs->stride = fb_devdata.yuv.y_stride;
	fb_devdata.regs->cru_stride = fb_devdata.yuv.u_stride;
	fb_devdata.regs->crv_stride = fb_devdata.yuv.v_stride;
	fb_devdata.regs->cru_frm_st = fb_devdata.yuv.u_offset;
	fb_devdata.regs->crv_frm_st = fb_devdata.yuv.v_offset;
}

static int set_yuv_pointer(void)
{
	int newbuf =	fb_devdata.yuv.displayed_buffer;

	if (newbuf > fb_devdata.yuv.num_buffers)
		return -1;

	fb_devdata.regs->frm_start = (unsigned int) ((void*)fb_devdata.yuv.virt_base) +
	    	fb_devdata.yuv_base[newbuf];

	return 0;
}

/*--------------------------------------------------------------------------------*/

static int set_rgb_pointer(void)
{
	if (current_par.rgb_bufno > CONFIG_ARCPGU_RGBBUFS)
		return -1;

	fb_devdata.regs->frm_start = (unsigned int)	 fb_devdata.fb_virt_start
		+ (current_par.rgb_bufno * fb_devdata.display->xres * fb_devdata.display->yres * (fb_devdata.display->bpp / 8)) ;

	fb_devdata.regs->stride = fb_devdata.display->xres * (fb_devdata.display->bpp / 8);

	return 0;
}


static void set_color_bitfields(struct arc_pgu_devdata *fb_devdata, struct fb_var_screeninfo *var)
{
	switch (var->bits_per_pixel)
	{
		case 16:
		{
			if (current_par.main_is_fb)
			{
				if(fb_devdata->regs->pgu_id == PGU_REG_ID)
				{
					var->red.offset = 11;
					var->red.length = 5;
					var->green.offset = 5;
					var->green.length = 6;
				}
				else
				{
					var->red.offset = 10;
					var->red.length = 5;
					var->green.offset = 5;
					var->green.length = 5;
				}
				var->blue.offset = 0;
				var->blue.length = 5;
				var->transp.offset = 0;
				var->transp.length = 0;
			}
			else
			{
				switch(current_par.overlay_mode)
				{
					case STATCTRL_OL_FMT_RGBA4444:
						var->red.offset = 8;
						var->red.length = 4;
						var->green.offset = 4;
						var->green.length = 4;
						var->blue.offset = 0;
						var->blue.length = 4;
						var->transp.offset = 12;
						var->transp.length = 4;
						break;
					case STATCTRL_OL_FMT_RGBA5551:
						var->red.offset = 10;
						var->red.length = 5;
						var->green.offset = 5;
						var->green.length = 5;
						var->blue.offset = 0;
						var->blue.length = 5;
						var->transp.offset = 15;
						var->transp.length = 1;
						break;
					case STATCTRL_OL_FMT_RGB555:
					default:
						var->red.offset = 10;
						var->red.length = 5;
						var->green.offset = 5;
						var->green.length = 5;
						var->blue.offset = 0;
						var->blue.length = 5;
						var->transp.offset = 0;
						var->transp.length = 0;
						break;
				}
			}
			break;
		}
		default:
			var->red.offset = 0;
			var->red.length = 0;
			var->green.offset = 0;
			var->green.length = 0;
			var->blue.offset = 0;
			var->blue.length = 0;
			var->transp.offset = 0;
			var->transp.length = 0;
			break;
	}
	var->red.msb_right = 0;
	var->green.msb_right = 0;
	var->blue.msb_right = 0;
	var->transp.msb_right = 0;
}


/*--------------------------------------------------------------------------------*/

static void copy_pgu(arc_pgu *src, arc_pgu *dest)
{
	memcpy((void *)dest, (void *)src,sizeof(arc_pgu));
}

/*--------------------------------------------------------------------------------*/

static int arc_pgu_setmode(void)
{
	int dispmode;
	int i;
	unsigned long freq;
	unsigned long clk_div;
	unsigned long high;

	/*	printk(KERN_INFO "arc_pgu: in arc_pgu_setmode, lcd index is %d\n",display_index); */
	dispmode = current_par.overlay_mode & STATCTRL_OL_FMT_MASK; /* current controller mode, not display mode. Make this boot param. */

	stop_arcpgu();

	/* initialise controller */
	fb_devdata.regs->disp_dim = ENCODE_PGU_DIMS((fb_devdata.display->xres-1), (fb_devdata.display->yres-1));
	arc_pgu_setdims(&fb_devdata.yuv);
	blank_yuv(&fb_devdata.yuv);
	memset((void *)fb_devdata.fb_virt_start,0,fb_devdata.rgb_size);

#ifdef CONFIG_ARCPGU_VCLK
	/* Check if DMT timing values are the defaults. If not, then we've got an old XBF and cannot access the VCLK control register. */
	if ((fb_devdata.regs->hsync_cfg != DMT_75_HSYNC) || (fb_devdata.regs->vsync_cfg != DMT_75_VSYNC))
	{
	    printk(KERN_INFO "arc_pgu: old PGU version (0x%08lx 0x%08lx) ... disabling VCLK control\n",fb_devdata.regs->hsync_cfg,fb_devdata.regs->vsync_cfg);
	    enable_video_pll = 0;
	    cpld_controls_pll = 0;
	}

	if (cpld_controls_pll)
	{
		reset_video_pll(1);
	}
	else if (enable_video_pll)
	{
		clk_div = 1;
	}
	else
#endif
	{
		clk_div = 1;
		freq = CONFIG_ARC700_CLK;
		while (freq/clk_div > fb_devdata.display->max_freq)
			clk_div++;

		clk_div--;
		high = clk_div/2;
		fb_devdata.regs->clk_cfg = (clk_div | (high << 16));
	}
	fb_devdata.regs->hsync_cfg = fb_devdata.display->hsync;
	fb_devdata.regs->vsync_cfg = fb_devdata.display->vsync;

	printk(KERN_INFO "arc_pgu_setmode: fbbase 0x%lx, ybase 0x%lx, ubase 0x%lx, vbase 0x%lx\n", fb_devdata.fb_virt_start,
				fb_devdata.regs->frm_start, fb_devdata.regs->cru_frm_st,fb_devdata.regs->crv_frm_st);


	fb_devdata.regs->ol_frm_st = fb_devdata.fb_virt_start;
	fb_devdata.regs->ol_stride = fb_devdata.display->xres * 2; // Always
	fb_devdata.regs->ol_scn_line = ENCODE_PGU_DIMS((0), (fb_devdata.display->yres-1));
	fb_devdata.regs->ol_col_key = 0;

	set_rgb_pointer();
	fb_devdata.regs->statctrl |= (STATCTRL_CONT_B_MASK | current_par.main_mode | current_par.overlay_mode | fb_devdata.display->control);
	if (fb_devdata.display->bl_active_high) fb_devdata.regs->statctrl |= STATCTRL_BACKLIGHT_MASK;
	else fb_devdata.regs->statctrl &= ~STATCTRL_BACKLIGHT_MASK;

	for (i = 0; i < 10000; i++) {}

	fb_devdata.regs->statctrl |= STATCTRL_REG_DISP_EN_MASK;
	set_color_bitfields (&fb_devdata, &arc_pgu_var);
	return 0;
}

/*--------------------------------------------------------------------------------*/

static int arc_pgu_setcolreg(unsigned regno, unsigned red, unsigned green,
			 unsigned blue, unsigned transp,
			 struct fb_info *info)
{
//	struct arc_pgu_devdata* i = (struct arc_pgu_devdata *)info;

	if (regno > 255)
		return 1;

#if 0
	switch(fb_devdata.display->bpp) {
	case 16:
		i->fbcon_cmap16[regno] =
			((red & 0xf800) >> 10) |
			((green & 0xf800) >> 5) |
			((blue & 0xf800));
		break;
	default:
		break;
	}
#endif

	if (info->fix.visual == FB_VISUAL_TRUECOLOR ||
	    info->fix.visual == FB_VISUAL_DIRECTCOLOR) {
		u32 v, green_mask;

		if(fb_devdata.regs->pgu_id == PGU_REG_ID)
			green_mask = 0xfc00;
		else
			green_mask = 0xf800;

		if (regno >= 16)
			return -EINVAL;

		v = ((red & 0xf800) >> info->var.red.offset) |
			((green & green_mask) >> info->var.green.offset) |
			((blue & 0xf800) >> info->var.blue.offset);

		((u32*)(info->pseudo_palette))[regno] = v;
	}

	return 0;
}

static int do_switch(int fb)
{
	current_par.main_is_fb = 0;
	fb_devdata.yuv.displayed_buffer = fb;
	return set_yuv_pointer() ? -EFAULT : 0;
}

static int arc_pgu_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	unsigned long result, tmplong;
	int tmpint;
	static arc_pgu temp_pgu;

	 //printk("arc_pgufb: in arc_pgufb_ioctl, cmd %d, pgu ptr is 0x%lx arg is %ld\n", cmd,  (unsigned long)fb_devdata.regs, arg);

	switch (cmd)
	{
		case ARCPGUFB_GETYUV:
			return copy_to_user((void *) arg, (void*) &(fb_devdata.yuv), sizeof(struct yuv_info)) ? -EFAULT : 0;

		case ARCPGUFB_GET_YUV_BASE:
			if (arg < 0 || arg > CONFIG_ARCPGU_YUVBUFS)
			    	return -1;
			return fb_devdata.yuv_base[arg];

		case ARCPGUFB_SETYUV:
			result = copy_from_user(&tmpyuv, (void *) arg, sizeof(struct yuv_info)) ? -EFAULT : 0;
			if (! result)
			{
				if (tmpyuv.alignment % BASIC_ALIGNMENT) return -EFAULT;
				down(&pgu_sem);
				tmplong = fb_devdata.regs->statctrl;
				stop_arcpgu();
				memcpy(&fb_devdata.yuv, &tmpyuv, sizeof(struct yuv_info));
				arc_pgu_setdims(&(fb_devdata.yuv));
				set_yuv_pointer();
				for (tmpint = 0; tmpint < 1000; tmpint++);
				fb_devdata.regs->statctrl = tmplong;
				up(&pgu_sem);
			}
			return result;

		case ARCPGUFB_FILLYUV:
			result = copy_from_user(&tmpyuv, (void *) arg, sizeof(unsigned long));
			if (! result)
			{
				calculate_yuv_offsets(&tmpyuv);
				result = copy_to_user((void *) arg, (void*) &(tmpyuv), sizeof(struct yuv_info)) ? -EFAULT : 0;
			}
			return result;

		case ARCPGUFB_SET_YUV_RES:
			if (copy_from_user(&(tmpyuv), (void *) arg, sizeof(struct yuv_info))) return -EFAULT;
			fb_devdata.yuv.width = tmpyuv.width;
			fb_devdata.yuv.height = tmpyuv.height;

			if (fb_devdata.yuv.width & 0xf) fb_devdata.yuv.width = ((fb_devdata.yuv.width + 0xf) & ~0xf);
			if (fb_devdata.yuv.height & 0xf) fb_devdata.yuv.height = ((fb_devdata.yuv.height + 0xf) & ~0xf);

			fb_devdata.yuv.y_stride = tmpyuv.width;
			fb_devdata.yuv.u_stride = fb_devdata.yuv.v_stride = tmpyuv.width/2;
			if (calculate_yuv_offsets(&(fb_devdata.yuv))) return -EFAULT;
			fb_devdata.yuv.displayed_buffer = 0;
			tmplong = fb_devdata.regs->statctrl;
			down(&pgu_sem);
			stop_arcpgu();
			arc_pgu_setdims(&(fb_devdata.yuv));
			set_yuv_pointer();
			blank_yuv(&(fb_devdata.yuv));
			for (tmpint = 0; tmpint < 1000; tmpint++);
			fb_devdata.regs->statctrl = tmplong;
			up(&pgu_sem);
			return 0;

		case ARCPGUFB_RESET:
			memcpy(&fb_devdata.yuv, &fb_devdata.default_yuv, sizeof(struct yuv_info));
			fb_devdata.yuv.displayed_buffer = 0;
			down(&pgu_sem);
			stop_arcpgu();
			calculate_yuv_offsets(&(fb_devdata.yuv));
			blank_yuv(&(fb_devdata.yuv));
			set_yuv_pointer();
			arc_pgu_setmode();
			fb_devdata.regs->ol_frm_st = fb_devdata.fb_virt_start;
			fb_devdata.regs->centreoffsets = 0;
			for (tmpint = 0; tmpint < 1000; tmpint++);
			fb_devdata.regs->statctrl |= STATCTRL_REG_DISP_EN_MASK;
			up(&pgu_sem);
			return 0;

		case ARCPGUFB_SWITCH_YUV:
			// Don't stop the controller for this
			if (! copy_from_user(&result, (void *) arg, sizeof(unsigned long)))
				return do_switch(result);
			return -EFAULT;

		case ARCPGUFB_BKLGHT_OFF:
			if (fb_devdata.display->bl_active_high) fb_devdata.regs->statctrl &= ~STATCTRL_BACKLIGHT_MASK;
			else fb_devdata.regs->statctrl |= STATCTRL_BACKLIGHT_MASK;
			return 0;

		case ARCPGUFB_BKLGHT_ON:
			if (fb_devdata.display->bl_active_high) fb_devdata.regs->statctrl |= STATCTRL_BACKLIGHT_MASK;
			else fb_devdata.regs->statctrl &= ~STATCTRL_BACKLIGHT_MASK;
			return 0;

		case ARCPGUFB_START_DISPLAY:
			fb_devdata.regs->statctrl |= STATCTRL_REG_DISP_EN_MASK;
			return 0;

		case ARCPGUFB_STOP_DISPLAY:
			down(&pgu_sem);
			stop_arcpgu();
			up(&pgu_sem);
			return 0;

		case ARCPGUFB_GET_PGU_STATE:
			down(&pgu_sem);
			copy_pgu(fb_devdata.regs,&temp_pgu);
			up(&pgu_sem);
			return copy_to_user((void *) arg, (void *) &temp_pgu, sizeof(arc_pgu)) ? -EFAULT : 0;

		case ARCPGUFB_SET_PGU_STATE:
			down(&pgu_sem);
			stop_arcpgu();
			result = copy_from_user((void *)&temp_pgu, (void *) arg, sizeof(arc_pgu)) ? -EFAULT : 0;
			if (!result)
			{
				copy_pgu(&temp_pgu,fb_devdata.regs);
			}
			up(&pgu_sem);
			return result;

		case ARCPGUFB_GET_FB_INFO:
			return copy_to_user((void *) arg, &fb_devdata, sizeof(struct arc_pgu_devdata)) ? -EFAULT : 0;

		case ARCPGUFB_GET_MAIN_MODE:
			result = fb_devdata.regs->statctrl & STATCTRL_PIX_FMT_MASK;
			return copy_to_user((void *) arg, (void*) result, sizeof(unsigned int)) ? -EFAULT : 0;

		case ARCPGUFB_SET_MAIN_MODE:
			if (! copy_from_user((void *) &result, (void *) arg, sizeof(unsigned long)))
			{
				down(&pgu_sem);
				current_par.main_mode = result & STATCTRL_PIX_FMT_MASK;
				fb_devdata.regs->statctrl &= ~STATCTRL_PIX_FMT_MASK;
				fb_devdata.regs->statctrl |=	current_par.main_mode;
				for (tmpint = 0; tmpint < 1000; tmpint++);
				up(&pgu_sem);
				return 0;
			}
			return -EFAULT;

		case ARCPGUFB_SET_RGB_BUF:
			if (! copy_from_user(&tmplong, (void *) arg, sizeof(unsigned long)))
			{
				current_par.main_is_fb = 1;
				current_par.rgb_bufno = tmplong;
				result = set_rgb_pointer() ? EFAULT : 0;
				return result;
			}
			return -EFAULT;

		case ARCPGUFB_GET_RGB_BUF:
			return copy_to_user((void *) arg, (void*) &current_par.rgb_bufno, sizeof(unsigned long)) ? -EFAULT : 0;

		case ARCPGUFB_GET_FB_MAIN:
			return copy_to_user((void *) arg, (void*) &current_par.main_is_fb, sizeof(unsigned long)) ? -EFAULT : 0;

		case ARCPGUFB_SET_OL_MODE:
			if (! copy_from_user(&result, (void *) arg, sizeof(unsigned long)))
			{
				down(&pgu_sem);
				stop_arcpgu();
				result &= STATCTRL_OL_FMT_MASK;
				fb_devdata.regs->statctrl &= ~STATCTRL_OL_FMT_MASK;
				fb_devdata.regs->statctrl |= result;
				for (tmpint = 0; tmpint < 1000; tmpint++);
				current_par.overlay_mode = result;
				up(&pgu_sem);
			}
			return -EFAULT;

		case ARCPGUFB_GET_OL_MODE:
			result =	(fb_devdata.regs->statctrl & STATCTRL_OL_FMT_MASK);
			return copy_to_user((void *) arg, (void*) &result, sizeof(unsigned int)) ? -EFAULT : 0;

		case ARCPGUFB_GET_OL_COLKEY:
			return copy_to_user((void *) arg, (const void *)&fb_devdata.regs->ol_col_key, sizeof(unsigned long)) ? -EFAULT : 0;

		case ARCPGUFB_SET_OL_COLKEY:
			result = copy_from_user((void*) &(tmplong), (void *) arg, sizeof(unsigned long));
			if (! result)
			{
				fb_devdata.regs->ol_col_key = tmplong;
			}
			return result;

		case ARCPGUFB_GET_OL_START:
			tmplong = (fb_devdata.regs->ol_scn_line & DISP_DIM_REG_PGU_X_RES_MASK);
			return copy_to_user((void *) arg, &tmplong, sizeof(unsigned long)) ? -EFAULT : 0;

		case ARCPGUFB_SET_OL_START:
			if (! copy_from_user((void*) &tmplong, (void *) arg, sizeof(unsigned int)))
			{
				fb_devdata.regs->ol_scn_line &= ~DISP_DIM_REG_PGU_X_RES_MASK;
				fb_devdata.regs->ol_scn_line |= tmplong;
				return 0;
			}
			return -EFAULT;

		case ARCPGUFB_GET_OL_END:
			tmplong = (fb_devdata.regs->ol_scn_line & DISP_DIM_REG_PGU_Y_RES_MASK) >> DISP_DIM_Y_SHIFT;
			return copy_to_user((void *) arg, &tmplong, sizeof(unsigned long)) ? -EFAULT : 0;

		case ARCPGUFB_SET_OL_END:
			if (! copy_from_user((void*) &tmplong, (void *) arg, sizeof(unsigned int)))
			{
				tmplong <<= DISP_DIM_Y_SHIFT;
				fb_devdata.regs->ol_scn_line &= ~DISP_DIM_REG_PGU_Y_RES_MASK;
				fb_devdata.regs->ol_scn_line |= tmplong;
				return 0;
			}
			return -EFAULT;

		case ARCPGUFB_SET_OL_OFFSET:
			if (! copy_from_user((void*) &result, (void *) arg, sizeof(unsigned long)))
			{
				fb_devdata.regs->ol_frm_st = fb_devdata.fb_virt_start + result;
				return 0;
			}
			return -EFAULT;

		case ARCPGUFB_GET_ORIGIN:
			tmplong = fb_devdata.regs->centreoffsets;
			return copy_to_user((void *) arg, &tmplong, sizeof(unsigned long)) ? -EFAULT : 0;

		case ARCPGUFB_SET_ORIGIN:
			if (! copy_from_user((void*) &result, (void *) arg, sizeof(unsigned long)))
			{
				down(&pgu_sem);
				stop_arcpgu();
				fb_devdata.regs->centreoffsets = result;
				fb_devdata.regs->statctrl |= STATCTRL_REG_DISP_EN_MASK;
				up(&pgu_sem);
				return 0;
			}
			return -EFAULT;

		case ARCPGUFB_GET_STATCTRL:
			return copy_to_user((void *) arg, (void*) &(fb_devdata.regs->statctrl), sizeof(unsigned long)) ? -EFAULT : 0;

		case ARCPGUFB_SET_STATCTRL:
			// On your own head be it.
			down(&pgu_sem);
			result = copy_from_user((void*) &(fb_devdata.regs->statctrl), (void *) arg, sizeof(unsigned long)) ? -EFAULT : 0;
			for (tmpint = 0; tmpint < 1000; tmpint++);
			up(&pgu_sem);
			return result;

#ifdef CONFIG_ARCPGU_VCLK
	        case ARCPGUFB_SET_VCLK:
			if (cpld_controls_pll)
			{
				result = 0;
				down(&pgu_sem);
				if (! copy_from_user((void*) &tmplong, (void *) arg, sizeof(unsigned int)))
				{
					stop_arcpgu();
					set_vlck(tmplong);
					fb_devdata.regs->statctrl |= STATCTRL_REG_DISP_EN_MASK;
				}
				else
					result = -EFAULT;
				up(&pgu_sem);
				return result;
			}
			else
				return -EFAULT;

	        case ARCPGUFB_GET_VCLK:
			if (cpld_controls_pll)
			{
				tmplong = *((unsigned long*) VLCK_ADDR) & VCLK_MASK;
				return copy_to_user((void *) arg, (void*) &(tmplong), sizeof(unsigned long)) ? -EFAULT : 0;
			}
			else
				return -EFAULT;

	        case ARCPGU_SET_VPLL:
			if (enable_video_pll)
			{
				if (! copy_from_user((void*) &tmplong, (void *) arg, sizeof(unsigned int)))
				{
					cpld_controls_pll = tmplong;
					reset_video_pll(cpld_controls_pll);
					return 0;
				}
			}
			return -EFAULT;

	        case ARCPGU_GET_VPLL:
			if (enable_video_pll)
			{
				return copy_to_user((void *) arg, (void*) &(cpld_controls_pll), sizeof(unsigned long)) ? -EFAULT : 0;
			}
			else
				return -EFAULT;
#endif

		default:
			return arc_vsync_ioctl(info, cmd, arg);
	}

	return -EINVAL;
}


static int arc_pgu_probe (struct platform_device *dev)
{
	unsigned long page;
	unsigned long x_res, y_res;

	if (!dev)
		return -EINVAL;

	/* Get the active display */
	display_index = CONFIG_ARCPGU_DISPTYPE;
	fb_devdata.display = &displays[display_index];
	fb_devdata.regs = (arc_pgu *)PGU_BASEADDR;

	printk(KERN_INFO "ARC PGU: controller version %lu, using the %s\n",
				 ((fb_devdata.regs->pgu_id & PGU_VERSION_MASK) >> PGU_VERSION_SHIFT),
				 fb_devdata.display->display_name);

	/* Pick up any size needed from arguments */
	x_res = arcpgu_yuv_xres;
	y_res = arcpgu_yuv_yres;

	/* This bit's easy */
	fb_devdata.rgb_size = CONFIG_ARCPGU_RGBBUFS * (fb_devdata.display->xres * fb_devdata.display->yres * (fb_devdata.display->bpp / 8));

	/* Store the current x and y dimensions */
	fb_devdata.yuv.width = x_res;
	fb_devdata.yuv.height = y_res;
	fb_devdata.yuv.num_buffers = CONFIG_ARCPGU_YUVBUFS;

	/* Work out our default YUV buffers arrangement, and save it */
	fb_devdata.yuv_max_size = CONFIG_ARCPGU_YUVBUFS * ((y_res * x_res) + ((y_res * x_res)/2) + (4 * DEFAULT_ALIGN_SIZE));
	fb_devdata.yuv.yuv_size = fb_devdata.yuv_max_size;
	fb_devdata.yuv.alignment = DEFAULT_ALIGN_SIZE;
	fb_devdata.yuv.start_offset = fb_devdata.rgb_size;
	calculate_yuv_offsets(&fb_devdata.yuv);

	/* Calculate the YUV buffer offset(s) and default strides */
	fb_devdata.yuv.displayed_buffer = 0;

	/*
	 * Panel dimensions x bpp must be divisible by 32
	 */
	if (((y_res * fb_devdata.display->bpp) % 32) != 0)
		printk("VERT %% 32\n");
	if (((x_res * fb_devdata.display->bpp) % 32) != 0)
		printk("HORZ %% 32\n");

	/*
	 * Allocate default framebuffers from system memory
	 */
	fb_devdata.fb_size = fb_devdata.rgb_size + fb_devdata.yuv.yuv_size + ((CONFIG_ARCPGU_YUVBUFS) * DEFAULT_ALIGN_SIZE);

	fb_devdata.fb_size += 0x200; // FIXME squelch buffer added for resolutions > VGA
	printk("ARC PGU: display/RGB resolution %03ldx%03ld, YUV resolution %03dx%03d\n",
				 fb_devdata.display->xres,fb_devdata.display->yres,
				 arcpgu_yuv_xres, arcpgu_yuv_yres);
	printk("Framebuffer: Size = 0x%x (RGB: 0x%lx, YUV: 0x%lx) %d RGB, %d YUV\n",
				 fb_devdata.fb_size, fb_devdata.rgb_size,
				 fb_devdata.yuv.yuv_size, CONFIG_ARCPGU_RGBBUFS, CONFIG_ARCPGU_YUVBUFS);

	fb_devdata.fb_virt_start = (unsigned long )
		__get_free_pages(GFP_ATOMIC, get_order(fb_devdata.fb_size + DEFAULT_ALIGN_SIZE));
	if (!fb_devdata.fb_virt_start) {
		printk("Unable to allocate fb memory; order = %d\n", get_order(fb_devdata.fb_size + DEFAULT_ALIGN_SIZE));
		return -ENOMEM;
	}
	fb_devdata.fb_phys = virt_to_phys((void *)fb_devdata.fb_virt_start);

	/*
	 * Set page reserved so that mmap will work. This is necessary
	 * since we'll be remapping normal memory.
	*/
	for (page = fb_devdata.fb_virt_start;
			 page < BUF_ALIGN(fb_devdata.fb_virt_start + fb_devdata.fb_size, DEFAULT_ALIGN_SIZE);
			 page += PAGE_SIZE) {
		SetPageReserved(virt_to_page(page));
	}
	memset((void *)fb_devdata.fb_virt_start, 0, get_order(fb_devdata.fb_size));

	if (!(fb_devdata.info.pseudo_palette = kmalloc(sizeof(u32) * 16, GFP_KERNEL)))
		return -ENOMEM;
	memset(fb_devdata.info.pseudo_palette, 0, sizeof(u32) * 16);

	printk(KERN_INFO "Framebuffer at 0x%lx (logical), 0x%lx (physical)\n", fb_devdata.fb_virt_start, fb_devdata.fb_phys);

	arc_pgu_var.xres = fb_devdata.display->xres;
	arc_pgu_var.xres_virtual = fb_devdata.display->xres;
	arc_pgu_var.yres = fb_devdata.display->yres;
	arc_pgu_var.yres_virtual = fb_devdata.display->yres;
	arc_pgu_var.bits_per_pixel = fb_devdata.display->bpp;

	/* FIX!!! only works for 8/16 bpp */
	current_par.num_rgbbufs = CONFIG_ARCPGU_RGBBUFS;
	current_par.line_length = fb_devdata.display->xres * fb_devdata.display->bpp / 8; // in bytes
	current_par.main_mode = STATCTRL_PIX_FMT_RGB555;
	current_par.overlay_mode = STATCTRL_OL_FMT_RGB555;
	current_par.rgb_bufno = 0;
	current_par.main_is_fb = 1;
	current_par.cmap_len = (fb_devdata.display->bpp == 8) ? 256 : 16;

	// Do stuff from encode_fix
	arc_pgu_fix.smem_start = fb_devdata.fb_phys;
	arc_pgu_fix.smem_len = fb_devdata.fb_size;
	arc_pgu_fix.mmio_start = (unsigned long)fb_devdata.regs;
	arc_pgu_fix.mmio_len = sizeof (arc_pgu);
	arc_pgu_fix.visual = (arc_pgu_var.bits_per_pixel == 8) ? FB_VISUAL_PSEUDOCOLOR : FB_VISUAL_TRUECOLOR;
	arc_pgu_fix.line_length = current_par.line_length;

	fb_devdata.yuv.phys_base = fb_devdata.fb_phys;
	fb_devdata.yuv.virt_base = fb_devdata.fb_virt_start;
	fb_devdata.info.node = -1;
	fb_devdata.info.fbops = &arc_pgu_ops;
	fb_devdata.info.flags = FBINFO_FLAG_DEFAULT;
	fb_devdata.info.screen_base = (char *) fb_devdata.fb_virt_start;

	fb_alloc_cmap(&fb_devdata.info.cmap, 256, 0);

	arc_pgu_setmode();
	fb_devdata.info.fix = arc_pgu_fix;
	fb_devdata.info.var = arc_pgu_var;
	memcpy(&fb_devdata.default_yuv, &fb_devdata.yuv, sizeof(struct yuv_info));

	platform_set_drvdata(dev, &fb_devdata);
	if (register_framebuffer(&fb_devdata.info) < 0)
		return -EINVAL;

	arc_vsync_init(do_switch);
	printk(KERN_INFO "fb%d: installed\n",MINOR(fb_devdata.info.node));

	/* uncomment this if your driver cannot be unloaded */
	/* MOD_INC_USE_COUNT; */
	return 0;
}


static int arc_pgu_remove(struct platform_device *dev)
{
	arc_vsync_uninit();
	/* Will need to free framebuffer memory if this is ever used as a module */
	fb_devdata.regs->statctrl = 0;
	unregister_framebuffer(&fb_devdata.info);
	vfree ((void *)fb_devdata.fb_virt_start);		// Free framebuffer memory
	return 0;
}
/*--------------------------------------------------------------------------------*/

static struct platform_driver arc_pgu_driver = {
	.driver = {
		.name   = "arc_pgu",
	},
	.probe  = arc_pgu_probe,
	.remove = arc_pgu_remove,

};

static struct platform_device *arc_pgu_device;

void arc_pgu_setup(char *options)
{
	char* this_opt;
	int i;
	int num_displays = sizeof(displays)/sizeof(struct known_displays);

	printk(KERN_INFO "FB options %s\n", options);

	if (!options || !*options)
		return;

	display_index = 1;
	while ((this_opt=strsep(&options, ",")) != NULL) {
		if (!strncmp(this_opt, "display:", 6)) {
			/* Get the display name, everything else if fixed */
			for (i=0; i<num_displays; i++) {
				if (!strncmp(this_opt+6, displays[i].display_name,
							strlen(this_opt))) {
					display_index = i;
					break;
				}
			}
		}
		else if (!strncmp(this_opt, "nohwcursor", 10)) {
			printk("nohwcursor\n");
			nohwcursor = 1;
		}
	}
}

static int __init arc_pgu_init (void)
{
	int ret;

        ret = platform_driver_register(&arc_pgu_driver);
        if (!ret) {
                arc_pgu_device = platform_device_alloc("arc_pgu", 0);
                if (arc_pgu_device) {
                        ret = platform_device_add(arc_pgu_device);
                } else {
                        ret = -ENOMEM;
                }
                if (ret) {
                        platform_device_put(arc_pgu_device);
                        platform_driver_unregister(&arc_pgu_driver);
                }
        }
        return ret;
}

static void __exit arc_pgu_exit(void)
{
	platform_device_unregister(arc_pgu_device);
	platform_driver_unregister(&arc_pgu_driver);
}

/*--------------------------------------------------------------------------------*/

module_init(arc_pgu_init);
module_exit(arc_pgu_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ARC International (Ported to 2.6.19 kernel by KT)");
MODULE_DESCRIPTION("ARC PGU framebuffer device driver");

