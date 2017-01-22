/*
 *  board/ums/arasan-emac-ahb.c
 *
 *  U-Boot driver for the Arasan EMAC-AHB Gigabit Ethernet controller.
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

#include <common.h>
#include <malloc.h>
#include <net.h>
#include <asm/io.h>
#include <command.h>

#ifdef CONFIG_ARASAN_GBE

#include "ruby.h"
#include <asm/arch/platform.h>
#include "ar8236.h"
#include "ar8237.h"
#include "rtl8367b/rtl8367b_init.h"
#include "ruby_board_cfg.h"
#include "board_cfg.h"
#include <asm/arch/arasan_emac_ahb.h>

#ifdef UBOOT_ENABLE_ETHERNET_DEBUG
#define DPRINTF(x...) printf(x)
#else
#define DPRINTF(x...)
#endif


#define RTL8211DS_PAGSEL	0x1F	/* Page mode slelect */
#define PAGSEL_PAGEXT		0x7		/* Switch to extension page */
#define PAGSEL_PAG0			0x0     /* Switch to page 0 */

#define RTL8211DS_PAGNO		0x1E	/* Page number */
#define PAGNO_140			0x8c	/* Page 140 */

#define RTL8211DS_RXCONFIG	0x18	/* Tx config ( phy -> mac) */
#define RXCONFIG_LINK		0x8000
#define RXCONFIG_DUPLEX		0x1000
#define RXCONFIG_SPEED		0x0c00
#define RXCONFIG_SPEED_BIT	10

#define RTL8211DS_SDSR		0x16	/* SerDes register */
#define SDSR_SPEED			0x3000	/* Rx speed */
#define SDSR_1000			0x2000
#define SDSR_100			0x1000
#define SDSR_10				0x0000


#define MAC_IS_MULTICAST(addr) ((addr)[0] & 0x1)

#if (defined(EMAC_SWITCH)) && ((defined(EMAC_BOND_MASTER)))
#error EMAC_SWITCH and EMAC_BOND_MASTER can not be defined the same time
#endif
int mdc_clk_divisor = 1;
static int emac_phy_init(struct emac_private *priv);


static inline unsigned long emac_align_begin(unsigned long addr)
{
	return (addr & (~(ARC_DCACHE_LINE_LEN - 1)));
}

static inline unsigned long emac_align_end(unsigned long addr)
{
	unsigned long aligned = (addr & (~(ARC_DCACHE_LINE_LEN - 1)));
	if (aligned != addr) {
		aligned += ARC_DCACHE_LINE_LEN;
	}
	return aligned;
}

static inline void emac_cache_inv(unsigned long begin, unsigned long end)
{
	invalidate_dcache_range(
		emac_align_begin(begin),
		emac_align_end(end));
}

static inline void emac_cache_flush(unsigned long begin, unsigned long end)
{
	flush_and_inv_dcache_range( /* flush _and_ invalidate to free few cache entries as we are not going to read them back */
		emac_align_begin(begin),
		emac_align_end(end));
}


/* 8 byte alignment required as DMA is operating in 64 bit mode.
   ARC_DCACHE_LINE_LEN alignment used to have safe cache invalidate/flush.
   As ARC_DCACHE_LINE_LEN is 32, both requirements are satisfied.
*/
static volatile struct emac_desc tx_ring_0[1] __attribute__ ((aligned (ARC_DCACHE_LINE_LEN)));
static volatile struct emac_desc rx_ring_0[NUM_RX_BUFS] __attribute__ ((aligned (ARC_DCACHE_LINE_LEN)));
static volatile u8 tx_buf_0[TX_BUF_SIZE] __attribute__ ((aligned (ARC_DCACHE_LINE_LEN)));
static volatile u8 rx_bufs_0[NUM_RX_BUFS * RX_BUF_SIZE] __attribute__ ((aligned (ARC_DCACHE_LINE_LEN)));

static volatile struct emac_desc tx_ring_1[1] __attribute__ ((aligned (ARC_DCACHE_LINE_LEN))) ;
static volatile struct emac_desc rx_ring_1[NUM_RX_BUFS] __attribute__ ((aligned (ARC_DCACHE_LINE_LEN)));
static volatile u8 tx_buf_1[TX_BUF_SIZE] __attribute__ ((aligned (ARC_DCACHE_LINE_LEN)));
static volatile u8 rx_bufs_1[NUM_RX_BUFS * RX_BUF_SIZE] __attribute__ ((aligned (ARC_DCACHE_LINE_LEN)));

static struct br_private __g_br_priv = {{0}};
static struct eth_device __g_br_dev;
static uint32_t emac_mdio_read(struct emac_private *priv, uint8_t reg);
static int emac_mdio_write(struct emac_private *priv, uint8_t reg, uint16_t val);
static struct emac_private_mdio __g_emac_mdio[2] = {
	{
		.base = ARASAN_MDIO_BASE,
		.read = &emac_mdio_read,
		.write = &emac_mdio_write,
	},
	{
		.base = ARASAN_MDIO_BASE,
		.read = &emac_mdio_read,
		.write = &emac_mdio_write,
	}
};

#undef ENABLE_LOOPBACK
#ifdef ENABLE_LOOPBACK
static uint32_t loopback = 0 ;
#endif

static struct emac_private __g_emac_priv[2] = {
	{
		.tx_bufs = (uintptr_t) &tx_buf_0[0],
		.rx_bufs = (uintptr_t) &rx_bufs_0[0],
		.tx_descs = &tx_ring_0[0],
		.rx_descs = &rx_ring_0[0],
		.tx_buf_size = TX_BUF_SIZE,
		.rx_buf_size = RX_BUF_SIZE,
		.tx_index = 0,
		.rx_index = 0,
		.ar8236dev = NULL,
		.phy_addr = 0,
		.phy_flags = 0,
		.emac = 0,
		.io_base= RUBY_ENET0_BASE_ADDR,
		.mdio = &__g_emac_mdio[0],
		.parent = &__g_br_dev,
	},
	{
		.tx_bufs = (uintptr_t) &tx_buf_1[0],
		.rx_bufs = (uintptr_t) &rx_bufs_1[0],
		.tx_descs = &tx_ring_1[0],
		.rx_descs = &rx_ring_1[0],
		.tx_buf_size = TX_BUF_SIZE,
		.rx_buf_size = RX_BUF_SIZE,
		.tx_index = 0,
		.rx_index = 0,
		.ar8236dev = NULL,
		.phy_addr = 0,
		.phy_flags = 0,
		.emac = 1,
		.io_base = RUBY_ENET1_BASE_ADDR,
		.mdio = &__g_emac_mdio[1],
		.parent = &__g_br_dev,
	}
};

/* Utility functions for reading/writing registers in the Ethernet MAC */

static uint32_t emac_rdreg(uintptr_t base, int reg)
{
	return readl(base + reg);
}

static void emac_wrreg(uintptr_t base, int reg, uint32_t val)
{
	writel(val, base + reg);
}

static void emac_setbits(uintptr_t base, int reg, uint32_t val)
{
	emac_wrreg(base, reg, emac_rdreg(base, reg) | val);
}

static void emac_clrbits(uintptr_t base, int reg, uint32_t val)
{
	emac_wrreg(base, reg, emac_rdreg(base, reg) & ~val);
}

/* Utility functions for driving the MDIO interface to the PHY from the MAC */

static int mdio_operation_complete(uintptr_t base)
{
	return !(emac_rdreg(base, EMAC_MAC_MDIO_CTRL) & 0x8000);
}

int mdio_poll_complete(uintptr_t base)
{
	int i = 0;

	while (!mdio_operation_complete(base)) {
		udelay(1000);
		if (++i >= 2000) {
			printf("mdio timeout\n");
			return -1;
		}
	}

	return 0;
}

void mdio_postrd_raw(uintptr_t base, int phy, int reg)
{
	if (mdio_poll_complete(base)) {
		return;
	}

	emac_wrreg(base, EMAC_MAC_MDIO_CTRL, (phy & 31) |
			((mdc_clk_divisor & MacMdioCtrlClkMask) << MacMdioCtrlClkShift) |
			((reg & 31) << 5) | (1 << 10) | (1 << 15));
}

int mdio_postwr_raw(uintptr_t base, int phy, int reg, u16 val)
{
	if (mdio_poll_complete(base)) {
		return -1;
	}
	emac_wrreg(base, EMAC_MAC_MDIO_DATA, val);
	emac_wrreg(base, EMAC_MAC_MDIO_CTRL, (phy & 31) |
			((mdc_clk_divisor & MacMdioCtrlClkMask) << MacMdioCtrlClkShift) |
			((reg & 31) << 5) | (1 << 15));
	return 0;
}

/* Check to see if an MDIO posted read command (mdio_postrd) has completed.
 * Returns the u16 data from the read, or MDIO_READ_FAIL if the data is not available.
 * Setting "wait" to TRUE makes the command poll until the data is available
 * or the command times-out.  Setting "wait" to FALSE stops the command
 * from polling; it only checks if the data is available once.
 */
uint32_t mdio_rdval_raw(uintptr_t base, int wait)
{
	if (wait) {
		mdio_poll_complete(base);
	}
	if (!mdio_operation_complete(base)) {
		printf("GMII: MDIO read timed out (%08x)\n",
				emac_rdreg(base, EMAC_MAC_MDIO_CTRL));
		return MDIO_READ_FAIL;
	}
	return (uint32_t)emac_rdreg(base, EMAC_MAC_MDIO_DATA);
}

static uint32_t emac_mdio_read(struct emac_private *priv, uint8_t reg)
{
	uint32_t val;

	mdio_postrd_raw(priv->mdio->base, priv->phy_addr, reg);
	if ((val = mdio_rdval_raw(priv->mdio->base, 1)) == MDIO_READ_FAIL) {
		return MDIO_READ_FAIL;
	}

	return val;
}

static int emac_mdio_write(struct emac_private *priv, uint8_t reg, uint16_t val)
{
	return mdio_postwr_raw(priv->mdio->base, priv->phy_addr, reg, val);
}

/* Taken from Linux MII support.  Return the link speed based on
 * the IEEE register values from the Ethernet PHY.
 */
static inline unsigned int mii_nway_result(unsigned int negotiated)
{
	unsigned int ret;

	if (negotiated & LPA_100FULL)
		ret = LPA_100FULL;
	else if (negotiated & LPA_100BASE4)
		ret = LPA_100BASE4;
	else if (negotiated & LPA_100HALF)
		ret = LPA_100HALF;
	else if (negotiated & LPA_10FULL)
		ret = LPA_10FULL;
	else
		ret = LPA_10HALF;

	return ret;
}

static void emac_stop_dma(uint32_t base)
{
	int i = 0;
	uint32_t val;

	/* Attempt to stop the DMA tx and rx in an orderly fashion */
	emac_clrbits(base, EMAC_DMA_CTRL, DmaStartTx | DmaStartRx);
	do {
		udelay(10000);
		val = emac_rdreg(base, EMAC_DMA_STATUS_IRQ);
		if (++i >= 100) {
			printf("GMII: Failed to stop DMA\n");
			break;
		}
	} while (val & (DmaTxStateMask | DmaRxStateMask));
}



static void emac_reset_dma(uint32_t base)
{
	emac_stop_dma(base);

	/* Don't read any registers whilst the block is reset (it hangs) */
	emac_wrreg(base, EMAC_DMA_CONFIG, DmaSoftReset);
	emac_wrreg(base, EMAC_DMA_CONFIG, 0);
}

#ifndef RUBY_MINI
static int rx_stats[] = { 0, 1, 2, 3, 4, 5, 6 };
#define NUM_RX_STATS (ARRAY_SIZE(rx_stats))

static int tx_stats[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
#define NUM_TX_STATS (ARRAY_SIZE(tx_stats))

static void emac_stats(struct emac_private * priv)
{
	int i;
	uint32_t val;
	int base = priv->io_base;
	int counter;
	unsigned int n;

	for (n = 0; n < NUM_RX_STATS; n++) {
		counter = rx_stats[n];
		emac_wrreg(base, EMAC_MAC_RXSTAT_CTRL, 0);
		emac_wrreg(base, EMAC_MAC_RXSTAT_CTRL, RxStatReadBusy | counter);
		i = 0;
		do {
			if (++i > 10000) {
				printf("GMII: Rx Stat timeout for %d\n", counter);
				return;
			}
		} while ((val = emac_rdreg(base, EMAC_MAC_RXSTAT_CTRL)) & RxStatReadBusy);
		val = emac_rdreg(base, EMAC_MAC_RXSTAT_DATA_HIGH) << 16;
		val |= (emac_rdreg(base, EMAC_MAC_RXSTAT_DATA_LOW) & 0xffff);
		printf("%d: Rx Stat %d = 0x%08x\n", n, rx_stats[n], val);
	}

	for (n = 0; n < NUM_TX_STATS; n++) {
		counter = tx_stats[n];
		emac_wrreg(base, EMAC_MAC_TXSTAT_CTRL, 0);
		emac_wrreg(base, EMAC_MAC_TXSTAT_CTRL, TxStatReadBusy | counter);
		i = 0;
		do {
			if (++i > 10000) {
				printf("GMII: Tx Stat timeout for %d 0x%08x\n", counter, val);
				return;
			}
		} while ((val = emac_rdreg(base, EMAC_MAC_TXSTAT_CTRL)) & TxStatReadBusy);
		val = emac_rdreg(base, EMAC_MAC_TXSTAT_DATA_HIGH) << 16;
		val |= (emac_rdreg(base, EMAC_MAC_TXSTAT_DATA_LOW) & 0xffff);
		printf("%d: Tx Stat %d = 0x%08x\n", n, tx_stats[n], val);
	}
}

static void emacs_stats(void)
{
	struct br_private * br_priv = &__g_br_priv;
	int i;

	for (i = 0; i < br_priv->nemacs; i++) {
		struct emac_private * emac_priv = br_priv->emacs[i];
		if (emac_priv) {
			emac_stats(emac_priv);
		}
	}
}
#endif


static int emac_reset(struct emac_private * priv)
{
	unsigned int i;
	uintptr_t base, mdio_base;
	uint16_t macaddr16[3];
	uint16_t rx_desc_size = 0;
	uint16_t tx_ring_size = 0;
	uint16_t rx_ring_size = 0;
	struct eth_device * br = priv->parent;

	base = priv->io_base;
	mdio_base = priv->mdio->base;
	memcpy(macaddr16, br->enetaddr, sizeof(macaddr16));
	emac_reset_dma(base);

	/* Initialise the buffer descriptors for Tx */
	priv->tx_descs[0].status = 0;
	priv->tx_descs[0].control = TxDescEndOfRing;
	priv->tx_descs[0].bufaddr1 = virt_to_bus((void *)priv->tx_bufs);
	priv->tx_descs[0].bufaddr2 = 0;

	if (priv->emac){
		rx_desc_size = ARRAY_SIZE(rx_ring_1);
	} else {
		rx_desc_size = ARRAY_SIZE(rx_ring_0);
	}
	/* Initialise the buffer descriptors for Rx */
	for (i = 0; i < rx_desc_size; i++) {
		priv->rx_descs[i].status = RxDescOwn;
		priv->rx_descs[i].control = (RX_BUF_SIZE & RxDescBuf1SizeMask)
			<< RxDescBuf1SizeShift;
		priv->rx_descs[i].bufaddr1 = virt_to_bus((void *)((int)priv->rx_bufs + (i * RX_BUF_SIZE)));
		priv->rx_descs[i].bufaddr2 = 0;
	}
	if (i > 0) {
		priv->rx_descs[i - 1].control |= RxDescEndOfRing;
	}

	priv->rx_index = 0;
	priv->tx_index = 0;

	/* We assume that all registers are in their default states from
	 * reset, so we only update those ones that are necessary.
	 */
	// !!! FIXME_UMS - do we need Robin+Wait flags here?  | DmaWait4Done
	emac_wrreg(base, EMAC_DMA_CONFIG, Dma16WordBurst | Dma64BitMode |
			DmaRoundRobin );
	emac_wrreg(base, EMAC_DMA_TX_AUTO_POLL, 16);
	emac_wrreg(base, EMAC_DMA_TX_BASE_ADDR, virt_to_bus((void *)(uintptr_t) priv->tx_descs));
	emac_wrreg(base, EMAC_DMA_RX_BASE_ADDR, virt_to_bus((void *)(uintptr_t) priv->rx_descs));
	emac_rdreg(base, EMAC_DMA_MISSED_FRAMES);
	emac_rdreg(base, EMAC_DMA_STOP_FLUSHES);
#ifdef EMAC_SWITCH
	emac_wrreg(base, EMAC_MAC_ADDR_CTRL, MacAddr1Enable | MacPromiscuous);
#else
	emac_wrreg(base, EMAC_MAC_ADDR_CTRL, MacAddr1Enable);
#endif
	emac_wrreg(base, EMAC_MAC_ADDR1_HIGH, macaddr16[0]);
	emac_wrreg(base, EMAC_MAC_FLOW_SA_HIGH, macaddr16[0]);
	emac_wrreg(base, EMAC_MAC_ADDR1_MED, macaddr16[1]);
	emac_wrreg(base, EMAC_MAC_FLOW_SA_MED, macaddr16[1]);
	emac_wrreg(base, EMAC_MAC_ADDR1_LOW, macaddr16[2]);
	emac_wrreg(base, EMAC_MAC_FLOW_SA_LOW, macaddr16[2]);

	/* !!! FIXME_UMS - whether or not we need this depends on whether
	 * the auto-pause generation uses it.  The auto function may just
	 * use 0xffff val to stop sending & then 0 to restart it.
	 */
	//!!!FIXME_UMS emac_wrreg(base, EMAC_MAC_FLOW_PAUSE_TIMEVAL, 0xffff);
	emac_wrreg(base, EMAC_MAC_FLOW_PAUSE_TIMEVAL, 0);

	/* Required by the datasheet */
	// emac_wrreg(base, EMAC_MAC_TX_ALMOST_FULL, 0x8);
	emac_wrreg(base, EMAC_MAC_TX_ALMOST_FULL, 0x1f8);

	/* Use safe store & forward value - valid for all speeds */
	emac_wrreg(base, EMAC_MAC_TX_START_THRESHOLD, 1518);

	emac_wrreg(base, EMAC_MAC_FLOW_CTRL, MacFlowDecodeEnable |
			MacFlowGenerationEnable | MacAutoFlowGenerationEnable |
			MacFlowMulticastMode | MacBlockPauseFrames);

	if (priv->emac){
		tx_ring_size = sizeof(tx_ring_1);
		rx_ring_size = sizeof(rx_ring_1);
	} else {
		tx_ring_size = sizeof(tx_ring_0);
		rx_ring_size = sizeof(rx_ring_0);
	}

	emac_cache_flush((uintptr_t) priv->tx_descs, (uintptr_t) priv->tx_descs + tx_ring_size);
	emac_cache_flush((uintptr_t) priv->rx_descs, (uintptr_t) priv->rx_descs + rx_ring_size);

	/* Setup speed and run! */
	if (!emac_phy_init(priv)) {
		return 0;
	}

	/* Setup speed and run! */
	emac_wrreg(base, EMAC_MAC_TX_CTRL, MacTxEnable | MacTxAutoRetry);
	emac_wrreg(base, EMAC_MAC_RX_CTRL, MacRxEnable | MacRxStoreAndForward);
	emac_setbits(base, EMAC_DMA_CTRL, DmaStartTx | DmaStartRx);

	return 1;
}

static void emac_halt(struct emac_private *priv)
{
	int base;

	if (priv) {
		base = priv->io_base;
		emac_clrbits(base, EMAC_MAC_TX_CTRL, MacTxEnable);
		emac_clrbits(base, EMAC_MAC_RX_CTRL, MacRxEnable);
		emac_stop_dma(base);
	}
}

static int emac_send(struct emac_private * priv, volatile void *packet, int length)
{
	volatile struct emac_desc *ptx_desc;
	uint32_t base, now;

	base = priv->io_base;

	ptx_desc = priv->tx_descs;

	if (readl(&ptx_desc->status) & TxDescOwn) {
		printf("No buffers available\n");
		return 0;
	}

	/* This is a simplification - only handle packets <= 2kB */
	if (length > priv->tx_buf_size) {
		printf("Packet too big\n");
		return 0;
	}

	/* copy packet */
	memcpy((void *)bus_to_virt(ptx_desc->bufaddr1), (void *)packet, length);
	emac_cache_flush((ulong)bus_to_virt(ptx_desc->bufaddr1),
			(ulong)bus_to_virt(ptx_desc->bufaddr1) + length);

	/* do tx */
	writel(TxDescFirstSeg | TxDescLastSeg | TxDescEndOfRing | (length & TxDescBuf1SizeMask) << TxDescBuf1SizeShift,
			&ptx_desc->control);
	writel(TxDescOwn, &ptx_desc->status); /* Hand-off frame to Ethernet DMA engines.. */
	emac_wrreg(base, EMAC_DMA_TX_POLL_DEMAND, 1); /* restart TX */

	/* ..and wait for it to go */
	now = get_timer(0);
	while(readl(&ptx_desc->status) & TxDescOwn) {
		if (get_timer(now) > TX_TIMEOUT_IN_TICKS) {
			printf("Transmit timeout\n");
			writel(0, &ptx_desc->status);
			return 0;
		}
	}

	return length;
}



#ifdef EMAC_SWITCH
extern int memcmp(const void *, const void *, __kernel_size_t);

static int emac_forward(struct emac_private * priv, volatile unsigned char *packet, int length)
{
	int ret;
	struct eth_device * br = priv->parent;
	struct br_private * br_priv = br->priv;
	if ((ret=memcmp((const void *)packet, br->enetaddr, 6))) {
		int i;
		if (MAC_IS_MULTICAST(packet))
			ret = 0;
		for (i = 0; i < br_priv->nemacs; i++){
			struct emac_private * emac_priv = br_priv->emacs[i];
			if ((emac_priv) && (priv->emac!=emac_priv->emac) && br_priv->dev_state & (1<<i)) {
				emac_send(emac_priv, packet, length);
			}
		}
	}
	return ret;
}
#endif

static int emac_recv(struct emac_private *priv)
{
	volatile struct emac_desc *prx_desc;
	uint32_t base, length;
	volatile uint32_t status;

	if (priv == NULL)
		return 0;

	base = priv->io_base;
	prx_desc = priv->rx_descs + priv->rx_index;
	status = readl(&prx_desc->status);
	if (status & RxDescOwn) {
		return 0;
	}


	length = (status >> RxDescFrameLenShift) & RxDescFrameLenMask;
	if (length >= RX_BUF_SIZE) {
		board_reset("\n\n***** reset: emac_recv: oversize packet\n\n");
	}
	if (length > 0) {
		emac_cache_inv((ulong)bus_to_virt(prx_desc->bufaddr1),
			(ulong)bus_to_virt(prx_desc->bufaddr1) + length);
#ifdef ENABLE_LOOPBACK
		if (loopback) {
			//printf("echo back %d\n",length);
			emac_send(priv, ARC_SRAM_ADDRESS(prx_desc->bufaddr1), length);
		} else {
			NetReceive((unsigned char *)bus_to_virt(prx_desc->bufaddr1), length);
		}
#else
#ifdef EMAC_SWITCH
		if (emac_forward(priv, bus_to_virt(prx_desc->bufaddr1), length)==0)
			NetReceive((unsigned char *)bus_to_virt(prx_desc->bufaddr1), length);
#else
		NetReceive((unsigned char *)bus_to_virt(prx_desc->bufaddr1), length);
#endif
#endif
	}

	priv->rx_index++;
	if (priv->rx_index == NUM_RX_BUFS) {
		priv->rx_index = 0;
	}
	/* Give descriptor back to the DMA engines */
	writel(RxDescOwn, &prx_desc->status);
	emac_wrreg(base, EMAC_DMA_RX_POLL_DEMAND, 1);

	return 1;
}

#define PHY_CHECK_RETRY 10
static int br_init(struct eth_device *dev, bd_t *bd)
{
	struct br_private *const priv = dev->priv;
	int i, j;

	priv->dev_state = 0;
	priv->send_mask = 0;
	for (j = 0; j < PHY_CHECK_RETRY; j++) {
		for (i = 0; i < priv->nemacs; i++) {
			if (emac_reset(priv->emacs[i])) {
				priv->dev_state |= (1 << i);
				priv->send_mask |= (1 << i);
			}
			if (had_ctrlc())
				goto out;
		}
		if (priv->dev_state)
			break;
	}
out:
#ifdef EMAC_BOND_MASTER
#if (EMAC_BOND_MASTER == 0)
	priv->send_mask &= 1;
#elif (EMAC_BOND_MASTER == 1)
	priv->send_mask &= 2;
#endif
#endif
	return !!priv->dev_state;
}

static void br_halt(struct eth_device *dev)
{
	struct br_private *const priv = dev->priv;
	int i;

	for (i = 0; i < priv->nemacs; i++) {
		emac_halt(priv->emacs[i]);
	}
	priv->dev_state = 0;
	priv->send_mask = 0;
}

static int br_send(struct eth_device *dev, volatile void *packet, int length)
{
	struct br_private *const priv = dev->priv;
	int i;
	int ret = 0;

	for (i = 0; i < priv->nemacs; i++) {
		if ((priv->dev_state & (1 << i)) && (priv->send_mask & (1 << i))) {
			int rc;
			struct emac_private *emac_priv = priv->emacs[i];
			if (emac_priv) {
				rc = emac_send(emac_priv, (void *) packet, length);
				if (rc) {
					ret = rc;
				}
			}
		}
	}

	return ret;
}

static int br_recv(struct eth_device *dev)
{
	struct br_private *const priv = dev->priv;
	int i;
	int ret = 0;

	for (i = 0; i < priv->nemacs; i++) {
		if (priv->dev_state & (1 << i)) {
			int rc = 0;
			rc = emac_recv(priv->emacs[i]);
			if (rc) {
				ret = rc;
			}
		}
	}

	return ret;
}
uint32_t probe_phy(struct emac_private *priv)
{
	uint32_t val;

	if ((val = priv->mdio->read(priv, PhyBMCR)) == MDIO_READ_FAIL) {
		printf("Probe PHY Failed\n");
		return -1;
	}

	return val;
}

static void arasan_initialize_gpio_reset_pin(int pin)
{
	printf("emac init GPIO pin %d reset seq\n", pin);
	gpio_config(pin, GPIO_MODE_OUTPUT);
	gpio_output(pin, 1);
	udelay(100);
	gpio_output(pin, 0);
	udelay(100000);
	gpio_output(pin, 1);
}

static void arasan_initialize_gpio_reset(uint32_t emac_cfg)
{
	if (emac_cfg & EMAC_PHY_GPIO1_RESET) {
		arasan_initialize_gpio_reset_pin(RUBY_GPIO_PIN1);
	}

	if (emac_cfg & EMAC_PHY_GPIO13_RESET) {
		arasan_initialize_gpio_reset_pin(RUBY_GPIO_PIN13);
	}
}

#define RXTX_DELAY 0x1e0000
static struct eth_device __g_br_dev = {
	.priv = &__g_br_priv,
	.init = br_init,
	.halt = br_halt,
	.send = br_send,
	.recv = br_recv,

};

static uint32_t get_phy_id(struct emac_private *priv)
{
	int phy_reg;
	uint32_t phy_id;

	if ((phy_reg = priv->mdio->read(priv, PhyPhysID1)) == MDIO_READ_FAIL) {
		printf("Read from MDIO failed.\n");
		return -1;
	}
	phy_id = (phy_reg & 0xffff) << 16;

	if ((phy_reg = priv->mdio->read(priv, PhyPhysID2)) == MDIO_READ_FAIL) {
		printf("Read from MDIO failed.\n");
		return -1;
	}

	phy_id |= (phy_reg & 0xffff);

	return phy_id;
}

static int phy_init(struct emac_private *priv)
{
	unsigned long mdio_base;
	uint32_t phy_id;
	struct br_private * br_priv = priv->parent->priv;
	mdio_base = priv->mdio->base;

	if (!(priv->phy_flags & EMAC_PHY_NOT_IN_USE)) {
		int found = 0;
		int i = 0;
		while (i++ < 100) {
			if (priv->phy_addr >= PHY_MAX_ADDR) {
				for (priv->phy_addr = 0; priv->phy_addr < PHY_MAX_ADDR; priv->phy_addr++) {
					int id;
					if (br_priv->phy_addr_mask & (1 << priv->phy_addr))
						continue;
					if ((id = probe_phy(priv)) != 0xffff) {
						printf("PHY found on MDIO addr:%x %d\n", id, priv->phy_addr);
						found = 1;
						break;
					}
					udelay(200);
				}
			} else {
				if (probe_phy(priv) != 0xffff) {
					printf("PHY found on MDIO addr:%d\n", priv->phy_addr);
					found = 1;
					break;
				}
				udelay(200);
			}
		}
		if (!found) {
			printf("No PHY found on MDIO addr:%d\n", priv->phy_addr);
			return 0;
		}

		/* Reset the PHY which then kick-offs the autonegotiation */
#if TOPAZ_FPGA_PLATFORM
		/* Must reset PHYs in the order listed below - PHY ADDR=(4, 1, 2, 3)
		   Only FPGA-B will bring-up the PHYs and configure them since FPGA-A
		   doesn't have a physical MDIO bus to configure the PHYs */
		mdio_postwr_raw(mdio_base, TOPAZ_FPGAB_PHY0_ADDR, PhyBMCR, PhySoftReset | PhyAutoNegEnable);
		mdio_postwr_raw(mdio_base, TOPAZ_FPGAB_PHY1_ADDR, PhyBMCR, PhySoftReset | PhyAutoNegEnable);
		mdio_postwr_raw(mdio_base, TOPAZ_FPGAA_PHY0_ADDR, PhyBMCR, PhySoftReset | PhyAutoNegEnable);
		mdio_postwr_raw(mdio_base, TOPAZ_FPGAA_PHY1_ADDR, PhyBMCR, PhySoftReset | PhyAutoNegEnable);
#else
		priv->mdio->write(priv, PhyBMCR, PhySoftReset | PhyAutoNegEnable);
#endif /*  #if TOPAZ_FPGA_PLATFORM */

		//Turn off 125MHz and enable SSC for Realtek(RTL8211E) VB
		phy_id = get_phy_id(priv);
		printf("PHY 0x%x found on MDIO addr:%d\n", phy_id, priv->phy_addr);
		if (phy_id == 0x1cc915) {
			uint32_t val;
			priv->mdio->write(priv, 31, 0);
			priv->mdio->write(priv, 16, 0x17E);
			priv->mdio->write(priv, 31, 7);
			priv->mdio->write(priv, 30, 0xA0);
			if ((val = priv->mdio->read(priv, 26)) == MDIO_READ_FAIL) {
				printf("u-boot: Failed to read phy reg#26\n");
				return 0;
			}
			priv->mdio->write(priv, 26, val & ~4);
			priv->mdio->write(priv, 31, 0);
			printf("Enable phy SSC\n");
		}

		/* This is a dummy read that ensures the write has happened */
		if (mdio_rdval_raw(priv->mdio->base, 1) == MDIO_READ_FAIL) {
			return 0;
		}
	} else if (priv->phy_flags & EMAC_PHY_AR8236) {
		priv->phy_addr = ar8236_init(mdio_base, priv->phy_addr);
	} else if (priv->phy_flags & EMAC_PHY_AR8327) {
		priv->phy_addr = ar8237_init(mdio_base, priv->phy_addr);
	}

	if (priv->phy_addr<PHY_MAX_ADDR)
		br_priv->phy_addr_mask |= (1 << priv->phy_addr);
	return 1;
}
static void emac_probe_phy(struct emac_private * priv, uint32_t cfg, uint32_t phy_addr)
{
	struct br_private * br_priv = priv->parent->priv;

	priv->phy_flags = cfg;
	priv->phy_addr = phy_addr;

	if (phy_init(priv)){
		emac_halt(priv);
		br_priv->emacs[br_priv->nemacs++] = priv;
	}
}

int board_eth_init(bd_t *bis)
{
	DECLARE_GLOBAL_DATA_PTR;
	struct eth_device *br_dev;
	struct br_private *br_priv;
	struct emac_private *emac_priv = &__g_emac_priv[0];
	uint32_t rgmii_timing = board_config(gd->bd->bi_board_id, BOARD_CFG_RGMII_TIMING);
	uint32_t emac0_cfg = board_config(gd->bd->bi_board_id, BOARD_CFG_EMAC0);
	uint32_t emac1_cfg = board_config(gd->bd->bi_board_id, BOARD_CFG_EMAC1);
	uint32_t emac_cfg = emac0_cfg | emac1_cfg;
	const char *dbg_bus = getenv("debug_bus");
	int i;

	if (!((emac0_cfg & EMAC_IN_USE) || (emac1_cfg & EMAC_IN_USE))) {
		printf("error: no emac enabled\n");
		return -1;
	}

	br_dev = &__g_br_dev;
	br_priv = &__g_br_priv;
	br_dev->priv = br_priv;
	for (i = 0; i < 6; i++) {
		br_dev->enetaddr[i] = gd->bd->bi_enetaddr[i];
	}

	sprintf(br_dev->name, "br");

	arasan_initialize_gpio_reset(emac_cfg);
	arasan_initialize_release_reset(emac0_cfg, emac1_cfg, rgmii_timing, 1);

	if (emac_cfg & (EMAC_PHY_RTL8363SB_P0 | EMAC_PHY_RTL8363SB_P1 | EMAC_PHY_RTL8365MB)) {
		rtl8367b_init(emac_priv, emac_cfg);
	}

	br_priv->nemacs = 0;
	br_priv->phy_addr_mask = 0;

	if (dbg_bus != NULL) {
		/* clear any other forced speeds possibly set */
		emac0_cfg &= ~(EMAC_PHY_FORCE_1000MB | EMAC_PHY_FORCE_100MB | EMAC_PHY_FORCE_10MB);
		emac1_cfg &= ~(EMAC_PHY_FORCE_1000MB | EMAC_PHY_FORCE_100MB | EMAC_PHY_FORCE_10MB);
		emac0_cfg |= EMAC_PHY_FORCE_1000MB;
		emac1_cfg |= EMAC_PHY_FORCE_1000MB;
	}
	if (emac0_cfg & EMAC_IN_USE) {
		emac_priv = &__g_emac_priv[0];

		emac_probe_phy(emac_priv, emac0_cfg, board_config(gd->bd->bi_board_id, BOARD_CFG_PHY0_ADDR));
	}
	if (emac1_cfg & EMAC_IN_USE) {
		emac_priv = &__g_emac_priv[1];
		emac_probe_phy(emac_priv, emac1_cfg, board_config(gd->bd->bi_board_id, BOARD_CFG_PHY1_ADDR));
	}
	/*
	 * if debug bus is set in hardware, then mdio is disabled so auto-negotiation
	 * will be set to 10Mbs. default speed will be 1Gbs if debug_bus=1.
	 */

	eth_register(br_dev);

	return 1;
}


#ifndef RUBY_MINI
int do_mdio(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	unsigned long base = ARASAN_MDIO_BASE;
	if (argc < 4) {
		cmd_usage(cmdtp);
		return 1;
	}
	if (strcmp(argv[1],"read") == 0) {
		uint32_t a1,a2,val;
		a1 = simple_strtoul (argv[2], NULL, 10);
		a2 = simple_strtoul (argv[3], NULL, 16);
		mdio_postrd_raw(base, a1, a2);
		val = mdio_rdval_raw(base, 1);
		printf("phy:%d reg:%d=0x%x\n",a1,a2,val);
	} else if (strcmp(argv[1],"write") == 0) {
		uint32_t a1, a2, a3;
		a1 = simple_strtoul (argv[2], NULL, 10);
		a2 = simple_strtoul (argv[3], NULL, 10);
		a3 = simple_strtoul (argv[4], NULL, 16);
		mdio_postwr_raw(base, a1, a2, a3);
		/* This is a dummy read that ensures the write has happened */
		if (mdio_rdval_raw(base, 1) == MDIO_READ_FAIL) {
			return -1;
		}
	} else {
		return -1;
	}

	return 0;
}
#endif

#if defined(CONFIG_CMD_ETHLOOP)
int mdio_write(int reg, u16 val)
{
	uint32_t base = ARASAN_MDIO_BASE;
	int ret = -1;
	ret = mdio_postwr(base, priv.phy_addr, reg, val);
	/* This is a dummy read that ensures the write has happened */
	if (mdio_rdval(base, 1) == (uint32_t)-1)
		return -1;

	return ret;
}

int mdio_read(int reg)
{
	uint32_t val, base = ARASAN_MDIO_BASE;
	mdio_postrd(base, priv.phy_addr, reg);
	val = mdio_rdval(base, 1);
	return val;
}

void enable_phy_loopback(void)
{
	uint32_t val;
	val = mdio_read(PhyBMCR);
	val |= PhyLoopback;  /* set loopback bit */
	mdio_write(PhyBMCR, val);
}

void disable_phy_loopback(void)
{
	uint32_t val;
	val = mdio_read(PhyBMCR);
	val &= ~PhyLoopback; /* reset loopback bit */
	mdio_write(PhyBMCR, val);
}
#endif
#ifndef RUBY_MINI
#ifdef ENABLE_LOOPBACK
int do_emaclb(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	int a1;
	a1 = simple_strtoul (argv[1], NULL, 16);
	if (a1 == 0) {
		loopback = 0;
	} else {
		loopback = 1;
	}
	while (1) {
		NetLoop (NETCONS);	/* kind of poll */
	}
	return 0;
}

U_BOOT_CMD(
		emaclb, CFG_MAXARGS, 2, do_emaclb,
		"emaclb - set emac loopback (0 or 1) \n",
		NULL
	  );
#endif
#define CFG_MAXARGS 5
U_BOOT_CMD(
		mdio, CFG_MAXARGS, 5, do_mdio,
		"read/write mdio",
		"mdio <read|write> <phy> <reg> <hex value>"
	  );

int do_emac(cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	emacs_stats();
	return 0;
}
U_BOOT_CMD(
		emac, CFG_MAXARGS, 3, do_emac,
		"emac read stats",
		NULL
	  );
#endif

static int emac_phy_init(struct emac_private *priv)
{
	const uintptr_t base = priv->io_base;
	const uintptr_t mdio_base = priv->mdio->base;
	const int emac = priv->emac;
	int i;
	uint32_t val, x;
	uint32_t advertise;
	uint32_t lpa = 0;
	uint32_t lpa2 = 0;
	uint32_t media = 0;
	uint32_t duplex = 0;

	if (!(priv->phy_flags & EMAC_PHY_NOT_IN_USE)) {
		if (priv->phy_flags & EMAC_PHY_RESET) {
			udelay(4);
		}
		if (priv->phy_flags & EMAC_PHY_RTL8211DS) {
			int link_status = 0;
			int speed_sel = 0;
			duplex = 0;
			lpa2 = 0;
			media = 0;
			priv->mdio->write(priv, RTL8211DS_PAGSEL, PAGSEL_PAGEXT);
			priv->mdio->write(priv, RTL8211DS_PAGNO, PAGNO_140);
			link_status = priv->mdio->read(priv, RTL8211DS_RXCONFIG);
			if (link_status & RXCONFIG_LINK) {
				speed_sel = priv->mdio->read(priv, RTL8211DS_SDSR);
				speed_sel &= ~SDSR_SPEED;
				switch ((link_status & RXCONFIG_SPEED) >> RXCONFIG_SPEED_BIT) {
					case 0:
						//printf(": 0x%x, 10-", speed_sel);
						speed_sel |= SDSR_10;
						break;
					case 1:
						//printf(": 0x%x, 100-", speed_sel);
						media = ADVERTISE_100FULL;
						speed_sel |= SDSR_100;
						break;
					case 2:
						//printf(": 0x%x, 1000-", speed_sel);
						lpa2 = LPA_1000FULL;
						speed_sel |= SDSR_1000;
						break;
					default:
						return 0;
				}
				if (link_status & RXCONFIG_DUPLEX) {
					//printf("Full: 0x%x\n",speed_sel);
					duplex = 1;
				}
				priv->mdio->write(priv, RTL8211DS_SDSR, speed_sel);
				priv->mdio->write(priv, RTL8211DS_PAGSEL, PAGSEL_PAG0);
			}
			else {
				printf("SGMIILinkDown\n");
				priv->mdio->write(priv, RTL8211DS_PAGSEL, PAGSEL_PAG0);
				return 0;
			}
		} else if ((priv->phy_flags & EMAC_PHY_AUTO_MASK) == 0) {
			/* Vitesse VSC8641 has a bug regarding writes to register 4 or 9
			 * after a soft reset unless an MDIO access occurs in between.
			 * We avoid this bug implicitly by the polling loop here.
			 */
			printf("AutoNegoEMAC%d\n",priv->emac);

			i = 0;
			do {
				/* Wait for autonegotiation to complete */
				udelay(500);
				if ((val = priv->mdio->read(priv, PhyBMSR)) == MDIO_READ_FAIL) {
					return 0;
				}
				i++;

				if (i >= 30000) {
					printf("AutoNegoTimeout\n");
					return 0;
				}
				if (!(val & PhyLinkIsUp) && (i >= 5000)) {
					printf("LinkDown\n");
					return 0;
				}
				if (ctrlc()) {
					printf("Ctrl+C detected\n");
					return 0;
				}
			} while (!(val & PhyAutoNegComplete));

			printf("Autonegotiation Done (BMSR=%08x)\n", val);

			/* Work out autonegotiation result using only IEEE registers */
			if ((advertise = priv->mdio->read(priv, PhyAdvertise)) == MDIO_READ_FAIL) {
				printf("GMII: Failed to read Advertise reg\n");
				return 0;
			}

			if ((lpa = priv->mdio->read(priv, PhyLpa)) == MDIO_READ_FAIL) {
				printf("GMII: Failed to read Lpa reg\n");
				return 0;
			}

			if ((lpa2 = priv->mdio->read(priv, PhyStat1000)) == MDIO_READ_FAIL) {
				printf("GMII: Failed to read Lpa2 reg\n");
				return 0;
			}

			printf("Autoneg status: Advertise=%08x, Lpa=%08x, Lpa2=%08x\n",
					advertise, lpa, lpa2);

			media = mii_nway_result(lpa & advertise);
			duplex = (media & ADVERTISE_FULL) ? 1 : 0;
			if (lpa2 & LPA_1000FULL) {
				duplex = 1;
			}

		} else {
			if (priv->phy_flags & EMAC_PHY_FORCE_1000MB) {
				i = 0x0040; /* Force 1G */
				lpa2 = LPA_1000FULL;
			} else if (priv->phy_flags & EMAC_PHY_FORCE_100MB) {
				i = 0x2000; /* Force 100M */
				media = ADVERTISE_100FULL;
			} else {
				i = 0x0000; /* Force 10M */
				if (priv->phy_flags & EMAC_PHY_FORCE_10MB ) {
					printf("GMII: ethlink not 10, 100 or 1000 - forcing 10Mbps\n");
				}
			}
			if (!(priv->phy_flags & EMAC_PHY_FORCE_HDX)) {
				i |= 0x100; /* Full duplex */
				duplex = 1;
			}

			/* Force link speed & duplex */
#if defined(TOPAZ_FPGA_PLATFORM)
			/*
			 *  Must reset PHYs in the order listed below - PHY ADDR=(4, 1, 2, 3)
			 *  Only FPGA-B will bring-up the PHYs and configure them since FPGA-A
			 *  doesn't have a physical MDIO bus to configure the PHYs
			 */
			mdio_postwr_raw(mdio_base, TOPAZ_FPGAB_PHY0_ADDR, PhyBMCR, i);
			mdio_postwr_raw(mdio_base, TOPAZ_FPGAB_PHY1_ADDR, PhyBMCR, i);
			mdio_postwr_raw(mdio_base, TOPAZ_FPGAA_PHY0_ADDR, PhyBMCR, i);
			mdio_postwr_raw(mdio_base, TOPAZ_FPGAA_PHY1_ADDR, PhyBMCR, i);
#else
			priv->mdio->write(priv, PhyBMCR, i);
#endif

			printf("GMII: Forced link speed is ");
		}
	} else {  // no phy
		if (priv->phy_flags & EMAC_PHY_FORCE_1000MB) {
			i = 0x0040; /* Force 1G */
			lpa2 = LPA_1000FULL;
		} else if (priv->phy_flags & EMAC_PHY_FORCE_100MB) {
			i = 0x2000; /* Force 100M */
			media = ADVERTISE_100FULL;
		} else {
			i = 0x0000; /* Force 10M */
			if (priv->phy_flags & EMAC_PHY_FORCE_10MB ) {
				printf("GMII: ethlink not 10, 100 or 1000 - forcing 10Mbps\n");
			}
		}
		if (!(priv->phy_flags & EMAC_PHY_FORCE_HDX)) {
			i |= 0x100; /* Full duplex */
			duplex = 1;
		}
	}
	x = 0;

	emac_wrreg(RUBY_SYS_CTL_BASE_ADDR, SYSCTRL_CTRL_MASK,
			emac ? RUBY_SYS_CTL_MASK_GMII1_TXCLK : RUBY_SYS_CTL_MASK_GMII0_TXCLK);

	if (lpa2 & (LPA_1000FULL | LPA_1000HALF)) {
		x |= MacSpeed1G;
		printf("1G");
		emac_wrreg(RUBY_SYS_CTL_BASE_ADDR, SYSCTRL_CTRL,
				emac ? RUBY_SYS_CTL_MASK_GMII1_1000M : RUBY_SYS_CTL_MASK_GMII0_1000M);

	}
	else if (media & (ADVERTISE_100FULL | ADVERTISE_100HALF)) {
		x |= MacSpeed100M;
		printf("100M");
		emac_wrreg(RUBY_SYS_CTL_BASE_ADDR, SYSCTRL_CTRL,
				emac ? RUBY_SYS_CTL_MASK_GMII1_100M : RUBY_SYS_CTL_MASK_GMII0_100M);
	}
	else {
		x |= MacSpeed10M;
		printf("10M");
		emac_wrreg(RUBY_SYS_CTL_BASE_ADDR, SYSCTRL_CTRL,
				emac ? RUBY_SYS_CTL_MASK_GMII1_10M : RUBY_SYS_CTL_MASK_GMII0_10M);
	}
	emac_wrreg(RUBY_SYS_CTL_BASE_ADDR, SYSCTRL_CTRL_MASK, 0);

	if (duplex) {
		x |= MacFullDuplex;
		printf("-FD\n");
	} else {
		printf("-HD\n");
	}

	/* for certain switches, poll waiting for a link up before returning */
	if (priv->phy_flags & EMAC_PHY_NOT_IN_USE) {
		if (priv->phy_flags & (EMAC_PHY_RTL8363SB_P0 | EMAC_PHY_RTL8363SB_P1)) {
			rtl8367b_poll_linkup(priv);
		}
	}

	emac_wrreg(base, EMAC_MAC_GLOBAL_CTRL, x);

	return 1;
}
#endif
