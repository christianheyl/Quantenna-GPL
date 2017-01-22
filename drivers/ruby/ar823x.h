#ifndef __AR823x_h__
#define __AR823x_h__
/*
 *  ar823x.h
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

////////////////////////////////////////////////////////////////////
// NOTE - we do not check for valid base in mdio access routines
// use must ensure device is initialized and valid prior
// to using MDIO funtions
///////////////////////////////////////////////////////////////////

/*********************************************************************
 Name:      ar823x_init
 Purpose:   Check for Atheros 823x switch, return pointer to device
            if found, NULL otherwise 
 Notes:     pass phy addr as -1 to scan for phy
*********************************************************************/
int ar823x_init(int phy_addr);

/*********************************************************************
 Name:      ar823x_mdio_read
 Purpose:   mdio read routine for AR823x device
 Notes:     This is a blocking call since we require
            more than one cycle to complete the write.
            checks for completion first
*********************************************************************/
int ar823x_mdio_read(struct mii_bus *bus, int phyAddr, int regAddr);

/*********************************************************************
 Name:      ar823x_mdio_write
 Purpose:   mdio write routine for AR823x device
 Notes:     reg_addr[1]=1 determines high word
*********************************************************************/
int ar823x_mdio_write(struct mii_bus *bus,int phyAddr, int regAddr, u16 data);

/*********************************************************************
 Name:      ar823x_mdio_write32
 Purpose:   mdio write routine for AR823x device
 Notes:     This is a partial blocking call since we require
            more than one cycle to complete the write.
            checks for completion first
*********************************************************************/
int ar823x_mdio_write32(int phyAddr, int regAddr, u32 data);

/*********************************************************************
 Name:      ar823x_mdio_poll
 Purpose:   mdio poll routine for AR823x device
 Notes:     Checks for mdio operation complete
*********************************************************************/
int ar823x_mdio_poll(void);

#endif // __AR823x_h__
