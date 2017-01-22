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

#ifndef __TOPAZ_FWT_CPUIF_PLATFORM_H
#define __TOPAZ_FWT_CPUIF_PLATFORM_H

#include <common/topaz_platform.h>
#include <qtn/mproc_sync_base.h>
#include <qtn/topaz_tqe_cpuif.h>
#include <qtn/qtn_net_packet.h>
#include <qtn/lhost_muc_comm.h>

#if defined(__linux__)
	#define TOPAZ_FWT_LOCAL_CPU	TOPAZ_FWT_LOOKUP_LHOST
#elif defined(ARCSHELL)
	#define	TOPAZ_FWT_LOCAL_CPU	TOPAZ_FWT_LOOKUP_LHOST
#elif defined(MUC_BUILD)
	#define	TOPAZ_FWT_LOCAL_CPU	TOPAZ_FWT_LOOKUP_MUC
#elif defined(DSP_BUILD)
	#define	TOPAZ_FWT_LOCAL_CPU	TOPAZ_FWT_LOOKUP_DSP
#elif defined(AUC_BUILD)
	#define	TOPAZ_FWT_LOCAL_CPU	TOPAZ_FWT_LOOKUP_AUC
#else
	#error No TOPAZ_FWT_LOCAL_CPU set
#endif

#define	TOPAZ_FWT_LOOKUP_REG		__TOPAZ_FWT_LOOKUP_REG(TOPAZ_FWT_LOCAL_CPU)
#define	TOPAZ_FWT_LOOKUP_MAC_LO		__TOPAZ_FWT_LOOKUP_MAC_LO(TOPAZ_FWT_LOCAL_CPU)
#define	TOPAZ_FWT_LOOKUP_MAC_HI		__TOPAZ_FWT_LOOKUP_MAC_HI(TOPAZ_FWT_LOCAL_CPU)
/* Hardware limitation for node entries */
#define TOPAZ_FWT_MAX_NODE_ENTRY (6)

/** Macro to get a number with its 'bits' LSB bits set */
#define SET_LSB_BITS(bits) ((1 << (bits)) - 1)

/** Macro to update a bit-field with a new value */
#define TOPAZ_FWT_SET_BIT_FIELD(var, start_bit, width, value) \
		(((var) & ~(SET_LSB_BITS(width)  << (start_bit))) | \
				((value) & SET_LSB_BITS(width)) << (start_bit))

/** Macro to extract a bit-field */
#define TOPAZ_FWT_GET_BIT_FIELD(data, start_offset, width)\
		(((data) >> (start_offset)) & SET_LSB_BITS(width))

#if defined(ARCSHELL)
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
#define ETH_ALEN 6
#define inline _Inline
#define unlikely
#define likely
#elif defined(MUC_BUILD)
#define ETH_ALEN 6
#endif

/*
 * Forwarding Table manipulation
 *
 * FWT has 2048 entries. First 1024 are mac address crc hash done by hardware.
 * Next 1024 are linked list (nxt_entry) for when hash collision occurs.
 *
 * Basic forwarding table flow is:
 * 1) Packet received
 * 2) crc32 of incoming packet macaddr forms a 10 bit number [0 .. 1023]
 * 3) This number is the index of the first FWT entry in the physical table. MAC address is compared.
 *   a) If MAC matches, this is the fwt entry
 *   b) If not, follow next entry index. Must be [1024 .. 2047], repeat.
 * 4) Interpret FWT if present
 *
 * In sw maintain tailqs to know which slots are occupied
 */

/*
 * FWT timestamp field is 10 bits. FWT clock is at 250MHz on BBIC4 ASIC.
 * UNIT affects clock ticks per timer reg clock tick.
 * SCALE affects shifting of the above register when applied to the timestamp field
 * when updating it.
 * ASIC: Values of 0xe unit and 13 (0xd) scale result in a time wrap time of 1 minute.
 * FPGA: 0xc unit, 0xa scale results in very rougly 1m wrap
 */

#define TOPAZ_FWT_TIMESTAMP_BITS	10
#define TOPAZ_FWT_TIMESTAMP_MASK	((1 << TOPAZ_FWT_TIMESTAMP_BITS) - 1)

#if TOPAZ_FPGA_PLATFORM
	#define TOPAZ_FWT_TIMESTAMP_UNIT	0xc
	#define TOPAZ_FWT_TIMESTAMP_SCALE	0xa
#else
	#define TOPAZ_FWT_TIMESTAMP_UNIT	0x1b /* (3500 ticks 14 usec) */
	#define TOPAZ_FWT_TIMESTAMP_SCALE	0x13 /* (2^19) */
	/*
	 * Resolution Calculation:
	 * (per tick) * (FWT clock [sec]) * (bits store location) = Resolution[sec]
	 * (3500)     * (1 / (250*10^6))  * (2^19)                = 7.34 [sec]
	 * */

	#define TOPAZ_FWT_RESOLUTION_MSEC	7340 /* Derive from unit & scale */
#endif

RUBY_INLINE uint32_t topaz_fwt_get_scaled_timestamp(void)
{
#ifdef CONSOLE_TEST
	return 0;
#else
	uint32_t tsr = qtn_mproc_sync_mem_read(TOPAZ_FWT_TIME_STAMP_CNT);
	return (tsr >> TOPAZ_FWT_TIMESTAMP_SCALE) & TOPAZ_FWT_TIMESTAMP_MASK;
#endif
}

union topaz_fwt_entry {
	struct {
		uint32_t	word0;	/* macaddr[30:1] */
		uint32_t	word1;	/* valid bit(31), portal(30), next_index(27:16), macaddr[47:32](15:0) */
		uint32_t	word2;	/* out_node + valid 0, 1, 2, 3 */
		uint32_t	word3;	/* out_node + valid 4, 5, outport, timestamp(9:0) */
	} raw;
	struct {
		uint8_t		mac_le[ETH_ALEN];
		uint16_t	__unused1	:1,
				next_index	:11,
				__unused2	:2,
				portal		:1,
				valid		:1;
		uint8_t		out_node_0	:7,
				out_node_vld_0	:1;
		uint8_t		out_node_1	:7,
				out_node_vld_1	:1;
		uint8_t		out_node_2	:7,
				out_node_vld_2	:1;
		uint8_t		out_node_3	:7,
				out_node_vld_3	:1;
		uint16_t	timestamp	:10,
				out_port	:4,
				__unused3	:2;
		uint8_t		out_node_4	:7,
				out_node_vld_4	:1;
		uint8_t		out_node_5	:7,
				out_node_vld_5	:1;
	} data;
};

#define FWT_ZERO_ENTRY_INIT	{ { 0, 0, 0, 0 } }

RUBY_INLINE union topaz_fwt_entry *topaz_fwt_get_hw_entry(uint16_t index)
{
#ifdef CONSOLE_TEST
	extern union topaz_fwt_entry test_hw_fwt[TOPAZ_FWT_HW_TOTAL_ENTRIES];
	return &test_hw_fwt[index];
#else
	union topaz_fwt_entry *e;
	e = (union topaz_fwt_entry *)(TOPAZ_FWT_TABLE_BASE + index * sizeof(*e));
	return e;
#endif
}

RUBY_INLINE int topaz_fwt_is_valid(const union topaz_fwt_entry *e)
{
	return e->raw.word1 & TOPAZ_FWT_ENTRY_VALID;
}

RUBY_INLINE uint16_t topaz_fwt_next_index(const union topaz_fwt_entry *e)
{
	return MS(e->raw.word1, TOPAZ_FWT_ENTRY_NXT_ENTRY);
}

RUBY_INLINE const uint8_t *topaz_fwt_macaddr(const union topaz_fwt_entry *e)
{
	return (void *) e;
}

RUBY_INLINE void topaz_fwt_set_next_index(union topaz_fwt_entry *e, uint16_t index)
{
	unsigned long word1 = e->raw.word1 & ~TOPAZ_FWT_ENTRY_NXT_ENTRY;
	e->raw.word1 = word1 | SM(index, TOPAZ_FWT_ENTRY_NXT_ENTRY);
}

RUBY_INLINE void topaz_fwt_copy_entry(union topaz_fwt_entry *dest, const union topaz_fwt_entry *src,
					int words)
{
	int i;
#pragma Off(Behaved)
	uint32_t *d = &dest->raw.word0;
	const uint32_t *s = &src->raw.word0;
#pragma On(Behaved)

	for (i = 0; i < words; i++) {
		*d++ = *s++;
	}
}

RUBY_INLINE void topaz_fwt_insert_entry(const union topaz_fwt_entry *newent, uint16_t index)
{
	union topaz_fwt_entry *hw_ent = topaz_fwt_get_hw_entry(index);
	topaz_fwt_copy_entry(hw_ent, newent, 4);
}

/*
 * Software FWT mirror. Used by MuC for Rx path
 * acceleration, without accessing the FWT memory
 */
union topaz_fwt_sw_entry {
	uint16_t raw;
	struct {
		uint8_t	valid		:1,
			mcast		:1,
			vsp		:1,
			portal		:1,
			port		:4;
		uint8_t	__pad		:1,
			node		:7;
	} unicast;
	struct {
		uint16_t valid		:1,
			 mcast		:1,
			 index		:14;
	} multicast;
};

/* 23 bits of multicast ipv4 -> mac leaves 5 bits of ambiguity */
#define TOPAZ_FWT_SW_IP_ALIAS_ENTRIES	32
#define TOPAZ_FWT_SW_NODE_MAX		128
#define TOPAZ_BITS_PER_WD		(32)
#define TOPAZ_FWT_SW_NODE_BITMAP_SIZE	(TOPAZ_FWT_SW_NODE_MAX / TOPAZ_BITS_PER_WD)

struct topaz_fwt_sw_mcast_entry {
	uint32_t node_bitmap[TOPAZ_FWT_SW_NODE_BITMAP_SIZE];
	uint8_t port_bitmap;
	uint8_t flood_forward;
	uint8_t seen;
#ifdef CONFIG_TOPAZ_DBDC_HOST
	uint8_t dev_bitmap;
#else
	uint8_t __pad[1];
#endif
};

#ifdef CONFIG_TOPAZ_DBDC_HOST
RUBY_INLINE int topaz_fwt_sw_mcast_dev_is_set(struct topaz_fwt_sw_mcast_entry *const e,
						const uint8_t dev_id)
{
	return (e->dev_bitmap & (1 << dev_id));
}

RUBY_INLINE int topaz_fwt_sw_mcast_dev_is_empty(struct topaz_fwt_sw_mcast_entry *const e)
{
	return (e->dev_bitmap == 0);
}

RUBY_INLINE void topaz_fwt_sw_mcast_dev_set(struct topaz_fwt_sw_mcast_entry *const e,
						const uint8_t dev_id)
{
	e->dev_bitmap |= (1 << dev_id);
}

RUBY_INLINE void topaz_fwt_sw_mcast_dev_clear(struct topaz_fwt_sw_mcast_entry *const e,
						const uint8_t dev_id)
{
	e->dev_bitmap &= ~(1 << dev_id);
}
#endif

struct topaz_fwt_sw_alias_table {
	int16_t mcast_entry_index[TOPAZ_FWT_SW_IP_ALIAS_ENTRIES];
};

RUBY_INLINE int8_t topaz_fwt_mcast_to_ip_alias(const void *addr, uint16_t ether_type)
{
	if (ether_type == htons(ETHERTYPE_IP)) {
		return qtn_mcast_ipv4_alias(addr);
	} else if (ether_type == htons(ETHERTYPE_IPV6)) {
		return 0;
	} else {
		return -1;
	}
}

RUBY_INLINE int topaz_fwt_sw_alias_table_index_valid(int16_t index)
{
	return index >= 0 && index < TOPAZ_FWT_MCAST_ENTRIES;
}

RUBY_INLINE int topaz_fwt_sw_mcast_entry_index_valid(int16_t index)
{
	return index >= 0 && index < TOPAZ_FWT_MCAST_ENTRIES;
}

RUBY_INLINE uint8_t topaz_fwt_sw_mcast_entry_nodes_clear(const struct topaz_fwt_sw_mcast_entry *e)
{
	unsigned int i;
	for (i = 0; i < TOPAZ_FWT_SW_NODE_BITMAP_SIZE; i++) {
		if (e->node_bitmap[i]) {
			return 0;
		}
	}
	return 1;
}

RUBY_INLINE int topaz_fwt_sw_alias_table_empty(const struct topaz_fwt_sw_alias_table *alias_table)
{
	unsigned int i;
	for (i = 0; i < TOPAZ_FWT_SW_IP_ALIAS_ENTRIES; i++) {
		if (topaz_fwt_sw_mcast_entry_index_valid(alias_table->mcast_entry_index[i])) {
			return 0;
		}
	}
	return 1;
}

RUBY_INLINE int topaz_fwt_sw_mcast_port_is_set(const uint8_t port_bitmap, const uint8_t port)
{
	return (port_bitmap & (1 << port));
}

RUBY_INLINE void topaz_fwt_sw_mcast_port_set(struct topaz_fwt_sw_mcast_entry *const e,
						const uint8_t port)
{
	e->port_bitmap |= (1 << port);
}

RUBY_INLINE void topaz_fwt_sw_mcast_port_clear(struct topaz_fwt_sw_mcast_entry *const e,
						const uint8_t port)
{
	e->port_bitmap &= ~(1 << port);
}

RUBY_INLINE int topaz_fwt_sw_mcast_port_has_nodes(const uint8_t port)
{
	return (port == TOPAZ_TQE_WMAC_PORT);
}

RUBY_INLINE void
topaz_fwt_sw_mcast_flood_forward_set(struct topaz_fwt_sw_mcast_entry *const e, const uint8_t enable)
{
	e->flood_forward = enable;
}

RUBY_INLINE int
topaz_fwt_sw_mcast_is_flood_forward(const struct topaz_fwt_sw_mcast_entry *const e)
{
	return (e->flood_forward);
}

RUBY_INLINE uint32_t topaz_fwt_sw_mcast_do_per_node(
	void (*handler)(const void *token1, void *token2, uint8_t node, uint8_t port, uint8_t tid),
	const struct topaz_fwt_sw_mcast_entry *mcast_ent,
	const void *token1, void *token2, uint8_t in_node, uint8_t port, uint8_t tid)
{
	uint8_t node;
	uint8_t node_cnt = 0;
	uint32_t bitmap;
	uint8_t i;
	uint8_t j;

	for (i = 0; i < TOPAZ_FWT_SW_NODE_BITMAP_SIZE; i++) {
		bitmap = mcast_ent->node_bitmap[i];
		j = 0;
		while (bitmap) {
			if (bitmap & 0x1) {
				node = (i * TOPAZ_BITS_PER_WD) + j;
				if ((in_node == 0) || (node != in_node)) {
					handler(token1, token2, port, node, tid);
					node_cnt++;
				}
			}
			bitmap >>= 1;
			j++;
		}
	}

	return node_cnt;
}

RUBY_INLINE int topaz_fwt_sw_mcast_node_is_set(const struct topaz_fwt_sw_mcast_entry *const e,
						const uint8_t port, const uint8_t node)
{
	if (port == TOPAZ_TQE_WMAC_PORT) {
		return (e->node_bitmap[node / TOPAZ_BITS_PER_WD] &
				(1 << (node % TOPAZ_BITS_PER_WD)));
	}
	return 0;
}

RUBY_INLINE void topaz_fwt_sw_mcast_node_set(struct topaz_fwt_sw_mcast_entry *const e,
						const uint8_t port, const uint16_t node)
{
	if (port == TOPAZ_TQE_WMAC_PORT) {
		e->node_bitmap[node / TOPAZ_BITS_PER_WD] |= (1 << (node % TOPAZ_BITS_PER_WD));
	}
}

RUBY_INLINE void topaz_fwt_sw_mcast_node_clear(struct topaz_fwt_sw_mcast_entry *const e,
						const uint8_t port, const uint16_t node)
{
	if (port == TOPAZ_TQE_WMAC_PORT) {
		e->node_bitmap[node / TOPAZ_BITS_PER_WD] &= ~(1 << (node % TOPAZ_BITS_PER_WD));
		if (!topaz_fwt_sw_mcast_is_flood_forward(e) &&
				topaz_fwt_sw_mcast_entry_nodes_clear(e)) {
			topaz_fwt_sw_mcast_port_clear(e, port);
		}
	}
}

RUBY_INLINE union topaz_fwt_sw_entry *topaz_fwt_sw_entry_get(uint16_t index)
{
	union topaz_fwt_sw_entry *fwt = (void *) (RUBY_SRAM_BEGIN + TOPAZ_FWT_SW_START);
	return &fwt[index];
}

RUBY_INLINE struct topaz_fwt_sw_mcast_entry *topaz_fwt_sw_mcast_ff_entry_get(void)
{
	return (void *)(RUBY_DRAM_BEGIN + TOPAZ_FWT_MCAST_TQE_FF_BASE);
}

RUBY_INLINE struct topaz_fwt_sw_mcast_entry *topaz_fwt_sw_mcast_entry_get(uint16_t index)
{
	struct topaz_fwt_sw_mcast_entry *fwt = (void *) (RUBY_DRAM_BEGIN + TOPAZ_FWT_MCAST_TQE_BASE);
	return &fwt[index];
}

RUBY_INLINE struct topaz_fwt_sw_alias_table *topaz_fwt_sw_alias_table_get(uint16_t index)
{
	struct topaz_fwt_sw_alias_table *fwt = (void *) (RUBY_DRAM_BEGIN + TOPAZ_FWT_MCAST_IPMAP_BASE);
	return &fwt[index];
}

RUBY_INLINE uint8_t topaz_fwt_sw_count_bits(uint32_t x)
{
	uint8_t bits_set = 0;

	while (x) {
		bits_set++;
		x &= x - 1;
	}

	return bits_set;
}

RUBY_INLINE uint32_t topaz_fwt_sw_mcast_enqueues(const struct topaz_fwt_sw_mcast_entry *const e,
		uint8_t port_bitmap, const uint8_t in_port, const uint8_t in_node)
{
	uint32_t enqueues = 0;
	uint8_t i;

	/* Exclude input port. If WMAC, the port doesn't contribute, only nodes. */
	port_bitmap &= ~((1 << in_port) | (1 << TOPAZ_TQE_WMAC_PORT));
	enqueues += topaz_fwt_sw_count_bits(port_bitmap);

	/* add wmac nodes */
	for (i = 0; i < ARRAY_SIZE(e->node_bitmap) ; i++) {
		enqueues += topaz_fwt_sw_count_bits(e->node_bitmap[i]);
	}

	/* must exclude the input node */
	if (topaz_fwt_sw_mcast_node_is_set(e, in_port, in_node)) {
		--enqueues;
	}

	return enqueues;
}

RUBY_INLINE void __topaz_fwt_hash_set(int enable)
{
	uint32_t reg = enable ? TOPAZ_FWT_HASH_CTRL_ENABLE : 0;
	qtn_mproc_sync_mem_write(TOPAZ_FWT_HASH_CTRL, reg);
}

RUBY_INLINE void __topaz_fwt_hw_lookup_write_be(const uint8_t *mac_be)
{
	uint32_t lo = mac_be[5] | (mac_be[4] << 8) | (mac_be[3] << 16) | (mac_be[2] << 24);
	uint32_t hi = mac_be[1] | (mac_be[0] << 8);

	qtn_mproc_sync_mem_write(TOPAZ_FWT_LOOKUP_MAC_LO, lo);
	qtn_mproc_sync_mem_write(TOPAZ_FWT_LOOKUP_MAC_HI, hi);
	qtn_mproc_sync_mem_write(TOPAZ_FWT_LOOKUP_REG, SM(1, TOPAZ_FWT_LOOKUP_TRIG));
}

RUBY_INLINE void __topaz_fwt_hw_lookup_write_le(const uint8_t *mac_le)
{
	uint32_t lo = mac_le[0] | (mac_le[1] << 8) | (mac_le[2] << 16) | (mac_le[3] << 24);
	uint32_t hi = mac_le[4] | (mac_le[5] << 8);

	qtn_mproc_sync_mem_write(TOPAZ_FWT_LOOKUP_MAC_LO, lo);
	qtn_mproc_sync_mem_write(TOPAZ_FWT_LOOKUP_MAC_HI, hi);
	qtn_mproc_sync_mem_write(TOPAZ_FWT_LOOKUP_REG, SM(1, TOPAZ_FWT_LOOKUP_TRIG));
}

RUBY_INLINE void topaz_fwt_reverse_mac(uint8_t *dest, const uint8_t *src)
{
	int i;
	for (i = 0; i < ETH_ALEN; i++) {
		dest[ETH_ALEN - i - 1] = src[i];
	}
}

RUBY_INLINE void topaz_fwt_setup_entry(union topaz_fwt_entry *ent, const uint8_t *mac_be,
					uint8_t out_port, const uint8_t *out_nodes,
					unsigned int out_node_count, uint8_t portal)
{
	ent->raw.word0 = 0;
	ent->raw.word1 = 0;
	ent->raw.word2 = 0;
	ent->raw.word3 = 0;

#pragma Off(Behaved)
	topaz_fwt_reverse_mac(ent->data.mac_le, mac_be);
#pragma On(Behaved)
	ent->data.valid = 1;
	ent->data.portal = portal;

#define __topaz_fwt_setup_entry_set_out_node(x)			\
	do {							\
		if (x < out_node_count) {			\
			ent->data.out_node_##x = out_nodes[x];	\
			ent->data.out_node_vld_##x = 1;		\
		}						\
	} while(0)

	__topaz_fwt_setup_entry_set_out_node(0);
	__topaz_fwt_setup_entry_set_out_node(1);
	__topaz_fwt_setup_entry_set_out_node(2);
	__topaz_fwt_setup_entry_set_out_node(3);
	__topaz_fwt_setup_entry_set_out_node(4);
	__topaz_fwt_setup_entry_set_out_node(5);

	ent->data.out_port = out_port;
	ent->data.timestamp = topaz_fwt_get_scaled_timestamp();
}

union topaz_fwt_lookup {
	struct {
		uint32_t	word0;
	} raw;
	struct {
		uint32_t	trig		:1,
				__unused	:7,
				hash_addr	:10,
				__unused2	:2,
				entry_addr	:11,
				valid		:1;
	} data;
};

#define FWT_ZERO_LOOKUP_INIT	{ { 0 } }

RUBY_INLINE union topaz_fwt_lookup __topaz_fwt_hw_lookup_rd(void)
{
	union topaz_fwt_lookup u;
	u.raw.word0 = qtn_mproc_sync_mem_read(TOPAZ_FWT_LOOKUP_REG);
	return u;
}

RUBY_INLINE union topaz_fwt_lookup __topaz_fwt_hw_lookup_wait_be(const uint8_t *mac_be, int *timeout)
{
	unsigned long timeouts = 0;
	union topaz_fwt_lookup u;
	union topaz_fwt_lookup zero_lookup = FWT_ZERO_LOOKUP_INIT;

	__topaz_fwt_hw_lookup_write_be(mac_be);
	while (1) {
		u = __topaz_fwt_hw_lookup_rd();
		if (u.data.trig == 0) {
			*timeout = 0;
			return u;
		} else {
			qtn_pipeline_drain();
			if (unlikely(timeouts++ > 1000)) {
				*timeout = 1;
				return zero_lookup;
			}
		}
	}

	return zero_lookup;
}

RUBY_INLINE union topaz_fwt_lookup topaz_fwt_hw_lookup_wait_be(const uint8_t *mac_be, int *timeout, uint8_t *false_miss)
{
#ifndef TOPAZ_DISABLE_FWT_WAR
	/*
	 * This is to workaround the FWT lookup false issue:
	 * It seems when EMAC is under heavy VLAN traffic, Lhost and MuC
	 * may get false miss from FWT -- FWT returns invalid while a MAC
	 * address truly exists in it. A double check reduces count of false
	 * misses significantly.
	 */
	uint8_t retries = 1;
#else
	uint8_t retries = 0;
#endif
	union topaz_fwt_lookup u;

	do {
		u = __topaz_fwt_hw_lookup_wait_be(mac_be, timeout);
	} while (!u.data.valid && retries--);

#if !defined(TOPAZ_DISABLE_FWT_WAR) && !defined(MUC_BUILD)
	*false_miss += (retries == 0);
#endif

	return u;
}

RUBY_INLINE uint32_t __topaz_fwt_cpu_access_rd(void)
{
	return qtn_mproc_sync_mem_read(TOPAZ_FWT_CPU_ACCESS);
}

RUBY_INLINE void __topaz_fwt_set_4addrmode(union topaz_fwt_entry *ent, uint8_t portal)
{
	ent->data.portal = !!portal;
}

RUBY_INLINE void __topaz_fwt_set_port(union topaz_fwt_entry *ent, uint8_t port)
{
	ent->data.out_port = port;
}


RUBY_INLINE void __topaz_fwt_set_node(union topaz_fwt_entry *ent,
		uint8_t node_index, uint8_t node_num, bool enable)
{
#define ____topaz_fwt_set_node(n)				\
	case n:	do {						\
			ent->data.out_node_##n = node_num;	\
			ent->data.out_node_vld_##n = !!enable;	\
		} while(0);					\
	break

	switch (node_index) {
		____topaz_fwt_set_node(0);
		____topaz_fwt_set_node(1);
		____topaz_fwt_set_node(2);
		____topaz_fwt_set_node(3);
		____topaz_fwt_set_node(4);
		____topaz_fwt_set_node(5);
	default:
		break;
	}
}

RUBY_INLINE int __topaz_fwt_cpu_access_start_wait(void)
{
#ifndef CONSOLE_TEST
	unsigned long timeouts = 0;
	qtn_mproc_sync_mem_write(TOPAZ_FWT_CPU_ACCESS, TOPAZ_FWT_CPU_ACCESS_REQ);
	while (timeouts++ < 1000) {
		uint32_t reg = __topaz_fwt_cpu_access_rd();
		if (MS(reg, TOPAZ_FWT_CPU_ACCESS_STATE) == TOPAZ_FWT_CPU_ACCESS_STATE_GRANTED) {
			return 0;
		}
		qtn_pipeline_drain();
	}

	return -1;
#else
	return 0;
#endif
}

RUBY_INLINE void __topaz_fwt_cpu_access_stop(void)
{
#ifndef CONSOLE_TEST
	qtn_mproc_sync_mem_write(TOPAZ_FWT_CPU_ACCESS, 0);
#endif
}

RUBY_INLINE int topaz_fwt_cpu_access_lock_wait_irqsave(unsigned long *flags)
{
#ifndef CONSOLE_TEST
	int rc;

	local_irq_save(*flags);
	rc = __topaz_fwt_cpu_access_start_wait();
	if (rc) {
		local_irq_restore(*flags);
	}
	return rc;
#else
	(void)flags;
	return 0;
#endif
}

RUBY_INLINE void topaz_fwt_cpu_access_unlock_irqrestore(unsigned long *flags)
{
#ifndef CONSOLE_TEST
	__topaz_fwt_cpu_access_stop();
	local_irq_restore(*flags);
#else
	(void)flags;
#endif
}
#ifndef TOPAZ_TEST_ASSERT_EQUAL
# define TOPAZ_TEST_ASSERT_EQUAL(a, b)	if ((a) != (b)) { return -1; }
#endif
RUBY_INLINE int topaz_fwt_lookup_bitfield_test(const union topaz_fwt_lookup *e)
{
	TOPAZ_TEST_ASSERT_EQUAL(MS(e->raw.word0, TOPAZ_FWT_LOOKUP_TRIG), e->data.trig);
	TOPAZ_TEST_ASSERT_EQUAL(MS(e->raw.word0, TOPAZ_FWT_LOOKUP_ENTRY_ADDR), e->data.entry_addr);
	TOPAZ_TEST_ASSERT_EQUAL(MS(e->raw.word0, TOPAZ_FWT_LOOKUP_HASH_ADDR), e->data.hash_addr);
	TOPAZ_TEST_ASSERT_EQUAL(MS(e->raw.word0, TOPAZ_FWT_LOOKUP_VALID), e->data.valid);

	return 0;
}

RUBY_INLINE int topaz_fwt_entry_bitfield_test(const union topaz_fwt_entry *e)
{
	TOPAZ_TEST_ASSERT_EQUAL(!!topaz_fwt_is_valid(e), e->data.valid);
	TOPAZ_TEST_ASSERT_EQUAL(MS(e->raw.word1, TOPAZ_FWT_ENTRY_NXT_ENTRY), e->data.next_index);
	TOPAZ_TEST_ASSERT_EQUAL(MS(e->raw.word2, TOPAZ_FWT_ENTRY_OUT_NODE_0), e->data.out_node_0);
	TOPAZ_TEST_ASSERT_EQUAL(MS(e->raw.word2, TOPAZ_FWT_ENTRY_OUT_NODE_VLD_0), e->data.out_node_vld_0);
	TOPAZ_TEST_ASSERT_EQUAL(MS(e->raw.word2, TOPAZ_FWT_ENTRY_OUT_NODE_1), e->data.out_node_1);
	TOPAZ_TEST_ASSERT_EQUAL(MS(e->raw.word2, TOPAZ_FWT_ENTRY_OUT_NODE_VLD_1), e->data.out_node_vld_1);
	TOPAZ_TEST_ASSERT_EQUAL(MS(e->raw.word2, TOPAZ_FWT_ENTRY_OUT_NODE_2), e->data.out_node_2);
	TOPAZ_TEST_ASSERT_EQUAL(MS(e->raw.word2, TOPAZ_FWT_ENTRY_OUT_NODE_VLD_2), e->data.out_node_vld_2);
	TOPAZ_TEST_ASSERT_EQUAL(MS(e->raw.word2, TOPAZ_FWT_ENTRY_OUT_NODE_3), e->data.out_node_3);
	TOPAZ_TEST_ASSERT_EQUAL(MS(e->raw.word2, TOPAZ_FWT_ENTRY_OUT_NODE_VLD_3), e->data.out_node_vld_3);
	TOPAZ_TEST_ASSERT_EQUAL(MS(e->raw.word3, TOPAZ_FWT_ENTRY_OUT_NODE_4), e->data.out_node_4);
	TOPAZ_TEST_ASSERT_EQUAL(MS(e->raw.word3, TOPAZ_FWT_ENTRY_OUT_NODE_VLD_4), e->data.out_node_vld_4);
	TOPAZ_TEST_ASSERT_EQUAL(MS(e->raw.word3, TOPAZ_FWT_ENTRY_OUT_NODE_5), e->data.out_node_5);
	TOPAZ_TEST_ASSERT_EQUAL(MS(e->raw.word3, TOPAZ_FWT_ENTRY_OUT_NODE_VLD_5), e->data.out_node_vld_5);
	TOPAZ_TEST_ASSERT_EQUAL(MS(e->raw.word3, TOPAZ_FWT_ENTRY_OUT_PORT), e->data.out_port);
	TOPAZ_TEST_ASSERT_EQUAL(MS(e->raw.word3, TOPAZ_FWT_ENTRY_TIMESTAMP), e->data.timestamp);
#ifdef __KERNEL_
	TOPAZ_TEST_ASSERT_EQUAL(memcmp(topaz_fwt_macaddr(e), e->data.mac_le, ETH_ALEN), 0);
#endif

	return 0;
}

struct topaz_ipmac_uc_entry {
	struct topaz_ipmac_uc_entry *next;
	union {
		uint8_t		ipv4_addr[4];
		uint8_t		ipv6_addr[16];
	}u;
	uint8_t		mac_addr[MAC_ADDR_LEN];
	uint16_t	type;
	struct topaz_ipmac_uc_entry *lhost_next;
};

#define TOPAZ_IPMAC_UC_HASH_SLOT	128
#define TOPAZ_IPMAC_UC_HASH_SLOT_MASK	0x7f
#define TOPAZ_IPMAC_UC_HASH_SIZE	(TOPAZ_IPMAC_UC_HASH_SLOT * sizeof(void *))

#define TOPAZ_IPMAC_UC_ENTRY_SIZE	sizeof(struct topaz_ipmac_uc_entry)
#define TOPAZ_IPMAC_UC_ENTRY_COUNT	31

struct topaz_ipmac_uc_table {
	struct topaz_ipmac_uc_entry *slots[TOPAZ_IPMAC_UC_HASH_SLOT];
	struct topaz_ipmac_uc_entry entries[TOPAZ_IPMAC_UC_ENTRY_COUNT];
	uint32_t	update_cnt_lhost;
	uint32_t	update_cnt_muc;
};

#define TOPAZ_IPMAC_UC_TBL_SIZE		(sizeof(struct topaz_ipmac_uc_table))

/*
 * The hash works under the assumption that in most cases, hosts behind
 * the STA are in the same IP subnet. The host number differs so we have
 * a good chance to have diverse MSB byte of a be IP address
 */
RUBY_INLINE uint16_t topaz_ipmac_uc_hash(__be32 ipaddr)
{
	return ((ipaddr >> 24) & TOPAZ_IPMAC_UC_HASH_SLOT_MASK);
}

RUBY_INLINE uint16_t topaz_ipmac_ipv6uc_hash(const uint8_t *ipv6_addr)
{
	return (ipv6_addr[15] & TOPAZ_IPMAC_UC_HASH_SLOT_MASK);
}

#endif	/* __TOPAZ_FWT_CPUIF_PLATFORM_H */

