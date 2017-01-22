/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Peter Shepherd
 */

#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/sound.h>
#include <linux/slab.h>
#include <linux/soundcard.h>
#include <linux/init.h>
#include <linux/poll.h>
#include <linux/pci.h>
#include <linux/bitops.h>
#include <linux/proc_fs.h>
#include <linux/spinlock.h>
#include <linux/smp_lock.h>
#include <linux/ac97_codec.h>
#include <linux/platform_device.h>
#include <asm/uaccess.h>
#include <asm/hardirq.h>
#include <asm/io.h>
#include <asm/arc_ac97.h>

/* --------------------------------------------------------------------- */

#undef OSS_DOCUMENTED_MIXER_SEMANTICS
//#define ARC_AC97_DEBUG
//#define ARC_AC97_VERBOSE_DEBUG

#define ARC_AC97_MODULE_NAME "ARC_AC97"
#define PFX ARC_AC97_MODULE_NAME
#define mem_map_reserve(p)  set_bit(PG_reserved, &((p)->flags))
#define mem_map_unreserve(p)    clear_bit(PG_reserved, &((p)->flags))

#ifdef ARC_AC97_DEBUG
#define dbg(fn, format, arg...) printk(KERN_DEBUG "%s : " format "\n" , fn, arg)
#define DBG_TRACE(arg...) printk(arg)
#else
#define dbg(format, arg...) do {} while (0)
#define DBG_TRACE(arg...)
#endif
#define err(format, arg...) printk(KERN_ERR PFX ": " format "\n" , ## arg)
#define info(format, arg...) printk(KERN_INFO PFX ": " format "\n" , ## arg)
#define warn(format, arg...) printk(KERN_WARNING PFX ": " format "\n" , ## arg)

/* misc stuff */
#define POLL_COUNT   0x5000
#define AC97_EXT_DACS (AC97_EXTID_SDAC | AC97_EXTID_CDAC | AC97_EXTID_LDAC)

/* Boot options */
static int      vra = 1;	// 0 = no VRA, 1 = use VRA if codec supports it
//MODULE_PARM(vra, "i");
//MODULE_PARM_DESC(vra, "if 1 use VRA if codec supports it");


/* Controller */
static volatile ARC_AC97_if* controller= (ARC_AC97_if *) AC97_CONTROLLER_BASE;

/* --------------------------------------------------------------------- */

static unsigned long tmpmask; /* OK, being dodgy here - assuming only one instance of the interface */
static unsigned long flags;

// Debugging counts
static int written = 0;
static int output = 0;
static int input = 0;
static int queued = 0;

// Set if the controller has the version 2 features;- ability to query queue lengths, extra status, etc.
static int extended_controller;

// Sample format conversion scratchpad
static unsigned char tmpbuf[TMPBUF_SIZE];

#define SAMPLE_SIZE 2

struct arc_ac97_state {
	/* soundcore stuff */
	int             dev_audio;

	/* Active FIFO interrupts */
	unsigned long intrs;

#ifdef ARC_AC97_DEBUG
	/* debug /proc entry */
	struct proc_dir_entry *ps;
	struct proc_dir_entry *ac97_ps;
#endif				/* ARC_AC97_DEBUG */

	struct ac97_codec *codec;
	unsigned int    codec_base_caps;// AC'97 reg 00h, "Reset Register"
	unsigned int    codec_ext_caps;	// AC'97 reg 28h, "Extended Audio ID"
	int             no_vra;	// do not use VRA

	spinlock_t      lock;
	struct semaphore open_sem;
	mode_t          open_mode;
	wait_queue_head_t open_wait;

	struct device_buf {
		struct semaphore sem;
		unsigned int    is_play;            // 0 -> capture, 1 -> play
		unsigned int    sample_rate;	      // Hz
		unsigned int    sample_size;	      // 8 or 16
		int             num_channels;	      // 1 = mono, 2 = stereo, 4, 6
		int             bytes_per_sample;   // DMA bytes per audio sample frame
		unsigned int    fifo_depth;
		unsigned int    transfer_size;

		unsigned int    bufsize;
		unsigned int    readpos;
		unsigned int    writepos;
		unsigned short* buffer;

		unsigned int      error;	           // over/underrun
		wait_queue_head_t wait;

		unsigned int    latency;
		unsigned int    threshold;         // Number of samples required before playback starts

		/* OSS stuff */
		unsigned        mapped:1;
		unsigned        ready:1;
		unsigned        stopped:1;

		unsigned int    subdivision;
		unsigned int    sample_count;
		int interrupted;
		int packed;
	} play, capture;
} arc_ac97_state;

/* --------------------------------------------------------------------- */

static void arc_ac97_delay(int msec)
{
	DBG_TRACE("%s\n",__FUNCTION__);

	__udelay(msec * 1000);          // FIXME - want this to be better behaved, revert to schedule() loop
}

/* --------------------------------------------------------------------- */

/* The buffers used are linear only - neither read nor write offset
   ever wraps round. This has two benefits; firstly, we only have to set
   a lock on the buffer when we perform this shuffle; and secondly, it
   simplifies the buffer handling code, as we never have to handle
   wrap-round conditions and can safely assume that the read offset is
   always <= write offset. The price of this arrangement is that we
   frequently have to shuffle the contents of the buffer down in order
   to maintain sufficient write space. This is done as infrequently as
   possible, and only ever in user context. This routine must be called
   with the buffer lock set, but - as said above - this is the only point
   in the driver where we need to protect user and kernel code from
   each other. Only problem is that this won't mmap out easily. */

static inline void pack_audio_buffer(struct device_buf *db)
{
	if (db->readpos)
	{
		db->interrupted = 0;
		db->packed = 1;
		memcpy(db->buffer,(db->buffer + db->readpos),((db->writepos-db->readpos)*SAMPLE_SIZE));
		db->writepos -= db->readpos;
		db->readpos = 0;
		if (db->interrupted != 0) {
			printk("@@@\n");
			db->interrupted = 0;
		}
	}
	DBG_TRACE("%s  readpos %d writepos %d\n",__FUNCTION__, db->readpos, db->writepos);
}

/* --------------------------------------------------------------------- */

static inline int audio_buffer_usage(struct device_buf *db)
{
	return (db->writepos - db->readpos)*SAMPLE_SIZE;
}

/* --------------------------------------------------------------------- */

static inline int audio_buffer_free_space(struct device_buf *db)
{
	return (db->bufsize - (db->writepos - db->readpos)*SAMPLE_SIZE);
}

/* --------------------------------------------------------------------- */

static inline int audio_buffer_writeable_space(struct device_buf *db)
{
	return (db->bufsize/SAMPLE_SIZE - db->writepos);
}

/* --------------------------------------------------------------------- */

static inline int audio_buffer_readable_space(struct device_buf *db)
{
	return (db->writepos - db->readpos);
}

/* --------------------------------------------------------------------- */

static void reset_audio_buffer(struct device_buf *db)
{
	db->writepos = 0;
	db->readpos = 0;
	db->sample_count = 0;
}

/* --------------------------------------------------------------------- */

#define READPTR(db) (db->buffer + db->readpos)
#define WRITEPTR(db) (db->buffer + db->writepos)

/* --------------------------------------------------------------------- */


static inline u8 S16_TO_U8(s16 ch)
{
	DBG_TRACE("%s\n",__FUNCTION__);
	return (u8) (ch >> 8) + 0x80;
}

/* --------------------------------------------------------------------- */

static inline s16 U8_TO_S16(u8 ch)
{
	DBG_TRACE("%s\n",__FUNCTION__);
	return (s16) (ch - 0x80) << 8;
}

/* --------------------------------------------------------------------- */

static int translate_from_user(struct device_buf *db,
			       const char* userbuf,
			       int count)
{
	unsigned char* thissample;
	short outval;
	unsigned short *bufptr;
	int bytes;

	// Should be no need to translate 16 bit samples, assuming endianness is already sorted out
	if (db->sample_size == 16)
	{
		db->interrupted = 0;
		count *= 2;
		bytes = copy_from_user((&db->buffer[db->writepos]),userbuf,count);
		count -= bytes; // copy_from_user returns no of bytes _not_ copied
		count /= 2;
		db->writepos += count;
		if (db->interrupted != 0) {
			printk("~~~\n");
			db->interrupted = 0;
		}
	}
	// Convert 8 bit samples
	else if (db->sample_size == 8)
	{
		if (count > TMPBUF_SIZE) count = TMPBUF_SIZE;
		if ((bytes = copy_from_user(tmpbuf,userbuf,count)) != 0) {
			dbg(__FUNCTION__, ": fault copying %d samples",count);
			return -EFAULT;
		}

		thissample = (unsigned char*) tmpbuf;
		count -= bytes;
		bufptr = &(db->buffer[db->writepos]);
		for (bytes = 0 ; bytes < count; bytes++)
		{
			outval = *thissample;
			outval -= 0x0080;
			*bufptr = (outval << 8);
			bufptr++;
			thissample++;
			db->writepos++;
		}
		db->writepos+=bytes;
	}
	else
		count = 0;

	DBG_TRACE("%s, translating %d samples of %d bit;s writepos %d\n", __FUNCTION__, count,
		db->sample_size, db->writepos);
	return count;	 // return number of samples copied
}

/* --------------------------------------------------------------------- */

static int translate_to_user(struct device_buf *db,
			     char* userbuf,
			     int count)
{
	unsigned short* thissample;
	unsigned char* thischar;
	int bytes;

	DBG_TRACE("%s, translating %d samples 0f %d bits userbuf 0x%x\n",__FUNCTION__,count,db->sample_size,userbuf);

	// Should be no need to translate 16 bit samples, assuming endianness is already sorted out
	if (db->sample_size == 16)
	{
		count *= 2;
		bytes = copy_to_user(userbuf,(db->buffer+db->readpos),count);
		count -= bytes;
		count /= 2;
		db->readpos += count;
	}
	// Convert 8 bit samples
	else if (db->sample_size == 8)
	{
		thissample = (unsigned short*) tmpbuf;
		thischar = (unsigned char*) db->buffer + db->readpos;

		if (count > TMPBUF_SIZE) printk("Bollocks\n");
		for (bytes = 0 ; bytes < count; bytes++)
		{
			*thissample = S16_TO_U8(*thischar);
			thissample++;
			thischar++;
			db->readpos++;
		}
		if ((bytes = copy_to_user(userbuf,tmpbuf,count)) != 0) {
			dbg(__FUNCTION__, ": fault copying %d samples",count);
			return -EFAULT;
		}
		count -= bytes; // copy_from_user returns no. of bytes _not_ copied
	}
	else
		count = 0;

	DBG_TRACE("%s, translated %d samples of %d bits\n",__FUNCTION__,count,db->sample_size);

	return count;	 // return number of samples copied
}

/* --------------------------------------------------------------------- */

static u16 rdcodec(struct ac97_codec *codec, u8 addr)
{
	u32 cmd;
	u16 data;
	int failcount, retries;

	DBG_TRACE("%s\n",__FUNCTION__);
//	spin_lock_irqsave(&s->lock, flags);

	tmpmask = controller->intr_mask;
	controller->intr_mask &= ~STATUS_INTR;
	//	controller->intr_mask |= STATUS_INTR;
	//controller->intr_mask = -1;

	cmd = addr;
	cmd <<= 16;
	cmd |= CMD_READ;

	retries = 0;

	do {
		failcount = 0;

 		controller->cmd = cmd;
		do {
			arc_ac97_delay(5);
			cmd = controller->intr_id;
			failcount++;
		}
		while ((! (cmd & STATUS_INTR)) && (failcount < 50));
		retries++;
	}
	while ((! (cmd & STATUS_INTR)) && (retries < 5));

	if (retries == 5) printk("Giving up on AC97 Codec read after 5 tries\n");

	cmd = controller->status;

	data = cmd & 0xffff;

	controller->intr_mask = tmpmask;
	//	controller->intr_ctrl = INTR_ENABLE;

//	spin_unlock_irqrestore(&s->lock, flags);

	return data;
}

/* --------------------------------------------------------------------- */

static void wrcodec(struct ac97_codec *codec, u8 addr, u16 data)
{
	u32 cmd;

	DBG_TRACE("%s\n",__FUNCTION__);
//	spin_lock_irqsave(&s->lock, flags);

	tmpmask = controller->intr_mask;
	controller->intr_mask &= ~STATUS_INTR;
	cmd = addr;
	cmd <<= 16;
	cmd &= 0xff0000;
	cmd |= CMD_WRITE;
	cmd |= data;

	controller->cmd = cmd;
	arc_ac97_delay(5);

	controller->intr_mask = tmpmask;
//	spin_unlock_irqrestore(&s->lock, flags);
}

/* --------------------------------------------------------------------- */

static void waitcodec(struct ac97_codec *codec)
{
	u16 temp;

	/* codec_wait is used to wait for a ready state after
		 an AC97C_RESET. */
	tmpmask = controller->intr_mask;
	controller->intr_mask &= ~STATUS_INTR;

	// get AC'97 powerdown control/status register
	temp = rdcodec(codec, AC97_POWER_CONTROL);

	if (temp & 0x7f00) {
		// Power on
		wrcodec(codec, AC97_POWER_CONTROL, 0);
		arc_ac97_delay(100);
		// Reread
		temp = rdcodec(codec, AC97_POWER_CONTROL);
	}

	// Check if Codec REF,ANL,DAC,ADC ready
	if ((temp & 0x7f0f) != 0x000f)
		err("codec reg 26 status (0x%x) not ready!!", temp);

	controller->intr_mask = tmpmask;
}


/* --------------------------------------------------------------------- */

/* stop the ADC before calling */
static void set_capture_rate(struct arc_ac97_state *s, unsigned rate)
{
	struct device_buf	 *capture = &s->capture;
	struct device_buf	 *play = &s->play;
	unsigned				capture_rate, play_rate;
	u16							ac97_extstat;

	DBG_TRACE("%s\n",__FUNCTION__);
	ac97_extstat = rdcodec(s->codec, AC97_EXTENDED_STATUS);

	rate = rate > 48000 ? 48000 : rate; // Shouldn't this be decided by the codec?

	// enable VRA
	wrcodec(s->codec, AC97_EXTENDED_STATUS,
		ac97_extstat | AC97_EXTSTAT_VRA);
	// now write the sample rate
	wrcodec(s->codec, AC97_PCM_LR_ADC_RATE, (u16) rate);
	// read it back for actual supported rate
	capture_rate = rdcodec(s->codec, AC97_PCM_LR_ADC_RATE);

#ifdef ARC_AC97_VERBOSE_DEBUG
	dbg(__FUNCTION__, ": set to %d Hz", capture_rate);
#endif

	// some codec's don't allow unequal DAC and ADC rates, in which case
	// writing one rate reg actually changes both.
	play_rate = rdcodec(s->codec, AC97_PCM_FRONT_DAC_RATE);
	if (play->num_channels > 2)
		wrcodec(s->codec, AC97_PCM_SURR_DAC_RATE, play_rate);
	if (play->num_channels > 4)
		wrcodec(s->codec, AC97_PCM_LFE_DAC_RATE, play_rate);

	capture->sample_rate = capture_rate;
	play->sample_rate = play_rate;
}

/* --------------------------------------------------------------------- */

/* stop the DAC before calling */
static void set_play_rate(struct arc_ac97_state *s, unsigned rate)
{
	struct device_buf	 *play = &s->play;
	struct device_buf	 *capture = &s->capture;
	unsigned				capture_rate, play_rate;
	u16							ac97_extstat;

	DBG_TRACE("%s new rate will be %d\n",__FUNCTION__,rate);
	ac97_extstat = rdcodec(s->codec, AC97_EXTENDED_STATUS);

	rate = rate > 48000 ? 48000 : rate;

	// enable VRA
	wrcodec(s->codec, AC97_EXTENDED_STATUS,
					ac97_extstat | AC97_EXTSTAT_VRA);
	// now write the sample rate
	wrcodec(s->codec, AC97_PCM_FRONT_DAC_RATE, (u16) rate);
	// I don't support different sample rates for multichannel,
	// so make these channels the same.
	if (play->num_channels > 2)
		wrcodec(s->codec, AC97_PCM_SURR_DAC_RATE, (u16) rate);
	if (play->num_channels > 4)
		wrcodec(s->codec, AC97_PCM_LFE_DAC_RATE, (u16) rate);
	// read it back for actual supported rate
	play_rate = rdcodec(s->codec, AC97_PCM_FRONT_DAC_RATE);

#ifdef ARC_AC97_VERBOSE_DEBUG
	dbg(__FUNCTION__, ": set to %d Hz", play_rate);
#endif
	DBG_TRACE("%s confirming play rate set to %d\n",__FUNCTION__,play_rate);

	// some codec's don't allow unequal DAC and ADC rates, in which case
	// writing one rate reg actually changes both.
	capture_rate = rdcodec(s->codec, AC97_PCM_LR_ADC_RATE);

	play->sample_rate = play_rate;
	capture->sample_rate = capture_rate;
}

/* --------------------------------------------------------------------- */

static void stop_play(struct arc_ac97_state *s)
{
	struct device_buf *db = &s->play;

	DBG_TRACE("%s\n",__FUNCTION__);
	DBG_TRACE("%s written %d output %d queued %d\n",__FUNCTION__,written,output,queued);
	if (db->stopped) {
		return;
	}

//	spin_lock_irqsave(&s->lock, flags);

	s->intrs &= ~PLAY_INTRS;

	controller->intr_mask = s->intrs;

 	controller->intr_ctrl = INTR_CLEAR;
	arc_ac97_delay(10);
	controller->intr_ctrl = 0;

	db->stopped = 1;

//	spin_unlock_irqrestore(&s->lock, flags);
}

/* --------------------------------------------------------------------- */

static void  stop_capture(struct arc_ac97_state *s)
{
	struct device_buf	 *db = &s->capture;

	DBG_TRACE("%s\n",__FUNCTION__);
	if (db->stopped)
		return;

//	spin_lock_irqsave(&s->lock, flags);

	s->intrs &= ~CAPTURE_INTRS;
	controller->intr_mask = s->intrs;

	controller->intr_ctrl = INTR_CLEAR;
	arc_ac97_delay(10);
	controller->intr_ctrl = 0;

	db->stopped = 1;

//	spin_unlock_irqrestore(&s->lock, flags);
}

/* --------------------------------------------------------------------- */

static unsigned long set_xmit_slots(int num_channels)
{
	unsigned long intmask = 0;

	DBG_TRACE("%s\n",__FUNCTION__);
	switch (num_channels) {
	case 1:		// mono
	case 2:		// stereo, slots 3,4
		controller->point1 = 3;
		controller->point2 = 4;
		controller->point3 = 0;
		intmask = PLAY1_INTR;
		controller->intr_mask = intmask;
		break;
	case 4:		// stereo with surround, slots 3,4,7,8
	case 6:		// stereo with surround and center/LFE, slots 3,4,6,7,8,9
	default:
		/* Only 3 slots available, so can't do. */
		break;
	}

	return intmask;
}

/* --------------------------------------------------------------------- */

static unsigned long set_recv_slots(int num_channels)
{
	// Slot mapping set up by set_xmit_slots
	// FIXME no capture interrupts just yet.
	DBG_TRACE("%s\n",__FUNCTION__);
	return 0;
}

/* --------------------------------------------------------------------- */

// Blast a set of samples into one of the FIFOs - FIXME use outw?
static inline int c_write_mono_to_controller(volatile unsigned long* play1, volatile unsigned long* play2, unsigned short *data, int limit) {
	int i = limit;

	while (i--) {
		*play1 = *data;
		*play2 = *data;
		data++;
	}
	return limit;
}

static inline int c_write_stereo_to_controller(volatile unsigned long* play1, volatile unsigned long* play2, unsigned short *data, int limit) {
	int i = limit;

	while (i--) {
		*play1 = *data;
		data++;
		*play2 = *data;
		data++;
	}
	return limit;
}

static inline int c_read_mono_from_controller(volatile unsigned long* play1, volatile unsigned long* play2, unsigned short *data, int limit) {
	int i = limit;

	while (i--) {
		*data = *play1;
		*data = *play2;
		data++;
	}
	return limit;
}

static inline int c_read_stereo_from_controller(volatile unsigned long* play1, volatile unsigned long* play2, unsigned short *data, int limit) {
	int i = limit;

	while (i--) {
		*data = *play1;
		data++;
		*data = *play2;
		data++;
	}
	return limit;
}

/* --------------------------------------------------------------------- */

static void fill_fifo(struct device_buf	*db)
{
	unsigned short* dataptr;
	int limit = 0, samples;


	db->interrupted = 1;
	db->packed = 0;

#ifdef ARC_EXTENDED_AC97
	if (extended_controller)
		limit = (db->fifo_depth - controller->play_count1)*db->num_channels;
	else
#endif

	    limit = (db->transfer_size*db->num_channels);

	if (audio_buffer_readable_space(db) < limit)
		limit = audio_buffer_readable_space(db);

	if (limit & 1) printk("Arse");
	// get the per-channel sample count
	samples = limit/db->num_channels;

	dataptr = &db->buffer[db->readpos];

	if (db->num_channels == 2) {
		c_write_stereo_to_controller(&(controller->play_data1),&(controller->play_data2),dataptr,samples);
	} else {
		c_write_mono_to_controller(&(controller->play_data1),&(controller->play_data2),dataptr,samples);
	}

	// update counts - limit is the total number of samples (so a stereo sample pair counts as 2)
	output += limit;
	db->readpos += limit;
	db->sample_count += samples;

	if (db->packed) { printk("!!!!!!\n"); db->packed = 0; }
	if (db->readpos >= db->writepos) printk("Overrun %d %d \n",db->readpos,db->writepos);
	DBG_TRACE("%s readpos = %x\n",__FUNCTION__, db->readpos);
}

/* --------------------------------------------------------------------- */

static void drain_fifo(struct device_buf *db)
{
	unsigned short* dataptr;
	int limit, samples;

	DBG_TRACE("%s\n",__FUNCTION__);

	if (audio_buffer_writeable_space(db) > 0)
	{
		// work out number of samples to be transferred
		limit = (audio_buffer_writeable_space(db) > (db->transfer_size*db->num_channels))
			? (db->transfer_size*db->num_channels) : audio_buffer_writeable_space(db);

		// get the per-channel sample count
		samples = limit / db->num_channels;

		dataptr = WRITEPTR(db);

		if (db->num_channels == 2) {
			c_read_stereo_from_controller(&(controller->play_data1),&(controller->play_data2),dataptr,samples);
		} else {
			c_read_mono_from_controller(&(controller->play_data1),&(controller->play_data2),dataptr,samples);
		}

		// update counts - limit is the total number of samples (so a stereo sample pair counts as 2)
		input += limit;
		db->writepos += limit;
		db->sample_count += samples;
	}
}

/* --------------------------------------------------------------------- */

static void start_play(struct arc_ac97_state *s)
{
	int i;
	struct device_buf	 *db = &s->play;

	DBG_TRACE("%s\n",__FUNCTION__);

	if (!db->stopped)
		return;

//	spin_lock_irqsave(&s->lock, flags);

	if (!audio_buffer_readable_space(db))
		return;

       	controller->intr_ctrl = 0;
	s->intrs = set_xmit_slots(db->num_channels);
	controller->intr_mask = s->intrs;

	for (i = 0; i < FIFO_LEN/(FRAGSIZE/2); i++) // FIXME read status from controller
	{
		fill_fifo(db);
	}
       	controller->intr_ctrl = INTR_ENABLE;
	db->stopped = 0;

//	spin_unlock_irqrestore(&s->lock, flags);
}

/* --------------------------------------------------------------------- */

static void start_capture(struct arc_ac97_state *s)
{
	struct device_buf	 *db = &s->capture;

	DBG_TRACE("%s\n",__FUNCTION__);
	if (!db->stopped)
		return;

//	spin_lock_irqsave(&s->lock, flags);

	set_recv_slots(db->num_channels);
	controller->intr_mask = s->intrs;
	//	do_read(db);

	db->stopped = 0;

//	spin_unlock_irqrestore(&s->lock, flags);
}

/* --------------------------------------------------------------------- */

static void release_dmabuf(struct arc_ac97_state *s, struct device_buf *db)
{
	struct page *page, *pend;

	DBG_TRACE("%s\n",__FUNCTION__);

	if (db->buffer) {
		pend = virt_to_page(db->buffer + db->bufsize);
		for (page = virt_to_page(db->buffer); page <= pend; page++)
			ClearPageReserved(virt_to_page(page));
		free_pages((unsigned long)db->buffer,get_order(db->bufsize));
	}
	db->mapped = db->ready = 0;
	db->bufsize = 0;
	db->buffer = 0;
}

/* --------------------------------------------------------------------- */

static int prog_dmabuf(struct arc_ac97_state *s, struct device_buf *db, char* ident)
{
	struct page *page, *pend;

	DBG_TRACE("%s %d\n",__FUNCTION__);

	if (db->buffer)
		release_dmabuf(s,db);

	db->buffer = (unsigned short *) __get_free_pages(GFP_KERNEL,get_order((db->bufsize)));

	if (!db->buffer)
	{
		return -ENOMEM;
	}

	pend = virt_to_page(db->buffer + db->bufsize);
	for (page = virt_to_page(db->buffer); page <= pend; page++)
		SetPageReserved(virt_to_page(page));

	db->threshold = START_THRESHOLD * db->sample_rate;
	if ((db->threshold > db->bufsize/4) || (db->threshold == 0))
		db->threshold = db->bufsize/4;

	db->ready = 1;

	printk(KERN_INFO "%s buffer at 0x%08x; size %d, threshold %d\n",
	       ident,  (unsigned int) db->buffer, db->bufsize, db->threshold);

	return 0;
}

/* --------------------------------------------------------------------- */

extern inline int prog_dmabuf_capture(struct arc_ac97_state *s)
{
	DBG_TRACE("%s\n",__FUNCTION__);
	stop_capture(s);
	return prog_dmabuf(s, &s->capture, "Capture");
}

/* --------------------------------------------------------------------- */

extern inline int prog_dmabuf_play(struct arc_ac97_state *s)
{
	DBG_TRACE("%s\n",__FUNCTION__);
	stop_play(s);
	return prog_dmabuf(s, &s->play, "Playback");
}

/* --------------------------------------------------------------------- */

/* hold spinlock for the following */
static void play_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct arc_ac97_state *s = (struct arc_ac97_state *) dev_id;
	struct device_buf *play = &s->play;

	DBG_TRACE("%s\n",__FUNCTION__);
	if (s->play.stopped) return;

	spin_lock_irqsave(&s->lock, flags);
	if (audio_buffer_readable_space(play) > 0)
		fill_fifo(play);
	else
	{
	    printk("! - %d -",audio_buffer_readable_space(play));
		stop_play(s);
	}
	spin_unlock_irqrestore(&s->lock, flags);

	/* wake up anybody listening */
	if (waitqueue_active(&play->wait))
		wake_up_interruptible(&(play->wait));
}

/* --------------------------------------------------------------------- */

static void capture_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct arc_ac97_state *s = (struct arc_ac97_state *) dev_id;
	struct device_buf *capture = &s->capture;

	DBG_TRACE("%s\n",__FUNCTION__);
	spin_lock(&s->lock);

	if (audio_buffer_writeable_space(capture) >= capture->transfer_size)
		drain_fifo(capture);
	else
		stop_capture(s);

	/* wake up anybody listening */
	if (waitqueue_active(&capture->wait))
		wake_up_interruptible(&(capture->wait));

	spin_unlock(&s->lock);
}

/* --------------------------------------------------------------------- */

static void status_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct arc_ac97_state *s = (struct arc_ac97_state *) dev_id;

	DBG_TRACE("%s\n",__FUNCTION__);
	spin_lock(&s->lock);


	DBG_TRACE("%s\n",__FUNCTION__);
 // FIXME fill in

	spin_unlock(&s->lock);
}

/* --------------------------------------------------------------------- */

static irqreturn_t arc_ac97_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	unsigned long int_id;

	int_id = controller->intr_id;
	controller->intr_ctrl = INTR_CLEAR;
	DBG_TRACE("Interrupt ID reg = 0x%x Interrupt mask reg = 0x%x\n", int_id, controller->intr_mask);

	if (int_id & PLAY_INTRS)
		play_interrupt(irq, dev_id, regs);
 	if (int_id & CAPTURE_INTRS)
 		capture_interrupt(irq, dev_id, regs);
	if (int_id & STATUS_INTR)
		status_interrupt(irq, dev_id, regs);

	controller->intr_ctrl = INTR_ENABLE;
	return IRQ_HANDLED;
}

/* --------------------------------------------------------------------- */

static loff_t arc_ac97_llseek(struct file *file, loff_t offset, int origin)
{
	DBG_TRACE("%s\n",__FUNCTION__);
	return -ESPIPE;
}

/* --------------------------------------------------------------------- */

static int arc_ac97_open_mixdev(struct inode *inode, struct file *file)
{
	DBG_TRACE("%s\n",__FUNCTION__);
	file->private_data = &arc_ac97_state;
	return 0;
}

/* --------------------------------------------------------------------- */

static int arc_ac97_release_mixdev(struct inode *inode, struct file *file)
{
	DBG_TRACE("%s\n",__FUNCTION__);
	return 0;
}

/* --------------------------------------------------------------------- */

static int mixdev_ioctl(struct ac97_codec *codec, unsigned int cmd,
												unsigned long arg)
{
	DBG_TRACE("%s\n",__FUNCTION__);
	return codec->mixer_ioctl(codec, cmd, arg);
}

/* --------------------------------------------------------------------- */

static int arc_ac97_ioctl_mixdev(struct inode *inode, struct file *file,
						 unsigned int cmd, unsigned long arg)
{
	struct arc_ac97_state *s = (struct arc_ac97_state *)file->private_data;
	struct ac97_codec *codec = s->codec;

	DBG_TRACE("%s\n",__FUNCTION__);
	return mixdev_ioctl(codec, cmd, arg);
}

/* --------------------------------------------------------------------- */

static /*const */ struct file_operations arc_ac97_mixer_fops =
{
	owner:THIS_MODULE,
	llseek:arc_ac97_llseek,
	ioctl:arc_ac97_ioctl_mixdev,
	open:arc_ac97_open_mixdev,
	release:arc_ac97_release_mixdev,
};

/* --------------------------------------------------------------------- */

static int drain_play(struct arc_ac97_state *s, int nonblock)
{
	int count, tmo;

	if (s->play.mapped || !s->play.ready || s->play.stopped)
		return 0;

	count = audio_buffer_readable_space(&(s->play));
	DBG_TRACE("%s %d to drain; readpos %d writepos %d\n", __FUNCTION__, count,
			s->play.readpos, s->play.writepos);
	for (;;) {
//		spin_lock_irqsave(&s->lock, flags);
//		spin_unlock_irqrestore(&s->lock, flags);
		if (count <= 0)
			break;
		if (signal_pending(current))
			break;
		if (nonblock)
			return -EBUSY;
		tmo = 1000 * count / (s->no_vra ?
				      48000 : s->play.sample_rate);
		tmo++;
		arc_ac97_delay(tmo);
	}

	s->intrs &= ~PLAY_INTRS;
	controller->intr_mask = s->intrs;

	if (signal_pending(current))
		return -ERESTARTSYS;
	return 0;
}

/* --------------------------------------------------------------------- */

static ssize_t arc_ac97_read(struct file *file, char *buffer,
			     size_t count, loff_t *ppos)
{
	struct arc_ac97_state *s = (struct arc_ac97_state *)file->private_data;
	struct device_buf *db = &s->capture;
	ssize_t	ret = 0;
	int cnt, origcnt, limit;

	DBG_TRACE("%s count is %d\n",__FUNCTION__, count);

	origcnt = count;
	if (ppos != &file->f_pos)
		return -ESPIPE;
	if (db->mapped)
		return -ENXIO;
	if (!access_ok(VERIFY_READ, buffer, count))
		return -EFAULT;

  	__set_current_state(TASK_INTERRUPTIBLE);

	while (count > 0) {
		limit = count/db->bytes_per_sample;
		if (audio_buffer_readable_space(db) < limit) {
			if (file->f_flags & O_NONBLOCK) {
				if (!ret)
					ret = -EAGAIN;
				goto out;
			}
			wait_event_interruptible((db->wait), (audio_buffer_readable_space(db) >= limit));
			if (signal_pending(current)) {
				if (!ret)
					ret = -ERESTARTSYS;
				goto out;
			}
		}

		down(&db->sem);
		//limit = audio_buffer_readable_space(db) > count/db->bytes_per_sample ? count/db->bytes_per_sample : audio_buffer_readable_space(db);
		cnt = translate_to_user(db, buffer, limit);
		up(&db->sem);

		cnt *= db->bytes_per_sample;

		if (audio_buffer_writeable_space(db) <= db->bufsize/8) {
			spin_lock_irqsave(&s->lock, flags);
			pack_audio_buffer(db);
			spin_unlock_irqrestore(&s->lock, flags);
		}

		if (cnt < 0) {
//			printk("****** Error, cnt is %d\n",cnt);
			ret = -EFAULT;
			goto out;
		}
		count -= cnt;
		ret += cnt;
	}			// while (count > 0)

out:
  	set_current_state(TASK_RUNNING);
	return ret;
}

/* --------------------------------------------------------------------- */

static ssize_t arc_ac97_write(struct file *file, const char *buffer,
			      size_t count, loff_t * ppos)
{
	struct arc_ac97_state *s = (struct arc_ac97_state *)file->private_data;
	struct device_buf *db = &s->play;
	ssize_t	ret = 0;
	int cnt, origcnt, limit;

	DBG_TRACE("%s \n",__FUNCTION__);

//	take_snap(SNAP_AUDIO_EV1, 0, NULL);
	origcnt = count;
	//if (ppos != &file->f_pos)
	//	return -ESPIPE;
	if (db->mapped)
	{
		return -ENXIO;
	}
	if (!access_ok(VERIFY_READ, buffer, count))
	{
		return -EFAULT;
	}

	__set_current_state(TASK_INTERRUPTIBLE);

	// Try and do the write in one go, avoiding locking the buffer unless necessary
	while (count > 0) {
		limit = count/db->bytes_per_sample;
		while (audio_buffer_free_space(db) < count)
		{
			if (file->f_flags & O_NONBLOCK)
			{
				if (!ret)
					ret = -EAGAIN;
				goto out;
			}
//			take_snap(SNAP_AUDIO_EV2, 0, NULL);
			wait_event_interruptible(db->wait, (audio_buffer_free_space(db) >= count));
//			take_snap(SNAP_AUDIO_EV3, 0, NULL);
			if (signal_pending(current))
			{
				if (!ret)
					ret = -ERESTARTSYS;
				goto out;
			}
		}
		if (audio_buffer_writeable_space(db) <= limit)
		{
//			take_snap(SNAP_AUDIO_EV4, 0, NULL);
			spin_lock_irqsave(&s->lock, flags);
			pack_audio_buffer(db);
			spin_unlock_irqrestore(&s->lock, flags);
//			take_snap(SNAP_AUDIO_EV5, 0, NULL);
		}

//		down(&db->sem);
//		take_snap(SNAP_AUDIO_EV6, 0, NULL);
		spin_lock_irqsave(&s->lock, flags);
		cnt = translate_from_user(db, buffer, limit);
		spin_unlock_irqrestore(&s->lock, flags);
//		take_snap(SNAP_AUDIO_EV7, 0, NULL);
//		up(&db->sem);

		if (cnt < 0)
		{
			ret = -EFAULT;
			goto out;
		}
		written += cnt;
		cnt *= db->bytes_per_sample;
		count -= cnt;
		ret += cnt;
		if (db->stopped && (audio_buffer_readable_space(db) > db->threshold))
		{
//			take_snap(SNAP_AUDIO_EV8, 0, NULL);
			start_play(s);
//			take_snap(SNAP_AUDIO_EV9, 0, NULL);
		}
	}			// while (count > 0)

	if (ret != origcnt) printk("%s : %d of %d bytes written %d output readpos %d writepos %d\n",
				   __FUNCTION__,ret,origcnt,output,db->readpos,db->writepos);

	if (db->writepos > db->bufsize) printk("Buffer overrun\n");
out:
	set_current_state(TASK_RUNNING);

//	take_snap(SNAP_AUDIO_EV10, 0, NULL);
//	printk("%s : %d bytes written %d output readpos %d writepos %d\n",__FUNCTION__,ret,output,db->readpos,db->writepos);
	return ret;
}

/* --------------------------------------------------------------------- */

/* No kernel lock - we have our own spinlock */
static unsigned int arc_ac97_poll(struct file *file,
				struct poll_table_struct *wait)
{
	struct arc_ac97_state *s = (struct arc_ac97_state *)file->private_data;
	unsigned int		mask = 0;

	DBG_TRACE("%s\n",__FUNCTION__);

	if (file->f_mode & FMODE_WRITE) {
		if (!s->play.ready)
			return 0;
		poll_wait(file, &s->play.wait, wait);
	}
	if (file->f_mode & FMODE_READ) {
		if (!s->capture.ready)
			return 0;
		poll_wait(file, &s->capture.wait, wait);
	}

//	spin_lock_irqsave(&s->lock, flags);

	if (file->f_mode & FMODE_READ) {
		if (audio_buffer_readable_space(&(s->capture)))
			mask |= POLLIN | POLLRDNORM;
	}
	if (file->f_mode & FMODE_WRITE) {
		if (audio_buffer_writeable_space(&(s->play)))
			mask |= POLLOUT | POLLWRNORM;
	}
//	spin_unlock_irqrestore(&s->lock, flags);
	return mask;
}

/* --------------------------------------------------------------------- */

static int arc_ac97_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct arc_ac97_state *s = (struct arc_ac97_state *)file->private_data;
	struct device_buf	 *db;
	unsigned long		size;
	int ret = 0;

	DBG_TRACE("%s\n",__FUNCTION__);
	dbg(__FUNCTION__,"vma at 0x%x",vma);

//	lock_kernel();
//	down(&s->sem);
	if (vma->vm_flags & VM_WRITE)
		db = &s->play;
	else if (vma->vm_flags & VM_READ)
		db = &s->capture;
	else
	{
		ret = -EINVAL;
		goto out;
	}
	if (vma->vm_pgoff != 0)
	{
		ret = -EINVAL;
		goto out;
	}
	size = vma->vm_end - vma->vm_start;
	if (size > db->bufsize)
	{
		ret = -EINVAL;
		goto out;
	}
	if (remap_pfn_range(vma, vma->vm_start, virt_to_phys(db->buffer) >> PAGE_SHIFT,
                                         size, vma->vm_page_prot)) {
                ret = -EAGAIN;
                goto out;
        }
	vma->vm_flags &= ~VM_IO;
	db->mapped = 1;
out:
//	up(&s->sem);
//	unlock_kernel();
	return ret;
}

/* --------------------------------------------------------------------- */

#ifdef ARC_AC97_VERBOSE_DEBUG
static struct ioctl_str_t {
	unsigned int		cmd;
	const char		 *str;
} ioctl_str[] = {
	{SNDCTL_DSP_RESET, "SNDCTL_DSP_RESET"},
	{SNDCTL_DSP_SYNC, "SNDCTL_DSP_SYNC"},
	{SNDCTL_DSP_SPEED, "SNDCTL_DSP_SPEED"},
	{SNDCTL_DSP_STEREO, "SNDCTL_DSP_STEREO"},
	{SNDCTL_DSP_GETBLKSIZE, "SNDCTL_DSP_GETBLKSIZE"},
	{SNDCTL_DSP_SAMPLESIZE, "SNDCTL_DSP_SAMPLESIZE"},
	{SNDCTL_DSP_CHANNELS, "SNDCTL_DSP_CHANNELS"},
	{SOUND_PCM_WRITE_CHANNELS, "SOUND_PCM_WRITE_CHANNELS"},
	{SOUND_PCM_WRITE_FILTER, "SOUND_PCM_WRITE_FILTER"},
	{SNDCTL_DSP_POST, "SNDCTL_DSP_POST"},
/*	{SNDCTL_DSP_SUBDIVIDE, "SNDCTL_DSP_SUBDIVIDE"}, */
/*	{SNDCTL_DSP_SETFRAGMENT, "SNDCTL_DSP_SETFRAGMENT"}, */
	{SNDCTL_DSP_GETFMTS, "SNDCTL_DSP_GETFMTS"},
	{SNDCTL_DSP_SETFMT, "SNDCTL_DSP_SETFMT"},
	{SNDCTL_DSP_GETOSPACE, "SNDCTL_DSP_GETOSPACE"},
	{SNDCTL_DSP_GETISPACE, "SNDCTL_DSP_GETISPACE"},
	{SNDCTL_DSP_NONBLOCK, "SNDCTL_DSP_NONBLOCK"},
	{SNDCTL_DSP_GETCAPS, "SNDCTL_DSP_GETCAPS"},
	{SNDCTL_DSP_GETTRIGGER, "SNDCTL_DSP_GETTRIGGER"},
	{SNDCTL_DSP_SETTRIGGER, "SNDCTL_DSP_SETTRIGGER"},
/*	{SNDCTL_DSP_GETIPTR, "SNDCTL_DSP_GETIPTR"}, */
/*	{SNDCTL_DSP_GETOPTR, "SNDCTL_DSP_GETOPTR"}, */
	{SNDCTL_DSP_MAPINBUF, "SNDCTL_DSP_MAPINBUF"},
	{SNDCTL_DSP_MAPOUTBUF, "SNDCTL_DSP_MAPOUTBUF"},
	{SNDCTL_DSP_SETSYNCRO, "SNDCTL_DSP_SETSYNCRO"},
	{SNDCTL_DSP_SETDUPLEX, "SNDCTL_DSP_SETDUPLEX"},
	{SNDCTL_DSP_GETODELAY, "SNDCTL_DSP_GETODELAY"},
	{SNDCTL_DSP_GETCHANNELMASK, "SNDCTL_DSP_GETCHANNELMASK"},
	{SNDCTL_DSP_BIND_CHANNEL, "SNDCTL_DSP_BIND_CHANNEL"},
	{OSS_GETVERSION, "OSS_GETVERSION"},
	{SOUND_PCM_READ_RATE, "SOUND_PCM_READ_RATE"},
	{SOUND_PCM_READ_CHANNELS, "SOUND_PCM_READ_CHANNELS"},
	{SOUND_PCM_READ_BITS, "SOUND_PCM_READ_BITS"},
	{SOUND_PCM_READ_FILTER, "SOUND_PCM_READ_FILTER"},
	{ARC_AC97_GET_SAMPLE_COUNT, "ARC_AC97_GET_SAMPLE_COUNT"},
	{ARC_AC97_RESET_SAMPLE_COUNT, "ARC_AC97_RESET_SAMPLE_COUNT"}
};
#endif

/* --------------------------------------------------------------------- */

static int arc_ac97_ioctl(struct inode *inode, struct file *file,
			  unsigned int cmd, unsigned long arg)
{
	struct arc_ac97_state *s = (struct arc_ac97_state *)file->private_data;
	audio_buf_info	abinfo;
	int count;
	int val, mapped;

	//take_snap(SNAP_AUDIO_EV11, 0, NULL);
	DBG_TRACE("%s - %d (0x%x)\n",__FUNCTION__,cmd,cmd);
	mapped = ((file->f_mode & FMODE_WRITE) && s->play.mapped) ||
		((file->f_mode & FMODE_READ) && s->capture.mapped);

#ifdef ARC_AC97_VERBOSE_DEBUG
	for (count=0; count<(sizeof(ioctl_str)/sizeof(struct ioctl_str_t)); count++) {
		if (ioctl_str[count].cmd == cmd)
			break;
	}
	if (count < sizeof(ioctl_str) / sizeof(struct ioctl_str_t))
		dbg(__FUNCTION__,"ioctl %s, arg=0x%lx", ioctl_str[count].str, arg);
	else
		dbg(__FUNCTION__,"ioctl 0x%x unknown, arg=0x%lx", cmd, arg);
#endif

	switch (cmd) {
	case OSS_GETVERSION:
		return put_user(SOUND_VERSION, (int *) arg);

	case SNDCTL_DSP_SYNC:
		if (file->f_mode & FMODE_WRITE) {
			printk("Draining in SNDCTL_DSP_SYNC\n");
			return drain_play(s, file->f_flags & O_NONBLOCK); }
		return 0;

	case SNDCTL_DSP_SETDUPLEX:
		return 0;

	case SNDCTL_DSP_GETCAPS:
		return put_user(DSP_CAP_DUPLEX | DSP_CAP_REALTIME |
				DSP_CAP_TRIGGER | DSP_CAP_MMAP, (int *)arg);

	case SNDCTL_DSP_RESET:
		printk("Resetting\n");
		if (file->f_mode & FMODE_WRITE) {
			stop_play(s);
			synchronize_irq();
//			reset_audio_buffer(&s->play);
		}
		if (file->f_mode & FMODE_READ) {
			stop_capture(s);
			synchronize_irq();
//			reset_audio_buffer(&s->capture);
		}
		return 0;

	case SNDCTL_DSP_SPEED:
		if (get_user(val, (int *) arg))
			return -EFAULT;
		if (val >= 0) {
			if (file->f_mode & FMODE_READ) {
				stop_capture(s);
				set_capture_rate(s, val);
			}
			if (file->f_mode & FMODE_WRITE) {
				stop_play(s);
				set_play_rate(s, val);
			}
/* 			if (s->open_mode & FMODE_READ) */
/* 				if ((ret = prog_dmabuf_capture(s))) */
/* 					return ret; */
/* 			if (s->open_mode & FMODE_WRITE) */
/* 				if ((ret = prog_dmabuf_play(s))) */
/* 					return ret; */
		}
		return put_user((file->f_mode & FMODE_READ) ?
				s->capture.sample_rate :
				s->play.sample_rate,
				(int *)arg);

	case SNDCTL_DSP_STEREO:
		if (get_user(val, (int *) arg))
			return -EFAULT;
		if (file->f_mode & FMODE_READ) {
			stop_capture(s);
			s->capture.num_channels = val ? 2 : 1;
/* 			if ((ret = prog_dmabuf_capture(s))) */
/* 				return ret; */
		}
		if (file->f_mode & FMODE_WRITE) {
			stop_play(s);
			s->play.num_channels = (val ? 2 : 1);
			DBG_TRACE("Number of channels %d\n",s->play.num_channels);
			if (s->codec_ext_caps & AC97_EXT_DACS) {
				// disable surround and center/lfe in AC'97
				u16 ext_stat = rdcodec(s->codec,
									 AC97_EXTENDED_STATUS);
				wrcodec(s->codec, AC97_EXTENDED_STATUS,
					ext_stat | (AC97_EXTSTAT_PRI |
								AC97_EXTSTAT_PRJ |
								AC97_EXTSTAT_PRK));
			}
/* 			if ((ret = prog_dmabuf_play(s))) */
/* 				return ret; */
		}
		return 0;

	case SNDCTL_DSP_CHANNELS:
		if (get_user(val, (int *) arg))
			return -EFAULT;
		if (val != 0) {
			if (file->f_mode & FMODE_READ) {
				if (val < 0 || val > 2)
					return -EINVAL;
				stop_capture(s);
				s->capture.num_channels = val;
/* 				if ((ret = prog_dmabuf_capture(s))) */
/* 					return ret; */
			}
			if (file->f_mode & FMODE_WRITE) {
				switch (val) {
				case 1:
				case 2:
					break;
				case 3:
				case 5:
					return -EINVAL;
				case 4:
					if (!(s->codec_ext_caps & AC97_EXTID_SDAC))
						return -EINVAL;
					break;
				case 6:
					if ((s->codec_ext_caps & AC97_EXT_DACS) != AC97_EXT_DACS)
						return -EINVAL;
					break;
				default:
					return -EINVAL;
				}

				stop_play(s);
				if (val <= 2 &&
						(s->codec_ext_caps & AC97_EXT_DACS)) {
					// disable surround and center/lfe
					// channels in AC'97
					u16 ext_stat = rdcodec(s->codec, AC97_EXTENDED_STATUS);
					wrcodec(s->codec, AC97_EXTENDED_STATUS,
						ext_stat | (AC97_EXTSTAT_PRI | AC97_EXTSTAT_PRJ | AC97_EXTSTAT_PRK));
				} else if (val >= 4) {
					// enable surround, center/lfe
					// channels in AC'97
					u16							ext_stat =
						rdcodec(s->codec,
							AC97_EXTENDED_STATUS);
					ext_stat &= ~AC97_EXTSTAT_PRJ;
					if (val == 6)
						ext_stat &= ~(AC97_EXTSTAT_PRI | AC97_EXTSTAT_PRK);
					wrcodec(s->codec, AC97_EXTENDED_STATUS, ext_stat);
				}

				s->play.num_channels = val;
/* 				if ((ret = prog_dmabuf_play(s))) */
/* 					return ret; */
			}
		}
		return put_user(val, (int *) arg);

	case SNDCTL_DSP_GETFMTS:	/* Returns a mask */
		return put_user(AFMT_S16_LE | AFMT_U8, (int *) arg);

	case SNDCTL_DSP_SETFMT: /* Selects ONE fmt */
		if (get_user(val, (int *) arg))
			return -EFAULT;
		if (val != AFMT_QUERY) {
			if (file->f_mode & FMODE_READ) {
				stop_capture(s);
				if (val == AFMT_S16_LE)
					s->capture.sample_size = 16;
				else {
					val = AFMT_U8;
					s->capture.sample_size = 8;
				}
/* 				if ((ret = prog_dmabuf_capture(s))) */
/* 					return ret; */
			}
			if (file->f_mode & FMODE_WRITE) {
				stop_play(s);
				if (val == AFMT_S16_LE)
					s->play.sample_size = 16;
				else {
					val = AFMT_U8;
					s->play.sample_size = 8;
				}
/* 				if ((ret = prog_dmabuf_play(s))) */
/* 					return ret; */
			}
		} else {
			if (file->f_mode & FMODE_READ)
				val = (s->capture.sample_size == 16) ? AFMT_S16_LE : AFMT_U8;
			else
				val = (s->play.sample_size == 16) ? AFMT_S16_LE : AFMT_U8;
		}
		return put_user(val, (int *) arg);

	case SNDCTL_DSP_POST:
		return 0;

	case SNDCTL_DSP_GETTRIGGER:
		val = 0;
//		spin_lock_irqsave(&s->lock, flags);
		if (file->f_mode & FMODE_READ && !s->capture.stopped)
			val |= PCM_ENABLE_INPUT;
		if (file->f_mode & FMODE_WRITE && !s->play.stopped)
			val |= PCM_ENABLE_OUTPUT;
//		spin_unlock_irqrestore(&s->lock, flags);
		return put_user(val, (int *) arg);

	case SNDCTL_DSP_SETTRIGGER:
		if (get_user(val, (int *) arg))
			return -EFAULT;
		if (file->f_mode & FMODE_READ) {
			if (val & PCM_ENABLE_INPUT)
				start_capture(s);
			else
				stop_capture(s);
		}
		if (file->f_mode & FMODE_WRITE) {
			if (val & PCM_ENABLE_OUTPUT)
				start_play(s);
			else
				stop_play(s);
		}
		return 0;

	case SNDCTL_DSP_GETOSPACE:
		if (!(file->f_mode & FMODE_WRITE))
			return -EINVAL;
		abinfo.fragsize = s->play.transfer_size * 8;
		abinfo.fragstotal = s->play.bufsize/abinfo.fragsize;
		abinfo.fragments = audio_buffer_free_space(&(s->play))/abinfo.fragsize;
		abinfo.fragments--;
		abinfo.bytes = audio_buffer_free_space(&(s->play));
		//take_snap(SNAP_AUDIO_EV12, 0, NULL);
		return copy_to_user((void *) arg, &abinfo, sizeof(abinfo)) ? -EFAULT : 0;

	case SNDCTL_DSP_GETISPACE:
		if (!(file->f_mode & FMODE_READ))
			return -EINVAL;
		abinfo.fragsize = s->capture.transfer_size * 8;
		abinfo.fragstotal = s->capture.bufsize/abinfo.fragsize;
		abinfo.fragments = audio_buffer_usage(&(s->capture))/abinfo.fragsize;
		abinfo.fragments--;
		abinfo.bytes = audio_buffer_usage(&(s->capture));
		return copy_to_user((void *) arg, &abinfo, sizeof(abinfo)) ? -EFAULT : 0;

	case SNDCTL_DSP_NONBLOCK:
		file->f_flags |= O_NONBLOCK;
		return 0;

	case SNDCTL_DSP_GETODELAY:
//		printk("SNDCTL_DSP_GETODELAY\n");
		if (!(file->f_mode & FMODE_WRITE))
			return -EINVAL;
//		spin_lock_irqsave(&s->lock, flags);
		count = audio_buffer_readable_space(&(s->play));
//		spin_unlock_irqrestore(&s->lock, flags);
		count *= s->play.bytes_per_sample;
		if (count < 0)
			count = 0;
		return put_user(count, (int *) arg);

/*	case SNDCTL_DSP_GETIPTR: */
/*		if (!(file->f_mode & FMODE_READ)) */
/*			return -EINVAL; */
/*		spin_lock_irqsave(&s->lock, flags); */
/*		cinfo.bytes = s->capture.total_bytes; */
/*		count = s->capture.count; */
/*		if (!s->capture.stopped) { */
/*			diff = dma_count_done(&s->capture); */
/*			count += diff; */
/*			cinfo.bytes += diff; */
/*			cinfo.ptr =	 virt_to_phys(s->capture.nextIn) + diff - */
/*				s->capture.dmaaddr; */
/*		} else */
/*			cinfo.ptr = virt_to_phys(s->capture.nextIn) - */
/*				s->capture.dmaaddr; */
/*		if (s->capture.mapped) */
/*			s->capture.count &= (s->capture.dma_fragsize-1); */
/*		spin_unlock_irqrestore(&s->lock, flags); */
/*		if (count < 0) */
/*			count = 0; */
/*		cinfo.blocks = count >> s->capture.fragshift; */
/*		return copy_to_user((void *) arg, &cinfo, sizeof(cinfo)); */

/*	case SNDCTL_DSP_GETOPTR: */
/*		if (!(file->f_mode & FMODE_READ)) */
/*			return -EINVAL; */
/*		spin_lock_irqsave(&s->lock, flags); */
/*		cinfo.bytes = s->play.total_bytes; */
/*		count = s->play.count; */
/*		if (!s->play.stopped) { */
/*			diff = dma_count_done(&s->play); */
/*			count -= diff; */
/*			cinfo.bytes += diff; */
/*			cinfo.ptr = virt_to_phys(s->play.nextOut) + diff - */
/*				s->play.dmaaddr; */
/*		} else */
/*			cinfo.ptr = virt_to_phys(s->play.nextOut) - */
/*				s->play.dmaaddr; */
/*		if (s->play.mapped) */
/*			s->play.count &= (s->play.dma_fragsize-1); */
/*		spin_unlock_irqrestore(&s->lock, flags); */
/*		if (count < 0) */
/*			count = 0; */
/*		cinfo.blocks = count >> s->play.fragshift; */
/*		return copy_to_user((void *) arg, &cinfo, sizeof(cinfo)); */

	case SNDCTL_DSP_GETBLKSIZE:
		if (file->f_mode & FMODE_WRITE)
			return put_user(s->play.transfer_size*8, (int *) arg);
		else
			return put_user(s->capture.transfer_size*8, (int *) arg);

/*	case SNDCTL_DSP_SETFRAGMENT: */
/*		if (get_user(val, (int *) arg)) */
/*			return -EFAULT; */
/*		if (file->f_mode & FMODE_READ) { */
/*			stop_capture(s); */
/*			s->capture.ossfragshift = val & 0xffff; */
/*			s->capture.ossmaxfrags = (val >> 16) & 0xffff; */
/*			if (s->capture.ossfragshift < 4) */
/*				s->capture.ossfragshift = 4; */
/*			if (s->capture.ossfragshift > 15) */
/*				s->capture.ossfragshift = 15; */
/*			if (s->capture.ossmaxfrags < 4) */
/*				s->capture.ossmaxfrags = 4; */
/*			if ((ret = prog_dmabuf_capture(s))) */
/*				return ret; */
/*		} */
/*		if (file->f_mode & FMODE_WRITE) { */
/*			stop_play(s); */
/*			s->play.ossfragshift = val & 0xffff; */
/*			s->play.ossmaxfrags = (val >> 16) & 0xffff; */
/*			if (s->play.ossfragshift < 4) */
/*				s->play.ossfragshift = 4; */
/*			if (s->play.ossfragshift > 15) */
/*				s->play.ossfragshift = 15; */
/*			if (s->play.ossmaxfrags < 4) */
/*				s->play.ossmaxfrags = 4; */
/*			if ((ret = prog_dmabuf_play(s))) */
/*				return ret; */
/*		} */
/*		return 0; */

/*	case SNDCTL_DSP_SUBDIVIDE: */
/*		if ((file->f_mode & FMODE_READ && s->capture.subdivision) || */
/*				(file->f_mode & FMODE_WRITE && s->play.subdivision)) */
/*			return -EINVAL; */
/*		if (get_user(val, (int *) arg)) */
/*			return -EFAULT; */
/*		if (val != 1 && val != 2 && val != 4) */
/*			return -EINVAL; */
/*		if (file->f_mode & FMODE_READ) { */
/*			stop_capture(s); */
/*			s->capture.subdivision = val; */
/*			if ((ret = prog_dmabuf_capture(s))) */
/*				return ret; */
/*		} */
/*		if (file->f_mode & FMODE_WRITE) { */
/*			stop_play(s); */
/*			s->play.subdivision = val; */
/*			if ((ret = prog_dmabuf_play(s))) */
/*				return ret; */
/*		} */
/*		return 0; */

	case SOUND_PCM_READ_RATE:
		return put_user((file->f_mode & FMODE_READ) ?
				s->capture.sample_rate :
				s->play.sample_rate,
				(int *)arg);

	case SOUND_PCM_READ_CHANNELS:
		if (file->f_mode & FMODE_READ)
			return put_user(s->capture.num_channels, (int *)arg);
		else
			return put_user(s->play.num_channels, (int *)arg);

	case SOUND_PCM_READ_BITS:
		if (file->f_mode & FMODE_READ)
			return put_user(s->capture.sample_size, (int *)arg);
		else
			return put_user(s->play.sample_size, (int *)arg);

	case ARC_AC97_GET_SAMPLE_COUNT:
		if (file->f_mode & FMODE_READ)
			return put_user(s->capture.sample_count, (int *)arg);
		else
			return put_user(s->play.sample_count, (int *)arg);

	case ARC_AC97_RESET_SAMPLE_COUNT:
		if (file->f_mode & FMODE_READ)
			s->capture.sample_count = 0;
		else
			s->play.sample_count = 0;
		return 0;

	case SOUND_PCM_WRITE_FILTER:
	case SNDCTL_DSP_SETSYNCRO:
	case SOUND_PCM_READ_FILTER:
		return -EINVAL;
	}

	return mixdev_ioctl(s->codec, cmd, arg);
}

/* --------------------------------------------------------------------- */

static int	arc_ac97_open(struct inode *inode, struct file *file)
{
	int minor = MINOR(inode->i_rdev);
	DECLARE_WAITQUEUE(wait, current);
	struct arc_ac97_state *s = &arc_ac97_state;

	written = 0;
	queued = 0;
	output = 0;

	DBG_TRACE("%s\n",__FUNCTION__);
#ifdef ARC_AC97_VERBOSE_DEBUG
	if (file->f_flags & O_NONBLOCK)
		dbg(__FUNCTION__, ": non-blocking, file at 0x%x",file);
	else
		dbg(__FUNCTION__ ,": blocking, file at 0x%x",file);
#endif

	file->private_data = s;
	/* wait for device to become free */
	down(&s->open_sem);
	while (s->open_mode & file->f_mode) {
		if (file->f_flags & O_NONBLOCK) {
			up(&s->open_sem);
			DBG_TRACE("Sound device busy\n");
			return -EBUSY;
		}
		add_wait_queue(&s->open_wait, &wait);
		__set_current_state(TASK_INTERRUPTIBLE);
		up(&s->open_sem);
		schedule();
		remove_wait_queue(&s->open_wait, &wait);
		set_current_state(TASK_RUNNING);
		if (signal_pending(current))
			return -ERESTARTSYS;
		down(&s->open_sem);
	}

	stop_play(s);
	stop_capture(s);

	reset_audio_buffer(&(s->play));
	reset_audio_buffer(&(s->capture));

	if (file->f_mode & FMODE_READ) {
		s->capture.num_channels = 1;
		s->capture.sample_size = 8;
		set_capture_rate(s, 8000);
		if ((minor & 0xf) == SND_DEV_DSP16)
			s->capture.sample_size = 16;
	}

	if (file->f_mode & FMODE_WRITE) {
		s->play.num_channels = 1;
		s->play.sample_size = 8;
		set_play_rate(s, 8000);
		if ((minor & 0xf) == SND_DEV_DSP16)
			s->play.sample_size = 16;
	}

/* 	if (file->f_mode & FMODE_READ) { */
/* 		if ((ret = prog_dmabuf_capture(s))) */
/* 			return ret; */
/* 	} */
/* 	if (file->f_mode & FMODE_WRITE) { */
/* 		if ((ret = prog_dmabuf_play(s))) */
/* 			return ret; */
/* 	} */

	s->open_mode |= file->f_mode & (FMODE_READ | FMODE_WRITE);
	up(&s->open_sem);
	init_MUTEX(&s->play.sem);
	init_MUTEX(&s->capture.sem);
	return 0;
}

/* --------------------------------------------------------------------- */

static int arc_ac97_release(struct inode *inode, struct file *file)
{
	struct arc_ac97_state *s = (struct arc_ac97_state *)file->private_data;

	DBG_TRACE("%s written %d output %d queued %d\n",__FUNCTION__,written,output,queued);

//	lock_kernel();

	if (file->f_mode & FMODE_WRITE) {
//		unlock_kernel();
		drain_play(s, file->f_flags & O_NONBLOCK);
//		lock_kernel();
	}

	down(&s->open_sem);
	if (file->f_mode & FMODE_WRITE) {
		stop_play(s);
	}
	if (file->f_mode & FMODE_READ) {
		stop_capture(s);
	}
	s->open_mode &= ((~file->f_mode) & (FMODE_READ|FMODE_WRITE));
	controller->intr_ctrl = 0;
	up(&s->open_sem);
	wake_up(&s->open_wait);
	reset_audio_buffer(&(s->play));
	reset_audio_buffer(&(s->capture));
//	unlock_kernel();

	return 0;
}

/* --------------------------------------------------------------------- */

static /*const */ struct file_operations arc_ac97_audio_fops = {
	owner:		THIS_MODULE,
	llseek:		arc_ac97_llseek,
	read:		arc_ac97_read,
	write:		arc_ac97_write,
	poll:		arc_ac97_poll,
	ioctl:		arc_ac97_ioctl,
	mmap:		arc_ac97_mmap,
	open:		arc_ac97_open,
	release:	arc_ac97_release,
};


/* --------------------------------------------------------------------- */


/* --------------------------------------------------------------------- */

/*
 * for debugging purposes, we'll create a proc device that dumps the
 * CODEC chipstate
 */

#ifdef ARC_AC97_DEBUG
static int proc_arc_ac97_dump(char *buf, char **start, off_t fpos,
					int length, int *eof, void *data)
{
	struct arc_ac97_state *s = &arc_ac97_state;
	int cnt, len = 0;

	DBG_TRACE("%s\n",__FUNCTION__);
	/* print out header */
	len += sprintf(buf + len, "\n\t\tARC_AC97 Audio Debug\n\n");

	/* Add something to look at the controller state */
  // FIXME right here

	/* print out CODEC state */
	len += sprintf(buf + len, "\nAC97 CODEC registers\n");
	len += sprintf(buf + len, "----------------------\n");
	for (cnt = 0; cnt <= 0x7e; cnt += 2)
		len += sprintf(buf + len, "reg %02x = %04x\n",
			       cnt, rdcodec(s->codec, cnt));

	if (fpos >= len) {
		*start = buf;
		*eof = 1;
		return 0;
	}
	*start = buf + fpos;
	if ((len -= fpos) > length)
		return length;
	*eof = 1;
	return len;

}
#endif /* ARC_AC97_DEBUG */

/* --------------------------------------------------------------------- */
static int __init arc_ac97_probe(struct platform_device *dev)
{
	struct arc_ac97_state *s = &arc_ac97_state;
	int             val;
#ifdef ARC_AC97_DEBUG
	char            proc_str[80];
#endif

	DBG_TRACE("%s\n",__FUNCTION__);
	memset(s, 0, sizeof(struct arc_ac97_state));

	init_waitqueue_head(&(s->capture.wait));
	init_waitqueue_head(&(s->play.wait));
	init_waitqueue_head(&(s->open_wait));
	init_MUTEX(&s->open_sem);
	spin_lock_init(&s->lock);

	s->play.transfer_size = TRANSFER_SIZE;
	s->capture.transfer_size = TRANSFER_SIZE;
	s->play.latency = DEFAULT_LATENCY;
	s->capture.latency = DEFAULT_LATENCY;
	s->play.stopped = 1;
	s->capture.stopped = 1;
	s->play.bytes_per_sample = 2;
	s->capture.bytes_per_sample = 2;

	s->play.fifo_depth = 4096;       // FIXME
	s->capture.fifo_depth = 4096;

	s->capture.bufsize  = BUFSIZE;
	s->play.bufsize  = BUFSIZE;
	s->capture.buffer = 0;
	s->play.buffer = 0;
	reset_audio_buffer(&(s->play));
	reset_audio_buffer(&(s->capture));

	prog_dmabuf_capture(s);
	prog_dmabuf_play(s);

	s->codec = ac97_alloc_codec();
	if(s->codec == NULL)
	{
		err("Out of memory");
		return -1;
	}
	s->codec->private_data = s;
	s->codec->id = 0;
	s->codec->codec_read = rdcodec;
	s->codec->codec_write = wrcodec;
	s->codec->codec_wait = waitcodec;

	/* Register interrupt, set up interrupt mask. */
	s->intrs = PLAY1_INTR;
	controller->intr_ctrl = 2;
	arc_ac97_delay(10);
	controller->intr_ctrl = 0;
	controller->intr_mask = s->intrs;
  /* Define channel mappings */
	controller->point1 = 3;
	controller->point2 = 4;
	controller->point3 = 0;
	controller->play_trig1 = TRIGGER_LEVEL;
	controller->play_trig2 = TRIGGER_LEVEL;
	//	controller->play_trig2 = (64 - TRANSFER_SIZE);
	controller->play_trig3 = 0;
	info("controller version %d",ID_REVISION(controller->ID));
	extended_controller = (ID_REVISION(controller->ID) > 1);
	request_irq(AC97_INTERRUPT, (irq_handler_t)arc_ac97_interrupt, 0 , "ARC AC97", (void *)s);

	/* register devices */
	if ((s->dev_audio = register_sound_dsp(&arc_ac97_audio_fops, -1)) < 0)
		goto err_dev1;
	if ((s->codec->dev_mixer =
	     register_sound_mixer(&arc_ac97_mixer_fops, -1)) < 0)
		goto err_dev2;

#ifdef ARC_AC97_DEBUG
	/* intialize the debug proc device */
	s->ps = create_proc_read_entry(ARC_AC97_MODULE_NAME, 0, NULL,
				       proc_arc_ac97_dump, NULL);
#endif /* ARC_AC97_DEBUG */

	/* FIXME may need to force a reset of the controller here */

	/* codec init */
	if (!ac97_probe_codec(s->codec))
		goto err_dev3;

	s->codec_base_caps = rdcodec(s->codec, AC97_RESET);
	s->codec_ext_caps = rdcodec(s->codec, AC97_EXTENDED_ID);
	info("AC'97 Base/Extended ID = %04x/%04x",
	     s->codec_base_caps, s->codec_ext_caps);

	s->codec->supported_mixers |= SOUND_MASK_ALTPCM;
	/*
	 * Now set AUX_OUT's default volume.
	 */
	val = 0x4343;
	mixdev_ioctl(s->codec, SOUND_MIXER_WRITE_ALTPCM,
		     (unsigned long) &val);

	if (!(s->codec_ext_caps & AC97_EXTID_VRA)) {
		// codec does not support VRA
		s->no_vra = 1;
	} else if (!vra) {
		// Boot option says disable VRA
		u16 ac97_extstat = rdcodec(s->codec, AC97_EXTENDED_STATUS);
		wrcodec(s->codec, AC97_EXTENDED_STATUS,
			ac97_extstat & ~AC97_EXTSTAT_VRA);
		s->no_vra = 1;
	}
	if (s->no_vra)
		info("no VRA, interpolating and decimating");

	/* set mic to be the recording source */
	val = SOUND_MASK_MIC;
	mixdev_ioctl(s->codec, SOUND_MIXER_WRITE_RECSRC,
		     (unsigned long) &val);

#ifdef ARC_AC97_DEBUG
	sprintf(proc_str, "driver/%s/%d/ac97", ARC_AC97_MODULE_NAME,
		s->codec->id);
	s->ac97_ps = create_proc_read_entry (proc_str, 0, NULL,
					     ac97_read_proc, &s->codec);
#endif

	return 0;

 err_dev3:
	unregister_sound_mixer(s->codec->dev_mixer);
 err_dev2:
	unregister_sound_dsp(s->dev_audio);
 err_dev1:
	ac97_release_codec(s->codec);
	return -1;
}

/* --------------------------------------------------------------------- */

static int arc_ac97_remove(struct platform_device *dev)
{
	struct arc_ac97_state *s = &arc_ac97_state;

	DBG_TRACE("%s\n",__FUNCTION__);
	if (!s)
		return -1;
#ifdef ARC_AC97_DEBUG
	if (s->ps)
		remove_proc_entry(ARC_AC97_MODULE_NAME, NULL);
#endif /* ARC_AC97_DEBUG */
	free_irq(AC97_INTERRUPT, 0);
	unregister_sound_dsp(s->dev_audio);
	unregister_sound_mixer(s->codec->dev_mixer);
	ac97_release_codec(s->codec);
	return 0;
}

#ifndef MODULE

static int arc_ac97_setup(char *options)
{
	char           *this_opt;

	if (!options || !*options)
		return 0;

	while((this_opt=strsep(&options, ",")) != NULL)
        {
                if (!strncmp(this_opt, "vra", 3)) {
                        vra = 1;
                }
        }
	return 1;
}

__setup("arc_ac97_audio=", arc_ac97_setup);

#endif /* MODULE */

static struct platform_driver arc_ac97_driver = {
        .driver = {
                .name = "arc_ac97",
        },
        .probe = arc_ac97_probe,
        .remove = arc_ac97_remove,
};

static struct platform_device *arc_ac97_device;

static int __init arc_ac97_init(void)
{
  int ret;

  DBG_TRACE("ARC AC97 controller\n");
  info("Built " __TIME__ " on " __DATE__);
  ret = platform_driver_register(&arc_ac97_driver);
  if (!ret) {
        arc_ac97_device = platform_device_register_simple ("arc_ac97", 0, NULL, 0);
        ret = (int)platform_get_drvdata (arc_ac97_device);
  }
  return ret;
}

static void __exit arc_ac97_exit(void)
{
        info("unloading");
        platform_device_unregister (arc_ac97_device);
        platform_driver_unregister (&arc_ac97_driver);
}

/* --------------------------------------------------------------------- */
module_init(arc_ac97_init);
module_exit(arc_ac97_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pete Shepherd of ARC fame (Ported to 2.6.19 kernel by KT)");
MODULE_DESCRIPTION("ARC AC97 Audio Driver");
