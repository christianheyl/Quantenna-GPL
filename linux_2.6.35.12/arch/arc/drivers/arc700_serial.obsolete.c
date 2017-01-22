/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Driver for console on serial interface for the ArcAngel4 board.
 *
 * vineetg: Nov 2009
 *  -Rewrote the driver register access macros so that multiple accesses
 *   in same function use "anchor" reg to save the base addr causing
 *   shorter instructions
 *
 * Vineetg: Mar 2009
 *  -For common case of 1 UART instance, Reg base addr embedded as long immed
 *    within code rather than accessed from globals
 *
 * Vineetg: Sept 3rd 2008:
 *  -Added Interrupt Safe raw printing routine
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/console.h>
#include <linux/serial.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/interrupt.h>
#include <linux/spinlock_types.h>
#include <linux/spinlock.h>

#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/serial.h>

/*
 * This is our internal structure for each serial port's state.
 *
 * Many fields are paralleled by the structure used by the serial_struct
 * structure.
 *
 * For definitions of the flags field, see tty.h
 */

struct arc_serial_dev {

    char is_cons;       /* Is this our console. */

    /*
     * We need to know the current clock divisor
     * to read the bps rate the chip has currently loaded.
     */
    int         baud;
    int         magic;
    int         baud_base;
    int         port;
    int         irq;
    int         flags;                  /* defined in tty.h */
    int         type;                   /* UART type        */

    struct tty_struct   *tty;

    int         read_status_mask;
    int         ignore_status_mask;
    int         timeout;
    int         xmit_fifo_size;
    int         custom_divisor;
    int         x_char;                 /* xon/xoff character */
    int         close_delay;

    unsigned short      closing_wait;
    unsigned short      closing_wait2;
    unsigned long       event;
    unsigned long       last_active;
    int                 line;
    int                 count;          /* # of fd on device */
    long                session;        /* Session of opening process */
    long                pgrp;           /* pgrp of opening process */
    unsigned char       *xmit_buf;
    int                 xmit_head;
    int                 xmit_tail;
    int                 xmit_cnt;
    struct work_struct  tqueue;
    struct work_struct  tqueue_hangup;

    struct mutex            port_write_mutex;
    struct tasklet_struct   tasklet;

    wait_queue_head_t   close_wait;
};

typedef volatile struct {
  unsigned char id0 __attribute__((aligned(4)));
  unsigned char id1 __attribute__((aligned(4)));
  unsigned char id2 __attribute__((aligned(4)));
  unsigned char id3 __attribute__((aligned(4)));
  unsigned char data __attribute__((aligned(4)));
  unsigned char status __attribute__((aligned(4)));
  unsigned char baudl __attribute__((aligned(4)));
  unsigned char baudh __attribute__((aligned(4)));
} arc_uart_dev;

#define SERIAL_MAGIC 0x5301

/*
 * Events are used to schedule things to happen at timer-interrupt
 * time, instead of at rs interrupt time.
 */
#define ARCSERIAL_EVENT_WRITE_WAKEUP    0

/* number of characters left in xmit buffer before we ask for more */
#define WAKEUP_CHARS 256



/*
 * Define the number of ports supported and their irqs.
 */
#define NR_PORTS CONFIG_ARC_SERIAL_NR_PORTS
#if NR_PORTS > 3
#error ARC Linux supports only 3 UARTS
#endif


#define UART_RXENB          0x04
#define UART_RXEMPTY        0x20
#define UART_FRAME_ERR      0x01
#define UART_OVERFLOW_ERR   0x02

#define UART_TXENB          0x40
#define UART_TXEMPTY        0x80

/* ioctls */
#define GET_BAUD 1
#define SET_BAUD 2



static int arc_console_baud = (CONFIG_ARC700_CLK/(CONFIG_ARC_SERIAL_BAUD * 4))
                                     - 1;

/*
 * Array of Serial devices indexed with their port numbers
 * Array of pointers to the serial devices indexed with their IRQ numbers
 * Map of the port numbers to the IRQ's on the cpu
 * Array of Base Address of the UART devices, memory mapped
 */

static struct arc_serial_dev ser_dev[NR_PORTS];
static struct arc_serial_dev *irq_ser_dev[NR_IRQS];

static unsigned int uart_irqs[NR_PORTS] = {
#if NR_PORTS >= 1
    VUART_IRQ
#endif
#if NR_PORTS >= 2
    ,VUART1_IRQ
#endif
#if NR_PORTS >= 3
    ,VUART2_IRQ
#endif
#if NR_PORTS >= 4
#error ARC Linux supports only 3 UARTS
#endif
};

/*--------------------------------------------------------------------
 * UART Register Access Wrappers:
 *  -Implement Optimal access w/o affecting rest of driver
 *  -Hide platform caveats such as cached/uncached access etc
 *-------------------------------------------------------------------*/

#ifdef ARC_SIMPLE_REG_ACCESS
/* Small optimisn trick:
 * base address can be used as long immediate and it will be embedded in
 * code itself instead of fetching from globals
 */
#if NR_PORTS == 1
#define UART_REG(line)  ((arc_uart_dev *)(UART_BASE0))
#else
#define UART_REG(line)  ((arc_uart_dev *)(UART_BASE0 + (line * 0x100)))
#endif

#else  /* !ARC_SIMPLE_REG_ACCESS */

/* This is even more optimised register access.
 * For back-back accesses, above code (even for 1 UART case) generates
 * 8 byte instructions
 *      ld  r0, [0xC0FC1000]
 *      ld  r1, [0xC0FC1014]
 * This piece of inline asm "fixes/anchors" the UART address, so multiple
 * accesses in a high level function, generate a anchorage and reg-relative
 * access, like this
 *      mov gp, 0xC0FC1000
 *      ld r0, [gp, 0]
 *      ld r1, [gp, 0x14]
 */

#if NR_PORTS == 1
static inline arc_uart_dev *const UART_REG(int line)    \
{                                                       \
    arc_uart_dev *p = (arc_uart_dev *) UART_BASE0;      \
    asm ("; fix %0": "+r" (p));                         \
    return p;                                           \
}
#else   /* !NR_PORTS == 1*/

arc_uart_dev *const arc_uart_reg_tbl[ ] = {
    (arc_uart_dev *) (UART_BASE0),
    (arc_uart_dev *) (UART_BASE0 + 1 * 0x100),
    (arc_uart_dev *) (UART_BASE0 + 2 * 0x100),
    (arc_uart_dev *) (UART_BASE0 + 3 * 0x100)
};

static inline arc_uart_dev *const UART_REG(int line)    \
{                                                       \
    arc_uart_dev *p = arc_uart_reg_tbl[line];           \
    asm ("; fix %0": "+r" (p));                         \
    return p;                                           \
}
#endif

#endif

static struct tty_driver *serial_driver;


static const char* uart_irq_names[] = { "UART0 Interrupt", "UART1 Interrupt"};

/* Forward Declarations */
static irqreturn_t arcserial_interrupt(int, void *);
static void arcserial_start(struct tty_struct *tty);
static void arcserial_flush_buffer(struct tty_struct *tty);

DEFINE_SPINLOCK(arcserial_rxlock);
DEFINE_SPINLOCK(arcserial_txlock);


// #define ARCSERIAL_USE_TASKLET


/*****************************************************************************
 *
 * Serial Driver Operations and their helper functions
 *
 ****************************************************************************/

/* This function  resets the vuart, this is done here to solve the
   problem that when we restart the debugger the old values remain
   in the VUART status register. Hence we reset the VUART here
*/

void __init reset_vuart(void)
{
    arc_uart_dev *uart = UART_REG(0);
    int temp, status;

    do {
        temp = uart->data;
        status = uart->status;
        status &= UART_RXEMPTY;
    } while (!status);

    uart->status = (UART_RXEMPTY | UART_TXEMPTY);
}


static void change_speed(struct arc_serial_dev *info, unsigned long newbaud)
{
    arc_uart_dev *uart = UART_REG(info->line);
    unsigned short port;
    unsigned long tmp;

    if (!info->tty || !info->tty->termios)
        return;

    if (!(port = info->port))
        return;

    //printk("Changing baud to %d\n",newbaud);

    while (! (uart->status & UART_TXEMPTY)) {}

    tmp = (CONFIG_ARC700_CLK/(newbaud*4)) - 1;

    info->baud = (int) newbaud;
    uart->baudl = (tmp & 0xff);
    uart->baudh = (tmp & 0xff00) >> 8;
}

static int startup(struct arc_serial_dev * info)
{
    arc_uart_dev *uart = UART_REG(info->line);
    // unsigned long flags;

    if (info->flags & ASYNC_INITIALIZED)
        return 0;

    if (!info->xmit_buf) {
        info->xmit_buf = (unsigned char *)get_zeroed_page(GFP_KERNEL);
        if (!info->xmit_buf)
            return -ENOMEM;
    }

    /*
     * Clear the FIFO buffers and disable them
     * (they will be reenabled in change_speed())
     */

    info->xmit_fifo_size = 1;

    if (info->tty)
        clear_bit(TTY_IO_ERROR, &info->tty->flags);

    /* and set the speed of the serial port */
    change_speed(info,CONFIG_ARC_SERIAL_BAUD);

    info->flags |= ASYNC_INITIALIZED;

    // spin_lock_irqsave(&arcserial_txlock, flags);
    info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;
    // spin_unlock_irqrestore(&arcserial_txlock, flags);

    uart->status = UART_RXENB | UART_TXENB;

    return 0;
}

static int block_til_ready(struct tty_struct *tty, struct file * filp,
                 struct arc_serial_dev *info)
{
    int retval = 0;

    /*
     * If the device is in the middle of being closed, then block
     * until it's done, and then try again.
     */

    if (info->flags & ASYNC_CLOSING) {

        interruptible_sleep_on(&info->close_wait);
#ifdef SERIAL_DO_RESTART
        if (info->flags & ASYNC_HUP_NOTIFY)
            return -EAGAIN;
        else
            return -ERESTARTSYS;
#else
        return -EAGAIN;
#endif
    }

    if (tty_hung_up_p(filp) || !(info->flags & ASYNC_INITIALIZED)) {
#ifdef SERIAL_DO_RESTART
        if (info->flags & ASYNC_HUP_NOTIFY)
            retval = -EAGAIN;
        else
            retval = -ERESTARTSYS;
#else
        retval = -EAGAIN;
#endif
    }

    if (retval)
        return retval;

    info->flags |= ASYNC_NORMAL_ACTIVE;
    return 0;
}

int arcserial_open(struct tty_struct *tty, struct file *filp)
{
    struct arc_serial_dev *info;
    int retval, line;

    line = tty->index;

    if ((line < 0) || (line >= NR_PORTS)) {
        printk("AA3 serial driver: check your lines\n");
        return -ENODEV;
    }

    info = &ser_dev[line];

    tty->driver_data = info;
    info->tty = tty;

    /*  Start up serial port */
    retval = startup(info);

    if (retval)
        return retval;

    retval = block_til_ready(tty, filp, info);

    if (retval)
        return retval;

    info->count++;
    return 0;
}

/*
 * This routine will shutdown a serial port; interrupts are disabled, and
 * DTR is dropped if the hangup on close termio flag is on.
 */
static void shutdown(struct arc_serial_dev * info)
{
    arc_uart_dev *uart = UART_REG(info->line);
    unsigned long flags;

    if (!(info->flags & ASYNC_INITIALIZED))
        return;

    mutex_lock(&(info->port_write_mutex));

    uart->status = 0; /* All off! */

    spin_lock_irqsave(&arcserial_rxlock, flags);
    spin_lock(&arcserial_txlock);

    if (info->xmit_buf) {
        free_page((unsigned long) info->xmit_buf);
        info->xmit_buf = 0;
    }

    if (info->tty)
        set_bit(TTY_IO_ERROR, &info->tty->flags);

    info->flags &= ~ASYNC_INITIALIZED;

    // Shutdown real port
    uart->status &= ~UART_RXENB;
    uart->status &= ~UART_TXENB;

    spin_unlock(&arcserial_txlock);
    spin_unlock_irqrestore(&arcserial_rxlock, flags);

    mutex_unlock(&(info->port_write_mutex));
}

/*
 * arcserial_close()
 *
 * This routine is called when the serial port gets closed.  First, we
 * wait for the last remaining data to be sent. Then, we unlink its
 * async structure from the interrupt chain if necessary, and we free
 * that IRQ if nothing is left in the chain.
 */

static void arcserial_close(struct tty_struct *tty, struct file * filp)
{
    struct arc_serial_dev * info = (struct arc_serial_dev *)tty->driver_data;

    if (tty_hung_up_p(filp))
        return;

    if ((tty->count == 1) && (info->count != 1)) {
        /*
         * Uh, oh. tty->count is 1, which means that the tty
         * structure will be freed. Info->count should always
         * be one in these conditions. If it's greater than
         * one, we've got real problems, since it means the
         * serial port won't be shutdown.
         */
        printk("arcserial_close: bad serial port count; tty->count is 1, "
                     "info->count is %d\n", info->count);
        info->count = 1;
    }

    if (--info->count < 0) {
        printk("arcserial_close: bad serial port count for ttyS%d: %d\n",
                     info->line, info->count);
        info->count = 0;
    }

    if (info->count)
        return;


    info->flags |= ASYNC_CLOSING;

    /*
     * Now we wait for the transmit buffer to clear; and we notify
     * the line discipline to only process XON/XOFF characters.
     */
    tty->closing = 1;

    if (info->closing_wait != ASYNC_CLOSING_WAIT_NONE)
        tty_wait_until_sent(tty, info->closing_wait);

    /*
     * At this point we stop accepting input. To do this, we
     * disable the receive line status interrupts, and tell the
     * interrupt driver to stop checking the data ready bit in the
     * line status register.
     */


    shutdown(info);
    arcserial_flush_buffer(tty);

    tty_ldisc_flush(tty);
    tty->closing = 0;
    info->event = 0;
    info->tty = 0;
    info->flags &= ~(ASYNC_NORMAL_ACTIVE | ASYNC_CLOSING);
    wake_up_interruptible(&info->close_wait);
}


void enable_uart_tx(struct arc_serial_dev *info)
{
#ifdef ARCSERIAL_USE_TASKLET
    tasklet_schedule(&info->tasklet);
#else
    unsigned long flags;
    arc_uart_dev *uart = UART_REG(info->line);
    spin_lock_irqsave(&arcserial_txlock, flags);
    uart->status |= UART_TXENB;
    spin_unlock_irqrestore(&arcserial_txlock, flags);
#endif
}


static int
arcserial_write(struct tty_struct * tty, const unsigned char *buf, int count)
{
    int c, total = 0;
    struct arc_serial_dev *info = (struct arc_serial_dev *)tty->driver_data;
    int xmit_tail, xmit_cnt;

    if (!tty || !info->xmit_buf)
        return 0;

    mutex_lock(&(info->port_write_mutex));

    if (!(info->flags & ASYNC_INITIALIZED))
    {
        mutex_unlock(&(info->port_write_mutex));
        return 0;
    }

    xmit_tail = info->xmit_tail;

    if (info->xmit_head >= xmit_tail)
    {
        xmit_cnt = info->xmit_head - xmit_tail;
    }
    else
    {
        xmit_cnt = info->xmit_head + SERIAL_XMIT_SIZE - xmit_tail;
    }


    // if the queue is full we discard the data
    while (1) {
        c = min_t(int, count, min(SERIAL_XMIT_SIZE - xmit_cnt - 1,
                      SERIAL_XMIT_SIZE - info->xmit_head));

        if (c <= 0)
            break;

        memcpy(info->xmit_buf + info->xmit_head, buf, c);

        info->xmit_head = (info->xmit_head + c) & (SERIAL_XMIT_SIZE-1);
        buf += c;
        count -= c;
        total += c;
    }

    mutex_unlock(&(info->port_write_mutex));

    if (total > 0 && !tty->stopped && !tty->hw_stopped)
        enable_uart_tx(info);

    return total;
}

static void arcserial_flush_chars(struct tty_struct *tty)
{
    struct arc_serial_dev *info = (struct arc_serial_dev *)tty->driver_data;
    int xmit_cnt, xmit_tail, xmit_head;

    xmit_tail = info->xmit_tail;
    xmit_head = info->xmit_head;

    if (xmit_head >= xmit_tail)
        xmit_cnt = xmit_head - xmit_tail;
    else
        xmit_cnt = xmit_head + SERIAL_XMIT_SIZE - xmit_tail;

    if (xmit_cnt <= 0 || tty->stopped || tty->hw_stopped || !info->xmit_buf)
        return;

    /* Enable transmitter */
    enable_uart_tx(info);
    tty_wakeup(tty);
}

static int arcserial_chars_in_buffer(struct tty_struct *tty)
{
    struct arc_serial_dev *info = (struct arc_serial_dev *)tty->driver_data;
    int xmit_cnt, xmit_tail, xmit_head;

    xmit_tail = info->xmit_tail;
    xmit_head = info->xmit_head;

    if (xmit_head >= xmit_tail)
        xmit_cnt = xmit_head - xmit_tail;
    else
        xmit_cnt = xmit_head + SERIAL_XMIT_SIZE - xmit_tail;

    return xmit_cnt;
}

static int arcserial_write_room(struct tty_struct *tty)
{
    return ((SERIAL_XMIT_SIZE - arcserial_chars_in_buffer(tty)) - 1);
}

/*
 * arcserial_ioctl() and friends
 */

// FIXME :: uncomment when ioctl's are implemented
#if 0

static int get_serial_info(struct arc_serial_dev * info,
                 struct serial_struct * retinfo)
{
    struct serial_struct tmp;

    if (!retinfo)
        return -EFAULT;

    memset(&tmp, 0, sizeof(tmp));
    tmp.type = info->type;
    tmp.line = info->line;
    tmp.port = info->port;
    tmp.irq = info->irq;
    tmp.flags = info->flags;
    tmp.baud_base = info->baud_base;
    tmp.close_delay = info->close_delay;
    tmp.closing_wait = info->closing_wait;
    tmp.custom_divisor = info->custom_divisor;
    copy_to_user(retinfo,&tmp,sizeof(*retinfo));
    return 0;
}

static int
set_serial_info(struct arc_serial_dev * info, struct serial_struct * new_info)
{
    struct serial_struct new_serial;
    struct arc_serial_dev old_info;
    int retval = 0;

    if (!new_info)
        return -EFAULT;

    copy_from_user(&new_serial,new_info,sizeof(new_serial));
    old_info = *info;

    if (!suser()) {
        if ((new_serial.baud_base != info->baud_base) ||
                (new_serial.type != info->type) ||
                (new_serial.close_delay != info->close_delay) ||
                ((new_serial.flags & ~ASYNC_USR_MASK) !=
                 (info->flags & ~ASYNC_USR_MASK)))
            return -EPERM;

        info->flags = ((info->flags & ~ASYNC_USR_MASK) |
                         (new_serial.flags & ASYNC_USR_MASK));

        info->custom_divisor = new_serial.custom_divisor;
        goto check_and_exit;
    }

    if (info->count > 1)
        return -EBUSY;

    /*
     * OK, past this point, all the error checking has been done.
     * At this point, we start making changes.....
     */

    info->baud_base = new_serial.baud_base;
    info->flags = ((info->flags & ~ASYNC_FLAGS) |
            (new_serial.flags & ASYNC_FLAGS));
    info->type = new_serial.type;
    info->close_delay = new_serial.close_delay;
    info->closing_wait = new_serial.closing_wait;

check_and_exit:
    retval = startup(info);
    return retval;
}
#endif  /* if 0 */

static int
arcserial_ioctl(struct tty_struct *tty, struct file *file, unsigned int cmd,
                                                            unsigned long arg)
{
    struct arc_serial_dev * info = (struct arc_serial_dev *)tty->driver_data;
    unsigned long tmpul;

    //printk("ARC: serial_driver, ioctl line %d, cmd 0x%x\n",info->line,cmd);

    switch (cmd)
    {
        case GET_BAUD:
            return copy_to_user((void *) arg, &(info->baud),
                                    sizeof(unsigned long)) ? -EFAULT : 0;

        case SET_BAUD:
            if (copy_from_user((void *)&tmpul, (void *) arg,
                                    sizeof(unsigned long)))
            return -EFAULT;

            change_speed(info, tmpul);
            return 0;

        default:
            // FIXME This is wrongwrongwrong, but how do I make it right?
            return n_tty_ioctl_helper(tty, file, cmd, arg);
            // return -EINVAL;
    }

    return -EINVAL;
}

static void
arcserial_set_termios(struct tty_struct *tty, struct ktermios *old_termios)
{
    struct arc_serial_dev *info = (struct arc_serial_dev *)tty->driver_data;

    //printk("AA3 serial: in arcserial_set_termios\n");
    if (tty->termios->c_cflag == old_termios->c_cflag)
        return;

    change_speed(info, CONFIG_ARC_SERIAL_BAUD);

    if ((old_termios->c_cflag & CRTSCTS) &&
            !(tty->termios->c_cflag & CRTSCTS)) {
        tty->hw_stopped = 0;
        arcserial_start(tty);
    }
}


/*
 * arcserial_throttle()
 *
 * This routine is called by the upper-layer tty layer to signal that
 * incoming characters should be throttled.
 */

static void arcserial_throttle(struct tty_struct * tty)
{
    struct arc_serial_dev *info = (struct arc_serial_dev *)tty->driver_data;

    if (I_IXOFF(tty))
        info->x_char = STOP_CHAR(tty);

    enable_uart_tx(info);

    /* Turn off RTS line (do this atomic) */
}


static void arcserial_unthrottle(struct tty_struct * tty)
{
    struct arc_serial_dev *info = (struct arc_serial_dev *)tty->driver_data;
    unsigned long flags;

    spin_lock_irqsave(&arcserial_txlock, flags);

    if (I_IXOFF(tty)) {
        if (info->x_char)
            info->x_char = 0;
        else
            info->x_char = START_CHAR(tty);
    }

    spin_unlock_irqrestore(&arcserial_txlock, flags);

    enable_uart_tx(info);
    /* Assert RTS line (do this atomic) */
}


/*
 * arcserial_stop() and arcserial_start()
 *
 * This routines are called before setting or resetting tty->stopped.
 * They enable or disable transmitter interrupts, as necessary.
 */

static void arcserial_stop(struct tty_struct *tty)
{
    arc_uart_dev *uart =
            UART_REG(((struct arc_serial_dev *)tty->driver_data)->line);
    unsigned long flags;

    spin_lock_irqsave(&arcserial_txlock, flags);
    uart->status &= ~UART_TXENB;
    spin_unlock_irqrestore(&arcserial_txlock, flags);
}


static void arcserial_start(struct tty_struct *tty)
{
    struct arc_serial_dev *info = (struct arc_serial_dev *)tty->driver_data;
    arc_uart_dev *uart = UART_REG(info->line);
    int xmit_head, xmit_tail, xmit_cnt;
    // unsigned long flags;

    xmit_tail = info->xmit_tail;
    xmit_head = info->xmit_head;

    if (xmit_head >= xmit_tail)
        xmit_cnt = xmit_head - xmit_tail;
    else
        xmit_cnt = xmit_head + SERIAL_XMIT_SIZE - xmit_tail;

    // spin_lock_irqsave(&arcserial_txlock, flags);
    if (xmit_cnt && info->xmit_buf && !(uart->status & UART_TXENB))
        uart->status |= UART_TXENB;
    // spin_unlock_irqrestore(&arcserial_txlock, flags);
}


/*
 * arcserial_hangup - called by tty_hangup() when a hangup is signaled.
 */

static void arcserial_hangup(struct tty_struct *tty)
{
    struct arc_serial_dev * info = (struct arc_serial_dev *)tty->driver_data;

    shutdown(info);
    arcserial_flush_buffer(tty);
    info->event = 0;
    info->count = 0;
    info->flags &= ~ASYNC_NORMAL_ACTIVE;
    info->tty = 0;
}


static void arcserial_flush_buffer(struct tty_struct *tty)
{
    struct arc_serial_dev *info = (struct arc_serial_dev *)tty->driver_data;
    unsigned long flags;

    spin_lock_irqsave(&arcserial_txlock, flags);
    info->xmit_cnt = info->xmit_head = info->xmit_tail = 0;
    spin_unlock_irqrestore(&arcserial_txlock, flags);

    tty_wakeup(tty);
}


static void arcserial_set_ldisc(struct tty_struct *tty)
{
    struct arc_serial_dev *info = (struct arc_serial_dev *)tty->driver_data;

    info->is_cons = (tty->termios->c_line == N_TTY);

    printk("AA3 serial driver: ttyS%d console mode %s\n",
                        info->line, info->is_cons ? "on" : "off");
}


static const struct tty_operations arcserial_ops = {
    .open = arcserial_open,
    .close = arcserial_close,
    .write = arcserial_write,
    .flush_chars = arcserial_flush_chars,
    .write_room = arcserial_write_room,
    .chars_in_buffer = arcserial_chars_in_buffer,
    .ioctl = arcserial_ioctl,
    .set_termios = arcserial_set_termios,
    .throttle = arcserial_throttle,
    .unthrottle = arcserial_unthrottle,
    .stop = arcserial_stop,
    .start = arcserial_start,
    .hangup = arcserial_hangup,
    .flush_buffer = arcserial_flush_buffer,
    .set_ldisc = arcserial_set_ldisc,
};

/*****************************************************************************
 *
 *                    Serial Driver Interrupt Handling
 *
 ****************************************************************************/


/*
 * This routine is used by the interrupt handler to schedule
 * processing in the software interrupt portion of the driver.
 */
static inline void sched_event(struct arc_serial_dev *info,
                        int event)
{
    info->event |= 1 << event;
    //  queue_task(&info->tqueue, &tq_serial);
    //  mark_bh(SERIAL_BH);
}


static void receive_chars(struct arc_serial_dev *info, unsigned short rx)
{
    struct tty_struct *tty = info->tty;
    unsigned char ch, flag;
    arc_uart_dev *uart = UART_REG(info->line);
    int status;

    //Originally added by Sameer, serial I/O slow without this
    tty->low_latency = 0;

    if(!tty)
    {
        printk("AA3 Serial: missing tty on line %d\n", info->line);
        goto clear_and_exit;
    }

    flag = TTY_NORMAL;

    do {
        ch = rx & 0x00ff;

        if(uart->status & UART_OVERFLOW_ERR) {
            printk("AA3 Serial Driver: "
                            "Overflow Error while receiving a character\n");
            flag = TTY_OVERRUN;
            uart->status &= ~UART_OVERFLOW_ERR;

        } else if(uart->status & UART_FRAME_ERR) {

            printk("AA3 Serial Driver: "
                            "Framing Error while receiving a character\n");
            flag = TTY_FRAME;
            uart->status &= ~UART_FRAME_ERR;

        } else {
               }

        tty_insert_flip_char(tty, ch, flag);

        if(!(status = uart->status & UART_RXEMPTY))
             rx = uart->data;

        /* keep reading characters till the RxFIFO becomes empty */
    } while(!status);

    tty_flip_buffer_push(tty);

clear_and_exit:
    return;
}

static void transmit_chars(unsigned long val)
{
    struct arc_serial_dev *info = (struct arc_serial_dev *)val;
    arc_uart_dev *uart = UART_REG(info->line);
    int head, xmit_cnt=0;
    bool uart_txenb = false;
    unsigned long flags;

    spin_lock_irqsave(&arcserial_txlock, flags);

    if (!(info->flags & ASYNC_INITIALIZED))
        goto clear_and_return;

    if (info->x_char) {
        /* Send next char */
        uart->data = info->x_char;
        info->x_char = 0;
        uart_txenb = true;
        goto clear_and_return;
    }

    if (info->tty->stopped)
    {
        uart_txenb = false;
        goto clear_and_return;
    }

    head = info->xmit_head;

    if (head >= info->xmit_tail)
        xmit_cnt = head - info->xmit_tail;
    else
        xmit_cnt = head + SERIAL_XMIT_SIZE - info->xmit_tail;


    if (xmit_cnt >= 1)
    {
        // Send the character and increment tail index
        uart->data = info->xmit_buf[info->xmit_tail];
        info->xmit_tail = (info->xmit_tail + 1) & (SERIAL_XMIT_SIZE-1);
    }

    if ((xmit_cnt-1) < WAKEUP_CHARS)
        sched_event(info, ARCSERIAL_EVENT_WRITE_WAKEUP);


clear_and_return:

    if (xmit_cnt > 1 || uart_txenb)
        uart->status |= UART_TXENB;
    else
        uart->status &= ~UART_TXENB;

    spin_unlock_irqrestore(&arcserial_txlock, flags);

    return;
}


/*
 * This is the serial driver's generic interrupt routine
 */
static irqreturn_t arcserial_interrupt(int irq, void *dev_id)
{
    struct arc_serial_dev *info;
    arc_uart_dev *uart;

    info = irq_ser_dev[irq];
    if(!info)
        return 0;

    uart = UART_REG(info->line);

    spin_lock(&arcserial_rxlock);

    if((uart->status & UART_RXENB) && !(uart->status & UART_RXEMPTY))
    {
        receive_chars(info, uart->data);
    }

    spin_unlock(&arcserial_rxlock);

    if((uart->status & UART_TXENB) && (uart->status & UART_TXEMPTY))
    {
        // Disable the interrupt, transmit_chars will reenable it,
        // if their is more data to send.
        uart->status &= ~UART_TXENB;

#ifdef ARCSERIAL_USE_TASKLET
        tasklet_schedule(&info->tasklet);
#else
        transmit_chars((unsigned long)info);
#endif

    }

    if(uart->status & UART_FRAME_ERR)
        uart->status &= ~UART_FRAME_ERR;
    if(uart->status & UART_OVERFLOW_ERR)
        uart->status &= ~UART_OVERFLOW_ERR;


    return 0;
}


/*
static void do_softint(void *private_)
{
    struct arc_serial_dev *info = (struct arc_serial_dev *) private_;
    struct tty_struct *tty;

    tty = info->tty;
    if (!tty)
        return;

    if (test_and_clear_bit(ARCSERIAL_EVENT_WRITE_WAKEUP, &info->event)) {
        tty_wakeup(tty);
    }
}
 */


/*
 *
 * This routine is called from the scheduler tqueue when the interrupt
 * routine has signalled that a hangup has occurred.    The path of
 * hangup processing is:
 *
 *  serial interrupt routine -> (scheduler tqueue) ->
 *  do_serial_hangup() -> tty->hangup() -> arcserial_hangup()
 *
 */

/*
static void do_serial_hangup(void *private_)
{
    struct arc_serial_dev *info = (struct arc_serial_dev *) private_;
    struct tty_struct *tty;

    tty = info->tty;
    if (!tty)
        return;

    tty_hangup(tty);
}
 */


/*****************************************************************************
 *
 * Serial Driver Initialiation and driver registration
 *
 ****************************************************************************/

static void arcserial_interrupts_init(void)
{
    int i, ret;
    unsigned long tmp;
    arc_uart_dev *uart;

    for(i=0; i<NR_PORTS; i++)
    {
        /* Make the interrupt edge triggered to try and
           eliminate spurious interrupts */

        tmp = read_new_aux_reg(AUX_ITRIGGER);
        tmp |= (1<<uart_irqs[i]);
        write_new_aux_reg(AUX_ITRIGGER,tmp);

        uart = UART_REG(i);
        uart->status &= ~UART_RXENB;
        uart->status &= ~UART_TXENB;

        ret = request_irq(uart_irqs[i], arcserial_interrupt,
                            IRQ_FLG_LOCK, uart_irq_names[i], NULL);
        if(ret)
            panic("Unable to attach arc serial interrupt\n");
    }
}

static int __init arcserial_init(void)
{
    struct arc_serial_dev *info;
    int i;

    serial_driver = alloc_tty_driver(CONFIG_ARC_SERIAL_NR_PORTS);
    if (!serial_driver)
        return -ENOMEM;


    /* Initialize the tty_driver structure */
    serial_driver->owner = THIS_MODULE;
    serial_driver->driver_name = "ARC Serial Driver";
    serial_driver->name = "ttyS";
    serial_driver->major = TTY_MAJOR;
    serial_driver->minor_start = 64;
    serial_driver->num = NR_PORTS;
    serial_driver->type = TTY_DRIVER_TYPE_SERIAL;
    serial_driver->subtype = SERIAL_TYPE_NORMAL;
    serial_driver->init_termios = tty_std_termios;

    serial_driver->init_termios.c_cflag =
        B9600 | CS8 | CREAD | HUPCL | CLOCAL;
    serial_driver->flags = TTY_DRIVER_REAL_RAW;
    tty_set_operations(serial_driver, &arcserial_ops);

    if (tty_register_driver(serial_driver)) {
        printk("%s: Couldn't register serial driver\n", __FUNCTION__);
        return(-EBUSY);
    }

    for(i=0;i<NR_PORTS;i++) {

        info = &ser_dev[i];
        info->magic = SERIAL_MAGIC;
        info->port = (unsigned int) UART_REG(i);
        info->tty = 0;
        info->irq = uart_irqs[i];
        info->custom_divisor = 16;
        info->close_delay = 50;
        info->closing_wait = 3000;
        info->x_char = 0;
        info->event = 0;
        info->count = 0;

        mutex_init(&info->port_write_mutex);
        tasklet_init(&info->tasklet, transmit_chars, (unsigned long)info);

        // info->tqueue.routine = do_softint;
        // info->tqueue.data = info;
        // info->tqueue_hangup.routine = do_serial_hangup;
        // info->tqueue_hangup.data = info;
        // info->callout_termios = callout_driver.init_termios;
        // info->normal_termios = serial_driver.init_termios;
        // INIT_WORK(&info->tqueue, do_softint, info);
        // INIT_WORK(&info->tqueue_hangup, do_serial_hangup, info);

        init_waitqueue_head(&info->close_wait);
        info->line = i;
        // info->is_cons = ((i == 0) ? 1 : 0);
        info->is_cons = ((i == 1) ? 1 : 0);

        printk("%s%d (irq = %d, address = 0x%x)", serial_driver->name,
                                            info->line, info->irq, info->port);
        printk(" is a builtin ARC UART\n");

        irq_ser_dev[info->irq] = info;
    }

    arcserial_interrupts_init();

    for(i=0; i< NR_PORTS; i++)
        printk("Port %u = IRQ %u\n", i, uart_irqs[i]);

    return 0;
}

/*
 * register_serial and unregister_serial allows for serial ports to be
 * configured at run-time, to support PCMCIA modems.
 */
int register_serial(struct serial_struct *req)
{
    return -1;
}

void unregister_serial(int line)
{
    return;
}

module_init(arcserial_init);


#ifdef CONFIG_ARC_SERIAL_CONSOLE

/******************************************************************************
 *
 * CONSOLE DRIVER : We use VUART0 as the console device
 *
 *****************************************************************************/

/* have we initialized the console already? */
static int console_inited = 0;

/*
 * Output a single character, using UART polled mode.
 */

void arcconsole_put_char(char ch)
{
    arc_uart_dev *uart = UART_REG(0);

    /*
     * check if TXEMPTY (bit 7) in the status reg is 1. Else, we
     * aren't supposed to write right now. Wait for some time.
     */

    while(!(uart->status & UART_TXEMPTY));

    /* actually write the character */
    uart->data = ch;
}

int arcconsole_setup(struct console *cp, char *arg)
{
    arc_uart_dev *uart;
    int baud_h, baud_l;
    int i;

    for (i = 0; i < NR_PORTS; i++)
    {
        uart = UART_REG(i);
        printk("Resetting ttyS%d @ %p\n", i, uart);
        baud_l = (arc_console_baud & 0xff);
        baud_h = (arc_console_baud & 0xff00) >> 8;
        /*
         * 1. set RXENB (bit 2) to receive interrupts on data receive.
         * 2. store value of the baud rate in BL and BH.
         */
        uart->status = UART_RXENB;
        uart->baudl = baud_l;
        uart->baudh = baud_h;
    }
    console_inited = 1 ;

    return 0; /* successful initialization */
}


static struct tty_driver *arcconsole_device(struct console *c, int *index)
{
        *index = c->index;
        return serial_driver;
}

void arcconsole_write(struct console *cp, const char *p, unsigned len)
{
    while (len-- > 0) {
        if (*p == '\n')
            arcconsole_put_char('\r');

        arcconsole_put_char(*p++);
    }
}

struct console arc_console = {
    .name    = "ttyS",
    .write   = arcconsole_write,
    .read    = NULL,                /*arc_console_read,*/
    .device  = arcconsole_device,
    .unblank = NULL,
    .setup   = arcconsole_setup,
    .flags   = CON_PRINTBUFFER,
    .index   = 0,
    .cflag   = 0,
    .next    = NULL
};

static int __init arcconsole_init(void)
{
    register_console(&arc_console);
    return 0;
}

console_initcall(arcconsole_init);

/************************************************************************
 * Interrupt Safe Raw Printing Routine so we dont have to worry about
 * possible side effects of calling printk( ) such as
 * context switching, semaphores, interrupts getting re-enabled etc
 *
 * Syntax: (1) Doesn't understand Format Specifiers
 *         (2) Can print one @string and one @number (in hex)
 *
 * Note that leaf functions raw_num ( ) and raw_str( ) are not ISR safe
 * But not disabling IRQ won't hurt, possibly clobber prints
 *
 *************************************************************************/
static char dec_to_hex[] = {'0','1','2','3','4','5','6','7','8','9',
                            'A','B','C','D','E','F'};

static void raw_num(unsigned int num, int zero_ok)
{
    int nibble;
    int i;
    int leading_zeroes = 1;

    /* If @num is Zero don't print it */
//    if ( num != 0 || zero_ok) {
    if ( zero_ok || num != 0 ) {

        /* break num into 8 nibbles */
        for (i = 7; i >=0; i--) {
            nibble = (num >> (i << 2)) & 0xF;
            if ( nibble == 0 ) {
                if ( leading_zeroes && i != 0 ) continue;
            }
            else {
                if ( leading_zeroes )
                    leading_zeroes = 0;
            }
            arcconsole_put_char(dec_to_hex[nibble]);
        }
    }

    arcconsole_put_char(' ');
}

static int raw_str(const char *str)
{
    int cr = 0;

    /* Loop until we find a Sentinel in the @str */
    while (*str != '\0') {

        /* If a CR found, make a note so that we
         * print it after printing the number
         */
        if ( *str == '\n' ) {
            cr = 1;
            break;
        }
        arcconsole_put_char(*str++);
    }

    return cr;
}

void raw_printk(const char *str, unsigned int num)
{
    int cr;
    unsigned long flags;

    local_irq_save(flags);

    cr = raw_str(str);
    raw_num(num, 0);

    /* Carriage Return and Line Feed */
    if (cr) {
        arcconsole_put_char('\r');
        arcconsole_put_char('\n');
    }

    local_irq_restore(flags);
}
EXPORT_SYMBOL(raw_printk);


void raw_printk5(const char *str, unsigned int n1, unsigned int n2,
                                  unsigned int n3, unsigned int n4)
{
    int cr;
    unsigned long flags;

    local_irq_save(flags);

    cr = raw_str(str);
    raw_num(n1, 1);
    raw_num(n2, 1);
    raw_num(n3, 1);
    raw_num(n4, 1);

    /* Carriage Return and Line Feed */
    if (cr) {
        arcconsole_put_char('\r');
        arcconsole_put_char('\n');
    }

    local_irq_restore(flags);
}
EXPORT_SYMBOL(raw_printk5);
#endif

#ifdef CONFIG_EARLY_PRINTK

#define UART_BASE   0xc0fc1000
#define UART_BAUDL  0x18
#define UART_BAUDH  0x1c
#define UART_STATUS 0x14
#define UART_TXRX   0x10
#define UART_RXENB  0x04
#define UART_TXEMPTY 0x80

// Demonstrating the earlyprintk functionality
// This uses the second serial port as early printk
// once the console is fully initialized control
// is transferred back the regular console.

extern void arcconsole_write(struct console *cp, const char *p, unsigned len);
extern int arcconsole_setup(struct console *cp, char *arg);

// Low level console write.


static void arc_console_write(struct console *co, const char *s,
            unsigned count)
{

    volatile unsigned char *status = (unsigned char *)UART_BASE + UART_STATUS;
    volatile unsigned char *tx = (unsigned char *)UART_BASE + UART_TXRX;
    while (count --)
    {
        while(!(*status & UART_TXEMPTY));

        *tx = *s;
        if (*s == 0x0a)  // Handle CR/LF.
        {
            while(!(*status & UART_TXEMPTY));
            *tx = 0x0d;
        }
        s++;
    }

}

// Initialize the UART.

static int __init arc_console_setup (struct console *co, char *options)
{

//    int cflag = CREAD | HUPCL | CLOCAL;
    volatile unsigned char *baudh = (unsigned char *)UART_BASE + UART_BAUDH;
    volatile unsigned char *baudl = (unsigned char *)UART_BASE + UART_BAUDL;
    volatile unsigned char *status = (unsigned char *)UART_BASE+ UART_STATUS;

    *status = UART_RXENB;
    *baudl = (arc_console_baud & 0xff);
    *baudh = (arc_console_baud & 0xff00) >> 8;

//    cflag |= B57600 | CS8 | 0;
//    co->cflag = cflag;

    return 0;
}





static struct console arc_early_console = {
    .name = "arc_console",

    .write = arc_console_write,
    .setup = arc_console_setup,

// You can use the regular serial console too
// rather than this included version.
//    .write = arcconsole_write,
//    .setup = arcconsole_setup,
    .flags = CON_PRINTBUFFER,
    .index = -1.
    };


static struct console *early_console = &arc_early_console;

static int __init setup_early_printk(char *buf)
{
	int keep_early = 0;

	if (!buf)
		return 0;

printk("Setting up early_printk\n");


	if (strstr(buf, "keep"))
		keep_early = 1;

    early_console = &arc_console;

	if (likely(early_console)) {
		if (keep_early)
			early_console->flags &= ~CON_BOOT;
		else
			early_console->flags |= CON_BOOT;
		register_console(early_console);
	}

	return 0;
}
early_param("earlyprintk", setup_early_printk);
#endif
