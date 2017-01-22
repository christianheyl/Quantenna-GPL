/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 * BRIEF MODULE DESCRIPTION
 *
 *	ARC Video Syncronisation Utility
 */

#include <linux/kfifo.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <asm/arc_vsync.h>
#include <asm/arc_pgu.h>

#define DEFAULT_FRAME_RATE 30

static int ref_cnt[CONFIG_ARCPGU_YUVBUFS]; /* reference count for buffers */
static struct kfifo display_q; /* queue of buffers to be displayed*/
static struct kfifo done_q; /* queue of buffers to be freed */
static double frame_rate = DEFAULT_FRAME_RATE;
static int frame_delay = HZ/DEFAULT_FRAME_RATE;
static double exact_frame_delay = (double)HZ/DEFAULT_FRAME_RATE;
static int run_len = 0;
static unsigned long run_start;
static int is_paused = 0;
static int is_framerate_changed = 0;
static unsigned long last_frame_timestamp = 0;

/* callback from driver to display buf on HW */
static int (*do_switch)(int buf);

/* queue of processes waiting for buffer allocation */
static DECLARE_WAIT_QUEUE_HEAD(blocked_q);

static struct timer_list flip_timer;

static struct timer_list pause_timer;

#ifdef CONFIG_PROC_FS

/* Stuff for /proc/fb_sync */
#define PROC_NAME "fb_sync"
#define TRACE_LEN 64 /* must be power of 2 */
static struct log_t {
    	int buf_idx;
	unsigned long jif;
	int qlen;
} trace_log[TRACE_LEN];

#define TRACE_NEXT(i) (((i)+1) & (TRACE_LEN - 1))

static int trace_log_freep = 0;

static int sync_read_cb(char *buf, char **start, off_t offset,
				  int length, int *eof, void *data)
{
	int i, len=0, freep = trace_log_freep;
	unsigned long last = 0;

	len += sprintf(buf+len, "display_q len = %d\n", kfifo_len(&display_q));
	len += sprintf(buf+len, "Ref_cnt: ");
	for (i=0; i<CONFIG_ARCPGU_YUVBUFS; i++)
	    	len += sprintf(buf+len, " %2d", ref_cnt[i]);
	len += sprintf(buf+len, "\n");

	len += sprintf(buf+len, "%3s %8s %4s %s\n", "idx", "time", "diff",
		"qlen");
	for (i = TRACE_NEXT(freep); i != freep; i = TRACE_NEXT(i))
	{
	    	unsigned long j = trace_log[i].jif;
		int x = trace_log[i].buf_idx;
		int q = trace_log[i].qlen;

		if (!j)
		    	continue;
		if (last)
			len += sprintf(buf+len, "%3d %8lu %4lu %d\n",
				x, j, j - last, q);
		last = j;
	}
	if (is_paused)
		len += sprintf(buf+len, "paused\n");

	len += sprintf(buf+len, "frame_rate=%d/1000\n", (int)(1000*frame_rate));

	*eof = 1;
	return len;
}

#endif

static void vsync_reset(void)
{
	int i;

	del_timer(&flip_timer);
	del_timer(&pause_timer);
	kfifo_reset(&display_q);
	kfifo_reset(&done_q);
	is_paused = 0;
	for (i=0; i<CONFIG_ARCPGU_YUVBUFS; i++)
		ref_cnt[i] = 0;
#ifdef CONFIG_PROC_FS
	memset(trace_log, 0, sizeof(trace_log));
#endif
}

static void log_trace(int buf)
{
#ifdef CONFIG_PROC_FS
 	trace_log[trace_log_freep].buf_idx = buf;
	trace_log[trace_log_freep].jif = jiffies;
	trace_log[trace_log_freep].qlen = kfifo_len(&display_q);

	trace_log_freep = TRACE_NEXT(trace_log_freep);
#endif
}

static void free_old_bufs(void)
{
	unsigned char i;

	/* We never free the last element in the queue, as it may
	   still be needed by the display hardware */
	if (kfifo_len(&done_q) <= 1)
		return;

	kfifo_out(&done_q, &i, sizeof(i));

	ref_cnt[i]--;
	if (!ref_cnt[i])
		wake_up_interruptible(&blocked_q);
}

static void refire_timer(void)
{
    	if (is_framerate_changed)
	{
	    	is_framerate_changed = 0;
		run_start = jiffies;
		run_len = 0;
	}
    	run_len++;
 	//flip_timer.expires = last_frame_timestamp + frame_delay;
 	flip_timer.expires = (unsigned long)(run_start + run_len * exact_frame_delay);
	if (time_before(flip_timer.expires, jiffies))
	{
	    	/* we are already late for next frame, so set timer to paint it now */
	    	flip_timer.expires = run_start = jiffies;
		run_len = 0;
	}
	add_timer(&flip_timer);
}

static void flip_cb(unsigned long ignore)
{
	unsigned char next;

	if (!kfifo_out(&display_q, &next, sizeof(next)))
	    	return; /* should never occur */

	do_switch(next);
	last_frame_timestamp = jiffies;

	log_trace(next);

	free_old_bufs();
	kfifo_in(&done_q, &next, sizeof(next));

	if (!kfifo_len(&display_q))
		return;
	/* we have more buffers: refire timer */
	refire_timer();
}

static int n_avail_bufs(void)
{
	int i, n=0;

	for (i=0; i<CONFIG_ARCPGU_YUVBUFS; i++)
		n += !ref_cnt[i];

	return n;
}

static void resume(unsigned long ignore)
{
	if (!is_paused)
		return;
	is_paused = 0;
	if (!kfifo_len(&display_q))
	    	return;
	run_len = 0;
	run_start = jiffies;
	flip_cb(0);
}

int arc_vsync_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	int i, res, len;
	unsigned char c;

	switch (cmd)
	{
	case ARCPGUFB_ALLOC:
		while (!n_avail_bufs()) {
			if (wait_event_interruptible(blocked_q, n_avail_bufs()))
				return -ERESTARTSYS;
		}
		for (i=0; i<CONFIG_ARCPGU_YUVBUFS && ref_cnt[i]; i++);
		ref_cnt[i]++;
		return put_user(i, (int *)arg);
	case ARCPGUFB_FREE:
		res = get_user(i, (int *)arg);
		if (res || i<0 || i >= CONFIG_ARCPGU_YUVBUFS)
			return -EINVAL;
		ref_cnt[i]--;
		return 0;
	case ARCPGUFB_SEND:
		res = get_user(i, (int *)arg);
		if (res || i<0 || i >= CONFIG_ARCPGU_YUVBUFS)
			return -EINVAL;
		ref_cnt[i]++;
		c = (unsigned char) i;
		kfifo_in(&display_q, &c, sizeof(c));
		if (timer_pending(&flip_timer) || is_paused)
		    	return 0;
		if (time_before_eq(last_frame_timestamp + (int)frame_delay, jiffies)) // XXX old non exact
		{
		    	/* already time for frame, so just display it */
		    	run_len = 0;
			run_start = jiffies;
		    	flip_cb(0);
			return 0;
		}
		refire_timer();
		return 0;

	case ARCPGUFB_FRAMERATE_SET:
		res = get_user(i, (int *)arg);
		if (res)
		    	return res;
		if (i == 0)
		    	return -EINVAL;
		frame_rate = i/1000.0;
		frame_delay = HZ/frame_rate;
		exact_frame_delay = (double)HZ/frame_rate;
		if (!frame_delay)
		    frame_delay = 1;
		is_framerate_changed = 1;
		/* we could modify the timer here */
		return 0;
	case ARCPGUFB_QUEUE_LEN_GET:
		len = kfifo_len(&display_q);
		return put_user(len, (int *)arg);
	case ARCPGUFB_NUM_FREE_BUFS:
		len = n_avail_bufs();
		return put_user(len, (int *)arg);
	case ARCPGUFB_FLUSH:
		del_timer(&flip_timer);
		while (kfifo_out(&display_q, &c, sizeof(c)))
			ref_cnt[c]--;
		return 0;
	case ARCPGUFB_FRAME_DROP:
		del_timer(&flip_timer);
		if (kfifo_out(&display_q, &c, sizeof(c)))
			ref_cnt[c]--;
		if (kfifo_len(&display_q))
			add_timer(&flip_timer);
		return 0;
	case ARCPGUFB_PAUSE:
	case ARCPGUFB_PAUSE_MS:
		if (is_paused)
			return -EINVAL;
		is_paused = 1;
		del_timer(&flip_timer);
		if (cmd == ARCPGUFB_PAUSE)
			return 0;
		res = get_user(i, (int *)arg);
		if (res)
		    	return res;
	    	pause_timer.expires = jiffies + (i * HZ)/1000;
		add_timer(&pause_timer);
		return 0;
	case ARCPGUFB_RESUME:
		if (!is_paused)
			return -EINVAL;
		resume(0);
		return 0;
	case ARCPGUFB_RESET_SYNC:
		vsync_reset();
		return 0;
	case ARCPGUFB_PEEK:
		if ((display_q.buffer) && (display_q.size))
			c = *((unsigned char*) display_q.buffer);
		i = (int) c;
		return put_user(i, (int *)arg);
	default:
		return -EINVAL;
	}
}

void arc_vsync_init(frame_switch_cb_t fun)
{
    int res1, res2;
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry *ent = create_proc_entry(PROC_NAME, 0, NULL);

	ent->read_proc = sync_read_cb;
#endif

	res1 = kfifo_alloc(&display_q, CONFIG_ARCPGU_YUVBUFS, GFP_ATOMIC);
	res2 = kfifo_alloc(&done_q, CONFIG_ARCPGU_YUVBUFS, GFP_ATOMIC);

    if (res1 || res2) {
        panic("vsync fifo alloc error\n");
    }

	flip_timer.function = flip_cb;
	pause_timer.function = resume;
	do_switch = fun;
}

void arc_vsync_uninit(void)
{
	del_timer(&flip_timer);
	del_timer(&pause_timer);
#ifdef CONFIG_PROC_FS
	remove_proc_entry(PROC_NAME, NULL);
#endif
	kfifo_free(&display_q);
	kfifo_free(&done_q);
}
