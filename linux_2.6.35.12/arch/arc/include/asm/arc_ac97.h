/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASMARC_AC97_H
#define __ASMARC_AC97_H

#ifdef __KERNEL__
#define ARC_EXTENDED_AC97
/* This is what our controller looks like: */

typedef struct {
	volatile unsigned long ID;

	volatile unsigned long padding1[3];

	volatile unsigned long play_data1;
	volatile unsigned long play_data2;
	volatile unsigned long play_data3;

#ifdef ARC_EXTENDED_AC97
	volatile unsigned long padding2a;

	volatile unsigned long play_count1;
	volatile unsigned long play_count2;
	volatile unsigned long play_count3;

	volatile unsigned long padding2b;
#else
	volatile unsigned long padding2[5];
#endif // ARC_EXTENDED_AC97

	volatile unsigned long play_trig1;
	volatile unsigned long play_trig2;
	volatile unsigned long play_trig3;

	volatile unsigned long padding3[5];

	volatile unsigned long cap_data1;
	volatile unsigned long cap_data2;
	volatile unsigned long cap_data3;

#ifdef ARC_EXTENDED_AC97
	volatile unsigned long padding4a;

	volatile unsigned long cap_count1;
	volatile unsigned long cap_count2;
	volatile unsigned long cap_count3;

	volatile unsigned long padding4b;
#else
	volatile unsigned long padding4[5];
#endif // ARC_EXTENDED_AC97

	volatile unsigned long cap_trig1;
	volatile unsigned long cap_trig2;
	volatile unsigned long cap_trig3;

	volatile unsigned long padding5[5];

	volatile unsigned long cmd;
	volatile unsigned long status;
	volatile unsigned long intr_id;
	volatile unsigned long intr_mask;
	volatile unsigned long intr_ctrl;
	volatile unsigned long point1;
	volatile unsigned long point2;
	volatile unsigned long point3;
} ARC_AC97_if;

/* Aurora-defined base address */
#define AC97_CONTROLLER_BASE  0xc0fcb000
#define AC97_INTERRUPT 12

#define MAX_FRAG 128
#define FRAGSIZE 2048
#define FIFO_LEN 4096
#define TRANSFER_SIZE 1024
#define TRIGGER_LEVEL  3072
#define TMPBUF_SIZE 32768
#define DEFAULT_LATENCY 1
#define START_THRESHOLD 0.5
#define BUFSIZE 0x20000

/* Some ID register decoding */
#define ID_CONFIG(regval)   ((regval & 0xff000000) > 24)
#define ID_REVISION(regval) ((regval & 0xff0000) > 16)
#define ID_NID(regval)      ((regval & 0x3f00) > 8)
#define ID_ID(regval)       (regval & 0x3f)

/* Command types */
#define CMD_READ 0x800000
#define CMD_WRITE 0x0

/* Interrupt IDs */
#define CAP1_INTR             0x00000004
#define CAP2_INTR             0x00000002
#define CAP3_INTR             0x00000001
#define PLAY1_INTR            0x00000020
#define PLAY2_INTR            0x00000010
#define PLAY3_INTR            0x00000008
#define STATUS_INTR           0x00000040

#ifdef ARC_EXTENDED_AC97

#define PLAY1_UNDER_INTR      0x00002000
#define PLAY2_UNDER_INTR      0x00001000
#define PLAY3_UNDER_INTR      0x00000800
#define CAP1_UNDER_INTR       0x00000400
#define CAP2_UNDER_INTR       0x00000200
#define CAP3_UNDER_INTR       0x00000100
#define STATUS_UNDER_INTR     0x00004000

#define PLAY1_OVER_INTR       0x00200000
#define PLAY2_OVER_INTR       0x00100000
#define PLAY3_OVER_INTR       0x00080000
#define CAP1_OVER_INTR        0x00040000
#define CAP2_OVER_INTR        0x00020000
#define CAP3_OVER_INTR        0x00010000
#define STATUS_OVER_INTR      0x00400000
#define CMD_OVER_INTR         0x00800000

#endif // ARC_EXTENDED_AC97

#define CAPTURE_INTRS 0x07
#define PLAY_INTRS    0x38

#define INTR_ENABLE 1
#define INTR_CLEAR  2
#define INTR_STATUS 3


/* Status bits */
#define AC97C_READY 0x8000



/* Working around some of the platform-specific stupidity in soundcore.c ... */

#ifdef valid_dma
#undef valid_dma
#endif
#define valid_dma(n) 1

#define MAX_DMA_CHANNELS 1

#endif /* __KERNEL__ */

/* Custom ioctls */
#define ARC_AC97_GET_SAMPLE_COUNT     0xffff0001
#define ARC_AC97_RESET_SAMPLE_COUNT   0xffff0002

#endif
