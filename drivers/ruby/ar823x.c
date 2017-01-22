/*
 *  ar823x.c
 *
 *  Driver for the Atheros 8236 & 8327 switches
 *
 *  Copyright (c) Quantenna Communications Incorporated 2009.
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

/********************************************************************
    Atheros 823x MDIO programming details
    The Arasan MDIO IP on our chip uses two registers to program
    MDIO.
    
    data[16:0]
    control
    1   1111   1    00000   00000   
    5   4321   0    98765   43210
    -   ----   --   -----   -----
    st  xxxx   op   reg     phy
    
    st: 1 - start, poll for completion
    op: 1 - read, 0 write
    
    
    These are encoded into the serial MDIO protocol as follows
    
    IEEE MDIO frame
    33 22   22222   22211   11  1111110000000000
    10 98   76543   21098   76  5432109876543210
    -- --   -----   -----   --  ----------------
    ST OP   PHY     REG     TA  DATA[15:0]
    
    TA and ST are encoded automatically by Arasan IP.
    
    
    This device uses 18 bits to specify register addresses.
    These bits are programmed into the device by paging as follows.
    
    aaaaaaaaa   aaaaaaaa   aa
    111111111   00000000   00
    876543210   98765432   10
    ---------   --------   ------
    page addr   reg addr   ignore
    
    Since the registers are all 32-bit, the lower two address
    bits are discarded.  The page is written first using the
    following format.  Note PHY is limited to three bits.
    
    8213 Page program command
    -----------------------------------------------
    33  22  22   222  22211   11  111111 0000000000
    10  98  76   543  21098   76  543210 9876543210
    --  --  --   ---  -----   --  ------ ----------
    ST  OP  CMD  PHY  xxxxx   TA  xxxxxx page addr      
    
    CMD: 11 - page address write
    CMD: 10 - reg access
    
    The tricky part is the reg access step following page programming
    Since the register format of arasan swaps the order of reg and
    phy, and since our register address spans these two fields, we
    have to swizzle the bits into place.

    8213 Reg read/write command
    ------------------------------------------------
    33  22  22    2222221   1   11  1111110000000000
    10  98  76    5432109   8   76  5432109876543210
    --  --  --    -------   -   --  ----------------
    ST  OP  CMD   reg adr   W   TA  DATA[15:0]      
    
    W: 0 - lower 16 bits, 1: upper 16 bits
    
    Programming this operation into Arasan requires
    
    phy = 'b10 << 3 | regAddr[9:7]
    reg = regAddr[6:2]
    
    mdioCmd = phy | reg << 5 | op | start
    
    
********************************************************************/
////////////////////////////////////////////////////////////////////
// NOTE - we do not check for valid base in mdio access routines
// use must ensure device is initialized and valid prior
// to using MDIO funtions
///////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////
//      Includes
////////////////////////////////////////////////////////////////////
#include <linux/module.h>
#include <linux/phy.h>
#include <asm/hardware.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/mii.h>
#include "ar823x.h"
#include <common/ruby_arasan_emac_ahb.h>

////////////////////////////////////////////////////////////////////
//      Defines
////////////////////////////////////////////////////////////////////
#define AR823x_MODE_CTRL            (0x04)

// use phy mode and invert tx clock for better hold margin
#define AR8236_MODE_MII_PHY         (0x80000600)
#define AR8236_MODE_MII_MAC         (0x80000004)
#define AR8236_FLOOD_MASK           (0x2c)
#define AR8236_FLOOD_MASK_DEF       (0xfe7f007f)
#define AR8236_PORT0_CTRL           (0x100)
#define AR8236_PORT0_CTRL_DEF       (0x7d)
#define AR8236_FLOW_LINK_EN         (0x1000)

#define AR823x_MASK_CTL             (0)
#define AR823x_MASK_CTL_RESET       (0x80000000)

#define AR8327_MASK_CLEAR_DEF       (0)
#define AR8327_MODE_RGMII_PHY     (0x07600000)
//#define AR8327_MODE_RGMII_PHY       (0x07402000)
#define AR8327_PWS_CTRL             (0x10)
#define AR8327_PWS_CTRL_DEF         (0x40000000)
#define AR8327_FWCTL_CTRL           (0x0624)
#define AR8327_FWCTL_CTRL_DEF       (0x007f7f7f)
#define AR8327_PORT6_CTRL           (0xc)
#define AR8327_PORT6_CTRL_DEF       (0x01000000)
#define AR8327_PORT0_CTRL           (0x7c)
#define AR8327_PORT0_CTRL_DEF       (0x7e)

#define AR823x_CPU_PORT_REG         (0x78)
#define AR823x_CPU_PORT_EN          (1 << 8)
#define AR823x_MIRROR_PORT_NONE     (0xf << 4)

#define AR823x_MDIO_START           (1 << 15)
#define AR823x_MDIO_WRITE           (0 << 10)
#define AR823x_MDIO_READ            (1 << 10)
#define AR823x_MDIO_HIGH_WORD       (1 << 0)

#define AR823x_MDIO_TIMEOUT         (0x1000)
#define AR823x_MDIO_PAGE            (0x18)
#define AR823x_MDIO_NORM            (0x10)
#define AR823x_PORT_CTRL(x)         (0x104 + 0x100 * (x))
#define AR823x_PORT_VLAN(x)         (0x108 + 0x100 * (x))
#define AR823x_NUM_PORTS            (5)

#define AR823x_ARP_LEAKY_EN         (1 << 22)
#define AR823x_LEARN_EN             (1 << 14)
#define AR823x_FORWARD              (1 << 2)

#define AR823x_MIN_PHY_NUM          (0)
#define AR823x_MAX_PHY_NUM          (4)

#define AR823x_QM_REG               (0x3c)
#define AR823x_ARP_EN               (1 << 15)
#define AR823x_ARP_REDIRECT         (1 << 14)

////////////////////////////////////////////////////////////////////
//      Types
////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////
//      Data
////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////
//      Functions
////////////////////////////////////////////////////////////////////
#define AR823x_REG_WRITE(x,y)    (*(volatile unsigned int *)IO_ADDRESS(x) = (unsigned int)(y))
#define AR823x_REG_READ(x)       (*(volatile unsigned int *)IO_ADDRESS(x))

extern int mdc_clk_divisor;

inline u32 ar823x_emac_rdreg(int reg)
{
	return AR823x_REG_READ(RUBY_ENET0_BASE_ADDR + reg);
}

inline void ar823x_emac_wrreg(int reg, u32 val)
{
	AR823x_REG_WRITE(RUBY_ENET0_BASE_ADDR + reg, val);
}

/*********************************************************************
 Name:      ar823x_mdio_poll
 Purpose:   mdio poll routine for AR823x device
 Notes:     Checks for mdio operation complete
*********************************************************************/
int ar823x_mdio_poll(void)
{
	u32 timeout = AR823x_MDIO_TIMEOUT;

	// check for clear MDIO status
	while (timeout--) {
		int status = ar823x_emac_rdreg(EMAC_MAC_MDIO_CTRL);
		if ((status & AR823x_MDIO_START) == 0) {
			break;
		}
	}
	if (timeout == 0) {
		printk("ar823x mdio timeout\n");
		return -1;
	}
	return 0;
}


/*********************************************************************
 Name:      ar823x_mdio_write
 Purpose:   mdio write routine for AR823x device
 Notes:     reg_addr[1]=1 determines high word
*********************************************************************/
int ar823x_mdio_write(struct mii_bus *bus, int phyAddr, int regAddr, u16 data)
{
	u32 highAddr = regAddr >> 9;
	// need to swizzle the bits into arasan's fields which are different          
	u32 rg = (regAddr & 0x3c) >> 1;
	u32 ph = (regAddr & 0x1c0) >> 6;

	// wait for completion
	if (ar823x_mdio_poll() != 0) {
		return -1;
	}

	if (regAddr & 0x2) {
		ar823x_emac_wrreg(EMAC_MAC_MDIO_DATA, highAddr);
		ar823x_emac_wrreg(EMAC_MAC_MDIO_CTRL, (phyAddr & 0x1f) |
				  ((mdc_clk_divisor & MacMdioCtrlClkMask) << MacMdioCtrlClkShift) |
				  AR823x_MDIO_START | AR823x_MDIO_PAGE);

		// wait for completion
		if (ar823x_mdio_poll() != 0) {
			return -1;
		}
		ar823x_emac_wrreg(EMAC_MAC_MDIO_DATA, data);
		ar823x_emac_wrreg(EMAC_MAC_MDIO_CTRL,
				  ((rg | AR823x_MDIO_HIGH_WORD) << 5) | ph |
				  ((mdc_clk_divisor & MacMdioCtrlClkMask) << MacMdioCtrlClkShift) |
				  AR823x_MDIO_START | AR823x_MDIO_NORM);
		if (ar823x_mdio_poll() != 0) {
			return -1;
		}
	} else {

		ar823x_emac_wrreg(EMAC_MAC_MDIO_DATA, data);
		ar823x_emac_wrreg(EMAC_MAC_MDIO_CTRL,
				  (rg  << 5) | ph |
				  ((mdc_clk_divisor & MacMdioCtrlClkMask) << MacMdioCtrlClkShift) |
				  AR823x_MDIO_START | AR823x_MDIO_NORM);
	}
	// return without waiting for final completion
	return 0;
}

/*********************************************************************
 Name:      ar823x_mdio_write32
 Purpose:   mdio write routine for AR823x device
 Notes:     This is a partial blocking call since we require
            more than one cycle to complete the write.
            checks for completion first
*********************************************************************/
int ar823x_mdio_write32(int phyAddr, int regAddr, u32 data)
{

	u32 highAddr = regAddr >> 9;
	// need to swizzle the bits into arasan's fields which are different          
	u32 rg = (regAddr & 0x3c) >> 1;
	u32 ph = (regAddr & 0x1c0) >> 6;

	// check for clear MDIO status
	if (ar823x_mdio_poll() != 0) {
		return -1;
	}

	ar823x_emac_wrreg(EMAC_MAC_MDIO_DATA, highAddr);
	ar823x_emac_wrreg(EMAC_MAC_MDIO_CTRL, (phyAddr & 0x1f) |
			  ((mdc_clk_divisor & MacMdioCtrlClkMask) << MacMdioCtrlClkShift) |
			  AR823x_MDIO_START | AR823x_MDIO_PAGE);

	// wait for completion
	if (ar823x_mdio_poll() != 0) {
		return -1;
	}

	ar823x_emac_wrreg(EMAC_MAC_MDIO_DATA, (data >> 16) & 0xffff);
	ar823x_emac_wrreg(EMAC_MAC_MDIO_CTRL,
			  ((rg | AR823x_MDIO_HIGH_WORD) << 5) | ph |
			  ((mdc_clk_divisor & MacMdioCtrlClkMask) << MacMdioCtrlClkShift) |
			  AR823x_MDIO_START | AR823x_MDIO_NORM);
	if (ar823x_mdio_poll() != 0) {
		return -1;
	}

	ar823x_emac_wrreg(EMAC_MAC_MDIO_DATA, data & 0xffff);
	ar823x_emac_wrreg(EMAC_MAC_MDIO_CTRL,
			  (rg << 5) | ph | AR823x_MDIO_START |
			  ((mdc_clk_divisor & MacMdioCtrlClkMask) << MacMdioCtrlClkShift) |
			  AR823x_MDIO_NORM);
	if (ar823x_mdio_poll() != 0) {
		return -1;
	}

	// return without waiting for final completion
	return 0;
}

/*********************************************************************
 Name:      ar823x_mdio_read
 Purpose:   mdio read routine for AR823x device
 Notes:     This is a blocking call since we require
            more than one cycle to complete the write.
            checks for completion first
*********************************************************************/
int ar823x_mdio_read(struct mii_bus *bus, int phyAddr, int regAddr)
{
	int data;
	u32 highAddr = regAddr >> 9;
	// need to swizzle the bits into arasan's fields which are different          
	u32 rg = (regAddr & 0x3c) >> 1;
	u32 ph = (regAddr & 0x1c0) >> 6;

	if (ar823x_mdio_poll() != 0) {
		return -1;
	}

	ar823x_emac_wrreg(EMAC_MAC_MDIO_DATA, highAddr);
	ar823x_emac_wrreg(EMAC_MAC_MDIO_CTRL, phyAddr | AR823x_MDIO_START |
			  ((mdc_clk_divisor & MacMdioCtrlClkMask) << MacMdioCtrlClkShift) |
			  AR823x_MDIO_READ | AR823x_MDIO_PAGE);
	if (ar823x_mdio_poll() != 0) {
		return -1;
	}

	ar823x_emac_wrreg(EMAC_MAC_MDIO_CTRL, (rg << 5) | AR823x_MDIO_START |
			  ((mdc_clk_divisor & MacMdioCtrlClkMask) << MacMdioCtrlClkShift) |
			  AR823x_MDIO_READ | ph | AR823x_MDIO_NORM);
	if (ar823x_mdio_poll() != 0) {
		return -1;
	}

	data = ar823x_emac_rdreg(EMAC_MAC_MDIO_DATA);

	ar823x_emac_wrreg(EMAC_MAC_MDIO_CTRL,
			  ((rg | AR823x_MDIO_HIGH_WORD) << 5) |
			  AR823x_MDIO_START | AR823x_MDIO_READ | ph |
			  ((mdc_clk_divisor & MacMdioCtrlClkMask) << MacMdioCtrlClkShift) |
			  AR823x_MDIO_NORM);

	if (ar823x_mdio_poll() != 0) {
		return -1;
	}

	data = data | (ar823x_emac_rdreg(EMAC_MAC_MDIO_DATA) << 16);
	return data;
}

static void ar823x_reset(const u32 phy_addr)
{
	int reset = AR823x_MASK_CTL_RESET;

	// do a clean reset and wait for completion 
	ar823x_mdio_write32(phy_addr, AR823x_MASK_CTL, AR823x_MASK_CTL_RESET);
	while (reset & AR823x_MASK_CTL_RESET) {
		reset = ar823x_mdio_read(NULL, phy_addr, AR823x_MASK_CTL);
	}
}

static u32 ar8236_init(const u32 phy_addr, const u32 devID)
{
	printk("Detected AR823x Switch %d:%x - set for MII, 100FD\n", phy_addr, devID);

	// do a softreset
	ar823x_mdio_write32(phy_addr, AR823x_MODE_CTRL, AR8236_MODE_MII_PHY);

	ar823x_reset(phy_addr);

	ar823x_mdio_write32(phy_addr, AR8236_FLOOD_MASK, AR8236_FLOOD_MASK_DEF);
	ar823x_mdio_write32(phy_addr, AR8236_PORT0_CTRL, AR8236_PORT0_CTRL_DEF);

	return phy_addr;
}

static u32 ar8327_init(const u32 phy_addr, const u32 devID)
{
	printk("Detected AR8327 Switch %d:%x - set for RGMII, 1000FD\n", phy_addr, devID);

	// do a softreset
	ar823x_mdio_write32(phy_addr, AR823x_MODE_CTRL, AR8327_MODE_RGMII_PHY);

	ar823x_reset(phy_addr);

	ar823x_mdio_write32(phy_addr, AR823x_MASK_CTL, AR8327_MASK_CLEAR_DEF);
	ar823x_mdio_write32(phy_addr, AR8327_PWS_CTRL, AR8327_PWS_CTRL_DEF);
	ar823x_mdio_write32(phy_addr, AR8327_FWCTL_CTRL, AR8327_FWCTL_CTRL_DEF);
	ar823x_mdio_write32(phy_addr, AR8327_PORT6_CTRL, AR8327_PORT6_CTRL_DEF);
	ar823x_mdio_write32(phy_addr, AR8327_PORT0_CTRL, AR8327_PORT0_CTRL_DEF);

	//set the register 0xe00000b4 for RGMII Dll control register
	*(volatile u32 *)(0xe00000b4) = 0x86868f8f;

	return phy_addr;
}

/*********************************************************************
Name:      ar823x_init
Purpose:   Check for Atheros 823x switch, return pointer to device
if found, NULL otherwise 
Notes:     pass phy addr as -1 to scan for phy
 *********************************************************************/
int ar823x_init(int phy_addr)
{
	u32 devID;
	u32 addr;

	// need to scan?
	if (phy_addr == 32) {
		addr = AR823x_MIN_PHY_NUM;
	} else {
		addr = phy_addr;
	}

	while (addr <= AR823x_MAX_PHY_NUM) {
		devID = ar823x_mdio_read(NULL, addr, AR823x_MASK_CTL);
		if ((devID & 0xff00) == 0x300) {
			return ar8236_init(addr, devID);
		} else if ((devID & 0xff00) == 0x1200) {
			return ar8327_init(addr, devID);
		}

		if (phy_addr == 32) {
			addr++;
		} else {
			// not found on passed addr
			break;
		}
	}
	return -1;
}
