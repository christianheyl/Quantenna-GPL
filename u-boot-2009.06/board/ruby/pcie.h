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

#define PCIE_REGION_BASE		0x00000000
#define PCIE_REGION_END			0xCFFFFFFF

#define PCIE_CFG_MSI			1
#define PCIE_CFG_BAR64			0

#define PCIE_ROM_MASK_ADDR		0xE9001030

#define	PCIE_BAR_32			0
#define	PCIE_BAR_64			(RUBY_BIT(2))
#define	PCIE_BAR_PREFETCH		(RUBY_BIT(3))
#define	PCIE_BAR_CFG(bar64)		(PCIE_BAR_PREFETCH | ((bar64) ? PCIE_BAR_64 : PCIE_BAR_32))
#define PCIE_BAR_ATU_MIN_SIZE		(0x10000)

#define	PCIE_BAR_SYSCTL			0
#define	PCIE_BAR_SYSCTL_LEN		0xFFFF
#define	PCIE_BAR_SYSCTL_LO		0xE0000000
#define	PCIE_BAR_SYSCTL_HI		0x00000000

#define	PCIE_BAR_SHMEM			2
#define	PCIE_BAR_SHMEM_LEN		(PCIE_BAR_ATU_MIN_SIZE - 1)
#define	PCIE_BAR_SHMEM_LO		(virt_to_bus((void *)RUBY_PCIE_BDA_ADDR))
#define	PCIE_BAR_SHMEM_HI		0x00000000

#define	PCIE_BAR_DMAREG			3
#define	PCIE_BAR_DMAREG_LEN		0xFFFF
#define	PCIE_BAR_DMAREG_LO		0xE9000000
#define	PCIE_BAR_DMAREG_HI		0x00000000

#define PCIE_QUANTENNA_BBIC3		(0x00081bb5)
#define PCIE_BASE_ADDRESS		(0xe9000000)
#define PCIE_CMDSTS			(PCIE_BASE_ADDRESS + 0x04)
#define PCIE_CAP_PTR			(PCIE_BASE_ADDRESS + 0x34)
#define PCIE_GEN2_CTL			(PCIE_BASE_ADDRESS + 0x80C)
#define PCIE_ATU_VIEWPORT		(PCIE_BASE_ADDRESS + 0x900)
#define PCIE_ATU_CTRL1			(PCIE_ATU_VIEWPORT + 0x04)
#define PCIE_ATU_CTRL2			(PCIE_ATU_VIEWPORT + 0x08)
#define PCIE_ATU_BASE_LOW		(PCIE_ATU_VIEWPORT + 0x0C)
#define PCIE_ATU_BASE_HIGH		(PCIE_ATU_VIEWPORT + 0x10)
#define PCIE_ATU_BASE_LIMIT		(PCIE_ATU_VIEWPORT + 0x14)
#define PCIE_ATU_TGT_LOW		(PCIE_ATU_VIEWPORT + 0x18)
#define PCIE_ATU_TGT_HIGH		(PCIE_ATU_VIEWPORT + 0x1C)

#define PCIE_DMA_BASE			(PCIE_BASE_ADDRESS + 0x970)
#define PCIE_DMA_WR_ENABLE		(PCIE_BASE_ADDRESS + 0x97C)
#define PCIE_DMA_WR_DOORBELL		(PCIE_BASE_ADDRESS + 0x980)
#define PCIE_DMA_WR_CHWTLOW		(PCIE_BASE_ADDRESS + 0x988)
#define PCIE_DMA_WR_CHWTHIG		(PCIE_BASE_ADDRESS + 0x98C)
#define PCIE_DMA_RD_ENABLE		(PCIE_BASE_ADDRESS + 0x99C)
#define PCIE_DMA_RD_DOORBELL		(PCIE_BASE_ADDRESS + 0x9A0)
#define PCIE_DMA_RD_CHWTLOW		(PCIE_BASE_ADDRESS + 0x9A8)
#define PCIE_DMA_RD_CHWTHIG		(PCIE_BASE_ADDRESS + 0x9AC)
#define PCIE_DMA_WR_INTSTS		(PCIE_BASE_ADDRESS + 0x9BC)
#define PCIE_DMA_WR_INTMASK		(PCIE_BASE_ADDRESS + 0x9C4)
#define PCIE_DMA_WR_INTCLER		(PCIE_BASE_ADDRESS + 0x9C8)
#define PCIE_DMA_RD_INTSTS		(PCIE_BASE_ADDRESS + 0xA10)
#define PCIE_DMA_RD_INTMASK		(PCIE_BASE_ADDRESS + 0xA18)
#define PCIE_DMA_RD_INTCLER		(PCIE_BASE_ADDRESS + 0xA1C)
#define	PCIE_DMA_RD_INT_ERR_STS_LO	(PCIE_BASE_ADDRESS + 0xA24)
#define	PCIE_DMA_RD_INT_ERR_STS_HI	(PCIE_BASE_ADDRESS + 0xA28)
#define	PCIE_DMA_CHNL_CONTEXT		(PCIE_BASE_ADDRESS + 0xA6C)
#define PCIE_DMA_CHNL_CNTRL		(PCIE_BASE_ADDRESS + 0xA70)
#define PCIE_DMA_XFR_SIZE		(PCIE_BASE_ADDRESS + 0xA78)
#define PCIE_DMA_SAR_LOW		(PCIE_BASE_ADDRESS + 0xA7C)
#define PCIE_DMA_SAR_HIGH		(PCIE_BASE_ADDRESS + 0xA80)
#define PCIE_DMA_DAR_LOW		(PCIE_BASE_ADDRESS + 0xA84)
#define PCIE_DMA_DAR_HIGH		(PCIE_BASE_ADDRESS + 0xA88)
#define PCIE_DMA_LLPTR_LOW		(PCIE_BASE_ADDRESS + 0xA8C)
#define PCIE_DMA_LLPTR_HIGH		(PCIE_BASE_ADDRESS + 0xA90)

#define	PCIE_DMA_RD_DONE_STS(ch)	(1 << (ch))
#define	PCIE_DMA_RD_ABORT_STS(ch)	((PCIE_DMA_RD_DONE_STS(ch)) << 16)
#define	PCIE_DMA_RD_DONE_STS_CLR(ch)	(1 << (ch))
#define	PCIE_DMA_RD_ABORT_STS_CLR(ch)	((PCIE_DMA_RD_DONE_STS_CLR(ch)) << 16)
#define	PCIE_DMA_RD_INT_WRITE_ERR_STS(err, ch)	(((err) & (1 << (ch))) & 0x000000FF)
#define	PCIE_DMA_RD_INT_LLE_ERR_STS(err, ch)	((((err) >> 16) & 0x000000FF) & (1 << (ch)))

#define	PCIE_LINKUP		(RUBY_BIT(8)|RUBY_BIT(9))

#if !TOPAZ_FPGA_PLATFORM
#define PCIE_DEFAULT_CFG0       (0x7fe06410)
#else
#define PCIE_DEFAULT_CFG0       (0xff3c9410)
#endif

/* ATU defines */

#define PCIE_ATU_MEMREGION		0x00000000
#define PCIE_ATU_BAR_MATCH		0xC0000000
#define PCIE_ATU_BAR_MIN_SIZE		0x00010000 /* 64k */


#define	PCIE_ATU_BAR_EN(n)		(((n) << 8) | PCIE_ATU_BAR_MATCH)

/* Shared memory ATU BAR setup */
#define PCIE_SHMEM_REGION		RUBY_PCIE_ATU_IB_REGION(0)
#define PCIE_SHMEM_ENABLE		(PCIE_ATU_BAR_EN(PCIE_BAR_SHMEM))

/* Syscfg ATU BAR setup */
#define PCIE_SYSCTL_REGION		RUBY_PCIE_ATU_IB_REGION(1)
#define PCIE_SYSCTL_ENABLE		(PCIE_ATU_BAR_EN(PCIE_BAR_SYSCTL))

/* PCIe DMA register setup */
#define PCIE_DMAREG_REGION		RUBY_PCIE_ATU_IB_REGION(2)
#define PCIE_DMAREG_ENABLE		(PCIE_ATU_BAR_EN(PCIE_BAR_DMAREG))

/* Size of MSI region */
#define PCIE_MSIMEM_SIZE		PCIE_ATU_BAR_MIN_SIZE
#define PCIE_MSIMEM_SIZE_MASK		(PCIE_ATU_BAR_MIN_SIZE - 1)

/* Size of Shared memory in Host */
#define PCIE_HOSTBD_SIZE                (2 * PCIE_ATU_BAR_MIN_SIZE)
#define PCIE_HOSTBD_SIZE_MASK           (PCIE_HOSTBD_SIZE - 1)

/* Outbound Host Memory translation setup */
#define PCIE_HOSTMEM_EP_START		PCIE_REGION_BASE
#define PCIE_HOSTMEM_EP_END		0xBFFFFFFF
#define PCIE_HOSTMEM_DMA_MASK		(PCIE_HOSTMEM_EP_END - PCIE_HOSTMEM_EP_START)
#define PCIE_HOSTMEM_REGION		(RUBY_PCIE_ATU_OB_REGION(1))
#define PCIE_HOSTMEM_EP_START_LO	PCIE_HOSTMEM_EP_START
#define PCIE_HOSTMEM_EP_START_HI	0x00000000
#define PCIE_HOSTMEM_START_LO		0x00000000
#define PCIE_HOSTMEM_START_HI		0x00000000
#define PCIE_HOSTMEM_REGION_ENABLE	RUBY_PCIE_ATU_OB_ENABLE

#define PCIE_HOSTMEM_ADDR_ALIGN_MASK	(0xffff0000)


/* Outbound MSI ATU setup */
/* MSI region is the upper 64kbytes of the PCIE slave area */

#define PCIE_MSI_BASE			0xE9000050
#define PCIE_MSI_CAP			(PCIE_MSI_BASE + 0x0)
#define PCIE_MSI_LOW_ADDR		(PCIE_MSI_BASE + 0x4)
#define PCIE_MSI_HIG_ADDR		(PCIE_MSI_BASE + 0x8)
#define PCIE_MSI_REGION			(RUBY_PCIE_ATU_OB_REGION(0))
#define PCIE_MSI_REGION_ENABLE		RUBY_PCIE_ATU_OB_ENABLE
#define MSI_EN				RUBY_BIT(0)
#define MSI_64_EN			RUBY_BIT(7)
#define PCIE_MSI_EP_END			PCIE_REGION_END
#define PCIE_MSI_EP_START_LO		(PCIE_REGION_END - PCIE_MSIMEM_SIZE_MASK)
#define PCIE_MSI_EP_START_HI		0x00000000
#define PCIE_MSI_ADDR_OFFSET(a)		((a) & 0xFFFF)
#define PCIE_MSI_ADDR_ALIGN(a)		((a) & (~0xFFFF))

/* Outbound Host buffer descriptor ATU setup */
/* Host BD region is the 128kbytes area just below MSI region */
#define PCIE_HOSTBD_EP_END		(PCIE_REGION_END - PCIE_MSIMEM_SIZE)
#define PCIE_HOSTBD_EP_START_LO		(PCIE_HOSTBD_EP_END - PCIE_HOSTBD_SIZE_MASK)
#define PCIE_HOSTBD_EP_START_HI		0x00000000
#define PCIE_HOSTBD_REGION		(RUBY_PCIE_ATU_OB_REGION(2))

/* Default pcie lzma load addr */
#define PCIE_FW_LZMA_LOAD		QTNBOOT_COPY_DRAM_ADDR

/* pcie Root Complex defines */
#define SYS_RST_PCIE			(0x00002000)
#define SYS_RST_IOSS			(0x00000020)

#if !TOPAZ_FPGA_PLATFORM
#define PCIE_CFG0_DEFAULT_VALUE		(0x7fe06410)
#define PCIE_CFG_RC_MODE		(0x4)
#else
#define PCIE_CFG0_DEFAULT_VALUE		(0xff3c9400)
#define PCIE_CFG_RC_MODE		(0x14)
#endif

#define PCIE_IO_EN			RUBY_BIT(0)
#define PCIE_MEM_EN			RUBY_BIT(1)
#define PCIE_BUS_MASTER_EN		RUBY_BIT(2)
#define BUS_SCAN_TRY_MAX		1000
#define PCIE_DEV_LINKUP_MASK		0x700

void pcie_ep_early_init(uint32_t flags);
void board_pcie_init(size_t memsz, uint32_t flags );

