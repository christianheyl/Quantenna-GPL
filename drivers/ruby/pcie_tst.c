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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/random.h>
#include <linux/timer.h>
#include <linux/io.h>
#include <linux/moduleparam.h>
#include <common/topaz_platform.h>

#define REMOTE_DDR_SIZE 0x100000
#define REMOTE_DDR_BASE 0xb0000000
#define REMOTE_ADDR_RANDOM


#define REG_READ(reg) readl(reg)
#if 0
#define REG_WRITE(reg, val)   do {\
		printk("\nwrite reg=%08x,val=%08x", reg, val);\
		writel((val), (reg));\
		printk(" read back val %08x", REG_READ(reg));\
					} while(0)
#else
#define REG_WRITE(reg, val)  writel((val), (reg))
#endif

DEFINE_SPINLOCK(pcie_dmawr_lock);
void *remote_ddr_vbase = NULL;

/******************************************************************************
 Function:	pcie_dma_wr
 Purpose:	Use pcie dma to transfer from local mem to remote mem (write channel).
 Returns:
 Note:
 *****************************************************************************/
void pcie_dma_wr(u32 sar, u32 dar, u32 size)
{
	u32 intstat;
	ulong deadline;

	REG_WRITE(PCIE_DMA_WR_ENABLE, 0x00000001);
	REG_WRITE(PCIE_DMA_WR_INTMASK, 0x00000000);
	REG_WRITE(PCIE_DMA_CHNL_CONTEXT, 0x00000000);
	REG_WRITE(PCIE_DMA_CHNL_CNTRL, 0x04000008);
	REG_WRITE(PCIE_DMA_XFR_SIZE, size);
	REG_WRITE(PCIE_DMA_SAR_LOW, sar);
	REG_WRITE(PCIE_DMA_SAR_HIGH, 0x00000000);
	REG_WRITE(PCIE_DMA_DAR_LOW, dar);
	REG_WRITE(PCIE_DMA_DAR_HIGH, 0x00000000);
	deadline = jiffies + msecs_to_jiffies(1);
	REG_WRITE(PCIE_DMA_WR_DOORBELL, 0x00000000);
	//Now check if DMA transfer is done
	while (((intstat = REG_READ(PCIE_DMA_WR_INTSTS)) & 1) == 0) {
		if (time_after(jiffies, deadline)) {
			printk("\nError, Can't get done bit");
			break;
		}
	}
	intstat = 0;
	//Clear status bit so can be used for next transfer
	intstat = intstat | 1;
	REG_WRITE(PCIE_DMA_WR_INTCLER, intstat);
	//printk(" INFO : Done DMA WR CHNL");
}

/******************************************************************************
 Function:	pcie_dma_rd
 Purpose:	Use pcie dma to transfer from remote mem to local mem(Read channel).
 Returns:
 Note:
 *****************************************************************************/
void pcie_dma_rd(u32 sar, u32 dar, u32 size)
{
	u32 intstat;
	ulong deadline;

	printk(" INFO : Start PCIE-DMA programming RD Channel");
	REG_WRITE(PCIE_DMA_RD_ENABLE, 0x00000001);
	REG_WRITE(PCIE_DMA_RD_INTMASK, 0x00000000);
	REG_WRITE(PCIE_DMA_CHNL_CONTEXT, 0x80000000);
	REG_WRITE(PCIE_DMA_CHNL_CNTRL, 0x04000008);
	REG_WRITE(PCIE_DMA_XFR_SIZE, size);
	REG_WRITE(PCIE_DMA_SAR_LOW, sar);
	REG_WRITE(PCIE_DMA_SAR_HIGH, 0x00000000);
	REG_WRITE(PCIE_DMA_DAR_LOW, dar);
	REG_WRITE(PCIE_DMA_DAR_HIGH, 0x00000000);
	REG_WRITE(PCIE_DMA_RD_DOORBELL, 0x00000000);
	//Now check if DMA transfer is done
	deadline = jiffies + msecs_to_jiffies(1);
	while (((intstat = REG_READ(PCIE_DMA_RD_INTSTS)) & 1) == 0) {
		if (time_after_eq(jiffies, deadline)) {
			printk("\nError, Can't get done bit");
			break;
		}
	}
	//DMA transfer is done. Check packet in SRAM.
	//Clear status bit so can be used for next transfer
	intstat = intstat | 1;
	REG_WRITE(PCIE_DMA_RD_INTCLER, intstat);
	printk(" INFO : Done DMA RD CHNL");
}

/* typpe
 * 0 ep
 * 1 rc
 */
int pcie_init(u32 type)
{
	u32 rdata;
	REG_WRITE(RUBY_SYS_CTL_CPU_VEC_MASK, RUBY_SYS_CTL_RESET_IOSS | RUBY_SYS_CTL_RESET_PCIE);
	REG_WRITE(RUBY_SYS_CTL_CPU_VEC, RUBY_SYS_CTL_RESET_IOSS | RUBY_SYS_CTL_RESET_PCIE);

	rdata = 0;
	rdata = 1 << 16;
	rdata = rdata + 0xa2;
	REG_WRITE(PCIE_PORT_LINK_CTL, rdata);
	rdata = (type) * 4;
	rdata += (0xff3c941 << 4);
	REG_WRITE(RUBY_SYS_CTL_PCIE_CFG0, rdata);
	REG_WRITE(RUBY_SYS_CTL_PCIE_CFG1, 0x00000001);
	REG_WRITE(RUBY_SYS_CTL_PCIE_CFG2, 0x0);
	REG_WRITE(RUBY_SYS_CTL_PCIE_CFG3, 0x45220000);
	REG_WRITE(RUBY_PCIE_CMD_REG, 0x00100007);

	//Now configure ATU in RTL for memory transactions.
	//Define Region 0 of ATU as inbound Memory.
	REG_WRITE(PCIE_ATU_VIEWPORT, 0x80000000);
	//Configure lower 32 bit of start address for translation region as 0000_0000.
	REG_WRITE(PCIE_ATU_BASE_LOW, 0x00000000);
	//Configure upper 32 bit of start address for translation region as 0000_0000.
	REG_WRITE(PCIE_ATU_BASE_HIGH, 0x00000000);
	//Set translation region limit upto 0x4FFF_FFFF
	REG_WRITE(PCIE_ATU_BASE_LIMIT, 0x03FFFFFF);
	//Set target translated address as 0x8000_0000 in lower and upper register.
	REG_WRITE(PCIE_ATU_TGT_LOW, 0x00000000);
	REG_WRITE(PCIE_ATU_TGT_HIGH, 0x00000000);
	//Configure ATU_CONTROL_1 register to Issue MRD/MWR request.
	REG_WRITE(PCIE_ATU_CTRL1, 0x00000000);
	//Configure ATU_CONTROL_2 register to Enable Address Translation.
	//This will match to BAR0 for Inboud only
	REG_WRITE(PCIE_ATU_CTRL2, 0xC0000000);
	//Now configure BAR0 with ATU target address programmed above
	REG_WRITE(PCIE_BAR0, 0x00000000);

	//Now define region 1 of ATU as outbound memory
	REG_WRITE(PCIE_ATU_VIEWPORT, 0x00000001);
	//Configure lower 32 bit of start address for translation region as PCIE address space.
	REG_WRITE(PCIE_ATU_BASE_LOW, REMOTE_DDR_BASE);
	//Configure upper 32 bit of start address for translation region as 0000_0000.
	REG_WRITE(PCIE_ATU_BASE_HIGH, 0x00000000);
	//Set translation region limit upto 0x0FFFFFFF
	REG_WRITE(PCIE_ATU_BASE_LIMIT, 0xB3FFFFFF);
	//Set target translated address as 0x8000_0000 in lower and upper register.
	REG_WRITE(PCIE_ATU_TGT_LOW, 0x00000000);
	REG_WRITE(PCIE_ATU_TGT_HIGH, 0x00000000);
	//Configure ATU_CONTROL_1 register to Issue MRD/MWR request.
	REG_WRITE(PCIE_ATU_CTRL1, 0x00000000);
	//Configure ATU_CONTROL_2 register to Enable Address Translation.
	REG_WRITE(PCIE_ATU_CTRL2, 0x80000000);

	return 0;
}

/*addr1 and addr2 is aligned with 4 bytes*/
inline static int pcie_cmp(u32 addr1, u32 addr2, int size)
{
	int count;
	int rc = 0;

	for (count = 0; count < (size & ~0x3); count += 4) {
		u32 word1 = *(u32 *) addr1;
		u32 word2 = *(u32 *) addr2;
		if (word1 != word2)	{
			printk("\ncount=%d, size=%d, 0x%08x (0x%08x) != 0x%08x (0x%08x)",
					count, size, addr1, word1, addr2, word2);
			rc = -1;
			break;
		}
		addr1 += 4;
		addr2 += 4;
	}
	/* compare the unaligned part in the tail */
	if (rc == 0) {
		for (count = (size & ~0x3); count < size; count += 1) {
			u8 byte1 = *(u8 *) addr1;
			u8 byte2 = *(u8 *) addr2;
			if (byte1 != byte2) {
				printk("\ncount=%d, size=%d, 0x%08x (0x%02x) != 0x%08x (0x%02x)",
						count, size, addr1, byte1, addr2, byte2);
				rc = -1;
				break;
			}
			addr1 += 1;
			addr2 += 1;
		}
	}

	return rc;
}


#ifdef REMOTE_ADDR_RANDOM

inline static u32 get_remote_offset(void)
{
	u32 addr = random32();
	addr &= ((REMOTE_DDR_SIZE - 1) & (~0x3));
	if (addr + 2048 > REMOTE_DDR_SIZE)
		addr -= 2048;
	return addr;
}

#else
static int remote_addr_offset = 0;
module_param(remote_addr_offset, int, 0600);

inline static u32 get_remote_offset(void)
{
	return (u32)remote_addr_offset;
}

#endif

void pcie_dma_tst(u32 local_vaddr, u32 local_paddr, int len)
{
	u8 *pad;
	u32 remote_vaddr, remote_paddr;
	u32 offset;
	int tmp = local_paddr & 0x3;
	spin_lock_irq(&pcie_dmawr_lock);

	if (tmp) {
		local_vaddr = local_vaddr + 4 - tmp;
		local_paddr = local_paddr + 4 - tmp;
		len -= 4 - tmp;
	}
	offset = get_remote_offset();
	remote_vaddr = (u32) remote_ddr_vbase + offset;
	remote_paddr = REMOTE_DDR_BASE + offset;
	pad = (u8 *) remote_vaddr + len;
	pad[0] = 0x5a;
	pad[1] = 0x7e;
	pad[2] = 0x88;
	pad[3] = 0xa5;

	if(remote_paddr < 0xb0000000 || remote_paddr >= 0xb0000000 + 0x4000000) {
		printk("\nRemote DDR address is invalid");
		goto test_end;
	}

	pcie_dma_wr(local_paddr, remote_paddr, len);
	//printk("\nlocal_paddr=%08x, remote_paddr=%08x", local_paddr, remote_paddr);

	if (pcie_cmp(local_vaddr, remote_vaddr, len))
	{
		printk(" Data miss match\n");
		goto test_end;
	}

	if (pad[0] != 0x5a || pad[1] != 0x7e || pad[2] != 0x88 || pad[3] != 0xa5)
	{
		printk("\nBuffer tail was dirted by PCIe DMA");
	}

test_end:
	spin_unlock_irq(&pcie_dmawr_lock);
}

static int __init pcie_tst_init_module(void)
{
	int i;
	unsigned int * ptr;

	pcie_init(1);

	remote_ddr_vbase = ioremap_nocache((ulong) REMOTE_DDR_BASE, REMOTE_DDR_SIZE);

	if (remote_ddr_vbase)
	{
		memset(remote_ddr_vbase, 0x7a, 0x10000);
		ptr = (unsigned int *) remote_ddr_vbase;
		printk("\n");
		for (i = 0; i < 20; i++)
		{
			printk(" %08x", *ptr++);
			if (i % 8 == 0)
				printk("\n");
		}
	}

	return 0;
}

static void __exit  pcie_tst_cleanup_module(void)
{
	if (remote_ddr_vbase) {
		iounmap(remote_ddr_vbase);
	}
}

module_init(pcie_tst_init_module);
module_exit(pcie_tst_cleanup_module);

