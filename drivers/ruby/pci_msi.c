/*
 * (C) Copyright 2011 Quantenna Communications Inc.
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

#include <linux/irq.h>
#include <asm/irq.h>
#include <linux/pci.h>
#include <linux/msi.h>
#include <asm/board/platform.h>
#include <linux/version.h>

/**
 * get_irq_msi- return the msi_desc of irq
 */
struct
msi_desc * get_irq_msi(unsigned int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);

	if (!desc) {
		printk(KERN_ERR
		       "Trying to install msi data for IRQ%d\n", irq);
		return NULL;
	}
	return desc->msi_desc;
}

/**
 *  set_irq_msi - Associated the irq with a MSI descriptor
 */
int
set_irq_msi(unsigned int irq, struct msi_desc *entry)
{
	struct irq_desc *desc = irq_to_desc(irq);

	if (!desc) {
		printk(KERN_ERR
		       "Trying to install msi data for IRQ%d\n", irq);
		return -EINVAL;
	}
	if (irq != RUBY_IRQ_MSI) {
		printk(KERN_ERR "set up msi irq(%u) out of expected\n", irq);
		return -EINVAL;
	}
	if (desc->msi_desc != NULL) {
		printk(KERN_WARNING "%s: overwriting previous msi entry of irq %u\n", __FUNCTION__, irq);
	}

	desc->msi_desc = entry;
	if (entry)
		entry->irq = irq;

	return 0;
}

static void
handle_bad_irq(unsigned int irq, struct irq_desc *desc)
{
	printk(KERN_ERR "unexpected IRQ %u\n", irq);
}

/**
 *	arch_teardown_msi_irq - cleanup a dynamically allocated msi irq
 *	@irq:	irq number to teardown
 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34)
#define PCIMSI_SPIN_LOCK_IRQSAVE(lock, flags) raw_spin_lock_irqsave(lock, flags)
#define PCIMSI_SPIN_UNLOCK_IRQSAVE(lock, flags) raw_spin_unlock_irqrestore(lock, flags)
#else
#define PCIMSI_SPIN_LOCK_IRQSAVE(lock, flags) spin_lock_irqsave(lock, flags)
#define PCIMSI_SPIN_UNLOCK_IRQSAVE(lock, flags) spin_unlock_irqrestore(lock, flags)
#endif

void
arch_teardown_msi_irq(unsigned int irq)
{
	struct irq_desc *desc = irq_to_desc(irq);
	unsigned long flags;

	if (!desc) {
		WARN(1, KERN_ERR "Trying to cleanup invalid IRQ%d\n", irq);
		return;
	}

	PCIMSI_SPIN_LOCK_IRQSAVE(&desc->lock, flags);

	if (desc->action) {
		PCIMSI_SPIN_UNLOCK_IRQSAVE(&desc->lock, flags);
		WARN(1, KERN_ERR "Destroying IRQ%d without calling free_irq\n", irq);
		return;
	}
	desc->msi_desc = NULL;
	desc->handler_data = NULL;
	desc->chip_data = NULL;
	desc->handle_irq = handle_bad_irq;
	desc->chip = NULL;
	desc->name = NULL;

	PCIMSI_SPIN_UNLOCK_IRQSAVE(&desc->lock, flags);
}


/**
 *	arch_setup_msi_irq - setup irq associated with a msi_desc
 *	 and write the MSI message info to dev config space
 *	@desc:
 */
int
arch_setup_msi_irq(struct pci_dev *pdev, struct msi_desc *desc)
{
	printk("%s-%s called\n", __FILE__, __FUNCTION__);

	struct msi_msg msg;
	unsigned int irq;

	irq = RUBY_IRQ_MSI;

	set_irq_msi(irq, desc);

	msg.address_hi = 0;
	msg.address_lo = RUBY_PCIE_MSI_REGION;

	msg.data = RUBY_MSI_DATA;

	write_msi_msg(irq, &msg);

	return 0;
}
