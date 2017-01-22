/*
 * (C) Copyright 2011 Quantenna Communications Inc.
 *
 * Description	  : ruby PCI bus setup
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

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <asm/io.h>
#include <linux/ctype.h>
#include <linux/pci.h>
#include <linux/io.h>
#include <linux/time.h>
#include <linux/timex.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <common/topaz_platform.h>
#include "pcibios.h"

#define BUS_SCAN_TRY_MAX		1000
#define PCIE_DEV_LINKUP_MASK TOPAZ_PCIE_LINKUP


static int
rpcic_read_config (struct pci_bus *bus,
				   unsigned int    devfn,
				   int			   where,
				   int			   size,
				   u32			  *val);

static int
rpcic_write_config (struct pci_bus	*bus,
					unsigned int	 devfn,
					int				 where,
					int				 size,
					u32				 val);



static struct pci_ops rpcic_ops = {
	.read =		rpcic_read_config,
	.write =	rpcic_write_config,
};

typedef struct
{
	//TODO: define DW_PCIE_regs?
} DW_PCIE_regs;

struct ruby_pci
{
	DW_PCIE_regs	 *regs;
	int				  irq;
	void __iomem	 *cfg_virt;
	void __iomem	 *cfg0;
	void __iomem	 *cfg1;
	void __iomem	 *cfg2;
	void __iomem	 *cfg3;
	struct resource  mem_res;
	struct resource  io_res;
	struct pci_bus	 *bus;
};

/* RC mode or EP mode, actually only RC mode in used */
int pci_mode=0;

/* PCI bus 0 */
struct ruby_pci rpcib0;


static int
rpcic_read_word(unsigned int  busno,
				 unsigned int  devfn,
				 int		   where,
				 u32		  *val)
{
	struct ruby_pci   *rpcic = &rpcib0;
	volatile uint32_t *addr;

	if (where&3)
		panic("%s read address not aligned 0x%x\n", __FUNCTION__, where);

	addr = rpcic->cfg_virt + ((devfn&0xff)<<8) + where;

	*val = readl(addr);

	DEBUG("%s addr 0x%X val_addr 0x%X \n", __FUNCTION__, addr, *val);
	DEBUG("%s bus %u devfn %u \n", __FUNCTION__, busno, devfn);

	return 0;
}

/*
 * if addr&0x4 we should do some workaround
 */
static int
rpcic_write_word(unsigned int busno,
				  unsigned int devfn,
				  int		   where,
				  u32		   val)
{
	struct ruby_pci    *rpcic = &rpcib0;
	volatile uint32_t  *addr;

	if(where&3)
		panic("%s read address not aligned 0x%x\n", __FUNCTION__, where);

	addr = rpcic->cfg_virt +  ((devfn&0xff)<<8) + where;
#if 0
	writel(val, addr);
#else
   if ((u32)addr & 0x4) {
		u32 temp = readl(RUBY_PCIE_CONFIG_REGION + 0x10);
		writel(val,RUBY_PCIE_CONFIG_REGION + 0x10);
		writel( val,addr);
		writel(temp,RUBY_PCIE_CONFIG_REGION + 0x10);
	} else {
		writel(val, addr);
	}
#endif

	DEBUG("%s addr 0x%X -> 0x%X \n", __FUNCTION__, addr, val);
	DEBUG("%s bus %u devfn %u \n", __FUNCTION__, busno, devfn);

	return 0;
}


/*
 * Read ruby PCI config space
 */
static int
rpcic_read_config (struct pci_bus *bus,
				   unsigned int    devfn,
				   int			   where,
				   int			   size,
				   u32			  *value)
{
	u32 val;
	int		 retval = -EINVAL;

	DEBUG("%s called with bno. %d devfn %u where %d size %d \n ",
		 __FUNCTION__, bus->number, devfn, where, size);

	if (bus->number != 0)
	   return -EINVAL;

	if (PCI_SLOT(devfn) > 1)
	   return 0;

	switch (size) {
		case 1:
			rpcic_read_word(bus->number, devfn, where&~3, &val);
			*value = 0xff & (val >> (8*(where & 3)));
			retval=0;
			break;

		case 2:
			if (where&1) return -EINVAL;
			rpcic_read_word(bus->number, devfn, where&~3, &val);
			*value = 0xffff & (val >> (8*(where & 3)));
			retval=0;
			break;

		case 4:
			if (where&3) return -EINVAL;
			rpcic_read_word(bus->number, devfn, where, value);
			retval=0;
			break;
	}

	DEBUG(" value=0x%x\n", *value);
	return retval;
}


/*
 * Write ruby PCI config space
 */
static int
rpcic_write_config (struct pci_bus	*bus,
					unsigned int	 devfn,
					int				 where,
					int				 size,
					u32				 val)
{
	u32  tv;

	DEBUG("%s called with bno. %d devfn %u where 0x%x size %d val 0x%x \n",
			__FUNCTION__, bus->number, devfn, where, size, val);

	if (bus->number != 0)
		return -EINVAL;

	switch (size) {
		case 1:
			rpcic_read_word(bus->number, devfn, where&~3, &tv);
			tv = (tv & ~(0xff << (8*(where&3)))) | ((0xff&val) << (8*(where&3)));
			return rpcic_write_word(bus->number, devfn, where&~3, tv);

		case 2:
			if (where&1) return -EINVAL;
			rpcic_read_word(bus->number, devfn, where&~3, &tv);
			tv = (tv & ~(0xffff << (8*(where&3)))) | ((0xffff&val) << (8*(where&3)));
			return rpcic_write_word(bus->number, devfn, where&~3, tv);

		case 4:
			if (where&3) return -EINVAL;
			return rpcic_write_word(bus->number, devfn, where, val);
	}

	return 0;
}

static int
rpcic_find_capability(int cap)
{
	uint32_t pos;
	uint32_t cap_found;

	pos = (readl(RUBY_PCIE_REG_BASE + PCI_CAPABILITY_LIST) & 0x000000ff);
	while (pos) {
		cap_found = (readl(RUBY_PCIE_REG_BASE + pos) & 0x0000ffff);
		if ((cap_found & 0x000000ff)== (uint32_t)cap)
			break;

		pos = ((cap_found >> 8) & 0x000000ff);
	}

	return pos;
}
/*
 *  PCI bus scan and initialization
 */
static void
pci_bus_init (void)
{
	struct ruby_pci *rpcic = NULL;
	unsigned int ep_up = 0;
	int i = 0;

	rpcic = &rpcib0;
	rpcic->cfg_virt = (void *)RUBY_PCIE_CONFIG_REGION;
	rpcic->mem_res.name  = "RUBY PCI Memory space";
	rpcic->mem_res.start = RUBY_PCI_RC_MEM_START;
	rpcic->mem_res.end	= RUBY_PCI_RC_MEM_START + RUBY_PCI_RC_MEM_WINDOW -1;
	rpcic->mem_res.flags = IORESOURCE_MEM;
	rpcic->irq = TOPAZ_IRQ_PCIE;

	if (request_resource(&iomem_resource, &rpcic->mem_res) < 0) {
		printk(KERN_WARNING "WARNING: Failed to alloc IOMem!\n");
		goto out;
	}

	/* waiting for end point linked up in the PCI bus */
	for (i = 0; i < BUS_SCAN_TRY_MAX; i++) {
		ep_up = readl(TOPAZ_PCIE_STAT);
		if ( (ep_up & PCIE_DEV_LINKUP_MASK) == PCIE_DEV_LINKUP_MASK ) {
			break;
		}
		if (i%100 == 0) {
			printk(KERN_INFO "PCI Bus Scan loop for device link up\n");
		}
		udelay(1000);
	}

	/* Set RC Max_Payload_Size to 256 for topaz */
	int pos;
	uint32_t dev_ctl_sts;
	pos = rpcic_find_capability(PCI_CAP_ID_EXP);
	if (!pos) {
		printk(KERN_ERR "Could not find PCI Express capability in RC config space!\n");
	} else {
		dev_ctl_sts = readl(RUBY_PCIE_REG_BASE + pos + PCI_EXP_DEVCTL);
		dev_ctl_sts = ((dev_ctl_sts & ~PCI_EXP_DEVCTL_PAYLOAD) | BIT(5));
		writel(dev_ctl_sts, RUBY_PCIE_REG_BASE + pos + PCI_EXP_DEVCTL);
	}

	if ( (ep_up & PCIE_DEV_LINKUP_MASK) == PCIE_DEV_LINKUP_MASK ) {
		rpcic->bus = pci_scan_bus(0, &rpcic_ops, rpcic);
		pci_assign_unassigned_resources();
		printk(KERN_INFO "PCI Bus Scan completed!\n");
	} else {
		printk(KERN_INFO "PCI Bus Scan doesn't find any device link up!\n");
	}

out:
	return;
}

/*
 * Called after each bus is probed, but before its children are examined.
 * Enable response for the resource of dev and assign IRQ num
 */
void __devinit
pcibios_fixup_bus (struct pci_bus *bus)
{
	struct ruby_pci *rpcic;
	struct pci_dev	*dev;
	int				 i, has_io, has_mem;
	u32				 cmd;

	printk(KERN_INFO "%s called bus name %.10s no %d flags %x next_dev %p \n",
		 __FUNCTION__, bus->name, bus->number, bus->bus_flags, bus->devices.next);

	rpcic = (struct ruby_pci *) bus->sysdata;

	bus->resource[0] = &rpcic->io_res;
	bus->resource[1] = &rpcic->mem_res;

	if (bus->number != 0) {
		printk(KERN_WARNING "pcibios_fixup_bus: nonzero bus 0x%x\n", bus->number);
		return;
	}

	list_for_each_entry(dev, &bus->devices, bus_list) {
		has_io = has_mem = 0;

		for (i=0; i < RUBY_PCIE_BAR_NUM; i++) {
			unsigned long f = dev->resource[i].flags;
			if (f & IORESOURCE_IO) {
				has_io = 1;
			} else if (f & IORESOURCE_MEM) {
				has_mem = 1;
			}
		}
		rpcic_read_config(dev->bus, dev->devfn, PCI_COMMAND, 2, &cmd);
		printk(KERN_INFO "%s: Device [%2x:%2x.%d] has mem %d io %d cmd %x \n", __FUNCTION__,
				   dev->bus->number, PCI_SLOT(dev->devfn), PCI_FUNC(dev->devfn), has_mem, has_io, cmd);

		if (has_io && !(cmd & PCI_COMMAND_IO)) {
			cmd |= PCI_COMMAND_IO;
			rpcic_write_config(dev->bus, dev->devfn, PCI_COMMAND, 2, cmd);
			printk(KERN_INFO "%s: Enabling I/O for device [%2x:%2x.%d]\n", __FUNCTION__,
				   dev->bus->number, PCI_SLOT(dev->devfn), PCI_FUNC(dev->devfn));
		}
		if (has_mem && !(cmd & PCI_COMMAND_MEMORY)) {
			cmd |= PCI_COMMAND_MEMORY;
			rpcic_write_config(dev->bus, dev->devfn, PCI_COMMAND, 2, cmd);
			printk(KERN_INFO "%s: Enabling memory for device [%2x:%2x.%d]\n", __FUNCTION__,
				dev->bus->number, PCI_SLOT(dev->devfn), PCI_FUNC(dev->devfn));
		}

		dev->irq = rpcic->irq;
	}

}

/*
 * pcibios align resources() is called every time generic PCI code
 * wants to generate a new address. The process of looking for
 * an available address, each candidate is first "aligned" and
 * then checked if the resource is available until a match is found.
 *
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34)
resource_size_t
pcibios_align_resource (void			 *data,
						const struct resource  *res,
						resource_size_t  size,
						resource_size_t  align)
{
	struct pci_dev	  *dev	 = data;
	struct ruby_pci   *rpcic = dev->sysdata;
	resource_size_t start = res->start;

	DEBUG("%s called\n ", __FUNCTION__);

	if (res->flags & IORESOURCE_IO) {
		/* Make sure we start at our min on all hoses */
		if (start < PCIBIOS_MIN_IO + rpcic->io_res.start)
			start = PCIBIOS_MIN_IO + rpcic->io_res.start;

		/*
		 * Put everything into 0x00-0xff region modulo 0x400
		 */
		if (start & 0x300)
			start = (start + 0x3ff) & ~0x3ff;

	} else if (res->flags & IORESOURCE_MEM) {
		/* Make sure we start at our min on all hoses */
		if (start < PCIBIOS_MIN_MEM + rpcic->mem_res.start)
			start = PCIBIOS_MIN_MEM + rpcic->mem_res.start;
	}

	return start;
}
#else
void
pcibios_align_resource (void			 *data,
						struct resource  *res,
						resource_size_t  size,
						resource_size_t  align)
{
	struct pci_dev	  *dev	 = data;
	struct ruby_pci   *rpcic = dev->sysdata;
	unsigned long	   start = res->start;

	DEBUG("%s called\n ", __FUNCTION__);

	if (res->flags & IORESOURCE_IO) {
		/* Make sure we start at our min on all hoses */
		if (start < PCIBIOS_MIN_IO + rpcic->io_res.start)
			start = PCIBIOS_MIN_IO + rpcic->io_res.start;

		/*
		 * Put everything into 0x00-0xff region modulo 0x400
		 */
		if (start & 0x300)
			start = (start + 0x3ff) & ~0x3ff;

	} else if (res->flags & IORESOURCE_MEM) {
		/* Make sure we start at our min on all hoses */
		if (start < PCIBIOS_MIN_MEM + rpcic->mem_res.start)
			start = PCIBIOS_MIN_MEM + rpcic->mem_res.start;
	}

	res->start = start;
}
#endif


/**
 * pcibios_enable_device - Enable I/O and memory.
 * @dev: PCI device to be enabled
 */
int
pcibios_enable_device (struct pci_dev *dev,
						int			 mask)
{
	u16				 cmd, old_cmd;
	int				 idx;
	struct resource  *r;
	int pos = 0;

	pci_read_config_word(dev, PCI_COMMAND, &cmd);
	old_cmd = cmd;

	for (idx=0; idx < RUBY_PCIE_BAR_NUM; idx++) {
		/* Only set up the requested stuff */
		if (!(mask & (1<<idx)))
			continue;

		r = &dev->resource[idx];

		if (!r->start && r->end) {
			printk(KERN_ERR "PCI: Device %s not available because of resource collisions\n",
				  pci_name(dev));
			return -EINVAL;
		}

		if (r->flags & IORESOURCE_IO) {
			cmd |= PCI_COMMAND_IO;
			printk(KERN_INFO "Enabling IO\n");
		}

		if (r->flags & IORESOURCE_MEM) {
			cmd |= PCI_COMMAND_MEMORY;
			printk(KERN_INFO "Enabling MEM\n");
		}
	}

	if (dev->resource[PCI_ROM_RESOURCE].start)
		cmd |= PCI_COMMAND_MEMORY;

	if (cmd != old_cmd) {
		printk(KERN_INFO "PCI: Enabling device %s (%04x -> %04x)\n", pci_name(dev), old_cmd, cmd);
		pci_write_config_word(dev, PCI_COMMAND, cmd);
	}

	pci_write_config_word(dev, PCI_COMMAND, PCI_COMMAND_IO|PCI_COMMAND_MEMORY);

	/* Set the device's MSI capability */
	pos = pci_find_capability(dev, PCI_CAP_ID_MSI);
	if (!pos) {
		printk(KERN_ERR "Error locating MSI capability position, using INTx instead\n");
	} else {
		/* Setup msi generation info */
		writel(TOPAZ_PCIE_MSI_REGION, TOPAZ_MSI_ADDR_LOWER);
		writel(0, TOPAZ_MSI_ADDR_UPPER);
		writel(BIT(0), TOPAZ_MSI_INT_ENABLE);
		writel(0, TOPAZ_PCIE_MSI_MASK);
	}

	return 0;
}

int
pcibios_assign_resource (struct pci_dev *pdev,
						 int			 resource)
{
	printk(KERN_INFO "%s called\n", __FUNCTION__);
	return -ENXIO;
}

void __init
pcibios_update_irq (struct pci_dev *dev,
					int				irq)
{
	printk(KERN_INFO "%s called for irq %d\n", __FUNCTION__, irq);
	pci_write_config_byte(dev, PCI_INTERRUPT_LINE, irq);
}


char * __devinit
pcibios_setup (char *str)
{
	printk(KERN_INFO "%s called str %s\n", __FUNCTION__, str);
	return str;
}

/**
 * ruby_pci_init - Initial PCI bus in RC mode
 */
static int __init
ruby_pci_init (void)
{
	ruby_pci_create_sysfs();
	pci_mode = RC_MODE;
	pci_bus_init();

	return 0;
}

subsys_initcall(ruby_pci_init);
