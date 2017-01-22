/*
 *  drivers/net/arasan_emac_ahb.h
 *
 *  Copyright (c) Quantenna Communications Incorporated 2007.
 *  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef __COMMON_RUBY_ARASAN_EMAC_AHB_H
#define __COMMON_RUBY_ARASAN_EMAC_AHB_H	1

#ifdef TOPAZ_AMBER_IP
#include <include/qtn/amber.h>
#endif

extern __inline__ void __mydelay(unsigned long loops)
{
	__asm__ __volatile__ ( "1: \n\t"
			     "sub.f %0, %1, 1\n\t"
			     "jpnz 1b"
			     : "=r" (loops)
			     : "0" (loops));
}

/*
 * Division by multiplication: you don't have to worry about loss of
 * precision.
 *
 * Use only for very small delays ( < 1 msec).  Should probably use a
 * lookup table, really, as the multiplications take much too long with
 * short delays.  This is a "reasonable" implementation, though (and the
 * first constant multiplications gets optimized away if the delay is
 * a constant)
 */
static inline void __const_myudelay(unsigned long xloops)
{
	__asm__ ("mpyhu %0, %1, %2"
		 : "=r" (xloops)
		 : "r" (xloops), "r" (1<<20));	/* Number derived from loops per jiffy */
	__mydelay(xloops * 100);		/* Jiffies per sec */
}

static inline void __myudelay(unsigned long usecs)
{
	__const_myudelay(usecs * 4295);	/* 2**32 / 1000000 */
}
#ifndef MAX_UDELAY_MS
#define MAX_UDELAY_MS	5
#endif

#ifndef mydelay
#define mydelay(n) (\
	(__builtin_constant_p(n) && (n)<=MAX_UDELAY_MS) ? __myudelay((n)*1000) : \
	({unsigned long __ms=(n); while (__ms--) __myudelay(1000);}))
#endif

#define		DELAY_40MILLISEC	(40)
#define		DELAY_50MILLISEC	(50)

/* Arasan Gigabit AHB controller register offsets */
#define EMAC_DMA_CONFIG			0x0000
#define EMAC_DMA_CTRL			0x0004
#define EMAC_DMA_STATUS_IRQ		0x0008
#define EMAC_DMA_INT_ENABLE		0x000C
#define EMAC_DMA_TX_AUTO_POLL		0x0010
#define EMAC_DMA_TX_POLL_DEMAND		0x0014
#define EMAC_DMA_RX_POLL_DEMAND		0x0018
#define EMAC_DMA_TX_BASE_ADDR		0x001C
#define EMAC_DMA_RX_BASE_ADDR		0x0020
#define EMAC_DMA_MISSED_FRAMES		0x0024
#define EMAC_DMA_STOP_FLUSHES		0x0028
#define EMAC_DMA_RX_IRQ_MITIGATION	0x002C
#define EMAC_DMA_CUR_TXDESC_PTR		0x0030
#define EMAC_DMA_CUR_TXBUF_PTR		0x0034
#define EMAC_DMA_CUR_RXDESC_PTR		0x0038
#define EMAC_DMA_CUR_RXBUF_PTR		0x003C

#define EMAC_MAC_GLOBAL_CTRL		0x0100
#define EMAC_MAC_TX_CTRL		0x0104
#define EMAC_MAC_RX_CTRL		0x0108
#define EMAC_MAC_MAX_FRAME_SIZE		0x010C
#define EMAC_MAC_TX_JABBER_SIZE		0x0110
#define EMAC_MAC_RX_JABBER_SIZE		0x0114
#define EMAC_MAC_ADDR_CTRL		0x0118
#define EMAC_MAC_ADDR1_HIGH		0x0120
#define EMAC_MAC_ADDR1_MED		0x0124
#define EMAC_MAC_ADDR1_LOW		0x0128
#define EMAC_MAC_ADDR2_HIGH		0x012C
#define EMAC_MAC_ADDR2_MED		0x0130
#define EMAC_MAC_ADDR2_LOW		0x0134
#define EMAC_MAC_ADDR3_HIGH		0x0138
#define EMAC_MAC_ADDR3_MED		0x013C
#define EMAC_MAC_ADDR3_LOW		0x0140
#define EMAC_MAC_ADDR4_HIGH		0x0144
#define EMAC_MAC_ADDR4_MED		0x0148
#define EMAC_MAC_ADDR4_LOW		0x014C
#define EMAC_MAC_TABLE1			0x0150
#define EMAC_MAC_TABLE2			0x0154
#define EMAC_MAC_TABLE3			0x0158
#define EMAC_MAC_TABLE4			0x015C
#define EMAC_MAC_FLOW_CTRL		0x0160
#define EMAC_MAC_FLOW_PAUSE_GENERATE	0x0164
#define EMAC_MAC_FLOW_SA_HIGH		0x0168
#define EMAC_MAC_FLOW_SA_MED		0x016C
#define EMAC_MAC_FLOW_SA_LOW		0x0170
#define EMAC_MAC_FLOW_DA_HIGH		0x0174
#define EMAC_MAC_FLOW_DA_MED		0x0178
#define EMAC_MAC_FLOW_DA_LOW		0x017C
#define EMAC_MAC_FLOW_PAUSE_TIMEVAL	0x0180
#define EMAC_MAC_MDIO_CTRL		0x01A0
#define EMAC_MAC_MDIO_DATA		0x01A4
#define EMAC_MAC_RXSTAT_CTRL		0x01A8
#define EMAC_MAC_RXSTAT_DATA_HIGH	0x01AC
#define EMAC_MAC_RXSTAT_DATA_LOW	0x01B0
#define EMAC_MAC_TXSTAT_CTRL		0x01B4
#define EMAC_MAC_TXSTAT_DATA_HIGH	0x01B8
#define EMAC_MAC_TXSTAT_DATA_LOW	0x01BC
#define EMAC_MAC_TX_ALMOST_FULL		0x01C0
#define EMAC_MAC_TX_START_THRESHOLD	0x01C4
#define EMAC_MAC_RX_START_THRESHOLD	0x01C8
#define EMAC_MAC_INT			0x01E0
#define EMAC_MAC_INT_ENABLE		0x01E4

#ifndef __ASSEMBLY__

/* Common structure for tx and rx descriptors */
struct emac_desc {
	volatile u32 status;
	volatile u32 control;
	volatile u32 bufaddr1;
	volatile u32 bufaddr2;
};

enum DmaRxDesc {
	/* status field */
	RxDescOwn = (1 << 31),
	RxDescFirstDesc = (1 << 30),
	RxDescLastDesc = (1 << 29),
	RxDescStatusLenErr = (1 << 23),
	RxDescStatusJabberErr = (1 << 22),
	RxDescStatusMaxLenErr = (1 << 21),
	RxDescStatusCRCErr = (1 << 20),
	RxDescStatusRuntFrame = (1 << 15),
	RxDescStatusAlignErr = (1 << 14),
	RxDescStatusShift = 14,
	RxDescStatusMask = 0x7fff,
	RxDescFrameLenShift = 0,
	RxDescFrameLenMask = 0x3fff,
	/* control field */
	RxDescEndOfRing = (1 << 26),
	RxDescChain2ndAddr = (1 << 25),
	RxDescBuf2SizeShift = 12,
	RxDescBuf2SizeMask = 0xfff,
	RxDescBuf1SizeShift = 0,
	RxDescBuf1SizeMask = 0xfff,
};

enum DmaTxDesc {
	/* status field */
	TxDescOwn = (1 << 31),
	TxDescStatusShift = 0,
	TxDescStatusMask = 0x8fffffff,
	/* control field */
	TxDescIntOnComplete = (1 << 31),
	TxDescLastSeg = (1 << 30),
	TxDescFirstSeg = (1 << 29),
	TxDescCrcDisable = (1 << 28),
	TxDescPadDisable = (1 << 27),
	TxDescEndOfRing = (1 << 26),
	TxDescChain2ndAddr = (1 << 25),
	TxDescForceEopErr = (1 << 24),
	TxDescBuf2SizeShift = 12,
	TxDescBuf2SizeMask = 0xfff,
	TxDescBuf1SizeShift = 0,
	TxDescBuf1SizeMask = 0xfff,
};

enum AraMacRegVals {
	/* DMA config register */
	DmaSoftReset = 1,
	Dma1WordBurst = (0x01 << 1),
	Dma4WordBurst = (0x04 << 1),
	Dma16WordBurst = (0x10 << 1),
	DmaRoundRobin = (1 << 15),
	DmaWait4Done = (1 << 16),
	DmaStrictBurst = (1 << 17),
	Dma64BitMode = (1 << 18),
	/* DMA control register */
	DmaStartTx = (1 << 0),
	DmaStartRx = (1 << 1),
	/* DMA status/interrupt & interrupt mask registers */
	DmaTxDone = (1 << 0),
	DmaNoTxDesc = (1 << 1),
	DmaTxStopped = (1 << 2),
	DmaRxDone = (1 << 4),
	DmaNoRxDesc = (1 << 5),
	DmaRxStopped = (1 << 6),
	DmaRxMissedFrame = (1 << 7),
	DmaMacInterrupt = (1 << 8),
	DmaAllInts = DmaTxDone | DmaNoTxDesc | DmaTxStopped | DmaRxDone | 
		DmaNoRxDesc | DmaRxStopped | DmaRxMissedFrame | DmaMacInterrupt,
	DmaTxStateMask = (7 << 16),
	DmaTxStateStopped = (0 << 16),
	DmaTxStateFetchDesc = (1 << 16),
	DmaTxStateFetchData = (2 << 16),
	DmaTxStateWaitEOT = (3 << 16),
	DmaTxStateCloseDesc = (4 << 16),
	DmaTxStateSuspended = (5 << 16),
	DmaRxStateMask = (15 << 21),
	DmaRxStateStopped = (0 << 21),
	DmaRxStateFetchDesc = (1 << 21),
	DmaRxStateWaitEOR = (2 << 21),
	DmaRxStateWaitFrame = (3 << 21),
	DmaRxStateSuspended = (4 << 21),
	DmaRxStateCloseDesc = (5 << 21),
	DmaRxStateFlushBuf = (6 << 21),
	DmaRxStatePutBuf = (7 << 21),
	DmaRxStateWaitStatus = (8 << 21),
	/* MAC global control register */
	MacSpeed10M = (0 << 0),
	MacSpeed100M = (1 << 0),
	MacSpeed1G = (2 << 0),
	MacSpeedMask = (3 << 0),
	MacFullDuplex = (1 << 2),
	MacResetRxStats = (1 << 3),
	MacResetTxStats = (1 << 4),
	/* MAC TX control */
	MacTxEnable = (1 << 0),
	MacTxInvertFCS = (1 << 1),
	MacTxDisableFCSInsertion = (1 << 2),
	MacTxAutoRetry = (1 << 3),
	MacTxIFG96 = (0 << 4),
	MacTxIFG64 = (1 << 4),
	MacTxIFG128 = (2 << 4),
	MacTxIFG256 = (3 << 4),
	MacTxPreamble7 = (0 << 6),
	MacTxPreamble3 = (2 << 6),
	MacTxPreamble5 = (3 << 6),
	/* MAC RX control */
	MacRxEnable = (1 << 0),
	MacRxStripFCS = (1 << 2),
	MacRxStoreAndForward = (1 << 3),
	MacAccountVLANs = (1 << 6),
	/* MAC address control */
	MacAddr1Enable = (1 << 0),
	MacAddr2Enable = (1 << 1),
	MacAddr3Enable = (1 << 2),
	MacAddr4Enable = (1 << 3),
	MacInverseAddr1Enable = (1 << 4),
	MacInverseAddr2Enable = (1 << 5),
	MacInverseAddr3Enable = (1 << 6),
	MacInverseAddr4Enable = (1 << 7),
	MacPromiscuous = (1 << 8),
	/* MAC flow control */
	MacFlowDecodeEnable = (1 << 0),
	MacFlowGenerationEnable = (1 << 1),
	MacAutoFlowGenerationEnable = (1 << 2),
	MacFlowMulticastMode = (1 << 3),
	MacBlockPauseFrames = (1 << 4),
	/* MDIO control register values */
	MacMdioCtrlPhyMask = 0x1f,
	MacMdioCtrlPhyShift = 0,
	MacMdioCtrlRegMask = 0x1f,
	MacMdioCtrlRegShift = 5,
	MacMdioCtrlRead = (1 << 10),
	MacMdioCtrlWrite = 0,
	MacMdioCtrlClkMask = 0x3,
	MacMdioCtrlClkShift = 11,
	MacMdioCtrlStart = (1 << 15),
	/* MDIO data register values */
	MacMdioDataMask = 0xffff,
	/* MAC interrupt & interrupt mask values */
	MacUnderrun = (1 << 0),
	MacJabber = (1 << 0),
	/* RX statistics counter control */
	RxStatReadBusy = (1 << 15),
	/* TX statistics counter control */
	TxStatReadBusy = (1 << 15),
};

enum ArasanTxStatisticsCounters {
	FramesSentOK = 0,
	FramesSentTotal = 1,
	OctetsSentOK = 2,
	FramesSentError = 3,
	FramesSentSingleCol = 4,
	FramesSentMultipleCol = 5,
	FramesSentLateCol = 6,
	FramesSentExcessiveCol = 7,
	FramesSentUnicast = 8,
	FramesSentMulticast = 9,
	FramesSentBroadcast = 10,
	FramesSentPause = 11,
	TxLastStatCounter = 11,
};

enum ArasanRxStatisticsCounters {
	FramesRxOK = 0,
	FramesRxTotal = 1,
	FramesRxCrcErr = 2,
	FramesRxAlignErr = 3,
	FramesRxErrTotal = 4,
	OctetsRxOK = 5,
	OctetsRxTotal = 6,
	FramesRxUnicast = 7,
	FramesRxMulticast = 8,
	FramesRxBroadcast = 9,
	FramesRxPause = 10,
	FramesRxLenErr = 11,
	FramesRxUndersized = 12,
	FramesRxOversized = 13,
	FramesRxFragments = 14,
	FramesRxJabber = 15,
	FramesRx64bytes = 16,
	FramesRx65to127bytes = 17,
	FramesRx128to255bytes = 18,
	FramesRx256to511bytes = 19,
	FramesRx512to1023bytes = 20,
	FramesRx1024to1518bytes = 21,
	FramesRxOver1518bytes = 22,
	FramesRxDroppedBufFull = 23,
	FramesRxTruncatedBufFull = 24,
	RxLastStatCounter = 24,
};

extern int mdc_clk_divisor;
static inline void arasan_initialize_release_reset(uint32_t emac0_cfg,
		uint32_t emac1_cfg, uint32_t rgmii_timing, uint32_t ext_reset)
{
	uint32_t emac_cfg = emac0_cfg | emac1_cfg;
	unsigned long reset_mask;
	uint32_t mii_value = 0x481 | ((mdc_clk_divisor & MacMdioCtrlClkMask) << MacMdioCtrlClkShift);

	if (!(emac_cfg & EMAC_IN_USE)) {
		return;
	}

	/* both interfaces (if enabled) must use same mii config so we can just or here */
	writel(RUBY_SYS_CTL_MASK_MII, RUBY_SYS_CTL_MASK);
	if (emac0_cfg & EMAC_PHY_MII) {
		writel(RUBY_SYS_CTL_MASK_MII_EMAC0, RUBY_SYS_CTL_CTRL);
	}
	if (emac1_cfg & EMAC_PHY_MII) {
		writel(RUBY_SYS_CTL_MASK_MII_EMAC1, RUBY_SYS_CTL_CTRL);
	}
	if (!(emac0_cfg & EMAC_PHY_MII) && !(emac1_cfg & EMAC_PHY_MII)){
		writel(0, RUBY_SYS_CTL_CTRL);
	}
	/* Have PLL clock signal go out */
	writel_topaz(TOPAZ_SYS_CTL_PLLCLKOUT_EN, RUBY_SYS_CTL_MASK);
	writel_topaz(TOPAZ_SYS_CTL_PLLCLKOUT_EN, RUBY_SYS_CTL_CTRL);

	/*
	 * if RGMII mode, we need to configure the clock before we release reset and also
	 * make sure we actually reset the block
	 */
	writel(rgmii_timing, RUBY_SYS_CTL_GMII_CLKDLL);

	/* Release Ethernet busses from reset separately to emacs */
	reset_mask = RUBY_SYS_CTL_RESET_NETSS | RUBY_SYS_CTL_RESET_IOSS;
	writel(reset_mask, RUBY_SYS_CTL_CPU_VEC_MASK);
	writel(reset_mask, RUBY_SYS_CTL_CPU_VEC);

	if (emac1_cfg & EMAC_IN_USE) {
		/*
		 * emac1 only or emac0 + emac1 configurations both require emac0
		 * to be taken out of reset, since both PHYs use a shared mdio bus
		 * starting from emac0
		 */
		reset_mask = RUBY_SYS_CTL_RESET_ENET0 | RUBY_SYS_CTL_RESET_ENET1;
	} else if (emac0_cfg & EMAC_IN_USE) {
		reset_mask = RUBY_SYS_CTL_RESET_ENET0;
	}
#ifdef TOPAZ_AMBER_IP
	amber_bus_flush_req(TOPAZ_AMBER_BUS_FLUSH_RGMII);
#endif
	if (ext_reset) {
		reset_mask |= RUBY_SYS_CTL_RESET_EXT;
	}
	if (reset_mask && (readl(RUBY_SYS_CTL_CPU_VEC) & reset_mask) != reset_mask) {
		writel(reset_mask, RUBY_SYS_CTL_CPU_VEC_MASK);
		if (ext_reset) {
			writel(RUBY_SYS_CTL_RESET_EXT, RUBY_SYS_CTL_CPU_VEC);
			mydelay(DELAY_40MILLISEC);
			reset_mask &= ~RUBY_SYS_CTL_RESET_EXT;
		}
		writel(0, RUBY_SYS_CTL_CPU_VEC);
		mydelay(DELAY_50MILLISEC);
	}
	/* Bring the EMAC out of reset */
	writel(reset_mask, RUBY_SYS_CTL_CPU_VEC_MASK);
	writel(reset_mask, RUBY_SYS_CTL_CPU_VEC);

	writel(0, RUBY_SYS_CTL_MASK);
	writel(0, RUBY_SYS_CTL_CPU_VEC_MASK);
#ifdef TOPAZ_AMBER_IP
	amber_bus_flush_release(TOPAZ_AMBER_BUS_FLUSH_RGMII);
#endif

	/*
	 * Trigger dummy MDIO read to set MDC clock
	 */
	writel(mii_value, RUBY_ENET0_BASE_ADDR + EMAC_MAC_MDIO_CTRL);

	/*
	 * Remove EMAC DMA from soft reset; all other EMAC register
	 * writes result in bus hang if the EMAC is in soft reset
	 */
	if (emac0_cfg & EMAC_IN_USE) {
		writel(0x0, RUBY_ENET0_BASE_ADDR + EMAC_DMA_CONFIG);
		writel(0x0, RUBY_ENET0_BASE_ADDR + EMAC_DMA_CTRL);
	}
	if (emac1_cfg & EMAC_IN_USE) {
		writel(0x0, RUBY_ENET1_BASE_ADDR + EMAC_DMA_CONFIG);
		writel(0x0, RUBY_ENET1_BASE_ADDR + EMAC_DMA_CTRL);
	}

}

#endif /* __ASSEMBLY__ */

#endif
