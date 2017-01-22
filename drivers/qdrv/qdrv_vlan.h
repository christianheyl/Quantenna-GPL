/**
  Copyright (c) 2008 - 2014 Quantenna Communications Inc
  All Rights Reserved

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

 **/

#ifndef _QDRV_VLAN_H
#define _QDRV_VLAN_H

struct qdrv_node *qdrv_vlan_alloc_group(struct qdrv_vap *qv, uint16_t vid);
void qdrv_vlan_free_group(struct qdrv_node *);

struct qdrv_node *qdrv_vlan_find_group_noref(struct qdrv_vap *qv, uint16_t vid);

#endif
