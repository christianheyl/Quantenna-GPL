/*
 * Copyright (C) 2004, 2007-2010, 2011-2012 Synopsys, Inc. (www.synopsys.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Vineetg: Aug 21st 2010
 *  -Is uart_tx_stopped() not done in tty write path as it has already been
 *   taken care of, in serial core
 *
 * Vineetg: Aug 18th 2010
 *  -New Serial Core based ARC UART driver
 *  -Derived largely from blackfin driver albiet with some major tweaks
 *
 * TODO:
 *  -check if sysreq works
 *  -suspend/resume, although it is non-hotpluggable
 */

#if defined(CONFIG_ARC_SERIAL_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/module.h>
#include <linux/serial.h>
#include <linux/console.h>
#include <linux/sysrq.h>
#include <linux/platform_device.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/serial_core.h>

#include <asm/irq.h>
#include <asm/board/plat_memmap.h>
#include <asm/board/plat_irq.h>

/* workaround for ISS VUART baudh=0 bug */
extern int running_on_hw;

/*************************************
 * ARC UART Hardware Specs
 ************************************/
#define ARC_UART_TX_FIFO_SIZE  RUBY_UART_FIFO_SIZE

/* UART Register set:
 * Word aligned, but only 8 bits wide
 */
typedef volatile struct {
  unsigned char id0 __attribute__((aligned(4)));
  unsigned char id1 __attribute__((aligned(4)));
  unsigned char id2 __attribute__((aligned(4)));
  unsigned char id3 __attribute__((aligned(4)));
  unsigned char data __attribute__((aligned(4)));
  unsigned char status __attribute__((aligned(4)));
  unsigned char baudl __attribute__((aligned(4)));
  unsigned char baudh __attribute__((aligned(4)));
} arc_uart_registers;

/* Bits for UART Status Reg (R/W) */
#define RXIENB  0x04    // Receive Interrupt Enable
#define TXIENB  0x40    // Transmit Interrupt Enable

#define RXEMPTY 0x20    // Receive FIFO Empty: No char received
#define TXEMPTY 0x80    // Transmit FIFO Empty, thus char can be written into

#define RXFULL  0x08    // Receive FIFO full
#define RXFULL1 0x10    // Receive FIFO has space for 1 char (tot space=4)

#define RXFERR  0x01    // Frame Error: Stop Bit not detected
#define RXOERR  0x02    // OverFlow Err: Char recv but RXFULL still set

/* Uart bit fiddling helpers: lowest level */
#define RBASE(uart)  ((arc_uart_registers *)(uart->port.membase))
#define UART_REG_SET(u, reg, val) (RBASE(u)->reg = (val))
#define UART_REG_OR(u, reg, val)  (RBASE(u)->reg |= (val))
#define UART_REG_CLR(u, reg, val) (RBASE(u)->reg &= (~(val)))
#define UART_REG_GET(u, reg)      (RBASE(u)->reg)

/* Uart bit fiddling helpers: API level */
#define UART_SET_DATA(uart, val)   UART_REG_SET(uart,data,val)
#define UART_GET_DATA(uart)        UART_REG_GET(uart,data)

#define UART_SET_BAUDH(uart, val)  UART_REG_SET(uart,baudh,val)
#define UART_SET_BAUDL(uart, val)  UART_REG_SET(uart,baudl,val)

#define UART_CLR_STATUS(uart, val) UART_REG_CLR(uart,status,val)
#define UART_GET_STATUS(uart)      UART_REG_GET(uart,status)

#define UART_ALL_IRQ_DISABLE(uart) UART_REG_CLR(uart,status,RXIENB|TXIENB)
#define UART_RX_IRQ_DISABLE(uart)  UART_REG_CLR(uart,status,RXIENB)
#define UART_TX_IRQ_DISABLE(uart)  UART_REG_CLR(uart,status,TXIENB)

#define UART_ALL_IRQ_ENABLE(uart)  UART_REG_OR(uart,status,RXIENB|TXIENB)
#define UART_RX_IRQ_ENABLE(uart)   UART_REG_OR(uart,status,RXIENB)
#define UART_TX_IRQ_ENABLE(uart)   UART_REG_OR(uart,status,TXIENB)

/* Board specific helpers */

#define ARC_UART_BASE(x) (RUBY_UART0_BASE_ADDR + x * 0x5000000)

static unsigned int arc_uart_irq[CONFIG_ARC_SERIAL_NR_PORTS] = {
#if CONFIG_ARC_SERIAL_NR_PORTS >= 1
    VUART_IRQ
#endif
#if CONFIG_ARC_SERIAL_NR_PORTS >= 2
    ,VUART1_IRQ
#endif
#if CONFIG_ARC_SERIAL_NR_PORTS >= 3
    ,VUART2_IRQ
#endif
};


#define ARC_SERIAL_NAME	"ttyS"
#define ARC_SERIAL_MAJOR	4 //204
#define ARC_SERIAL_MINOR	64

struct arc_serial_port {
	struct uart_port	port;
};

static struct arc_serial_port arc_serial_ports[CONFIG_ARC_SERIAL_NR_PORTS];


#ifdef CONFIG_ARC_SERIAL_CONSOLE
static struct console arc_serial_console;
#define ARC_SERIAL_CONSOLE	&arc_serial_console
#else
#define ARC_SERIAL_CONSOLE	NULL
#endif

/* Although compile time option, could be over-ridden by bootup tag parsing */
unsigned long serial_baudrate = CONFIG_ARC_SERIAL_BAUD;

static int arc_serial_probe(struct platform_device *dev);
static int arc_serial_remove(struct platform_device *dev);

static struct uart_driver arc_uart_driver = {
	.owner			= THIS_MODULE,
	.driver_name	= "arc-uart",
	.dev_name		= ARC_SERIAL_NAME,
	.major			= ARC_SERIAL_MAJOR,
	.minor			= ARC_SERIAL_MINOR,
	.nr			    = CONFIG_ARC_SERIAL_NR_PORTS,
	.cons			= ARC_SERIAL_CONSOLE,
};

static struct platform_driver arc_platform_driver = {
	.probe		= arc_serial_probe,
	.remove		= arc_serial_remove,
//	.suspend	= arc_serial_suspend,
//	.resume		= arc_serial_resume,
	.driver		= {
		.name	= "arc-uart",
		.owner	= THIS_MODULE,
	},
};

static int arc_serial_probe(struct platform_device *dev)
{
    int i;

	for (i = 0; i < CONFIG_ARC_SERIAL_NR_PORTS; i++) {
        platform_set_drvdata(dev, &arc_serial_ports[i].port);
        //arc_serial_ports[i].port.dev = &dev->dev;
        uart_add_one_port(&arc_uart_driver, &arc_serial_ports[i].port);
    }

    return 0;
}

static int arc_serial_remove(struct platform_device *dev)
{
    int i;

	for (i = 0; i < CONFIG_ARC_SERIAL_NR_PORTS; i++) {
	    uart_remove_one_port(&arc_uart_driver, &arc_serial_ports[i].port);
	    arc_serial_ports[i].port.dev = NULL;
    }

    return 0;
}


static void arc_serial_stop_rx(struct uart_port *port)
{
	struct arc_serial_port *uart = (struct arc_serial_port *)port;

    UART_RX_IRQ_DISABLE(uart);
}

static void arc_serial_stop_tx(struct uart_port *port)
{
	struct arc_serial_port *uart = (struct arc_serial_port *)port;

    while (!(UART_GET_STATUS(uart) & TXEMPTY))
        cpu_relax();

    UART_TX_IRQ_DISABLE(uart);
}

/*
 * Return TIOCSER_TEMT when transmitter is not busy.
 */
static unsigned int arc_serial_tx_empty(struct uart_port *port)
{
	struct arc_serial_port *uart = (struct arc_serial_port *)port;
    unsigned int stat;

    stat = UART_GET_STATUS(uart);
    if (stat & TXEMPTY )
        return TIOCSER_TEMT;
    else
        return 0;

}

// TODO:remove this
int arc_cnt;

/* Driver internal routine, used by both tty(serial core) as well as tx-isr
 *  -Called under spinlock in either cases
 *  -also tty->stopped / tty->hw_stopped has already been checked
 *     = by uart_start( ) before calling us
 *     = tx_ist checks that too before calling
 */
static void arc_serial_tx_chars(struct arc_serial_port *uart)
{
	struct circ_buf *xmit = &uart->port.state->xmit;
    int sent=0;
    unsigned char ch;

	if (unlikely(uart->port.x_char)) {
		UART_SET_DATA(uart, uart->port.x_char);
		uart->port.icount.tx++;
		uart->port.x_char = 0;
        sent = 1;
	}
    else if ( xmit->tail != xmit->head) {  // TODO: uart_circ_empty
        ch = xmit->buf[xmit->tail];
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		uart->port.icount.tx++;
        while( !(UART_GET_STATUS(uart) & TXEMPTY))
            arc_cnt++;
		UART_SET_DATA(uart, ch);
        sent = 1;
	}

    /* If num chars in xmit buffer are too few, ask for more from tty layer */
    // By Hard ISR to schedule processing in software interrupt part */
	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(&uart->port);

    //if (uart_circ_chars_pending(xmit))
    if (sent)
        UART_TX_IRQ_ENABLE(uart);

}


/*
 * port is locked and interrupts are disabled
 * uart_start( ) calls us under the port spinlock irqsave
 */
static void arc_serial_start_tx(struct uart_port *port)
{
	struct arc_serial_port *uart = (struct arc_serial_port *)port;

	arc_serial_tx_chars(uart);
}

static void arc_serial_rx_chars(struct arc_serial_port *uart)
{
	struct tty_struct *tty = NULL;
	unsigned int status, ch, flg=0;

	tty = uart->port.state->port.tty;

    /* UART controller has 4 deep RX-FIFO. Driver's recongnition of this fact
     * is very subtle. Here's how ...
    *  Upon getting a RX-Intr, such that RX-EMPTY=0, meaning data available,
    *  driver reads the DATA Reg and keeps doing that in a loop,
    *  until RX-EMPTY=1. Multiple chars being avail, with a single Interrupt,
    *  before RX-EMPTY=0, implies some sort of buffering going on in the
    *  controller, which is indeed the Rx-FIFO.
    */
  while( !( (status = UART_GET_STATUS(uart)) & RXEMPTY)) {

 	ch = UART_GET_DATA(uart);
 	uart->port.icount.rx++;

    if (unlikely(status & (RXOERR|RXFERR))) {
	    if (status & RXOERR) {
		    uart->port.icount.overrun++;
            flg = TTY_OVERRUN;
            UART_CLR_STATUS(uart, RXOERR);
        }

	    if (status & RXFERR) {
		    uart->port.icount.frame++;
		    flg = TTY_FRAME;
            UART_CLR_STATUS(uart, RXFERR);
        }
    }
	else
		flg = TTY_NORMAL;

	if (unlikely(uart_handle_sysrq_char(&uart->port, ch)))
        goto done;

    uart_insert_char(&uart->port, status, RXOERR, ch, flg);

   done:
	    tty_flip_buffer_push(tty);
  }
}

/* A note on the Interrupt handling state machine of this driver
 *
 * kernel printk writes funnel thru the console driver framework and in order
 * to keep things simple as well as efficient, it writes to UART in polled
 * mode, in one shot, and exits.
 *
 * OTOH, Userland output (via tty layer), uses interrupt based writes as there
 * can be undeterministic delay between char writes.
 *
 * Thus Rx-interrupts are always enabled, while tx-interrupts are by default
 * disabled.
 *
 * When tty has some data to send out, serial core calls driver's start_tx
 * which
 *   -checks-if-tty-buffer-has-char-to-send
 *   -writes-data-to-uart
 *   -enable-tx-intr
 *
 * Once data bits are pushed out, controller raises the Tx-room-avail-Interrupt.
 * The first thing Tx ISR does is disable further Tx interrupts (as this could
 * be the last char to send, before settling down into the quiet polled mode).
 * It then calls the exact routine used by tty layer write to send out any
 * more char in tty buffer. In case of sending, it re-enables Tx-intr. In case
 * of no data, it remains disabled.
 * This is how the transmit state machine is dynamically switched on/off
 *
 */

static irqreturn_t arc_serial_isr(int irq, void *dev_id)
{
	struct arc_serial_port *uart = dev_id;
    unsigned int status;

    status = UART_GET_STATUS(uart);

    /* Single IRQ for both Rx (data available) Tx (room available) Interrupt
     * notifications from the UART Controller.
     * To demultiplex between the two, we check the relevant bits
     */
    if ((status & RXIENB) && !(status & RXEMPTY)) {

	    spin_lock(&uart->port.lock);  // already in ISR, no need of xx_irqsave
        arc_serial_rx_chars(uart);
	    spin_unlock(&uart->port.lock);
    }

    if ((status & TXIENB) && (status & TXEMPTY)) {

        /* Unconditionally disable further Tx-Interrupts.
         * will be enabled by tx_chars() if needed.
         */
        UART_TX_IRQ_DISABLE(uart);

	    spin_lock(&uart->port.lock);

	    if (!uart_tx_stopped(&uart->port)) {
		    arc_serial_tx_chars(uart);
        }

	    spin_unlock(&uart->port.lock);
    }

	return IRQ_HANDLED;
}


static unsigned int arc_serial_get_mctrl(struct uart_port *port)
{
    /* Pretend we have a Modem status reg and following bits are
     *  always set, to satify the serial core state machine
     *  (DSR) Data Set Ready
     *  (CTS) Clear To Send
     *  (CAR) Carrier Detect
     */
	return TIOCM_CTS | TIOCM_DSR | TIOCM_CAR;
}

static void arc_serial_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
    // MCR not present
}

/* Enable Modem Status Interrupts */

static void arc_serial_enable_ms(struct uart_port *port)
{
    // MSR not present
}


static void arc_serial_break_ctl(struct uart_port *port, int break_state)
{
    // ARC UART doesn't support sendind Break signal
}

static int arc_serial_startup(struct uart_port *port)
{
	struct arc_serial_port *uart = (struct arc_serial_port *)port;
    unsigned int tmp;

    // Edge Triggered Interrupts to eliminate Spurious Interrupts
    tmp = read_new_aux_reg(AUX_ITRIGGER);
    tmp |= (1 << uart->port.irq);
    write_new_aux_reg(AUX_ITRIGGER,tmp);

    /* Before we hook up the ISR, Disable all UART Interrupts */
	UART_ALL_IRQ_DISABLE(uart);

    // TODO: generic request_irq takes a difft set of flags
    // We dont need IRQF_DISABLED since further IRQ auto-disabled

	if (request_irq(uart->port.irq, arc_serial_isr, IRQ_FLG_LOCK,
	     "ARC_UART_RX", uart)) {
		printk(KERN_NOTICE "Unable to attach ARC UART interrupt\n");
		return -EBUSY;
	}

	UART_RX_IRQ_ENABLE(uart);  // Only Rx IRQ enabled. WHY ?

	return 0;
}

/* This is not really needed */
static void arc_serial_shutdown(struct uart_port *port)
{
	struct arc_serial_port *uart = (struct arc_serial_port *)port;
    free_irq(uart->port.irq, uart);
}

static void arc_serial_set_ldisc(struct uart_port *port)
{
    // this might need implementing for the touch driver
}

static void
arc_serial_set_termios(struct uart_port *port, struct ktermios *termios,
		   struct ktermios *old)
{
	struct arc_serial_port *uart = (struct arc_serial_port *)port;
	unsigned int baud, uartl, uarth, hw_val;
    unsigned long flags;

    // TBD : We need to support more functions


    /* Use the generic handler so that any specially encoded baud rates
     *  such as SPD_xx flags or "%B0" can be handled
     */
    // Max Baud I suppose will not be more than current 115K * 4
	baud = uart_get_baud_rate(port, termios, old, 0, 460800);

    /* Formula for ARC UART is: hw-val = ((CLK/(BAUD*4)) -1)
     * spread over two 8-bit registers
     */
    hw_val = uart->port.uartclk / (baud*4) -1;
    uartl = hw_val & 0xFF;
    uarth = (hw_val >> 8) & 0xFF;

    /* ISS UART emulation has a subtle bug:
     * A existing value of Baudh = 0 is used as a indication to startup
     * it's internal state machine.
     * Thus if baudh is set to 0, 2 times, it chokes.
     * This happens with BAUD=115200 and the formaula above
     * Until that is fixed, when runnign on ISS, we will set baudh to !0
     */
    if (!running_on_hw) {
        uarth = 1;
    }

	spin_lock_irqsave(&uart->port.lock, flags);

	UART_ALL_IRQ_DISABLE(uart);

    UART_SET_BAUDL(uart, uartl);
    UART_SET_BAUDH(uart, uarth);

	UART_RX_IRQ_ENABLE(uart);

	/* Port speed changed, update the per-port timeout. */
	uart_update_timeout(port, termios->c_cflag, baud);

	spin_unlock_irqrestore(&uart->port.lock, flags);
}

static const char *arc_serial_type(struct uart_port *port)
{
	struct arc_serial_port *uart = (struct arc_serial_port *)port;

	return uart->port.type == PORT_ARC ? "ARC-UART" : NULL;
}

/*
 * Release the memory region(s) being used by 'port'.
 */
static void arc_serial_release_port(struct uart_port *port)
{
}

/*
 * Request the memory region(s) being used by 'port'.
 */
static int arc_serial_request_port(struct uart_port *port)
{
	return 0;
}


/*
 * Verify the new serial_struct (for TIOCSSERIAL).
 */
static int
arc_serial_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	return 0;
}

/*
 * Configure/autoconfigure the port.
 */
static void arc_serial_config_port(struct uart_port *port, int flags)
{
	struct arc_serial_port *uart = (struct arc_serial_port *)port;

	if (flags & UART_CONFIG_TYPE &&
	    arc_serial_request_port(&uart->port) == 0)
		uart->port.type = PORT_ARC;
}

#ifdef CONFIG_CONSOLE_POLL
static void arc_serial_poll_put_char(struct uart_port *port, unsigned char chr)
{
    while( !( (status = UART_GET_STATUS(uart)) & TXEMPTY)) {
		cpu_relax();
    }

 	UART_SET_DATA(uart, chr);
}

static int arc_serial_poll_get_char(struct uart_port *port)
{
	struct arc_serial_port *uart = (struct arc_serial_port *)port;
	unsigned char chr;

    while( !( (status = UART_GET_STATUS(uart)) & RXEMPTY)) {
		cpu_relax();

 	chr = UART_GET_DATA(uart);
	return chr;
}
#endif

static struct uart_ops arc_serial_pops = {
	.tx_empty	= arc_serial_tx_empty,
	.set_mctrl	= arc_serial_set_mctrl,
	.get_mctrl	= arc_serial_get_mctrl,
	.stop_tx	= arc_serial_stop_tx,
	.start_tx	= arc_serial_start_tx,
	.stop_rx	= arc_serial_stop_rx,
	.enable_ms	= arc_serial_enable_ms,
	.break_ctl	= arc_serial_break_ctl,
	.startup	= arc_serial_startup,
	.shutdown	= arc_serial_shutdown,
	.set_termios	= arc_serial_set_termios,
	.set_ldisc	= arc_serial_set_ldisc,
	.type		= arc_serial_type,
	.release_port	= arc_serial_release_port,
	.request_port	= arc_serial_request_port,
	.config_port	= arc_serial_config_port,
	.verify_port	= arc_serial_verify_port,
#ifdef CONFIG_CONSOLE_POLL
	.poll_put_char	= arc_serial_poll_put_char,
	.poll_get_char	= arc_serial_poll_get_char,
#endif
};

static void __init arc_serial_init_ports(void)
{
	static int first = 1;
	int i;

	if (!first)
		return;
	first = 0;

	for (i = 0; i < CONFIG_ARC_SERIAL_NR_PORTS; i++) {
		arc_serial_ports[i].port.uartclk   = CONFIG_ARC700_CLK; //get_sclk();
		arc_serial_ports[i].port.fifosize  = ARC_UART_TX_FIFO_SIZE;
		arc_serial_ports[i].port.ops       = &arc_serial_pops;
		arc_serial_ports[i].port.line      = i;
		arc_serial_ports[i].port.iotype    = UPIO_MEM;
		arc_serial_ports[i].port.membase   = (void *)ARC_UART_BASE(i);
		arc_serial_ports[i].port.mapbase   = ARC_UART_BASE(i);
		arc_serial_ports[i].port.irq       = arc_uart_irq[i];
		arc_serial_ports[i].port.flags     = UPF_BOOT_AUTOCONF;

        /* uart_insert_char( ) uses it in decideding whether to ignore a
         * char or not. Explicitly setting it here, removes the subtelty
         */
       	arc_serial_ports[i].port.ignore_status_mask = 0;
	}
}

static struct platform_device *dev[CONFIG_ARC_SERIAL_NR_PORTS];

static int __init arc_serial_init(void)
{
	int ret, i=0;

	pr_info("Serial: ARC serial driver: platform register\n");

	arc_serial_init_ports();

	ret = uart_register_driver(&arc_uart_driver);
    if (ret)
	    return ret;

    for ( i= 0; i < CONFIG_ARC_SERIAL_NR_PORTS; i++) {
        dev[i] = platform_device_register_simple("arc-uart",i,NULL,0);
        if (IS_ERR(dev[i])) {
            printk("ARC serial: plat device regisn failed\n");
            return PTR_ERR(dev[i]);
        }
    }

	ret = platform_driver_register(&arc_platform_driver);
	if (ret) {
		pr_debug("uart register failed\n");
		uart_unregister_driver(&arc_uart_driver);
	}

	return ret;
}

static void __exit arc_serial_exit(void)
{
	platform_driver_unregister(&arc_platform_driver);
	uart_unregister_driver(&arc_uart_driver);
}


module_init(arc_serial_init);
module_exit(arc_serial_exit);

#ifdef CONFIG_ARC_SERIAL_CONSOLE

static int __init
arc_serial_console_setup(struct console *co, char *options)
{
	struct arc_serial_port *uart;
	int baud = CONFIG_ARC_SERIAL_BAUD;
	int bits = 8;
	int parity = 'n';
	int flow = 'n';

	if (co->index == -1 || co->index >= CONFIG_ARC_SERIAL_NR_PORTS)
		co->index = 0;

	uart = &arc_serial_ports[co->index];

    /* Serial core will call port->ops->set_termios( )
     * which will set the baud reg
     */
	return uart_set_options(&uart->port, co, baud, parity, bits, flow);
}


static void arc_serial_console_putchar(struct uart_port *port, int ch)
{
	struct arc_serial_port *uart = (struct arc_serial_port *)port;

    while (!(UART_GET_STATUS(uart) & TXEMPTY));

	UART_SET_DATA(uart, ch);
}

/*
 * Interrupts are disabled on entering
 */
static void
arc_serial_console_write(struct console *co, const char *s, unsigned int count)
{
	struct arc_serial_port *uart = &arc_serial_ports[co->index];
	unsigned long flags;

	spin_lock_irqsave(&uart->port.lock, flags);
	uart_console_write(&uart->port, s, count, arc_serial_console_putchar);
	spin_unlock_irqrestore(&uart->port.lock, flags);
}


static struct console arc_serial_console = {
	.name		= ARC_SERIAL_NAME,
	.write		= arc_serial_console_write,
	.device		= uart_console_device,
	.setup		= arc_serial_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= 0,
	.data		= &arc_uart_driver
};

static int __init arc_serial_rs_console_init(void)
{
	arc_serial_init_ports();
	register_console(&arc_serial_console);

	return 0;
}
console_initcall(arc_serial_rs_console_init);

#ifdef CONFIG_EARLY_PRINTK

static __init void early_serial_putc(struct uart_port *port, int ch)
{
	unsigned timeout = 0xffff;
	struct arc_serial_port *uart = (struct arc_serial_port *)port;

    while( !(UART_GET_STATUS(uart) & TXEMPTY) &&
	        --timeout)
		cpu_relax();

 	UART_SET_DATA(uart, ch);
}

static __init void early_serial_write(struct console *con, const char *s,
					unsigned int n)
{
	struct arc_serial_port *uart = &arc_serial_ports[con->index];
	unsigned int i;

	for (i = 0; i < n; i++, s++) {
		if (*s == '\n')
			early_serial_putc(&uart->port, '\r');
		early_serial_putc(&uart->port, *s);
	}
}

static int __init arc_early_serial_init(struct console *co, char *options)
{
	struct arc_serial_port *uart;
	unsigned int baud, uartl, uarth, hw_val;

	arc_serial_init_ports();

	uart = &arc_serial_ports[co->index];

    baud = CONFIG_ARC_SERIAL_BAUD;
    hw_val = uart->port.uartclk / (baud*4) -1;
    uartl = hw_val & 0xFF;
    uarth = (hw_val >> 8) & 0xFF;

    if (!running_on_hw) {
        uarth = 1;
    }

    UART_SET_BAUDL(uart, uartl);
    UART_SET_BAUDH(uart, uarth);
    UART_RX_IRQ_ENABLE(uart);

    return 0;
}

static struct __initdata console arc_early_serial_console = {
	.name = "early_ARCuart",
	.write = early_serial_write,
	.device = uart_console_device,
	.flags = CON_PRINTBUFFER | CON_BOOT,
	.setup = arc_early_serial_init,
	.index = -1
};

void __init arc_early_serial_reg(void)
{
    register_console(&arc_early_serial_console);
}

#endif /* CONFIG_EARLY_PRINTK */

#endif
