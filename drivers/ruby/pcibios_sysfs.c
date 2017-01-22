/**
 * Copyright (c) 2009-2011 Quantenna Communications, Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <asm/io.h>
#include <common/topaz_platform.h>
#include "pcibios.h"

static inline void
test_config_cycles(void)
{
	int i = 0;

	printk("Config Cycles \n");
	/* print out first 20 config for test */
	for (i=0;i<20;i++) {
	   printk(KERN_INFO "[0x%x] = 0x%x\n", ((u32)RUBY_PCIE_CONFIG_REGION+i), *(u8 *)((u8 *)RUBY_PCIE_CONFIG_REGION+i));
	}

}

static inline void
test_mem_cycles(void)
{
	int i = 0;

	/* r/w first 40 config for test */
	printk(KERN_INFO "Test mem cycles\n");
	printk(KERN_INFO "Mem Cycles Reading\n");
	for (i=0; i<40; i=i+4) {
		printk(KERN_INFO "[0x%x] = 0x%x\n", ((u32)RUBY_PCI_RC_MEM_START+i), *(u32 *)((u8 *)RUBY_PCI_RC_MEM_START+i));

	}
	printk(KERN_INFO "Mem Cycles Writing\n");
	for (i=0; i<40; i=i+4) {
	   writel(0xa0a0a0a0, ((u32)RUBY_PCI_RC_MEM_START+i));

	}
	printk(KERN_INFO "Mem Cycles Reading after writing\n");
	for (i=0; i<40; i=i+4) {
		printk(KERN_INFO "[0x%x] = 0x%x\n", ((u32)RUBY_PCI_RC_MEM_START+i), *(u32 *)((u8 *)RUBY_PCI_RC_MEM_START+i));

	}
}

static inline void
ruby_atu_reg_dump(char *buf, int *len)
{

	*len += sprintf(buf+(*len), "-- Ruby ATU register dump --\n");

	*len += sprintf(buf+(*len), "View port	  0x%x : 0x%x \n", RUBY_PCIE_ATU_VIEW, readl(RUBY_PCIE_ATU_VIEW));
	*len += sprintf(buf+(*len), "LBAR		  0x%x : 0x%x \n", RUBY_PCIE_ATU_BASE_LO, readl(RUBY_PCIE_ATU_BASE_LO));
	*len += sprintf(buf+(*len), "UBAR		  0x%x : 0x%x \n", RUBY_PCIE_ATU_BASE_HI, readl(RUBY_PCIE_ATU_BASE_HI));
	*len += sprintf(buf+(*len), "LAR		  0x%x : 0x%x \n", RUBY_PCIE_ATU_BASE_LIMIT, readl(RUBY_PCIE_ATU_BASE_LIMIT));
	*len += sprintf(buf+(*len), "LTAR		  0x%x : 0x%x \n", RUBY_PCIE_ATU_TARGET_LO, readl(RUBY_PCIE_ATU_TARGET_LO));
	*len += sprintf(buf+(*len), "UTAR		  0x%x : 0x%x \n", RUBY_PCIE_ATU_TARGET_HI, readl(RUBY_PCIE_ATU_TARGET_HI));
	*len += sprintf(buf+(*len), "CTL1		  0x%x : 0x%x \n", RUBY_PCIE_ATU_CTL1, readl(RUBY_PCIE_ATU_CTL1));
	*len += sprintf(buf+(*len), "CTL2		  0x%x : 0x%x \n", RUBY_PCIE_ATU_CTL2, readl(RUBY_PCIE_ATU_CTL2));

}


static inline void
ruby_pcie_reg_dump (char *buf, int *len)
{
	ruby_csr_t csr;

	printk(KERN_INFO "-- Ruby PCI register dump --\n");
	*len += sprintf(buf+(*len), "Reset Mask Reg 0x%08x = 0x%08x\n",RUBY_SYS_CTL_CPU_VEC_MASK,  readl(RUBY_SYS_CTL_CPU_VEC_MASK));
	*len += sprintf(buf+(*len), "Reset Vect Reg 0x%08x = 0x%08x\n",RUBY_SYS_CTL_CPU_VEC,  readl(RUBY_SYS_CTL_CPU_VEC));
	*len += sprintf(buf+(*len), "Cntrl Mask Reg 0x%08x = 0x%08x\n",RUBY_SYS_CTL_MASK,  readl(RUBY_SYS_CTL_MASK));
	*len += sprintf(buf+(*len), "Cntrl Vect Reg 0x%08x = 0x%08x\n",RUBY_SYS_CTL_CTRL,  readl(RUBY_SYS_CTL_CTRL));

	csr.data = readl(RUBY_SYS_CTL_CSR);
	*len += sprintf(buf+(*len), "CSR		Reg 0x%08x = 0x%08x\n",RUBY_SYS_CTL_CSR,  csr.data);
	*len += sprintf(buf+(*len), "Chip id 0x%x  dlink %d phylink %d phyisr %d rst_req %d clk_rem %d fatl_err %d\n",
		   csr.r.chipid, csr.r.pci_dlink, csr.r.pci_phylink, csr.r.pci_phylink_isr,
		   csr.r.pci_rst_req, csr.r.pci_clk_rem, csr.r.pci_fatl_err				);
	*len += sprintf(buf+(*len), "PCIe CFG0	Reg 0x%08x = 0x%08x\n",RUBY_SYS_CTL_PCIE_CFG0,	readl(RUBY_SYS_CTL_PCIE_CFG0));
	*len += sprintf(buf+(*len), "PCIe CFG1	Reg 0x%08x = 0x%08x\n",RUBY_SYS_CTL_PCIE_CFG1,	readl(RUBY_SYS_CTL_PCIE_CFG1));
	*len += sprintf(buf+(*len), "PCIe CFG2	Reg 0x%08x = 0x%08x\n",RUBY_SYS_CTL_PCIE_CFG2,	readl(RUBY_SYS_CTL_PCIE_CFG2));
	*len += sprintf(buf+(*len), "PCIe CFG3	Reg 0x%08x = 0x%08x\n",RUBY_SYS_CTL_PCIE_CFG3,	readl(RUBY_SYS_CTL_PCIE_CFG3));
	*len += sprintf(buf+(*len), "PCIe CFG4	Reg 0x%08x = 0x%08x\n",RUBY_SYS_CTL_PCIE_CFG4,	readl(RUBY_SYS_CTL_PCIE_CFG4));
	*len += sprintf(buf+(*len), "PCIe Int Mask	0x%08x = 0x%08x\n",RUBY_PCIE_INT_MASK,	readl(RUBY_PCIE_INT_MASK));
}


ssize_t	ruby_sysfs_show (struct kobject *kobj, struct attribute *attr,
						char *buf)
{
	int feature = attr->name[0]-'0';
	int len=0;
	switch (feature) {
		case 1: /* get mode */
			len += sprintf(buf+len, "1 - RC, 2 - EP, Current %d", pci_mode);
			break;

		case 2: /* dump pcie regs */
			if (pci_mode)
				ruby_pcie_reg_dump(buf+len, &len);
			else
				len += sprintf(buf+len, "Mode is not set\n");

			break;

		case 3:
			if (pci_mode)
				ruby_atu_reg_dump(buf+len, &len);
			else
				len += sprintf(buf+len, "Mode is not set\n");

			break;

		default:
			break;
	}

	len += sprintf(buf+len, "\n");
	return len;
}

ssize_t	ruby_sysfs_store (struct kobject *kobj, struct attribute *attr,
			const char *buf, size_t size)
{
	int feature = attr->name[0]-'0';
	int input	= buf[0]-'0';

	DEBUG("%s -> (%d)%s\n",attr->name,input,buf);

	switch (feature) {
		case 1: /* Set mode: RC or EP */
			if (input == 1) {
				/* RC mode */
				pci_mode = RC_MODE;
			}
			else if (input == 2) {
				pci_mode = EP_MODE;
			}
			else
				printk(KERN_WARNING "1 - RC, 2 - EP, Cur Mode %d, invalid mode input %d", pci_mode, input);

			break;

		case 2: /* test config in RC mode */
			if (pci_mode == RC_MODE)
				test_config_cycles();
			else
				printk(KERN_INFO "Test config cycles in RC mode only\n");
			break;
		case 3: /* test mem in RC mode */
			if (pci_mode == RC_MODE) {
				test_mem_cycles();
			}
			else
				printk(KERN_INFO "Test mem cycles in RC mode only\n");
			break;

		default:
			break;
	}
	printk(KERN_INFO "\n");
	return size;
}


static struct sysfs_ops ruby_sysfs_ops =
{
	.show = ruby_sysfs_show,
	.store = ruby_sysfs_store
};

static struct kobj_type ruby_ktype =
{
	.sysfs_ops = &ruby_sysfs_ops
};

static struct kobject *ruby_kobj;
static struct attribute ruby_sysfs_attrs[] =
{
	{
		.name = "1_mode",
		.mode = S_IRUGO|S_IWUGO,
	},
	{
		.name = "2_config",
		.mode = S_IRUGO|S_IWUGO,
	},
	{
		.name = "3_mem",
		.mode = S_IRUGO|S_IWUGO,
	},

};

/*
** Number of elements in an array
*/
#define ArraySize(X)  (sizeof(X)/sizeof(X[0]))

void ruby_pci_create_sysfs (void)
{
	int i=0;
	int file_num = ArraySize(ruby_sysfs_attrs);

	ruby_kobj = kobject_create_and_add("ruby_PCI",NULL);
	ruby_kobj->ktype = &ruby_ktype;

	for (i=0; i<file_num; i++)
	  sysfs_create_file(ruby_kobj, &ruby_sysfs_attrs[i]);
}

void ruby_pci_cleanup_sysfs (void)
{
	int i=0;
	int file_num = ArraySize(ruby_sysfs_attrs);

	for (i=0; i<file_num; i++)
	  sysfs_remove_file(ruby_kobj, &ruby_sysfs_attrs[i]);
}



