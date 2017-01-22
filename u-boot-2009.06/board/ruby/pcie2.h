/*
 * Copyright (c) 2010 Quantenna Communications, Inc.
 * All rights reserved.
 *
 *  PCI driver
 */

#ifndef __PCI_H__
#define __PCI_H__
#include <net.h>
#include "pcie.h"

#define PCIE_MSI_CAP_ENABLED                   (BIT(16))

#define PCIE_CONFIG_ID			(0)
#define PCIE_CONFIG_BAR0		(0x10)
#define PCIE_CONFIG_BAR1		(0x14)
#define PCIE_CONFIG_BAR2		(0x18)
#define PCIE_CONFIG_BAR3		(0x1c)
#define PCIE_CONFIG_BAR4		(0x20)
#define PCIE_CONFIG_BAR5		(0x24)

#define PCIE_CMD			(PCIE_BASE_ADDRESS + 0x0004)
#define PCIE_BAR0			(PCIE_BASE_ADDRESS + 0x0010)
#define PCIE_BAR1			(PCIE_BASE_ADDRESS + 0x0014)
#define PCIE_BAR2			(PCIE_BASE_ADDRESS + 0x0018)
#define PCIE_BAR3			(PCIE_BASE_ADDRESS + 0x001c)
#define PCIE_BAR4			(PCIE_BASE_ADDRESS + 0x0020)
#define PCIE_BAR5			(PCIE_BASE_ADDRESS + 0x0024)

#define PCIE_BAR0_MASK			(PCIE_BASE_ADDRESS + 0x1010)
#define PCIE_BAR1_MASK			(PCIE_BASE_ADDRESS + 0x1014)
#define PCIE_BAR2_MASK			(PCIE_BASE_ADDRESS + 0x1018)
#define PCIE_BAR3_MASK			(PCIE_BASE_ADDRESS + 0x101c)
#define PCIE_BAR4_MASK			(PCIE_BASE_ADDRESS + 0x1020)
#define PCIE_BAR5_MASK			(PCIE_BASE_ADDRESS + 0x1024)


#define PCIE_ATU_VIEW			(PCIE_BASE_ADDRESS + 0x0900)
#define PCIE_ATU_VIEW_OUTBOUND		(0)
#define PCIE_ATU_VIEW_INBOUND		(BIT(31))

#define PCIE_ATU_CTL1			(PCIE_BASE_ADDRESS + 0x0904)
#define PCIE_ATU_CTL2			(PCIE_BASE_ADDRESS + 0x0908)
#define PCIE_ATU_LBAR			(PCIE_BASE_ADDRESS + 0x090c)
#define PCIE_ATU_UBAR			(PCIE_BASE_ADDRESS + 0x0910)
#define PCIE_ATU_LAR			(PCIE_BASE_ADDRESS + 0x0914)
#define PCIE_ATU_LTAR			(PCIE_BASE_ADDRESS + 0x0918)
#define PCIE_ATU_UTAR			(PCIE_BASE_ADDRESS + 0x091c)

// MSI registers (RC)
#define PCIE_MSI_ADDR			(PCIE_BASE_ADDRESS + 0x0820)
#define PCIE_MSI_ADDR_UPPER		(PCIE_BASE_ADDRESS + 0x0824)
#define PCIE_MSI_ENABLE			(PCIE_BASE_ADDRESS + 0x0828)
#define PCIE_MSI_MASK			(PCIE_BASE_ADDRESS + 0x082c)
#define PCIE_MSI_STATUS			(PCIE_BASE_ADDRESS + 0x0830)
#define PCIE_MSI_GPIO			(PCIE_BASE_ADDRESS + 0x0888)
#define	PCIE_MSI_LBAR			(PCIE_BASE_ADDRESS + 0x0054)

#define USE_BAR_MATCH_MODE
#define PCIE_ATU_OB_REGION		(BIT(0))
#define PCIE_ATU_EN_REGION		(BIT(31))
#define PCIE_ATU_EN_MATCH		(BIT(30))
#define PCIE_BASE_REGION		(0xb0000000)
#define PCIE_MEM_MAP_SIZE		(512*1024)

#define PCIE_OB_REG_REGION	(0xcf000000)
#define PCIE_CONFIG_REGION	(0xcf000000)
#define PCIE_CONFIG_SIZE	(4096)
#define PCIE_CONFIG_CH		(1)

// inbound mapping
#define PCIE_IB_BAR0		(0x00000000)	// ddr
#define PCIE_IB_BAR0_CH		(0)
#define PCIE_IB_BAR3		(0xe0000000)	// sys_reg
#define PCIE_IB_BAR3_CH		(1)


// outbound mapping
#define PCIE_MEM_CH		(0)
#define PCIE_REG_CH		(1)
#define PCIE_MEM_REGION		(0xc0000000)
#define	PCIE_MEM_SIZE		(0x000fffff)
#if 1
#define PCIE_MEM_TAR		(0x80000000)
#else
#define PCIE_MEM_TAR		(0x00000000)
#endif


#define PCIE_MSI_SIZE		(KBYTE(4)-1)
#define PCIE_MSI_CH		(1)


#define DMA_FLAG_LOOP		(BIT(31))
#define DMA_FLAG_SIZE(x)	((x) & 0xf)
#define DMA_FLAG_CHANNEL(x)	(((x) >> 4) & 0xf)


///////////////////////////////////////////////////////////////////////////////
//             Defines
///////////////////////////////////////////////////////////////////////////////
#define DMA_BASE_ADDR RUBY_DMA_BASE_ADDR
#define DMA_NUM_CHANNELS		(4)
#define	DMA_SAR(x)			(DMA_BASE_ADDR + 0x00 + (x)*0x58)
#define	DMA_DAR(x)			(DMA_BASE_ADDR + 0x08 + (x)*0x58)
#define	DMA_LLP(x)			(DMA_BASE_ADDR + 0x10 + (x)*0x58)
#define	DMA_CTL(x)			(DMA_BASE_ADDR + 0x18 + (x)*0x58)
#define	DMA_SIZE(x)			(DMA_BASE_ADDR + 0x1c + (x)*0x58)
#define	DMA_SSTAT(x)			(DMA_BASE_ADDR + 0x20 + (x)*0x58)
#define	DMA_DSTAT(x)			(DMA_BASE_ADDR + 0x28 + (x)*0x58)
#define	DMA_SSTATAR(x)			(DMA_BASE_ADDR + 0x30 + (x)*0x58)
#define	DMA_DSTATAR(x)			(DMA_BASE_ADDR + 0x38 + (x)*0x58)
#define	DMA_CFG(x)			(DMA_BASE_ADDR + 0x40 + (x)*0x58)
#define	DMA_SGR(x)			(DMA_BASE_ADDR + 0x48 + (x)*0x58)
#define	DMA_DSR(x)			(DMA_BASE_ADDR + 0x50 + (x)*0x58)

#define	DMA_STATUS			(DMA_BASE_ADDR + 0x360)
#define DMA_STATUS_ERR			(BIT(4))
#define DMA_STATUS_DSTT			(BIT(3))
#define DMA_STATUS_SRCT			(BIT(2))
#define DMA_STATUS_BLOCK		(BIT(1))
#define DMA_STATUS_TFR			(BIT(0))

#define DMA_RAW_TFR			(DMA_BASE_ADDR + 0x2c0)
#define DMA_RAW_BLK			(DMA_BASE_ADDR + 0x2c8)
#define DMA_RAW_SRC			(DMA_BASE_ADDR + 0x2d0)
#define DMA_RAW_DST			(DMA_BASE_ADDR + 0x2d8)
#define DMA_RAW_ERR			(DMA_BASE_ADDR + 0x2e0)
#define DMA_STS_TFR			(DMA_BASE_ADDR + 0x2e8)
#define DMA_STS_BLK			(DMA_BASE_ADDR + 0x2f0)
#define DMA_STS_SRC			(DMA_BASE_ADDR + 0x2f8)
#define DMA_STS_DST			(DMA_BASE_ADDR + 0x300)
#define DMA_STS_ERR			(DMA_BASE_ADDR + 0x308)
#define DMA_MSK_TFR			(DMA_BASE_ADDR + 0x310)
#define DMA_MSK_BLK			(DMA_BASE_ADDR + 0x318)
#define DMA_MSK_SRC			(DMA_BASE_ADDR + 0x320)
#define DMA_MSK_DST			(DMA_BASE_ADDR + 0x328)
#define DMA_MSK_ERR			(DMA_BASE_ADDR + 0x330)
#define DMA_TFR_CLR			(DMA_BASE_ADDR + 0x338)
#define DMA_BLK_CLR			(DMA_BASE_ADDR + 0x340)
#define DMA_SRC_CLR			(DMA_BASE_ADDR + 0x348)
#define DMA_DST_CLR			(DMA_BASE_ADDR + 0x350)
#define DMA_ERR_CLR			(DMA_BASE_ADDR + 0x358)
#define DMA_STS_INT			(DMA_BASE_ADDR + 0x360)
#define DMA_REQ_SRC			(DMA_BASE_ADDR + 0x368)
#define DMA_REQ_DST			(DMA_BASE_ADDR + 0x370)
#define DMA_SGL_REQ_SRC			(DMA_BASE_ADDR + 0x378)
#define DMA_SGL_REQ_DST			(DMA_BASE_ADDR + 0x380)
#define DMA_LST_SRC			(DMA_BASE_ADDR + 0x388)
#define DMA_LST_DST			(DMA_BASE_ADDR + 0x390)
#define DMA_DMA_CFG			(DMA_BASE_ADDR + 0x398)
#define DMA_CH_EN			(DMA_BASE_ADDR + 0x3a0)
#define DMA_ID				(DMA_BASE_ADDR + 0x3a8)
#define DMA_TEST			(DMA_BASE_ADDR + 0x3b0)

#define DMA_CFG_ENABLE			(BIT(0))
#define DMA_CH_EN_WEN			(0xf<<8)
#define DMA_CTL_LLP_SRC_EN		(BIT(28))
#define DMA_CTL_LLP_DST_EN		(BIT(27))
#define DMA_CTL_SRC_M1			(0)
#define DMA_CTL_SRC_M2			(BIT(25))
#define DMA_CTL_DST_M1			(0)
#define DMA_CTL_DST_M2			(BIT(23))
#define DMA_CTL_MEM2MEM			(0)
#define DMA_CTL_DST_SCATTER		(BIT(18))
#define DMA_CTL_SRC_SCATTER		(BIT(17))

#define DMA_CTL_SRC_BURST4		(0)
#define DMA_CTL_SRC_BURST8		(1<<14)
#define DMA_CTL_SRC_BURST16		(2<<14)
#define DMA_CTL_SRC_BURST32		(3<<14)
#define DMA_CTL_SRC_BURST64		(4<<14)

#define DMA_CTL_DST_BURST4		(0)
#define DMA_CTL_DST_BURST8		(1<<11)
#define DMA_CTL_DST_BURST16		(2<<11)
#define DMA_CTL_DST_BURST32		(3<<11)
#define DMA_CTL_DST_BURST64		(4<<11)

#define DMA_CTL_SRC_WIDTH8		(0<<4)
#define DMA_CTL_SRC_WIDTH16		(1<<4)
#define DMA_CTL_SRC_WIDTH32		(2<<4)
#define DMA_CTL_SRC_WIDTH64		(3<<4)
#define DMA_CTL_DST_WIDTH8		(0<<1)
#define DMA_CTL_DST_WIDTH16		(1<<1)
#define DMA_CTL_DST_WIDTH32		(2<<1)
#define DMA_CTL_DST_WIDTH64		(3<<1)
#define DMA_INT_EN			(BIT(0))

#define DMA_CTL_SRC_SCATTER		(BIT(17))

#endif // __PCI_H__

