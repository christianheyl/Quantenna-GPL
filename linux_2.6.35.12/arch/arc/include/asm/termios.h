/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ASM_ARC_TERMIOS_H
#define _ASM_ARC_TERMIOS_H

#include <asm/termbits.h>
#include <asm/ioctls.h>

struct winsize {
	unsigned short ws_row;
	unsigned short ws_col;
	unsigned short ws_xpixel;
	unsigned short ws_ypixel;
};

#define NCC 8
struct termio {
	unsigned short c_iflag;		/* input mode flags */
	unsigned short c_oflag;		/* output mode flags */
	unsigned short c_cflag;		/* control mode flags */
	unsigned short c_lflag;		/* local mode flags */
	unsigned char c_line;		/* line discipline */
	unsigned char c_cc[NCC];	/* control characters */
};

#ifdef __KERNEL__
/*	intr=^C		quit=^|		erase=del	kill=^U
	eof=^D		vtime=\0	vmin=\1		sxtc=\0
	start=^Q	stop=^S		susp=^Z		eol=\0
	reprint=^R	discard=^U	werase=^W	lnext=^V
	eol2=\0
*/
#define INIT_C_CC "\003\034\177\025\004\0\1\0\021\023\032\0\022\017\027\026\0"
#endif

/* modem lines */
#define TIOCM_LE	0x001   /* line enable */
#define TIOCM_DTR	0x002   /* data terminal ready */
#define TIOCM_RTS	0x004   /* request to send */
#define TIOCM_ST	0x008   /* secondary transmit */
#define TIOCM_SR	0x010   /* secondary receive */
#define TIOCM_CTS	0x020   /* clear to send */
#define TIOCM_CAR	0x040   /* carrier detect */
#define TIOCM_RNG	0x080   /* ring */
#define TIOCM_DSR	0x100   /* data set ready */
#define TIOCM_CD	TIOCM_CAR
#define TIOCM_RI	TIOCM_RNG
#define TIOCM_OUT1	0x2000
#define TIOCM_OUT2	0x4000
#define TIOCM_LOOP	0x8000

/* ioctl (fd, TIOCSERGETLSR, &result) where result may be as below */

/* line disciplines */
#define N_TTY		0
#define N_SLIP		1
#define N_MOUSE		2
#define N_PPP		3
#define N_STRIP		4
#define N_AX25		5
#define N_X25		6	/* X.25 async */
#define N_6PACK		7
#define N_MASC		8	/* Reserved for Mobitex module <kaz@cafe.net> */
#define N_R3964		9	/* Reserved for Simatic R3964 module */
#define N_PROFIBUS_FDL	10	/* Reserved for Profibus <Dave@mvhi.com> */
#define N_IRDA		11	/* Linux IrDa - http://irda.sourceforge.net/ */
#define N_SMSBLOCK	12	/* SMS block mode - for talking to GSM data cards about SMS messages */
#define N_HDLC		13	/* synchronous HDLC */
#define N_SYNC_PPP	14
#define N_HCI		15  /* Bluetooth HCI UART */

#ifdef __KERNEL__

#include "asm/uaccess.h"

/*
 * Translate a "termio" structure into a "termios". Ugh.
 */
static inline int user_termio_to_kernel_termios(struct ktermios *termios,
                        struct termio __user *termio)
{
    struct termio tmpio;

    if (copy_from_user(&tmpio, termio, sizeof(struct termio)) != 0)
        return -EFAULT;

    termios->c_iflag = (0xffff0000 & termios->c_iflag) | tmpio.c_iflag;
    termios->c_oflag = (0xffff0000 & termios->c_oflag) | tmpio.c_oflag;
    termios->c_cflag = (0xffff0000 & termios->c_cflag) | tmpio.c_cflag;
    termios->c_lflag = (0xffff0000 & termios->c_lflag) | tmpio.c_lflag;
    termios->c_line = tmpio.c_line;
    memcpy(termios->c_cc, &tmpio.c_cc, NCC);
    return 0;
}

/*
 * Translate a "termios" structure into a "termio". Ugh.
 */
static inline int kernel_termios_to_user_termio(struct termio __user *termio,
                        struct ktermios *termios)
{
    struct termio tmpio;

    tmpio.c_iflag = termios->c_iflag;
    tmpio.c_oflag = termios->c_oflag;
    tmpio.c_cflag = termios->c_cflag;
    tmpio.c_lflag = termios->c_lflag;
    tmpio.c_line = termios->c_line;
    memcpy(&tmpio.c_cc, termios->c_cc, NCC);

    if (copy_to_user(termio, &tmpio, sizeof(struct termio)) != 0)
        return -EFAULT;

    return 0;
}

#define user_termios_to_kernel_termios(k, u) copy_from_user(k, u, sizeof(struct termios))
#define kernel_termios_to_user_termios(u, k) copy_to_user(u, k, sizeof(struct termios))

#endif	/* __KERNEL__ */

#endif	/* _ASM_ARC_TERMIOS_H */
