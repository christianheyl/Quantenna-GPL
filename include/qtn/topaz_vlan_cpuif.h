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

#ifndef __TOPAZ_VLAN_CPUIF_PLATFORM_H
#define __TOPAZ_VLAN_CPUIF_PLATFORM_H

#include <common/topaz_platform.h>
#include <qtn/mproc_sync_base.h>

union topaz_vlan_entry {
	uint32_t raw;
	struct {
		uint32_t	out_node	:7,
				out_port	:3,
				valid		:1,
				__unused	:22;
	} data;
};

#define TOPAZ_VLAN_ENTRY_INIT	{ 0 }

RUBY_INLINE void topaz_vlan_set_entry(uint16_t vlan_id, union topaz_vlan_entry e)
{
	qtn_mproc_sync_mem_write(TOPAZ_VLAN_ENTRY_ADDR(vlan_id), e.raw);
}

RUBY_INLINE void topaz_vlan_clear_entry(uint16_t vlan_id)
{
	qtn_mproc_sync_mem_write(TOPAZ_VLAN_ENTRY_ADDR(vlan_id), 0x0);
}

RUBY_INLINE void topaz_vlan_clear_all_entries(void)
{
	int vlan_id;
	for (vlan_id = 0; vlan_id < TOPAZ_VLAN_ENTRIES; ++vlan_id) {
		topaz_vlan_clear_entry(vlan_id);
	}
}

RUBY_INLINE void topaz_vlan_set(uint16_t vlan_id, uint8_t out_port, uint8_t out_node)
{
	union topaz_vlan_entry e = TOPAZ_VLAN_ENTRY_INIT;
	e.data.out_node = out_node;
	e.data.out_port = out_port;
	e.data.valid = 1;
	topaz_vlan_set_entry(vlan_id, e);
}

RUBY_INLINE union topaz_vlan_entry topaz_vlan_get_entry(uint16_t vlan_id)
{
	union topaz_vlan_entry e;
	e.raw = qtn_mproc_sync_mem_read(TOPAZ_VLAN_ENTRY_ADDR(vlan_id));
	return e;
}

#ifndef TOPAZ_TEST_ASSERT_EQUAL
# define TOPAZ_TEST_ASSERT_EQUAL(a, b)	if ((a) != (b)) { return -1; }
#endif
RUBY_INLINE int topaz_vlan_entry_bitfield_test(const union topaz_vlan_entry *e)
{
	TOPAZ_TEST_ASSERT_EQUAL(MS(e->raw, TOPAZ_VLAN_OUT_NODE), e->data.out_node);
	TOPAZ_TEST_ASSERT_EQUAL(MS(e->raw, TOPAZ_VLAN_OUT_PORT), e->data.out_port);
	TOPAZ_TEST_ASSERT_EQUAL(MS(e->raw, TOPAZ_VLAN_VALID), e->data.valid);

	return 0;
}

#endif	/* __TOPAZ_VLAN_CPUIF_PLATFORM_H */

