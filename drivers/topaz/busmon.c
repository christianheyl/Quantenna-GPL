/**
 * (C) Copyright 2012 Quantenna Communications Inc.
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
 **/

#include <linux/proc_fs.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <asm/io.h>

#include <qtn/qtn_debug.h>
#include <qtn/busmon.h>

/**
 * AHB monitor driver. Linux kernel API is declared in busmon.h. Interfaces to user space are
 * done via procfs and sysfs. RO /proc/topaz_busmon file provides information about bus monitor
 * status and control registers. AHB bus is registered in kernel and the appropriate element is
 * created (/bus/ahb). The following files are created in sysfs to contol the AHB monitor
 * /sys/bus/ahb/ahbm_ranges: rw file to specify start and end range addresses to be monitored. Up to
				4 ranges can be specified
 * /sys/bus/ahb/ahbm_outside: rw file to control inside/outside range check
 * /sys/bus/ahb/ahbm_timeout: rw file to specify clock cycles to time out in 250MHz clock
 * /sys/bus/ahb/ahbm_range_test_on: rw file to toggle range test on or off. The new values of ranges
					and outside take effect only after writing 1 to this file
 * /sys/bus/ahb/ahbm_timeout_test_on: rf file to toggle timeout test on or off. The new timeout value
					takes effect only after writing 1 to this file
 */

#define PROC_NAME "topaz_busmon"

static const char *master_names[] = TOPAZ_BUSMON_MASTER_NAMES;

#define BUSMON_TIMEOUT_BITS(t)		\
	(TOPAZ_BUSMON_TIMER_INT_EN |	\
	 TOPAZ_BUSMON_TIMEOUT(t))

#define BUSMON_RANGE_BITS(r)		\
	(TOPAZ_BUSMON_REGION_VALID(r) |	\
	 TOPAZ_BUSMON_ADDR_CHECK_EN |	\
	 TOPAZ_BUSMON_BLOCK_TRANS_EN |	\
	 TOPAZ_BUSMON_OUTSIDE_ADDR_CHECK)
/**
 * When enabled range test is running
 */
static unsigned int range_test_on = 0;

/**
 * When enabled timeout test is running
 */
static unsigned int timeout_test_on = 0;

/**
 * Clock cycles to time out in 250MHz clock
 */
static uint16_t timeout = 255;

/**
 * Lower and upper limits of address ranges 0-3
 */
static struct topaz_busmon_range ranges[TOPAZ_BUSMON_MAX_RANGES] = {
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 }
};

/**
 * Enables/disables outside address check
 */
static unsigned int outside = 0;

/**
 * AHB bus definition appeared in /sys/bus
 */
static struct bus_type ahb = {
	.name = "ahb",
};

/**
 * Setter and getter functions for AHB monitor parameters presented over sysfs
 */

static ssize_t ahbm_outside_show(struct bus_type *bus, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", outside);
}

static ssize_t ahbm_outside_store(struct bus_type *bus, const char *buf, size_t count)
{
	sscanf(buf, "%u", &outside);
	return count;
}
BUS_ATTR(ahbm_outside, S_IRUGO | S_IWUSR, ahbm_outside_show, ahbm_outside_store);

static ssize_t ahbm_range_test_show(struct bus_type *bus, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", range_test_on);
}

static ssize_t ahbm_range_test_store(struct bus_type *bus, const char *buf, size_t count)
{
	sscanf(buf, "%u", &range_test_on);

	if (range_test_on) {
		topaz_busmon_range_check(TOPAZ_BUSMON_LHOST, ranges, ARRAY_SIZE(ranges), outside);
	} else {
		topaz_busmon_range_check_disable(TOPAZ_BUSMON_LHOST);
	}

	return count;
}
BUS_ATTR(ahbm_range_test_on, S_IRUGO | S_IWUSR, ahbm_range_test_show, ahbm_range_test_store);

static ssize_t ahbm_timeout_test_show(struct bus_type *bus, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", timeout_test_on);
}

static ssize_t ahbm_timeout_test_store(struct bus_type *bus, const char *buf, size_t count)
{
	sscanf(buf, "%u", &timeout_test_on);

	if (timeout_test_on) {
		topaz_busmon_timeout_en(TOPAZ_BUSMON_LHOST, timeout);
	} else {
		topaz_busmon_timeout_dis(TOPAZ_BUSMON_LHOST);
	}

	return count;
}
BUS_ATTR(ahbm_timeout_test_on, S_IRUGO | S_IWUSR, ahbm_timeout_test_show, ahbm_timeout_test_store);

static ssize_t ahbm_timeout_show(struct bus_type *bus, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%hu\n", timeout);
}

static ssize_t ahbm_timeout_store(struct bus_type *bus, const char *buf, size_t count)
{
	sscanf(buf, "%hu", &timeout);
	return count;
}
BUS_ATTR(ahbm_timeout, S_IRUGO | S_IWUSR, ahbm_timeout_show, ahbm_timeout_store);

static ssize_t ahbm_ranges_show(struct bus_type *bus, char *buf)
{
	ssize_t	n;
	int i;

	for (i = 0, n = 0; i < TOPAZ_BUSMON_MAX_RANGES; i++) {
		n += scnprintf(buf + n, PAGE_SIZE, "0x%08x:0x%08x\n", (uint32_t)ranges[i].start,
		               (uint32_t)ranges[i].end);
	}

	return n;
}

static ssize_t ahbm_ranges_store(struct bus_type *bus, const char *buf, size_t count)
{
	ssize_t		n;
	int		i;
	bool		is_err = false;

	for (i = 0, n = 0; i < TOPAZ_BUSMON_MAX_RANGES; i++) {
		if (!is_err) {
			if (sscanf(buf + n, "0x%08x:0x%08x\n",
			                (uint32_t *)&ranges[i].start, (uint32_t *)&ranges[i].end) == 2) {
				n += 22;
			} else {
				is_err = true;
				ranges[i].start = 0;
				ranges[i].end = 0;
			}
		} else {
			ranges[i].start = 0;
			ranges[i].end = 0;
		}
	}

	return count;
}
BUS_ATTR(ahbm_ranges, S_IRUGO | S_IWUSR, ahbm_ranges_show, ahbm_ranges_store);

/**
 * Enables/disables mask bits for AHB monitor IRQ
 */
static void topaz_busmon_irq_set(uint32_t bit, bool enable)
{
	uint32_t busmon_intr_mask;

	busmon_intr_mask = readl(TOPAZ_BUSMON_INTR_MASK);

	if (enable) {
		busmon_intr_mask |= bit;
	} else {
		busmon_intr_mask &= ~bit;
	}

	writel(busmon_intr_mask, TOPAZ_BUSMON_INTR_MASK);
}

/**
 * Enables/disables AHB monitor timeout interrupt generation and sets timeout value
 */
void topaz_busmon_timeout(uint8_t bus, uint16_t tm, bool enable)
{
	uint32_t busmon_ctrl;

	timeout = tm;
	timeout_test_on = enable;

	/*
	 * Add timeout settings, preserving existing range-check settings
	 */
	busmon_ctrl = readl(TOPAZ_BUSMON_CTL(bus));
	busmon_ctrl &= BUSMON_RANGE_BITS(~0);

	if (enable) {
		busmon_ctrl |= BUSMON_TIMEOUT_BITS(timeout);
	}

	writel(busmon_ctrl, TOPAZ_BUSMON_CTL(bus));

	if (enable) {
		/* enable/disable timeout interrupt for this bus master monitor */
		topaz_busmon_irq_set(TOPAZ_BUSMON_INTR_MASK_TIMEOUT_EN(bus), enable);
	}
}
EXPORT_SYMBOL(topaz_busmon_timeout);

/**
 * Enables/disables AHB monitor range interrupt generation and defines the ranges
 */
void topaz_busmon_range_check(uint8_t bus,
                              const struct topaz_busmon_range *range,
                              size_t nranges, bool out)
{
	uint32_t busmon_ctrl;
	int i;

	outside = out;

	/*
	 * temporarily disable range checking for this bus master monitor,
	 * preserving other settings like timeout checking
	 */
	busmon_ctrl = readl(TOPAZ_BUSMON_CTL(bus));
	busmon_ctrl &= BUSMON_TIMEOUT_BITS(~0);
	writel(busmon_ctrl, TOPAZ_BUSMON_CTL(bus));

	/* initialize address range registers, and busmon_ctrl filter enable */
	for (i = 0; i < TOPAZ_BUSMON_MAX_RANGES; i++) {
		if (i < nranges) {
			memcpy(&ranges[i], &range[i], sizeof(ranges[i]));
			writel(range[i].start, TOPAZ_BUSMON_CTL_RANGE_LOW(bus, i));
			writel(range[i].end,   TOPAZ_BUSMON_CTL_RANGE_HIGH(bus, i));
			busmon_ctrl |= TOPAZ_BUSMON_REGION_VALID(BIT(i));
		} else {
			memset(&ranges[i], 0, sizeof(ranges[i]));
			writel(0, TOPAZ_BUSMON_CTL_RANGE_LOW(bus, i));
			writel(0, TOPAZ_BUSMON_CTL_RANGE_HIGH(bus, i));
		}
	}

	/* enable/disable range checking */
	if (nranges) {
		range_test_on = 1;
		busmon_ctrl |= TOPAZ_BUSMON_ADDR_CHECK_EN;
		busmon_ctrl |= TOPAZ_BUSMON_BLOCK_TRANS_EN;

		if (outside) {
			busmon_ctrl |= TOPAZ_BUSMON_OUTSIDE_ADDR_CHECK;
		}
	} else {
		range_test_on = 0;
	}

	writel(busmon_ctrl, TOPAZ_BUSMON_CTL(bus));

	/* enable/disable range check interrupt for this bus master monitor */
	topaz_busmon_irq_set(TOPAZ_BUSMON_INTR_MASK_RANGE_CHECK_EN(bus), nranges > 0);
}
EXPORT_SYMBOL(topaz_busmon_range_check);

static int topaz_busmon_dump_master(char *const p, unsigned int master)
{
	unsigned int reg;
	uint32_t debug_regs[TOPAZ_BUSMON_DEBUG_MAX];

	for (reg = 0; reg < ARRAY_SIZE(debug_regs); reg++) {
		writel(TOPAZ_BUSMON_DEBUG_VIEW_MASTER(master) |
		       TOPAZ_BUSMON_DEBUG_VIEW_DATA_SEL(reg),
		       TOPAZ_BUSMON_DEBUG_VIEW);
		debug_regs[reg] = readl(TOPAZ_BUSMON_DEBUG_STATUS);
	}

	return sprintf(p, "master %-5s addr 0x%08x rd %08x%08x wr %08x%08x ctrl %08x %08x %08x\n",
	               master_names[master], debug_regs[TOPAZ_BUSMON_ADDR],
	               debug_regs[TOPAZ_BUSMON_RD_H32], debug_regs[TOPAZ_BUSMON_RD_L32],
	               debug_regs[TOPAZ_BUSMON_WR_H32], debug_regs[TOPAZ_BUSMON_WR_L32],
	               debug_regs[TOPAZ_BUSMON_CTRL0],
	               debug_regs[TOPAZ_BUSMON_CTRL1],
	               debug_regs[TOPAZ_BUSMON_CTRL2]);
}

static irqreturn_t topaz_busmon_irq_handler(int irq, void *arg)
{
	unsigned int master;
	char buf[128];
	uint32_t ahb_mon_int_status = readl(TOPAZ_BUSMON_INTR_STATUS);

	uint32_t busmon_ctrl = readl(TOPAZ_BUSMON_CTL(TOPAZ_BUSMON_LHOST));
	busmon_ctrl &= ~TOPAZ_BUSMON_TIMER_INT_EN;
	writel(busmon_ctrl, TOPAZ_BUSMON_CTL(TOPAZ_BUSMON_LHOST));

	printk("%s, irq %d, ahb_mon_int_status 0x%x\n",
	       __FUNCTION__, irq, ahb_mon_int_status);

	for (master = 0; master < ARRAY_SIZE(master_names); master++) {
		topaz_busmon_dump_master(buf, master);
		printk("%s", buf);
	}

	writel(~0, TOPAZ_BUSMON_INTR_STATUS);

	/* Dump task stack */
	printk("Current task = '%s', PID = %u, ASID = %lu\n", current->comm,
	       current->pid, current->active_mm->context.asid);
	show_stacktrace(current, NULL);

	busmon_ctrl |= TOPAZ_BUSMON_TIMER_INT_EN;
	writel(busmon_ctrl, TOPAZ_BUSMON_CTL(TOPAZ_BUSMON_LHOST));

	return IRQ_HANDLED;
}

static int topaz_busmon_read_proc(char *page, char **start, off_t off,
                                  int count, int *eof, void *_unused)
{
	char *p = page;
	unsigned int master;
	const char *master_names[] = TOPAZ_BUSMON_MASTER_NAMES;
	unsigned long flags;

	local_irq_save(flags);

	for (master = 0; master < ARRAY_SIZE(master_names); master++) {
		p += topaz_busmon_dump_master(p, master);
	}

	local_irq_restore(flags);

	*eof = 1;
	return p - page;
}

static int __init topaz_busmon_create_proc(void)
{
	struct proc_dir_entry *entry = create_proc_entry(PROC_NAME, 0600, NULL);

	if (!entry) {
		return -ENOMEM;
	}

	entry->write_proc = NULL;
	entry->read_proc = topaz_busmon_read_proc;

	return 0;
}

int __init topaz_busmon_init(void)
{
	int rc;

	rc = request_irq(TOPAZ_IRQ_MISC_AHB_MON, topaz_busmon_irq_handler,
	                 IRQF_DISABLED, "ahb bus monitor", NULL);

	if (rc) {
		goto error;
	}

	rc = topaz_busmon_create_proc();

	if (rc) {
		printk(KERN_WARNING "procfs: error creating proc entry: %d\n", rc);
		goto error1;
	}

	rc = bus_register(&ahb);

	if (rc < 0) {
		printk(KERN_WARNING "sysfs: error register bus: %d\n", rc);
		goto error2;
	}

	rc = bus_create_file(&ahb, &bus_attr_ahbm_range_test_on);

	if (rc < 0) {
		printk(KERN_WARNING "sysfs: error creating busfile\n");
		goto error3;
	}

	rc = bus_create_file(&ahb, &bus_attr_ahbm_timeout_test_on);

	if (rc < 0) {
		printk(KERN_WARNING "sysfs: error creating busfile\n");
		goto error4;
	}

	rc = bus_create_file(&ahb, &bus_attr_ahbm_timeout);

	if (rc < 0) {
		printk(KERN_WARNING "sysfs: error creating busfile\n");
		goto error5;
	}

	rc = bus_create_file(&ahb, &bus_attr_ahbm_ranges);

	if (rc < 0) {
		printk(KERN_WARNING "sysfs: error creating busfile\n");
		goto error6;
	}

	rc = bus_create_file(&ahb, &bus_attr_ahbm_outside);

	if (rc < 0) {
		printk(KERN_WARNING "sysfs: error creating busfile\n");
		goto error7;
	}

	printk(KERN_DEBUG "%s success\n", __FUNCTION__);

	return 0;
error7:
	bus_remove_file(&ahb, &bus_attr_ahbm_ranges);
error6:
	bus_remove_file(&ahb, &bus_attr_ahbm_timeout);
error5:
	bus_remove_file(&ahb, &bus_attr_ahbm_timeout_test_on);
error4:
	bus_remove_file(&ahb, &bus_attr_ahbm_range_test_on);
error3:
	bus_unregister(&ahb);
error2:
	remove_proc_entry(PROC_NAME, NULL);
error1:
	free_irq(TOPAZ_IRQ_MISC_AHB_MON, NULL);
error:
	return rc;
}

static void __exit topaz_busmon_exit(void)
{
	bus_remove_file(&ahb, &bus_attr_ahbm_outside);
	bus_remove_file(&ahb, &bus_attr_ahbm_ranges);
	bus_remove_file(&ahb, &bus_attr_ahbm_timeout);
	bus_remove_file(&ahb, &bus_attr_ahbm_timeout_test_on);
	bus_remove_file(&ahb, &bus_attr_ahbm_range_test_on);
	bus_unregister(&ahb);
	remove_proc_entry(PROC_NAME, NULL);
	free_irq(TOPAZ_IRQ_MISC_AHB_MON, NULL);
}

module_init(topaz_busmon_init);
module_exit(topaz_busmon_exit);

MODULE_DESCRIPTION("Topaz AHB Bus Monitors");
MODULE_AUTHOR("Quantenna");
MODULE_LICENSE("GPL");

