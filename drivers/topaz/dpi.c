/**
 * (C) Copyright 2012-2013 Quantenna Communications Inc.
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
 **/

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <linux/hardirq.h>

#include <asm/hardware.h>
#include <asm/board/platform.h>

#include <common/queue.h>
#include <qtn/topaz_dpi.h>

/**
 * Each DPI filter is a mix of DPI fields, and DPI IP tuples.
 * A field is a versatile way of matching any aspect of packet. An example field,
 * which matches only large packets is:
 *  - anchor on the IPv4 header
 *  - Offset 0. Offset works with 32 bit words, length field is 1st word
 *  - Comparison operator >=
 *  - Mask 0x0000FFFF (Length is bytes 2 & 3, network endian)
 *  - Val  0x00000400 (1024 bytes or more)
 * There are 32 hardware DPI fields.
 *
 * An IP tuple is a kind of field, specifically for matching whole IPv4 of IPv6 addresses,
 * and UDP or TCP ports. One IP tuple has 9 matching words:
 *  - word0	1st word of ipv6 source addr, or ipv4 source addr
 *  - word[1-3] other words for ipv6 source addr match
 *  - word4	1st word of ipv6 dest addr, or ipv4 dest addr
 *  - word[5-7] other words for ipv6 dest addr match
 *  - word9	TCP/UDP source + dest ports.
 * An IP tuple can match on any combination of the source address, dest address,
 * source port, dest port, but it must be an exact match.
 * There are 8 hardware IP tuples.
 *
 * A DPI filter is some combination of fields and/or ip tuples, grouped together with the
 * TOPAZ_EMAC_RX_DPI_IPT_GROUP(x) and TOPAZ_EMAC_RX_DPI_FIELD_GROUP(x) registers.
 * For example, filter 2 is composed of:
 *  - IP tuple 3, configured to match on source & dest address (not ports)
 *  - Field 17 matches something else
 * The filter is composed by setting bit 17 TOPAZ_EMAC_RX_DPI_FIELD_GROUP(2), and 
 * bits 3 (enable ip tuple 3 saddr match) and 11 (enable ip tuple 3 dest addr match)
 * of TOPAZ_EMAC_RX_DPI_IPT_GROUP(2).
 * There are 16 hardware DPI filters.
 *
 * 
 * IP Tuple memory is arranged as:
 * - 8 lwords wide (for 8 different ip tuples)
 * - 9 lwords deep (called 'entries')
 * Entries 0-3 are for IP source address matching
 * Entries 4-7 are for IP destination address matching
 * Entry 8 is TCP/UDP source/dest address matching
 *
 * The entire width for an entry is read/written at once, controlled by
 * TOPAZ_EMAC_RX_DPI_IPT_MEM_COM (0x620). So when modifying an address, order is:
 * - Read entry, so old unchanged values will be retained
 * - Poll on entry read complete
 * - Modify word (TOPAZ_EMAC_RX_DPI_IPT_MEM_DATA(x), 0x600 + 4 * x)
 * - Write entry back
 * - Poll on write complete
 */


struct topaz_dpi_field {
	int refcount;
	struct topaz_dpi_field_def def;
};

struct topaz_dpi_iptuple_addr {
	int refcount;
	struct in6_addr addr;
};

struct topaz_dpi_iptuple_port {
	int refcount;
	uint16_t port;
};

struct topaz_dpi_filter {
	uint8_t index;
	TAILQ_ENTRY(topaz_dpi_filter) next;

	struct {
		uint8_t srcaddr[howmany(TOPAZ_EMAC_NUM_DPI_IPTUPLES, NBBY)];
		uint8_t destaddr[howmany(TOPAZ_EMAC_NUM_DPI_IPTUPLES, NBBY)];
		uint8_t srcport[howmany(TOPAZ_EMAC_NUM_DPI_IPTUPLES, NBBY)];
		uint8_t destport[howmany(TOPAZ_EMAC_NUM_DPI_IPTUPLES, NBBY)];
		uint8_t fields[howmany(TOPAZ_EMAC_NUM_DPI_FIELDS, NBBY)];
	} used;
};
typedef TAILQ_HEAD(filter_head_s, topaz_dpi_filter) topaz_dpi_filter_head;

struct topaz_dpi_info {
	unsigned long base;	/* emac ctl address base */
	spinlock_t lock;

	struct topaz_dpi_field fields[TOPAZ_EMAC_NUM_DPI_FIELDS];
	struct topaz_dpi_iptuple_addr ipt_srcaddr[TOPAZ_EMAC_NUM_DPI_IPTUPLES];
	struct topaz_dpi_iptuple_addr ipt_destaddr[TOPAZ_EMAC_NUM_DPI_IPTUPLES];
	struct topaz_dpi_iptuple_port ipt_srcport[TOPAZ_EMAC_NUM_DPI_IPTUPLES];
	struct topaz_dpi_iptuple_port ipt_destport[TOPAZ_EMAC_NUM_DPI_IPTUPLES];

	topaz_dpi_filter_head used_filters_head;
	topaz_dpi_filter_head unused_filters_head;
	struct topaz_dpi_filter filters[TOPAZ_EMAC_NUM_DPI_FILTERS];
};

static void topaz_dpi_lock(struct topaz_dpi_info *info)
{
	spin_lock_bh(&info->lock);
}

static void topaz_dpi_unlock(struct topaz_dpi_info *info)
{
	spin_unlock_bh(&info->lock);
}

static void topaz_dpi_info_init(struct topaz_dpi_info *info, unsigned long base_addr)
{
	int i;

	memset(info, 0, sizeof(*info));
	info->base = base_addr;
	spin_lock_init(&info->lock);

	for (i = 0; i < TOPAZ_EMAC_NUM_DPI_IPTUPLES; i++) {
		info->ipt_srcaddr[i].refcount = 0;
		info->ipt_srcport[i].refcount = 0;
		info->ipt_destaddr[i].refcount = 0;
		info->ipt_destport[i].refcount = 0;
	}

	for (i = 0; i < TOPAZ_EMAC_NUM_DPI_FIELDS; i++) {
		info->fields[i].refcount = 0;
	}

	TAILQ_INIT(&info->used_filters_head);
	TAILQ_INIT(&info->unused_filters_head);
	for (i = 0; i < TOPAZ_EMAC_NUM_DPI_FILTERS; i++) {
		info->filters[i].index = i;
		TAILQ_INSERT_TAIL(&info->unused_filters_head, &info->filters[i], next);
	}
}

static int topaz_dpi_get_filter_fields(struct topaz_dpi_info *info,
		const struct topaz_dpi_filter_request *req, uint8_t *used)
{
	unsigned int i;
	unsigned int j;

	/* look for identical fields */
	for (i = 0; i < req->field_count; i++) {
		const struct topaz_dpi_field_def *new_field_def = &req->fields[i];
		int unused_index = -1;
		int found = 0;

		for (j = 0; !found && j < TOPAZ_EMAC_NUM_DPI_FIELDS; j++) {
			const struct topaz_dpi_field *field = &info->fields[j];

			if (field->refcount == 0 && !isset(used, j) && unused_index < 0) {
				unused_index = j;
			} else if (field->refcount && memcmp(new_field_def, &field->def,
						sizeof(*new_field_def)) == 0) {
				/* share existing field */
				setbit(used, j);
				found = 1;
			}
		}

		if (found) {
			/* use shared field */
		} else if (unused_index >= 0) {
			/* use new field */
			info->fields[unused_index].def = *new_field_def;
			setbit(used, unused_index);
		} else {
			/* out of fields */
			return -1;
		}
	}

	return 0;
}

static int topaz_dpi_get_filter_ipt_addr(struct topaz_dpi_iptuple_addr *addrs,
		const struct in6_addr *req_addr, uint8_t *used)
{
	int i;
	int found = 0;
	int unused_index = -1;
	const struct in6_addr zero = IN6ADDR_ANY_INIT;

	if (memcmp(&zero, req_addr, sizeof(*req_addr)) == 0) {
		return 0;
	}

	for (i = 0; !found && i < TOPAZ_EMAC_NUM_DPI_IPTUPLES; i++) {
		if (addrs[i].refcount == 0 && unused_index < 0) {
			unused_index = i;
		} else if (addrs[i].refcount && memcmp(&addrs[i].addr, req_addr, sizeof(*req_addr)) == 0) {
			found = 1;
			setbit(used, i);
		}
	}

	if (found) {
		/* use shared field */
	} else if (unused_index >= 0) {
		addrs[unused_index].addr = *req_addr;
		setbit(used, unused_index);
	} else {
		return -1;
	}

	return 0;
}

static int topaz_dpi_get_filter_ipt_port(struct topaz_dpi_iptuple_port *ports,
		uint16_t req_port, uint8_t *used)
{
	int i;
	int found = 0;
	int unused_index = -1;

	if (!req_port) {
		return 0;
	}

	for (i = 0; !found && i < TOPAZ_EMAC_NUM_DPI_IPTUPLES; i++) {
		if (ports[i].refcount == 0 && unused_index < 0) {
			unused_index = i;
		} else if (ports[i].refcount && ports[i].port == req_port) {
			found = 1;
			setbit(used, i);
		}
	}

	if (found) {
		/* use shared field */
	} else if (unused_index >= 0) {
		ports[unused_index].port = req_port;
		setbit(used, unused_index);
	} else {
		return -1;
	}

	return 0;
}

static struct topaz_dpi_filter * topaz_dpi_get_filter(struct topaz_dpi_info *info,
		const struct topaz_dpi_filter_request *req)
{
	struct topaz_dpi_filter *filter;

	filter = TAILQ_FIRST(&info->unused_filters_head);
	if (!filter)
		return NULL;

	if (topaz_dpi_get_filter_fields(info, req, filter->used.fields))
		goto insufficient_fields;

	if (topaz_dpi_get_filter_ipt_addr(info->ipt_srcaddr, &req->srcaddr, filter->used.srcaddr))
		goto insufficient_fields;

	if (topaz_dpi_get_filter_ipt_addr(info->ipt_destaddr, &req->destaddr, filter->used.destaddr))
		goto insufficient_fields;

	if (topaz_dpi_get_filter_ipt_port(info->ipt_srcport, req->srcport, filter->used.srcport))
		goto insufficient_fields;

	if (topaz_dpi_get_filter_ipt_port(info->ipt_destport, req->destport, filter->used.destport))
		goto insufficient_fields;

	TAILQ_REMOVE(&info->unused_filters_head, filter, next);
	TAILQ_INSERT_TAIL(&info->used_filters_head, filter, next);

	return filter;

insufficient_fields:
	/* not enough fields available */
	memset(&filter->used, 0, sizeof(filter->used));
	return NULL;
}

static struct topaz_dpi_info *topaz_dpi_info_get(unsigned int emac)
{
	static struct topaz_dpi_info emac0_dpi_info;
	static struct topaz_dpi_info emac1_dpi_info;

	return emac ? &emac1_dpi_info : &emac0_dpi_info;
}

static void topaz_dpi_filter_add_set_tidmap(struct topaz_dpi_info *info, uint8_t index, uint8_t tid)
{
	uint32_t tidmap_reg_addr = TOPAZ_EMAC_RXP_DPI_TID_MAP_INDEX(index);
	uint32_t tidmap_reg_val;
	uint32_t tidmap_change_shift = TOPAZ_EMAC_RXP_TID_MAP_INDEX_SHIFT(index);
	uint32_t tidmap_change_mask = TOPAZ_EMAC_RXP_TID_MAP_INDEX_MASK(index);

	tidmap_reg_val = readl(info->base + tidmap_reg_addr);
	tidmap_reg_val = (tidmap_reg_val & ~tidmap_change_mask) |
		((tid << tidmap_change_shift) & tidmap_change_mask);
	writel(tidmap_reg_val, info->base + tidmap_reg_addr);
}

static void topaz_dpi_iptuple_set(const struct topaz_dpi_info *info,
		uint8_t iptuple, uint8_t entry, uint32_t value)
{
	topaz_dpi_iptuple_read_entry(info->base, entry);
	writel(value, info->base + TOPAZ_EMAC_RX_DPI_IPT_MEM_DATA(iptuple));
	topaz_dpi_iptuple_write_entry(info->base, entry);
}

static void topaz_dpi_iptuple_set_srcaddr(const struct topaz_dpi_info *info,
		uint8_t iptuple, const struct in6_addr *addr)
{
	int entry;

	for (entry = TOPAZ_EMAC_RX_DPI_IPT_ENTRY_SRCADDR_START;
			entry < TOPAZ_EMAC_RX_DPI_IPT_ENTRY_SRCADDR_END; entry++) {
		topaz_dpi_iptuple_set(info, iptuple, entry, ntohl(addr->s6_addr32[entry - 0]));
	}
}

static void topaz_dpi_iptuple_set_destaddr(const struct topaz_dpi_info *info,
		uint8_t iptuple, const struct in6_addr *addr)
{
	int entry;

	for (entry = TOPAZ_EMAC_RX_DPI_IPT_ENTRY_DESTADDR_START;
			entry < TOPAZ_EMAC_RX_DPI_IPT_ENTRY_DESTADDR_END; entry++) {
		topaz_dpi_iptuple_set(info, iptuple, entry, ntohl(addr->s6_addr32[entry - 4]));
	}
}

static void topaz_dpi_iptuple_set_port(const struct topaz_dpi_info *info,
		uint8_t iptuple, uint16_t port, bool is_src)
{
	uint32_t reg;
	uint16_t srcport;
	uint16_t destport;

	/*
	 * ip proto memory ports are little endian
	 * request format is network endian
	 */
	topaz_dpi_iptuple_read_entry(info->base, TOPAZ_EMAC_RX_DPI_IPT_ENTRY_PORTS);

	reg = readl(info->base + TOPAZ_EMAC_RX_DPI_IPT_MEM_DATA(iptuple));

	srcport = MS(reg, TOPAZ_EMAC_RX_DPI_IPT_PORT_SRC);
	destport = MS(reg, TOPAZ_EMAC_RX_DPI_IPT_PORT_DEST);
	if (is_src) {
		srcport = ntohs(port);
	} else {
		destport = ntohs(port);
	}
	reg = SM(destport, TOPAZ_EMAC_RX_DPI_IPT_PORT_DEST) |
		SM(srcport, TOPAZ_EMAC_RX_DPI_IPT_PORT_SRC);

	writel(reg, info->base + TOPAZ_EMAC_RX_DPI_IPT_MEM_DATA(iptuple));

	topaz_dpi_iptuple_write_entry(info->base, TOPAZ_EMAC_RX_DPI_IPT_ENTRY_PORTS);
}

static void topaz_dpi_iptuple_set_srcport(const struct topaz_dpi_info *info, uint8_t iptuple, uint16_t port)
{
	topaz_dpi_iptuple_set_port(info, iptuple, port, 1);
}

static void topaz_dpi_iptuple_set_destport(const struct topaz_dpi_info *info, uint8_t iptuple, uint16_t port)
{
	topaz_dpi_iptuple_set_port(info, iptuple, port, 0);
}

#if 0
void __topaz_dpi_iptuple_dump(const struct topaz_dpi_info *info, const char *func, int line)
{
	int entry;
	int reg;

	printk("%s caller %s:%d\n", __FUNCTION__, func, line);
	for (entry = 0; entry <= 8; entry++) {
		topaz_dpi_iptuple_read_entry(info->base, entry);
		printk("entry %d: ", entry);
		for (reg = 0; reg < 8; reg++) {
			printk(" 0x%08x", readl(info->base + TOPAZ_EMAC_RX_DPI_IPT_MEM_DATA(reg)));
		}
		printk("\n");
	}

	for (reg = 0; reg < 8; reg++) {
		printk("ipt %d refs sa %d da %d sp %d dp %d\n",
				reg,
				info->ipt_srcaddr[reg].refcount,
				info->ipt_destaddr[reg].refcount,
				info->ipt_srcport[reg].refcount,
				info->ipt_destport[reg].refcount);
	}
}
#define topaz_dpi_iptuple_dump(_info)	__topaz_dpi_iptuple_dump(_info, __FUNCTION__, __LINE__)
#endif

int topaz_dpi_filter_add(unsigned int emac,
		const struct topaz_dpi_filter_request *req)
{
	struct topaz_dpi_info *info;
	struct topaz_dpi_filter *filter;
	uint32_t field_group = 0;
	uint32_t iptuple_group = 0;
	uint32_t out_ctrl;
	uint32_t out_combo = 0;
	int i;

	info = topaz_dpi_info_get(emac);

	topaz_dpi_lock(info);

	filter = topaz_dpi_get_filter(info, req);
	if (filter == NULL) {
		topaz_dpi_unlock(info);
		return -1;
	}

	/*
	 * Increment reference count for each used ip tuple part, and each dpi field.
	 * If reference count leaves zero, also modify hardware
	 */
	for (i = 0; i < TOPAZ_EMAC_NUM_DPI_IPTUPLES; i++) {
		if (isset(filter->used.srcaddr, i)) {
			iptuple_group |= TOPAZ_EMAC_RX_DPI_IPT_GROUP_SRCADDR(i);
			if (info->ipt_srcaddr[i].refcount++ == 0) {
				topaz_dpi_iptuple_set_srcaddr(info, i, &req->srcaddr);
			}
		}
		if (isset(filter->used.destaddr, i)) {
			iptuple_group |= TOPAZ_EMAC_RX_DPI_IPT_GROUP_DESTADDR(i);
			if (info->ipt_destaddr[i].refcount++ == 0) {
				topaz_dpi_iptuple_set_destaddr(info, i, &req->destaddr);
			}
		}
		if (isset(filter->used.srcport, i)) {
			iptuple_group |= TOPAZ_EMAC_RX_DPI_IPT_GROUP_SRCPORT(i);
			if (info->ipt_srcport[i].refcount++ == 0) {
				topaz_dpi_iptuple_set_srcport(info, i, req->srcport);
			}
		}
		if (isset(filter->used.destport, i)) {
			iptuple_group |= TOPAZ_EMAC_RX_DPI_IPT_GROUP_DESTPORT(i);
			if (info->ipt_destport[i].refcount++ == 0) {
				topaz_dpi_iptuple_set_destport(info, i, req->destport);
			}
		}
	}
	for (i = 0; i < TOPAZ_EMAC_NUM_DPI_FIELDS; i++) {
		if (isset(filter->used.fields, i)) {
			field_group |= (1 << i);
			if (info->fields[i].refcount++ == 0) {
				writel(info->fields[i].def.ctrl.raw, info->base + TOPAZ_EMAC_RX_DPI_FIELD_CTRL(i));
				writel(info->fields[i].def.val, info->base + TOPAZ_EMAC_RX_DPI_FIELD_VAL(i));
				writel(info->fields[i].def.mask, info->base + TOPAZ_EMAC_RX_DPI_FIELD_MASK(i));
			}
		}
	}

	/*
	 * Enable DPI filter:
	 *   - set dpi -> tid map
	 *   - set filter field group
	 *   - set filter iptuple group
	 *   - set output port/node
	 *   - enable dpi filter
	 */
	if (field_group)
		out_combo |= TOPAZ_EMAC_RX_DPI_OUT_CTRL_DPI;
	if (iptuple_group)
		out_combo |= TOPAZ_EMAC_RX_DPI_OUT_CTRL_IPTUPLE;
	out_ctrl = SM(req->out_node, TOPAZ_EMAC_RX_DPI_OUT_CTRL_NODE) |
		SM(req->out_port, TOPAZ_EMAC_RX_DPI_OUT_CTRL_PORT) |
		SM(out_combo, TOPAZ_EMAC_RX_DPI_OUT_CTRL_COMBO);
	topaz_dpi_filter_add_set_tidmap(info, filter->index, req->tid);
	writel(field_group, info->base + TOPAZ_EMAC_RX_DPI_FIELD_GROUP(filter->index));
	writel(iptuple_group, info->base + TOPAZ_EMAC_RX_DPI_IPT_GROUP(filter->index));
	writel(out_ctrl, info->base + TOPAZ_EMAC_RX_DPI_OUT_CTRL(filter->index));

	topaz_dpi_unlock(info);

	return filter->index;
}
EXPORT_SYMBOL(topaz_dpi_filter_add);

void topaz_dpi_filter_del(unsigned int emac, int filter_no)
{
	struct topaz_dpi_info *info = topaz_dpi_info_get(emac);
	struct topaz_dpi_filter *filter;
	unsigned int i;

	if (filter_no < 0 || filter_no >= TOPAZ_EMAC_NUM_DPI_FILTERS)
		return;

	topaz_dpi_lock(info);

	filter = &info->filters[filter_no];

	/* disable filters, field group & ip tuple group */
	writel(readl(info->base + TOPAZ_EMAC_RXP_DPI_CTRL) & ~BIT(filter->index),
			info->base + TOPAZ_EMAC_RXP_DPI_CTRL);
	writel(0, info->base + TOPAZ_EMAC_RX_DPI_OUT_CTRL(filter->index));
	writel(0, info->base + TOPAZ_EMAC_RX_DPI_FIELD_GROUP(filter->index));
	writel(0, info->base + TOPAZ_EMAC_RX_DPI_IPT_GROUP(filter->index));

	/* decrement reference counts, clear hw if they hit zero */
	for (i = 0; i < TOPAZ_EMAC_NUM_DPI_IPTUPLES; i++) {
		const struct in6_addr zero = IN6ADDR_ANY_INIT;
		if (isset(filter->used.srcaddr, i) &&
				--info->ipt_srcaddr[i].refcount == 0) {
			topaz_dpi_iptuple_set_srcaddr(info, i, &zero);
		}
		if (isset(filter->used.destaddr, i) &&
				--info->ipt_destaddr[i].refcount == 0) {
			topaz_dpi_iptuple_set_destaddr(info, i, &zero);
		}
		if (isset(filter->used.srcport, i) &&
				--info->ipt_srcport[i].refcount == 0) {
			topaz_dpi_iptuple_set_srcport(info, i, 0);
		}
		if (isset(filter->used.destport, i) &&
				--info->ipt_destport[i].refcount == 0) {
			topaz_dpi_iptuple_set_destport(info, i, 0);
		}
	}
	for (i = 0; i < TOPAZ_EMAC_NUM_DPI_FIELDS; i++) {
		if (isset(filter->used.fields, i) &&
				--info->fields[i].refcount == 0) {
			writel(0, info->base + TOPAZ_EMAC_RX_DPI_FIELD_CTRL(i));
			writel(0, info->base + TOPAZ_EMAC_RX_DPI_FIELD_VAL(i));
			writel(0, info->base + TOPAZ_EMAC_RX_DPI_FIELD_MASK(i));
		}
	}

	memset(&filter->used, 0, sizeof(filter->used));

	TAILQ_REMOVE(&info->used_filters_head, filter, next);
	TAILQ_INSERT_TAIL(&info->unused_filters_head, filter, next);

	topaz_dpi_unlock(info);
}
EXPORT_SYMBOL(topaz_dpi_filter_del);

static void topaz_dpi_hw_init(unsigned int emac)
{
	struct topaz_dpi_info *info = topaz_dpi_info_get(emac);
	int i;

	/* clear dpi fields */
	for (i = 0; i < TOPAZ_EMAC_NUM_DPI_FIELDS; i++) {
		writel(0, info->base + TOPAZ_EMAC_RX_DPI_FIELD_VAL(i));
		writel(0, info->base + TOPAZ_EMAC_RX_DPI_FIELD_MASK(i));
		writel(0, info->base + TOPAZ_EMAC_RX_DPI_FIELD_CTRL(i));
	}

	/* clear dpi filters and group registers */
	for (i = 0; i < TOPAZ_EMAC_NUM_DPI_FILTERS; i++) {
		writel(0, info->base + TOPAZ_EMAC_RX_DPI_OUT_CTRL(i));
		writel(0, info->base + TOPAZ_EMAC_RX_DPI_FIELD_GROUP(i));
		writel(0, info->base + TOPAZ_EMAC_RX_DPI_IPT_GROUP(i));
	}

	/* clear ip tuple memory */
	for (i = 0; i < TOPAZ_EMAC_RX_DPI_IPT_MEM_DATA_MAX; i++) {
		writel(0, info->base + TOPAZ_EMAC_RX_DPI_IPT_MEM_DATA(i));
	}
	for (i = 0; i < TOPAZ_EMAC_RX_DPI_IPT_ENTRIES; i++) {
		topaz_dpi_iptuple_write_entry(info->base, i);
	}
}

int topaz_dpi_init(unsigned int emac)
{
	unsigned int base_addr;

	if (emac == 0) {
		base_addr = RUBY_ENET0_BASE_ADDR;
	} else if (emac == 1) {
		base_addr = RUBY_ENET1_BASE_ADDR;
	} else {
		return -EINVAL;
	}

	topaz_dpi_info_init(topaz_dpi_info_get(emac), base_addr);
	topaz_dpi_hw_init(emac);

	return 0;
}
EXPORT_SYMBOL(topaz_dpi_init);

MODULE_DESCRIPTION("Topaz EMAC DPI filters");
MODULE_AUTHOR("Quantenna");
MODULE_LICENSE("GPL");

