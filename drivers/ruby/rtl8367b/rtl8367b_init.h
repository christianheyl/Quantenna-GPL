#ifndef _RTL8367B_INIT_H_
#define _RTL8367B_INIT_H_

#include <linux/types.h>

#if defined(CONFIG_SWITCH_RTL8363SB) || defined(CONFIG_SWITCH_RTL8363SB_MODULE)
#define CHIP_RTL8363SB
#define RTL_SWITCH
#define RTL_SWITCH_NAME "RTL8363SB"
#endif

#if defined(CONFIG_SWITCH_RTL8365MB) || defined(CONFIG_SWITCH_RTL8365MB_MODULE)
#define CHIP_RTL8365MB
#define RTL_SWITCH
#define RTL_SWITCH_NAME "RTL8365MB"
#endif

struct mii_bus;
int rtl8367b_init(struct mii_bus *mii,
		int (*internal_read)(struct mii_bus *bus, int phy_addr, int reg),
		int (*internal_write)(struct mii_bus *bus, int phy_addr, int reg, uint16_t value),
		uint32_t emac_cfg, int port);
void rtl8367b_exit(void);
void rtl8367b_dump_stats(void);
void rtl8367b_dump_status(void);
int rtl8367b_ext_port_enable(int emac);
int rtl8367b_ext_port_disable(int emac);

#endif	// _RTL8367B_INIT_H_

