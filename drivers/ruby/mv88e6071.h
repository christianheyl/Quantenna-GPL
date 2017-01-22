/*
 *  Driver for Marvell 88E6071 switch
 *
 *  Copyright (c) Quantenna Communications, Incorporated 2013
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
#ifndef __QTN_MV88E6071_H__
#define __QTN_MV88E6071_H__

#include "emac_lib.h"

int mv88e6071_init(struct emac_common *privc);
int mv88e6071_mdio_read(struct mii_bus *bus, int phy_addr, int reg);
int mv88e6071_mdio_write(struct mii_bus *bus, int phy_addr, int reg, u16 value);

#endif // __QTN_MV88E6071_H__

