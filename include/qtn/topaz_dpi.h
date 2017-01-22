/*
 * (C) Copyright 2012 Quantenna Communications Inc.
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

#ifndef __TOPAZ_DPI_H
#define __TOPAZ_DPI_H

#include <common/topaz_emac.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <compat.h>

struct topaz_dpi_field_def {
	uint32_t val;
	uint32_t mask;
	union topaz_emac_rx_dpi_ctrl ctrl;
};

#define TOPAZ_DPI_ANCHOR_FRAME_START	0x0000
#define TOPAZ_DPI_ANCHOR_VLAN0		0x0001
#define TOPAZ_DPI_ANCHOR_VLAN1		0x0002
#define TOPAZ_DPI_ANCHOR_VLAN2		0x0003
#define TOPAZ_DPI_ANCHOR_VLAN3		0x0004
#define TOPAZ_DPI_ANCHOR_OTHER		0x0005
#define TOPAZ_DPI_ANCHOR_LLC		0x0006
#define TOPAZ_DPI_ANCHOR_IPV4		0x0007
#define TOPAZ_DPI_ANCHOR_IPV6		0x0008
#define TOPAZ_DPI_ANCHOR_TCP		0x0009
#define TOPAZ_DPI_ANCHOR_UDP		0x000a

#define TOPAZ_DPI_CMPOP_EQ		0x00
#define TOPAZ_DPI_CMPOP_NE		0x01
#define TOPAZ_DPI_CMPOP_GT		0x02
#define TOPAZ_DPI_CMPOP_LT		0x03

#define TOPAZ_DPI_DISABLE		0x0
#define TOPAZ_DPI_ENABLE		0x1

struct topaz_dpi_filter_request {
	uint8_t out_port;
	uint8_t out_node;
	uint8_t tid;
	struct topaz_dpi_field_def *fields;
	unsigned int field_count;
	struct in6_addr srcaddr;
	struct in6_addr destaddr;
	uint16_t srcport;
	uint16_t destport;
};

int topaz_dpi_filter_add(unsigned int emac,
		const struct topaz_dpi_filter_request *req);
void topaz_dpi_filter_del(unsigned int emac, int filter_no);
int topaz_dpi_init(unsigned int emac);

static inline void topaz_dpi_iptuple_poll_complete(unsigned long base)
{
	while (readl(base + TOPAZ_EMAC_RX_DPI_IPT_MEM_COM)
			& (TOPAZ_EMAC_RX_DPI_IPT_MEM_COM_WRITE | TOPAZ_EMAC_RX_DPI_IPT_MEM_COM_READ)) {}

}

static inline void topaz_dpi_iptuple_read_entry(unsigned long base, uint8_t entry)
{
	writel(TOPAZ_EMAC_RX_DPI_IPT_MEM_COM_READ | SM(entry, TOPAZ_EMAC_RX_DPI_IPT_MEM_COM_ENT),
			base + TOPAZ_EMAC_RX_DPI_IPT_MEM_COM);
	topaz_dpi_iptuple_poll_complete(base);
}

static inline void topaz_dpi_iptuple_write_entry(unsigned long base, uint8_t entry)
{
	writel(TOPAZ_EMAC_RX_DPI_IPT_MEM_COM_WRITE | SM(entry, TOPAZ_EMAC_RX_DPI_IPT_MEM_COM_ENT),
			base + TOPAZ_EMAC_RX_DPI_IPT_MEM_COM);
	topaz_dpi_iptuple_poll_complete(base);
}

#endif	/* __TOPAZ_DPI_H */

