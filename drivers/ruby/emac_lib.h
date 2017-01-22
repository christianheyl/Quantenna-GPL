/*
 *  Copyright (c) Quantenna Communications, Inc. 2012
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

#ifndef __EMAC_LIB_H
#define __EMAC_LIB_H

#include <linux/netdevice.h>
#include <common/ruby_arasan_emac_ahb.h>
#include <common/topaz_platform.h>
#include <qtn/dmautil.h>

#ifdef CONFIG_QVSP
#include <qtn/qdrv_sch.h>
#include <qtn/qvsp.h>
#endif

#define EMAC_REG_DEBUG	1

#define QTN_DSCP_MIN		0
#define QTN_DSCP_MAX		63

#ifndef __ASSEMBLY__

#define MULTICAST_FILTER_LIMIT	64

#define TIMEOUT_MAC_MDIO_CTRL	1000	/*ms*/

/*
 * State which is common to Topaz and Ruby EMAC usage
 */

typedef ALIGNED_DMA_DESC(struct, emac_desc) aligned_emac_descs;

enum emac_dma_counter {
	DmaMissedFrame = 0,
	DmaStopFlush = 1,
	DmaLastStatCounter = 1,
};

struct emac_stats {
	uint32_t tx[TxLastStatCounter + 1];
	uint32_t rx[RxLastStatCounter + 1];
	uint32_t dma[DmaLastStatCounter + 1];
};

struct emac_common {
	u32 vbase;
	u32 mdio_vbase;
	u32 emac_cfg;
	int mac_id;
	int phy_addr;
	struct phy_device *phy_dev;
	struct mii_bus *mii_bus;
	int old_link;
	struct net_device *dev;

	aligned_emac_descs rx;
	aligned_emac_descs tx;

	struct notifier_block pm_notifier;
	uint32_t pm_adv_mode;

	int current_stats;
	struct emac_stats stats[2];
};

/*
 * Utility functions for reading/writing registers in the Ethernet MAC
 */
__always_inline static u32 emac_rd(struct emac_common *arapc, int reg)
{
	return readl(IO_ADDRESS(arapc->vbase + reg));
}
__always_inline static void emac_wr(struct emac_common *arapc, int reg, u32 val)
{
	writel(val, IO_ADDRESS(arapc->vbase + reg));
	/* HW bug workaround - dummy access breaks up bus transactions. */
	readl(RUBY_SYS_CTL_BASE_ADDR);
}

static inline bool emac_lib_rtl_switch(uint32_t cfg)
{
	return cfg & (EMAC_PHY_RTL8363SB_P0 | EMAC_PHY_RTL8363SB_P1 | EMAC_PHY_RTL8365MB);
}

/*
 * Utility functions for reading/writing registers in the MDIO
 */
inline static u32 mdio_rd(struct emac_common *arapc, int reg)
{
	return readl(IO_ADDRESS(arapc->mdio_vbase + reg));
}
inline static void mdio_wr(struct emac_common *arapc, int reg, u32 val)
{
	writel(val, IO_ADDRESS(arapc->mdio_vbase + reg));
	/* HW bug workaround - dummy access breaks up bus transactions. */
	readl(RUBY_SYS_CTL_BASE_ADDR);
}

inline static void emac_setbits(struct emac_common *arapc, int reg, u32 val)
{
	emac_wr(arapc, reg, emac_rd(arapc, reg) | val);
}

inline static void emac_clrbits(struct emac_common *arapc, int reg, u32 val)
{
	emac_wr(arapc, reg, emac_rd(arapc, reg) & ~val);
}

int emac_lib_poll_wait(struct emac_common *arapc, u32(*read_func)(struct emac_common*, int),
		int reg, u32 mask, u32 val, unsigned long ms, const char *func);

inline static int mdio_wait(struct emac_common *arapc, int reg, u32 mask,
		u32 val, unsigned long ms, const char *func)
{
	return emac_lib_poll_wait(arapc, mdio_rd, reg, mask, val, ms, func);
}

inline static int emac_wait(struct emac_common *arapc, int reg, u32 mask,
		u32 val, unsigned long ms, const char *func)
{
	return emac_lib_poll_wait(arapc, emac_rd, reg, mask, val, ms, func);
}

extern int g_dscp_flag;
extern int g_dscp_value[];
extern const struct ethtool_ops emac_lib_ethtool_ops;
void emac_lib_enable(uint32_t);
int emac_lib_mii_init(struct net_device *dev);
void emac_lib_mii_exit(struct net_device *dev);
int emac_lib_mdio_read(struct mii_bus *bus, int phy_addr, int reg);
int emac_lib_mdio_write(struct mii_bus *bus, int phy_addr, int reg, uint16_t value);
int emac_lib_mdio_sysfs_create(struct net_device *dev);
void emac_lib_mdio_sysfs_remove(struct net_device *dev);
int emac_lib_board_cfg(int port, int *emac_cfg, int *emac_phy);
void emac_lib_descs_free(struct net_device *dev);
int emac_lib_descs_alloc(struct net_device *dev,
		u32 rxdescs, bool rxdescs_sram,
		u32 txdescs, bool txdescs_sram);
void emac_lib_init_mac(struct net_device *dev);
void emac_lib_init_dma(struct emac_common *privc);
void emac_lib_phy_start(struct net_device *dev);
void emac_lib_phy_stop(struct net_device *dev);
void emac_lib_set_rx_mode(struct net_device *dev);
int emac_lib_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
struct net_device_stats *emac_lib_stats(struct net_device *dev);
int emac_lib_stats_sprintf(char *buf, struct net_device *dev);
void emac_lib_pm_save_add_notifier(void);
void emac_lib_pm_save_remove_notifier(void);
int emac_lib_phy_power_create_proc(struct net_device *dev);
void emac_lib_phy_power_remove_proc(struct net_device *dev);
int emac_lib_phy_reg_create_proc(struct net_device *dev);
void emac_lib_phy_reg_remove_proc(struct net_device *dev);

void emac_lib_update_link_vars(const uint32_t dual_link);
void emac_lib_pm_emac_add_notifier(struct net_device *dev);
void emac_lib_pm_emac_remove_notifier(struct net_device *dev);

#if EMAC_REG_DEBUG
void emac_lib_reg_debug(u32 base);
#else
#define emac_reg_debug(x)
#endif

#ifdef CONFIG_QVSP
extern struct qvsp_wrapper emac_qvsp;
#endif

#endif	// __ASSEMBLY__
#endif	// __EMAC_LIB_H

