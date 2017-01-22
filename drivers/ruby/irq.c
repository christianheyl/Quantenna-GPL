/*
 * (C) Copyright 2010 Quantenna Communications Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */


/******************************************************************************
 * Copyright ARC International (www.arc.com) 2007-2008
 *
 * Vineetg: Mar 2009
 *  -use generic irqaction to store IRQ requests
 *  -device ISRs no longer take pt_regs (rest of the kernel convention)
 *  -request_irq( ) definition matches declaration in inc/linux/interrupt.h
 *
 * Vineetg: Mar 2009 (Supporting 2 levels of Interrupts)
 *  -local_irq_enable shd be cognizant of current IRQ state
 *    It is lot more involved now and thus re-written in "C"
 *  -set_irq_regs in common ISR now always done and not dependent 
 *      on CONFIG_PROFILEas it is used by
 *
 * Vineetg: Jan 2009
 *  -Cosmetic change to display the registered ISR name for an IRQ
 *  -free_irq( ) cleaned up to not have special first-node/other node cases
 *
 * Vineetg: June 17th 2008
 *  -Added set_irq_regs() to top level ISR for profiling 
 *  -Don't Need __cli just before irq_exit(). Intr already disabled
 *  -Disabled compile time ARC_IRQ_DBG
 *
 *****************************************************************************/
/******************************************************************************
 * Copyright Codito Technologies (www.codito.com) Oct 01, 2004
 * 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *****************************************************************************/

/*
 * arch/arc/kernel/irq.c
 *
 */

#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/kernel_stat.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kallsyms.h>
#include <linux/io.h>
#include <linux/magic.h>
#include <linux/random.h>

#include <linux/sched.h>
#include <asm/system.h>
#include <asm/errno.h>
#include <asm/arcregs.h>
#include <asm/hardware.h>

#include <asm/board/board_config.h>
#include <asm/board/platform.h>
#include <asm/board/mem_check.h>

#include <linux/version.h>
#if LINUX_VERSION_CODE == KERNEL_VERSION(2,6,30)
#include <trace/irq.h>
#else
#include <trace/events/irq.h>
#endif

#include <qtn/ruby_cpumon.h>

//#define ARC_IRQ_DBG

//#define TEST_IRQ_REG

#ifdef ARC_IRQ_DBG
#define ASSERT(expr)	BUG_ON(!(expr))
#else
#define ASSERT(expr)
#endif

#define TIMER_INT_MSK(t)	TOPAZ_SYS_CTL_INTR_TIMER_MSK(t)

enum toggle_irq {
	DISABLE,
	ENABLE,
};

enum first_isr {
	MULTIPLE_ISRS,
	FIRST_ISR,
};

enum invert_bit {
	NON_INVERT,
	INVERT,
};

DEFINE_TRACE(irq_handler_entry);
DEFINE_TRACE(irq_handler_exit);

static void ruby_en_dis_ext_irq(unsigned irq, enum toggle_irq toggle);
static void ruby_en_dis_common_irq(unsigned irq, enum toggle_irq toggle);

/* table for system interrupt handlers, including handlers for extended interrupts. */
struct irq_desc irq_desc[NR_IRQS];

static inline struct irqaction *irq_get_action(unsigned int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);
	return desc->action;
}

/* IRQ status spinlock - enable, disable */
static spinlock_t irq_controller_lock;

extern void smp_ipi_init(void);

static void run_handlers(unsigned irq);

/* Return true if irq is extended. */
static __always_inline int ruby_is_ext_irq(unsigned irq)
{
	return (irq > RUBY_MAX_IRQ_VECTOR);
}

void __init arc_irq_init(void)
{
	extern int _int_vec_base_lds;

	/* set the base for the interrupt vector tabe as defined in Linker File
	   Interrupts will be enabled in start_kernel
	 */
	write_new_aux_reg(AUX_INTR_VEC_BASE, &_int_vec_base_lds);

	/* vineetg: Jan 28th 2008
	   Disable all IRQs on CPU side
	   We will selectively enable them as devices request for IRQ
	 */
	write_new_aux_reg(AUX_IENABLE, 0);

#ifdef CONFIG_ARCH_ARC_LV2_INTR
{
	int level_mask = 0;
	/* If any of the peripherals is Level2 Interrupt (high Prio),
	   set it up that way
	 */
#ifdef  CONFIG_TIMER_LV2
	level_mask |= (1 << TIMER0_INT );
#endif

#ifdef  CONFIG_SERIAL_LV2
	level_mask |= (1 << VUART_IRQ);
#endif

#ifdef  CONFIG_EMAC_LV2
	level_mask |= (1 << VMAC_IRQ );
#endif

	if (level_mask) {
		printk("setup as level-2 interrupt/s \n");
		write_new_aux_reg(AUX_IRQ_LEV, level_mask);
	}
}
#endif

}

static irqreturn_t uart_irq_demux_handler(int irq, void *dev_id)
{
	int i, handled = 0;
	u32 rcstatus = readl(IO_ADDRESS(RUBY_SYS_CTL_RESET_CAUSE));
	for (i = 0; i < 2; i++) {
		u32 rcset = rcstatus & RUBY_RESET_CAUSE_UART(i);
		if (rcset && irq_has_action(RUBY_IRQ_UART0 + i)) {
			run_handlers(RUBY_IRQ_UART0 + i);
			handled = 1;
		}
	}
	return handled ? IRQ_HANDLED : IRQ_NONE;
}

static irqreturn_t timer_irq_demux_handler(int irq, void *dev_id)
{
	int i, handled = 0;
	u32 rcstatus = readl(IO_ADDRESS(RUBY_SYS_CTL_RESET_CAUSE));
	for (i = 0; i < RUBY_NUM_TIMERS; i++) {
		if ((rcstatus & TIMER_INT_MSK(i)) && irq_has_action(RUBY_IRQ_TIMER0 + i)) {
			run_handlers(RUBY_IRQ_TIMER0 + i);
			handled = 1;
		}
	}
	return handled ? IRQ_HANDLED : IRQ_NONE;
}

static irqreturn_t misc_irq_demux_handler(int irq, void *dev_id)
{
	bool handled = false;
	int i;
	uint32_t rcstatus;

	rcstatus = readl(IO_ADDRESS(RUBY_SYS_CTL_RESET_CAUSE));

	for (i = 0; i < QTN_IRQ_MISC_EXT_IRQ_COUNT; i++) {
		const int demux_irq = RUBY_IRQ_MISC_EXT_IRQ_START + i;
		const unsigned int rcbit = QTN_IRQ_MISC_RST_CAUSE_START + i;

		if ((rcstatus & (1 << rcbit)) && irq_has_action(demux_irq)) {
			run_handlers(demux_irq);
			handled = true;
		}
	}

	return handled ? IRQ_HANDLED : IRQ_NONE;
}

/* remember desired triggering behavior of each gpio isr registered */
static unsigned long gpio_trig_flags[RUBY_GPIO_IRQ_MAX];

static __always_inline unsigned long read_gpio_inv_status(void)
{
	return readl(IO_ADDRESS(RUBY_SYS_CTL_INTR_INV0));
}

static __always_inline void write_gpio_inv_status_bit(int gpio, enum invert_bit invert)
{
	if (invert == INVERT) {
		writel_or(RUBY_BIT(gpio), IO_ADDRESS(RUBY_SYS_CTL_INTR_INV0));
	} else {
		writel_and(~RUBY_BIT(gpio), IO_ADDRESS(RUBY_SYS_CTL_INTR_INV0));
	}
}

static __always_inline unsigned long read_gpio_input_status(void)
{
	return readl(IO_ADDRESS(RUBY_GPIO_INPUT));
}

static void init_gpio_irq(int irq, unsigned long flags)
{
	int gpio = irq - RUBY_IRQ_GPIO0;
	u32 line_status;

	if (gpio >= RUBY_GPIO_IRQ_MAX || gpio < 0) {
		panic("error in gpio isr init! irq: %d, flags: 0x%lx\n", irq, flags);
	}

	gpio_trig_flags[gpio] = flags & IRQF_TRIGGER_MASK;
	line_status = read_gpio_input_status() & (1 << gpio);

	if (flags & IRQF_TRIGGER_HIGH) {
		/* set inversion line off for high level trigger */
		write_gpio_inv_status_bit(gpio, NON_INVERT);
	}
	else if (flags & IRQF_TRIGGER_LOW) {
		write_gpio_inv_status_bit(gpio, INVERT);
	}
	else if (flags & IRQF_TRIGGER_RISING) {
		/*
		 * for rising edge trigger, invert off,
		 * then enable invert after each rising edge interrupt.
		 * when the edge falls again, invert off to accept the next rising edge.
		 * set to the opposite of the input pin for starters, so registering
		 * the driver doesn't trigger an interrupt on the requested level
		 * without an edge.
		 */
		if (line_status) {
			write_gpio_inv_status_bit(gpio, INVERT);
		} else {
			write_gpio_inv_status_bit(gpio, NON_INVERT);
		}
	}
	else if (flags & IRQF_TRIGGER_FALLING) {
		if (line_status) {
			write_gpio_inv_status_bit(gpio, NON_INVERT);
		} else {
			write_gpio_inv_status_bit(gpio, INVERT);
		}
	}
	else {
		/*
		 * assume this has been manually set, leave as default or prior
		 */
	}
}


/**
 * gpio demux handler
 *
 * call handlers based on status of gpio input lines, and line invert level.
 * provides software support for edge triggered interrupts for gpio buttons etc.
 */
static irqreturn_t gpio_irq_demux_handler(int irq, void *dev_id)
{
	int i;
	int handle[RUBY_GPIO_IRQ_MAX];
	unsigned long flags, gpio_input_status, gpio_inv_status;

	spin_lock_irqsave(&irq_controller_lock, flags);

	/*
	 * determine which handlers to run and manipulate registers
	 * holding the lock, then run handlers afterwards
	 */
	gpio_input_status = read_gpio_input_status();
	gpio_inv_status = read_gpio_inv_status();
	for (i = 0; i < RUBY_GPIO_IRQ_MAX; i++) {
		int handle_line = 0;
		int ext_irq = RUBY_IRQ_GPIO0 + i;

		if (irq_has_action(ext_irq)) {

			unsigned long gpio_line_flags = gpio_trig_flags[i];
			unsigned long line_high = gpio_input_status & (0x1 << i);
			unsigned long inv_on = gpio_inv_status & (0x1 << i);

#if 0
			printk("%s irq %d gpio %d input %x inv %x flags %x lhigh %d inv %d\n",
					__FUNCTION__, irq, i, gpio_input_status, gpio_inv_status, gpio_line_flags,
					line_high != 0, inv_on != 0);
#endif

			if (gpio_line_flags & IRQF_TRIGGER_HIGH) {
				handle_line = line_high;
			} else if (gpio_line_flags & IRQF_TRIGGER_LOW) {
				handle_line = !line_high;
			} else if (gpio_line_flags & IRQF_TRIGGER_RISING) {
				/*
				 * rising edge trigger. if inverted, dont handle, uninvert.
				 * if uninverted, handle and invert.
				 *
				 * if neither of these cases are true, then either:
				 * 1) this line didn't cause this interrupt, or
				 * 2) the input_status register has not updated yet.
				 *    this was observed during test, spurious interrupts
				 *    would occur where input_status is not set appropriately,
				 *    even though an interrupt was caused by this line.
				 *    This just means another interrupt will fire until
				 *    the gpio input status reaches its appropriate state
				 */
				if (!line_high && inv_on) {
					write_gpio_inv_status_bit(i, NON_INVERT);
				} else if (line_high && !inv_on) {
					handle_line = 1;
					write_gpio_inv_status_bit(i, INVERT);
				}
			} else if (gpio_line_flags & IRQF_TRIGGER_FALLING) {
				if (!line_high && inv_on) {
					write_gpio_inv_status_bit(i, NON_INVERT);
					handle_line = 1;
				} else if (line_high && !inv_on) {
					write_gpio_inv_status_bit(i, INVERT);
				}
			} else {
				handle_line = (line_high && !inv_on) || (!line_high && inv_on);
			}
		}

		handle[i] = handle_line;
	}

	spin_unlock_irqrestore(&irq_controller_lock, flags);

	for (i = 0; i < RUBY_GPIO_IRQ_MAX; i++) {
		if (handle[i]) {
			run_handlers(RUBY_IRQ_GPIO0 + i);
		}
	}

	return IRQ_HANDLED;
}


static struct irqaction gpio_irq = {
	.name       = "GPIO demux",
	.flags      = 0,
	.handler    = &gpio_irq_demux_handler,
	.dev_id     = NULL,
};

static struct irqaction uart_irq = {
	.name       = "UART demux",
	.flags      = 0,
	.handler    = &uart_irq_demux_handler,
	.dev_id     = NULL,
};

static struct irqaction timer_irq = {
	.name       = "Timers demux",
	.flags      = 0,
	.handler    = &timer_irq_demux_handler,
	.dev_id     = NULL,
};

static struct irqaction misc_irq = {
	.name       = "Misc demux",
	.flags      = 0,
	.handler    = &misc_irq_demux_handler,
	.dev_id     = NULL,
};

#ifdef TEST_IRQ_REG
static irqreturn_t test_irq_handler(int irq, void *dev_id)
{
	if (printk_ratelimit()) {
		printk(KERN_WARNING "%s, %s irq %d\n", __FUNCTION__, (const char*) dev_id, irq);
	} else {
		unsigned long flags;
		spin_lock_irqsave(&irq_controller_lock, flags);
		ruby_en_dis_ext_irq(irq, DISABLE);
		spin_unlock_irqrestore(&irq_controller_lock, flags);
	}
	return IRQ_HANDLED;
}

/*
 * attempt to register and deregister handlers on every irq
 */
static void test_irq_reg(void)
{
	int j, k;
	for (j = 0; j < RUBY_IRQ_EXT_VECTORS_NUM; j++) {
		char bufj[32];
		snprintf(bufj, 32, "test.j.%d", j);
		int req2 = request_irq(j, test_irq_handler, IRQF_SHARED, "testj", bufj);
		for (k = 0; k < 2; k++) {
			char bufk[32];
			snprintf(bufk, 32, "test.k.%d", k);
			int req3 = request_irq(j, test_irq_handler, IRQF_SHARED, "testk", bufk);
			if (!req3) {
				disable_irq(j);
				disable_irq(j);
				enable_irq(j);
				enable_irq(j);
				free_irq(j, bufk);
			}
		}
		if (!req2) {
			free_irq(j, bufj);
		} else {
			printk(KERN_WARNING "%s could not register %s\n",
					__FUNCTION__, bufj);
		}
	}
}
#endif

#ifndef TOPAZ_AMBER_IP
/*
 *
 * Some checking to prevent registration of isrs on gpio lines that are in use
 *
 */
static int check_uart1_use(void)
{
	int use_uart1 = 0;

	if (get_board_config(BOARD_CFG_UART1, &use_uart1) != 0) {
		printk(KERN_ERR "get_board_config returned error status for UART1\n");
	}

	return use_uart1;
}
#endif

static struct disallowed_isr {
	int	irq;
	int	gpio;
	char*	use;
	int	(*checkfunc)(void);	/* returns 0 if the line is available */
}
disallowed_isrs[] =
{
#ifndef TOPAZ_AMBER_IP
	{ -1,	RUBY_GPIO_UART0_SI,	"uart0 rx",	NULL			},
	{ -1,	RUBY_GPIO_UART0_SO,	"uart0 tx",	NULL			},
	{ -1,	RUBY_GPIO_UART1_SI,	"uart1 rx",	&check_uart1_use	},
	{ -1,	RUBY_GPIO_UART1_SO,	"uart1 tx",	&check_uart1_use	},
	{ -1,	RUBY_GPIO_I2C_SCL,	"i2c scl",	NULL			},
	{ -1,	RUBY_GPIO_I2C_SDA,	"i2c sda",	NULL			},
#endif /*TOPAZ_AMBER_IP*/
	{ RUBY_IRQ_TIMER0,	-1,	"ruby timer0",	NULL			},
};

/* returns 0 if this gpio pin may have an isr installed on it */
static int ext_isr_check(int irq)
{
	int i;
	const struct disallowed_isr *inv;
	int gpio = (irq - RUBY_IRQ_GPIO0);

	if (!ruby_is_ext_irq(irq)) {
		panic("%s: invalid irq: %d\n", __FUNCTION__, irq);
		return -EBUSY;
	}

	for (i = 0; i < ARRAY_SIZE(disallowed_isrs); i++) {
		inv = &disallowed_isrs[i];
		/* check if pin or irq num matches */
		if (inv->irq == irq ||
				(inv->irq < 0 &&
				 inv->gpio == gpio &&
				 gpio < RUBY_GPIO_IRQ_MAX)) {
			/* no checkfunc means always busy */
			int used = 1;
			if (inv->checkfunc) {
				used = inv->checkfunc();
			}

			if (used) {
				printk(KERN_ERR "%s: irq %d in use by %s\n",
						__FUNCTION__, irq, inv->use);
				return -EBUSY;
			}
		}
	}

	return 0;
}

static __always_inline u32 count_bits_set(u32 value)
{
	u32 count = 0;
	for (; value; value >>= 1) {
		count += value & 0x1;
	}
	return count;
}

/* initialise the irq table */
void __init init_IRQ(void)
{
	memset(&irq_desc[0], 0, sizeof(irq_desc));

	setup_irq(RUBY_IRQ_UART, &uart_irq);
	setup_irq(RUBY_IRQ_GPIO, &gpio_irq);
	setup_irq(RUBY_IRQ_TIMER, &timer_irq);
	setup_irq(RUBY_IRQ_MISC, &misc_irq);

#ifdef CONFIG_SMP
	smp_ipi_init();
#endif
}


/* Map extended IRQ to common IRQ line */
static unsigned int ruby_map_ext_irq(unsigned irq)
{
	/*
	 ************************************************
	 * remap shared irq's at this level - there are
	 * only 32 physical vectors so we cannot just
	 * map 64 irq's directly.
	 * The irq handler is responsible for demuxing
	 * further - there are only a couple of
	 * cases as shown below.  Otherwise we can
	 * implement a second level irq scheme by
	 * registering irq's in the init with demux
	 * handlers.
	 *************************************************
	 */

	if (ruby_is_ext_irq(irq)) {
		/* Adjust extended irq to common irq line. */
		if (irq <= RUBY_IRQ_GPIO15) {
			irq = RUBY_IRQ_GPIO;
		} else if (irq <= RUBY_IRQ_UART1) {
			irq = RUBY_IRQ_UART;
		} else if (irq <= RUBY_IRQ_TIMER3) {
			irq = RUBY_IRQ_TIMER;
		} else {
			irq = RUBY_IRQ_MISC;
		}
	}

	return irq;
}

/* Enable/disable Ruby extended interrupt requests */
static void ruby_en_dis_ext_irq(unsigned irq, enum toggle_irq toggle)
{
	if (!ruby_is_ext_irq(irq)) {
		printk(KERN_ERR"IRQ %u must be never used with this function.\n", irq);
		panic("IRQ handling failure\n");

	} else {

		unsigned long status = readl(IO_ADDRESS(RUBY_SYS_CTL_LHOST_ORINT_EN));
		if (toggle == ENABLE) {
			status |= (1 << (irq - RUBY_IRQ_VECTORS_NUM));
		} else {
			status &= ~(1 << (irq - RUBY_IRQ_VECTORS_NUM));
		}
		writel(status, IO_ADDRESS(RUBY_SYS_CTL_LHOST_ORINT_EN));
	}
}

/* Enable/disable Ruby common interrupt requests */
static void ruby_en_dis_common_irq(unsigned irq, enum toggle_irq toggle)
{
	if (ruby_is_ext_irq(irq)) {
		printk(KERN_ERR"IRQ %u must be never used with this function.\n", irq);
		panic("IRQ handling failure\n");

	} else if (irq >= RUBY_IRQ_WATCHDOG) {

		/* If higher than timer, this is external irq to cpu and needs additional reg set up. */
		unsigned long status = readl(IO_ADDRESS(RUBY_SYS_CTL_LHOST_INT_EN));
		if (toggle == ENABLE) {
			status |= (1 << (irq - RUBY_IRQ_WATCHDOG));
		} else {
			status &= ~(1 << (irq - RUBY_IRQ_WATCHDOG));
		}
		writel(status, IO_ADDRESS(RUBY_SYS_CTL_LHOST_INT_EN));
	}
}

/* Check that setup request is correct. */
static int ruby_setup_check_1(unsigned irq, unsigned mapped_irq, struct irqaction *node)
{
	int ret = 0;

	if (ruby_is_ext_irq(irq)) {
		ret = ext_isr_check(irq);
	}

	if (node->flags & IRQF_TRIGGER_MASK) {
		if (mapped_irq == RUBY_IRQ_GPIO) {
			if (count_bits_set(node->flags & IRQF_TRIGGER_MASK) > 1) {
				printk(KERN_ERR"IRQ %d (0x%x) does not support multiple IRQF_TRIGGER_* flags\n",
						irq, (unsigned)node->flags);

				ret = -EINVAL;
			}
		} else {
			printk(KERN_ERR"IRQ %d (0x%x) does not support IRQF_TRIGGER_* flags (fixme)\n",
					irq, (unsigned)node->flags);
			ret = -EINVAL;
		}
	}

	return ret;
}

/* Check that setup request is correct. */
static int ruby_setup_check_2(unsigned irq, unsigned mapped_irq, struct irqaction *node)
{
	int ret = 0;

	if (!(irq_get_action(irq)->flags & IRQF_SHARED) || !(node->flags & IRQF_SHARED)) {

		/* Enforce condition that all (if more than one) ISRs belong to the same
		 * vector must be shared.
		 */
		printk(KERN_ERR"%s: %s incompatible with IRQ %d from %s\n",
			__FUNCTION__, node->name, irq, irq_get_action(irq)->name);
		ret = -EBUSY;

	}

	return ret;
}

/* Enable IRQ during setup */
static void ruby_setup_enable_irq(unsigned irq)
{
	if (ruby_is_ext_irq(irq)) {
		/* enable extended interrupt. */
		ruby_en_dis_ext_irq(irq, ENABLE);
	} else {
		/* Enable this IRQ in CPU AUX_IENABLE Reg */
		unmask_interrupt((1 << irq));
		/* Enable IRQ on Ruby interrupt controller */
		ruby_en_dis_common_irq(irq, ENABLE);
	}
}

/* Link node during setup */
static void ruby_setup_link_irq(enum first_isr is_first, unsigned irq, struct irqaction *node)
{
	if (is_first == FIRST_ISR) {

		/* Add ISR to the head */
		irq_to_desc(irq)->action = node;

	} else {

		/* Add the ISR to link-list of ISRs per IRQ */
		struct irqaction *curr = irq_get_action(irq);
		while (curr->next) {
			curr = curr->next;
		}
		curr->next = node;
	}
}

/* setup_irq:
 * Typically used by architecure special interrupts for
 * registering handler to IRQ line
 */
int setup_irq(unsigned irq, struct irqaction *node)
{
	int ret = 0;
	unsigned mapped_irq = ruby_map_ext_irq(irq);
	unsigned long flags;
	enum first_isr is_first;

#ifdef  ARC_IRQ_DBG
	printk(KERN_INFO"---IRQ Request (%d) ISR\n", irq);
#endif

	if (node->flags & IRQF_SAMPLE_RANDOM)
		rand_initialize_irq(irq);

	spin_lock_irqsave(&irq_controller_lock, flags);

	ret = ruby_setup_check_1(irq, mapped_irq, node);
	if (ret) {
		goto error;
	}

	if (!irq_has_action(irq)) {
		is_first = FIRST_ISR;
	} else {
		is_first = MULTIPLE_ISRS;

		/* additional checks for multiple isrs */
		ret = ruby_setup_check_2(irq, mapped_irq, node);
		if (ret) {
			goto error;
		}
	}

	/* additional pin settings for gpio irqs */
	if (ruby_is_ext_irq(irq) && mapped_irq == RUBY_IRQ_GPIO) {
		init_gpio_irq(irq, node->flags);
	}

	ruby_setup_link_irq(is_first, irq, node);
	ruby_setup_enable_irq(irq);

error:
	spin_unlock_irqrestore(&irq_controller_lock, flags);

	return ret;
}

/* request_irq:
 * Exported to device drivers / modules to assign handler to IRQ line
 */
int request_irq(unsigned irq,
		irqreturn_t (*handler)(int, void *),
		unsigned long flags, const char *name, void *dev_id)
{
	struct irqaction *node;
	int retval;

	if (irq >= NR_IRQS) {
		printk("%s: Unknown IRQ %d\n", __FUNCTION__, irq);
		return -ENXIO;
	}

	node = (struct irqaction *)kmalloc(sizeof(struct irqaction), GFP_KERNEL);
	if (!node)
		return -ENOMEM;

	node->handler = handler;
	node->flags = flags;
	node->dev_id = dev_id;
	node->name = name;
	node->next = NULL;

	/* insert the new irq registered into the irq list */

	retval = setup_irq(irq, node);
	if (retval)
		kfree(node);
	return retval;
}
EXPORT_SYMBOL(request_irq);

/* free an irq node for the irq list */
void free_irq(unsigned irq, void *dev_id)
{
	unsigned long flags;
	struct irqaction *tmp = NULL, **node;

	if (irq >= NR_IRQS) {
		printk(KERN_ERR "%s: Unknown IRQ %d\n", __FUNCTION__, irq);
		return;
	}

	spin_lock_irqsave(&irq_controller_lock, flags); /* delete atomically */

	/* Traverse through linked-list of ISRs. */
	node = &irq_to_desc(irq)->action;
	while (*node) {
		if ((*node)->dev_id == dev_id) {
			tmp = *node;
			(*node) = (*node)->next;
			kfree(tmp);
		} else {
			node = &((*node)->next);
		}
	}

	/* Disable IRQ if found in linked-list. */
	if (tmp) {
		/* Disable if last ISR deleted. */
		if (!irq_has_action(irq)) {
			/* If it is extended irq - disable it. */
			if (ruby_is_ext_irq(irq)) {
				ruby_en_dis_ext_irq(irq, DISABLE);
			} else {
				/* Disable this IRQ in CPU AUX_IENABLE Reg */
				mask_interrupt((1 << irq));
				/* Disable IRQ on Ruby interrupt controller */
				ruby_en_dis_common_irq(irq, DISABLE);
			}
		}
	}

	spin_unlock_irqrestore(&irq_controller_lock, flags);

	if (!tmp) {
		printk(KERN_ERR"%s: tried to remove invalid interrupt", __FUNCTION__);
	}
}
EXPORT_SYMBOL(free_irq);

#if defined(CONFIG_PCI_MSI)
static inline void clear_msi_request(void)
{
	writel(RUBY_PCIE_MSI_CLEAR, RUBY_PCIE_MSI_STATUS);
}
#endif

struct ruby_cpumon_data {
	uint64_t sleep_cycles;
	uint64_t awake_cycles;
	uint32_t last_cyc;
	int last_was_asleep;
};

static __sram_data struct ruby_cpumon_data ruby_cpumon;

static __always_inline uint32_t ruby_cpumon_get_clock(void)
{
	return read_new_aux_reg(ARC_REG_TIMER1_CNT);
}

void ruby_cpumon_get_cycles(uint64_t *sleep, uint64_t *awake)
{
	uint32_t cyc;
	unsigned long flags;
	struct ruby_cpumon_data *cd = &ruby_cpumon;

	local_irq_save(flags);

	WARN_ON_ONCE(cd->last_was_asleep);

	cyc = ruby_cpumon_get_clock();
	cd->awake_cycles += cyc - cd->last_cyc;
	cd->last_cyc = cyc;

	if (sleep) {
		*sleep = cd->sleep_cycles;
	}
	if (awake) {
		*awake = cd->awake_cycles;
	}

	local_irq_restore(flags);
}
EXPORT_SYMBOL(ruby_cpumon_get_cycles);

static int ruby_cpumon_proc_read(char *page, char **start, off_t offset, int count, int *eof, void *data)
{
	char *p = page;
	uint64_t awake;
	uint64_t sleep;

	if (offset > 0) {
		*eof = 1;
		return 0;
	}

	ruby_cpumon_get_cycles(&sleep, &awake);

	p += sprintf(p, "up_msecs %u sleep %Lu awake %Lu\n", jiffies_to_msecs(jiffies), sleep, awake);

	return p - page;
}

static int __init ruby_cpumon_register_proc(void)
{
	struct ruby_cpumon_data *cd = &ruby_cpumon;

	create_proc_read_entry("ruby_cpumon", 0, NULL, ruby_cpumon_proc_read, NULL);
	memset(cd, 0, sizeof(*cd));
	cd->last_cyc = ruby_cpumon_get_clock();

	return 0;
}
late_initcall(ruby_cpumon_register_proc);

void arch_idle(void)
{
	uint32_t sleep_start;
	unsigned long flags;
	struct ruby_cpumon_data *cd = &ruby_cpumon;

	local_irq_save(flags);
	sleep_start = ruby_cpumon_get_clock();
	cd->awake_cycles += sleep_start - cd->last_cyc;
	cd->last_cyc = sleep_start;
	cd->last_was_asleep = 1;
	local_irq_restore_and_sleep(flags);
}

static inline void ruby_cpumon_interrupt(void)
{
	struct ruby_cpumon_data *cd = &ruby_cpumon;

	if (unlikely(cd->last_was_asleep)) {
		uint32_t awake_start = ruby_cpumon_get_clock();
		cd->sleep_cycles += awake_start - cd->last_cyc;
		cd->last_cyc = awake_start;
		cd->last_was_asleep = 0;
	}
}

static void __sram_text run_handlers(unsigned irq)
{
	struct irqaction *node;

	ASSERT((irq < NR_IRQS) && irq_has_action(irq));
#if defined(CONFIG_PCI_MSI)
	if (irq == RUBY_IRQ_MSI) {
		clear_msi_request();
	}
#endif
	/* call all the ISR's in the list for that interrupt source */
	node = irq_get_action(irq);
	while (node) {
		kstat_cpu(smp_processor_id()).irqs[irq]++;
		trace_irq_handler_entry(irq, 0);
		node->handler(irq, node->dev_id);
		trace_irq_handler_exit(irq, 0, 0);
		if (node->flags & IRQF_SAMPLE_RANDOM)
			add_interrupt_randomness(irq);
		node = node->next;
	}

#ifdef  ARC_IRQ_DBG
	if (!irq_has_action(irq))
		printk(KERN_ERR "Spurious interrupt : irq no %u on cpu %u", irq,
		smp_processor_id());
#endif
}

/* handle the irq */
void __sram_text process_interrupt(unsigned irq, struct pt_regs *fp)
{
	struct pt_regs *old = set_irq_regs(fp);

	irq_enter();

	ruby_cpumon_interrupt();
	run_handlers(irq);

	irq_exit();

	check_stack_consistency(__FUNCTION__);

	set_irq_regs(old);
	return;
}

/* IRQ Autodetect not required for ARC
 * However the stubs still need to be exported for IDE et all
 */
unsigned long probe_irq_on(void)
{
	return 0;
}
EXPORT_SYMBOL(probe_irq_on);

int probe_irq_off(unsigned long irqs)
{
	return 0;
}
EXPORT_SYMBOL(probe_irq_off);

/* FIXME: implement if necessary */
void init_irq_proc(void)
{
	// for implementing /proc/irq/xxx
}

int show_interrupts(struct seq_file *p, void *v)
{
	int i = *(loff_t *) v, j;

	if (i == 0) {	  // First line, first CPU
		seq_printf(p,"\t");
		for_each_online_cpu(j) {
			seq_printf(p,"CPU%-8d",j);
		}
		seq_putc(p,'\n');

#ifdef TEST_IRQ_REG
		test_irq_reg();
#endif
	}

	if (i < NR_IRQS) {
		int irq = i;
		const struct irqaction *node = irq_get_action(irq);
		while (node) {
			seq_printf(p,"%u:\t",i);
			if (strlen(node->name) < 8) {
				for_each_online_cpu(j) {
					seq_printf(p,"%s\t\t\t%u\n",
							node->name, kstat_cpu(j).irqs[i]);
				}
			} else {
				for_each_online_cpu(j) {
					seq_printf(p,"%s\t\t%u\n",
							node->name, kstat_cpu(j).irqs[i]);
				}
			}
			node = node->next;
		}
	}

	return 0;
}

/**
 *      disable_irq - disable an irq and wait for completion
 *      @irq: Interrupt to disable
 *
 *      Disable the selected interrupt line.  We do this lazily.
 *
 *      This function may be called from IRQ context.
 */
void disable_irq(unsigned irq)
{
	unsigned long flags;

	spin_lock_irqsave(&irq_controller_lock, flags);

	if ((irq < NR_IRQS) && irq_has_action(irq)) {
		if (!irq_to_desc(irq)->depth++) {
			if (ruby_is_ext_irq(irq)) {
				ruby_en_dis_ext_irq(irq, DISABLE);
			} else {
				ruby_en_dis_common_irq(irq, DISABLE);
			}
		}
	}

	spin_unlock_irqrestore(&irq_controller_lock, flags);
}
EXPORT_SYMBOL(disable_irq);

/**
 *      enable_irq - enable interrupt handling on an irq
 *      @irq: Interrupt to enable
 *
 *      Re-enables the processing of interrupts on this IRQ line.
 *      Note that this may call the interrupt handler, so you may
 *      get unexpected results if you hold IRQs disabled.
 *
 *      This function may be called from IRQ context.
 */
void enable_irq(unsigned irq)
{
	unsigned long flags;

	spin_lock_irqsave(&irq_controller_lock, flags);

	if ((irq < NR_IRQS) && irq_has_action(irq)) {
		if (irq_to_desc(irq)->depth) {
			if (!--irq_to_desc(irq)->depth) {
				if (ruby_is_ext_irq(irq)) {
					ruby_en_dis_ext_irq(irq, ENABLE);
				} else {
					ruby_en_dis_common_irq(irq, ENABLE);
				}
			}
		} else {
			printk(KERN_ERR"Unbalanced IRQ action %d %s\n", irq, __FUNCTION__);
		}
	}

	spin_unlock_irqrestore(&irq_controller_lock, flags);
}
EXPORT_SYMBOL(enable_irq);

#ifdef CONFIG_SMP

int get_hw_config_num_irq()
{
	uint32_t val = read_new_aux_reg(ARC_REG_VECBASE_BCR);

	switch(val & 0x03)
	{
		case 0: return 16;
		case 1: return 32;
		case 2: return 8;
		default: return 0;
	}

	return 0;
}

#endif

/* Enable interrupts.
 * 1. Explicitly called to re-enable interrupts
 * 2. Implicitly called from spin_unlock_irq, write_unlock_irq etc
 *    which maybe in hard ISR itself
 *
 * Semantics of this function change depending on where it is called from:
 *
 * -If called from hard-ISR, it must not invert interrupt priorities
 *  e.g. suppose TIMER is high priority (Level 2) IRQ
 *    Time hard-ISR, timer_interrupt( ) calls spin_unlock_irq several times.
 *    Here local_irq_enable( ) shd not re-enable lower priority interrupts 
 * -If called from soft-ISR, it must re-enable all interrupts   
 *    soft ISR are low prioity jobs which can be very slow, thus all IRQs
 *    must be enabled while they run. 
 *    Now hardware context wise we may still be in L2 ISR (not done rtie)
 *    still we must re-enable both L1 and L2 IRQs
 *  Another twist is prev scenario with flow being
 *     L1 ISR ==> interrupted by L2 ISR  ==> L2 soft ISR
 *     here we must not re-enable Ll as prev Ll Interrupt's h/w context will get 
 *     over-written (this is deficiency in ARC700 Interrupt mechanism)
 */

#ifdef CONFIG_ARCH_ARC_LV2_INTR     // Complex version for 2 levels of Intr

void __sram_text local_irq_enable(void) {

	unsigned long flags;
	local_save_flags(flags);

	/* Allow both L1 and L2 at the onset */
	flags |= (STATUS_E1_MASK | STATUS_E2_MASK);

	/* Called from hard ISR (between irq_enter and irq_exit) */
	if (in_irq()) {

		/* If in L2 ISR, don't re-enable any further IRQs as this can cause
		 * IRQ priorities to get upside down.
		 * L1 can be taken while in L2 hard ISR which is wron in theory ans
		 * can also cause the dreaded L1-L2-L1 scenario
		 */
		if (flags & STATUS_A2_MASK) {
			flags &= ~(STATUS_E1_MASK | STATUS_E2_MASK);
		}

		/* Even if in L1 ISR, allowe Higher prio L2 IRQs */
		else if (flags & STATUS_A1_MASK) {
			flags &= ~(STATUS_E1_MASK);
		}
	}

	/* called from soft IRQ, ideally we want to re-enable all levels */

	else if (in_softirq()) {

		/* However if this is case of L1 interrupted by L2,
		 * re-enabling both may cause whaco L1-L2-L1 scenario
		 * because ARC700 allows level 1 to interrupt an active L2 ISR
		 * Thus we disable both
		 * However some code, executing in soft ISR wants some IRQs to be
		 * enabled so we re-enable L2 only
		 *
		 * How do we determine L1 intr by L2
		 *  -A2 is set (means in L2 ISR)
		 *  -E1 is set in this ISR's pt_regs->status32 which is
		 *      saved copy of status32_l2 when l2 ISR happened
		 */
		struct pt_regs *pt = get_irq_regs();
		if ((flags & STATUS_A2_MASK) && pt &&
			(pt->status32 & STATUS_A1_MASK ) ) {
			//flags &= ~(STATUS_E1_MASK | STATUS_E2_MASK);
			flags &= ~(STATUS_E1_MASK);
		}
	}

	local_irq_restore(flags);
}

#else  /* ! CONFIG_ARCH_ARC_LV2_INTR */

 /* Simpler version for only 1 level of interrupt
  * Here we only Worry about Level 1 Bits
  */

void __sram_text local_irq_enable(void) {

	unsigned long flags;
	local_save_flags(flags);
	flags |= (STATUS_E1_MASK | STATUS_E2_MASK);

	/* If called from hard ISR (between irq_enter and irq_exit)
	 * don't allow Level 1. In Soft ISR we allow further Level 1s
	 */

	if (in_irq()) {
		flags &= ~(STATUS_E1_MASK | STATUS_E2_MASK);
	}

	local_irq_restore(flags);
}
#endif

EXPORT_SYMBOL(local_irq_enable);

