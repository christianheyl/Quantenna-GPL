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

#ifndef __TOPAZ_IPPROTO_TABLE_H
#define __TOPAZ_IPPROTO_TABLE_H

#include <common/topaz_emac.h>
#include <qtn/mproc_sync_base.h>

union topaz_ipprt_entry {
	uint32_t raw;
	struct {
		uint32_t	out_node	:7,
				out_port	:4,
				valid		:1,
				__unused	:21;
	} data;
};


#define TOPAZ_IPPRT_ENTRY_INIT	{ 0 }

RUBY_INLINE void topaz_ipprt_set_entry(uint8_t emac, uint8_t ip_proto, union topaz_ipprt_entry e)
{
	uint32_t emac_base = emac ? RUBY_ENET1_BASE_ADDR : RUBY_ENET0_BASE_ADDR;
	qtn_mproc_sync_mem_write(emac_base + TOPAZ_EMAC_IP_PROTO_ENTRY(ip_proto), e.raw);
}

RUBY_INLINE void topaz_ipprt_clear_entry(uint8_t emac, uint8_t ip_proto)
{
	uint32_t emac_base = emac ? RUBY_ENET1_BASE_ADDR : RUBY_ENET0_BASE_ADDR;
	qtn_mproc_sync_mem_write(emac_base + TOPAZ_EMAC_IP_PROTO_ENTRY(ip_proto), 0x0);
}

RUBY_INLINE void topaz_ipprt_clear_all_entries(uint8_t emac)
{
	int proto;
	for (proto = 0; proto < TOPAZ_EMAC_IP_PROTO_ENTRIES; ++proto) {
		topaz_ipprt_clear_entry(emac, proto);
	}
}

RUBY_INLINE void topaz_ipprt_set(uint8_t emac, uint8_t ip_proto, uint8_t out_port, uint8_t out_node)
{
	union topaz_ipprt_entry e = TOPAZ_IPPRT_ENTRY_INIT;
	e.data.out_node = out_node;
	e.data.out_port = out_port;
	e.data.valid = 1;
	topaz_ipprt_set_entry(emac, ip_proto, e);
}

RUBY_INLINE union topaz_ipprt_entry topaz_ipprt_get_entry(uint8_t emac, uint8_t ip_proto)
{
	uint32_t emac_base = emac ? RUBY_ENET1_BASE_ADDR : RUBY_ENET0_BASE_ADDR;
	union topaz_ipprt_entry e;
	e.raw = qtn_mproc_sync_mem_read(emac_base + TOPAZ_EMAC_IP_PROTO_ENTRY(ip_proto));
	return e;
}

#ifndef TOPAZ_TEST_ASSERT_EQUAL
# define TOPAZ_TEST_ASSERT_EQUAL(a, b)	if ((a) != (b)) { return -1; }
#endif
RUBY_INLINE int topaz_ipprt_entry_bitfield_test(const union topaz_ipprt_entry *e)
{
	TOPAZ_TEST_ASSERT_EQUAL(MS(e->raw, TOPAZ_EMAC_IP_PROTO_OUT_NODE), e->data.out_node);
	TOPAZ_TEST_ASSERT_EQUAL(MS(e->raw, TOPAZ_EMAC_IP_PROTO_OUT_PORT), e->data.out_port);
	TOPAZ_TEST_ASSERT_EQUAL(MS(e->raw, TOPAZ_EMAC_IP_PROTO_VALID), e->data.valid);

	return 0;
}

#endif	/* __TOPAZ_IPPROTO_TABLE_H */

