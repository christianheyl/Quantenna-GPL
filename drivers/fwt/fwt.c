/*SH0
*******************************************************************************
**                                                                           **
**         Copyright (c) 2014 Quantenna Communications Inc                   **
**                            All Rights Reserved                            **
**                                                                           **
**                                                                           **
*******************************************************************************
EH0*/

#ifndef CONSOLE_TEST
# define DEBUG_FWT(...)	//		if (1) printk("%s: ", __FUNCTION__); if (1) printk
# include <linux/crc32.h>
# include <linux/timer.h>
# include <linux/proc_fs.h>
#endif
#include <linux/types.h>
#include <linux/if_ether.h>
#include <qtn/qtn_debug.h>
#include <common/queue.h>
#include <qtn/topaz_fwt.h>
#include <qtn/topaz_fwt_sw.h>
#include <qtn/shared_defs.h>
#include <qtn/shared_params.h>
#include <qtn/hardware_revision.h>

MODULE_DESCRIPTION("Forwarding Table");
MODULE_AUTHOR("Quantenna Communications, Inc.");
MODULE_LICENSE("Proprietary");
MODULE_VERSION("1.0");

#define PROC_NAME	"topaz_fwt"
#define PROC_NAME_IPFF	"topaz_fwt_ipff"

static const char fwt_trunc_msg[] = "\nTRUNCATED\n";

#define FWT_GAIN_ACCESS()	\
	do { \
		if (unlikely(__topaz_fwt_cpu_access_start_wait())) {	\
			panic("%s: %d: fwt cpu access timeout", __func__, __LINE__);	\
		} \
	} while (0)
#define FWT_RELEASE_ACCESS()	\
	do {	\
		__topaz_fwt_cpu_access_stop();	\
	} while (0)

struct topaz_fwt_slot {
	uint16_t index;
	uint16_t in_use;
	TAILQ_ENTRY(topaz_fwt_slot) next;
};

typedef TAILQ_HEAD(, topaz_fwt_slot) topaz_fwt_slot_head;
static topaz_fwt_slot_head unused_level1_slots;
static topaz_fwt_slot_head unused_level2_slots;
static topaz_fwt_slot_head used_slots;
static struct topaz_fwt_slot __slots[TOPAZ_FWT_HW_TOTAL_ENTRIES];
static int topaz_fwt_hash_enabled = 0;

/* Inform cbk for overwriting fwt entries in order to maintain 1st level empty */
static fwt_notify_swap g_fwt_overwrite_hook = NULL;

static const union topaz_fwt_entry zeroent = FWT_ZERO_ENTRY_INIT;

static inline struct topaz_fwt_slot *topaz_fwt_get_slot(uint16_t index)
{
	return &__slots[index];
}

static void topaz_fwt_hash_enable(int enable)
{
	topaz_fwt_hash_enabled = enable;
#ifndef CONSOLE_TEST
	__topaz_fwt_hash_set(enable);
#endif
}

uint16_t topaz_fwt_hash(const uint8_t *mac_le)
{
	uint16_t hash;

	if (topaz_fwt_hash_enabled) {
		hash = ~crc32(~0, mac_le, ETH_ALEN);
	} else {
		hash = mac_le[0] | (mac_le[1] << 8);
	}

	return hash & TOPAZ_FWT_HW_HASH_MASK;
}
EXPORT_SYMBOL(topaz_fwt_hash);

int topaz_get_mac_be_from_index(uint16_t index, uint8_t *mac_be)
{
	union topaz_fwt_entry* hw_ent;
	uint8_t mac_le[ETH_ALEN];
	unsigned long flags;
	int rc;

	topaz_fwt_cpu_access_lock_wait_irqsave(&flags);
	hw_ent = topaz_fwt_get_hw_entry(index);
	if (!topaz_fwt_is_valid(hw_ent)) {
		rc = 0;
	} else {
		memcpy(mac_le, hw_ent->data.mac_le, ETH_ALEN);
		topaz_fwt_reverse_mac(mac_be, mac_le);
		rc = 1;
	}
	topaz_fwt_cpu_access_unlock_irqrestore(&flags);

	return rc;
}

void topaz_fwt_sw_entry_set(uint16_t index, uint8_t out_port,
		const uint8_t *out_nodes, unsigned int out_node_count, uint8_t portal)
{
	union topaz_fwt_sw_entry *sw = topaz_fwt_sw_entry_get(index);
	union topaz_fwt_sw_entry n;

	n.raw = 0;
	if (out_node_count <= 1) {
		/* Unicast only. Multicast must be handled by the calling code */
		n.unicast.valid = 1;
		n.unicast.portal = !!portal;
		n.unicast.port = out_port;
		n.unicast.node = out_nodes ? out_nodes[0] : 0;
	}

	arc_write_uncached_16(&sw->raw, n.raw);
}

void topaz_fwt_sw_entry_set_multicast(uint16_t fwt_index, uint16_t mcast_index)
{
	union topaz_fwt_sw_entry *sw = topaz_fwt_sw_entry_get(fwt_index);
	union topaz_fwt_sw_entry n;

	n.multicast.valid = 1;
	n.multicast.mcast = 1;
	n.multicast.index = mcast_index;

	arc_write_uncached_16(&sw->raw, n.raw);
}

void topaz_fwt_sw_entry_del(uint16_t index)
{
	union topaz_fwt_sw_entry *sw = topaz_fwt_sw_entry_get(index);

	arc_write_uncached_16(&sw->raw, 0);
}

static uint32_t sw_reflect_detect = 0;
module_param(sw_reflect_detect, uint, 0644);
MODULE_PARM_DESC(sw_reflect_detect, "Enable software based reflection detection and prevention");
static uint8_t topaz_fwt_remap_port(uint8_t port)
{
	if (sw_reflect_detect) {
		if ((port == TOPAZ_TQE_EMAC_0_PORT) || (port == TOPAZ_TQE_EMAC_1_PORT)) {
			port = TOPAZ_TQE_LHOST_PORT;
		}
	}

	return port;
}

static int __topaz_fwt_add_entry(const uint8_t *mac_be, uint8_t out_port,
		const uint8_t *out_nodes, unsigned int out_node_count, uint8_t portal)
{
	union topaz_fwt_entry newent;
	union topaz_fwt_entry *hw_ent;
	uint16_t index;
	uint16_t loops;

	topaz_fwt_setup_entry(&newent, mac_be, topaz_fwt_remap_port(out_port),
			out_nodes, out_node_count, portal);

	index = topaz_fwt_hash(newent.data.mac_le);
	DEBUG_FWT(DBGMACVAR " index %u\n", DBGMACFMT(mac_be), index);

	hw_ent = topaz_fwt_get_hw_entry(index);
	FWT_GAIN_ACCESS();
	if (!topaz_fwt_is_valid(hw_ent)) {
		/* place new entry in the top level hash table */
		struct topaz_fwt_slot *slot;

		topaz_fwt_insert_entry(&newent, index);
		topaz_fwt_sw_entry_set(index, out_port, out_nodes, out_node_count, portal);

		FWT_RELEASE_ACCESS();

		slot = topaz_fwt_get_slot(index);
		slot->in_use = 1;
		TAILQ_REMOVE(&unused_level1_slots, slot, next);
		TAILQ_INSERT_TAIL(&used_slots, slot, next);

		DEBUG_FWT(DBGMACVAR " inserted at index %u\n", DBGMACFMT(mac_be), index);

		return index;
	}
	FWT_RELEASE_ACCESS();

	for (loops = 0; loops < TOPAZ_FWT_HW_TOTAL_ENTRIES; loops++) {
		union topaz_fwt_entry oldent;
		uint16_t next_index;

		FWT_GAIN_ACCESS();
		topaz_fwt_copy_entry(&oldent, hw_ent, 2);
		FWT_RELEASE_ACCESS();

		/* prevent duplicates */
		if (unlikely(memcmp(topaz_fwt_macaddr(&oldent),
						topaz_fwt_macaddr(&newent),
						ETH_ALEN) == 0)) {
			DEBUG_FWT(DBGMACVAR " already present\n", DBGMACFMT(mac_be));
			return -EEXIST;
		}

		next_index = topaz_fwt_next_index(&oldent);

		DEBUG_FWT("mac inserting be " DBGMACVAR
				" le " DBGMACVAR
				" slot has mac le " DBGMACVAR
				" next_index %u loops %d\n",
				DBGMACFMT(mac_be),
				DBGMACFMT(topaz_fwt_macaddr(&newent)),
				DBGMACFMT(topaz_fwt_macaddr(&oldent)),
				next_index, loops);

		if (next_index == 0) {
			/*
			 * end of chain reached. append to this entry
			 */
			struct topaz_fwt_slot *slot;

			slot = TAILQ_FIRST(&unused_level2_slots);
			if (slot == NULL) {
				DEBUG_FWT(DBGMACVAR " could not insert, full\n",
						DBGMACFMT(mac_be));
				return -ENOSPC;
			}

			if (unlikely(__topaz_fwt_cpu_access_start_wait())) {
				panic("topaz_fwt_add_entry cpu access timeout");
			}

			next_index = slot->index;

			topaz_fwt_insert_entry(&newent, next_index);
			topaz_fwt_set_next_index(hw_ent, next_index);
			topaz_fwt_sw_entry_set(next_index, out_port, out_nodes, out_node_count, portal);

			__topaz_fwt_cpu_access_stop();

			slot->in_use = 1;
			TAILQ_REMOVE(&unused_level2_slots, slot, next);
			TAILQ_INSERT_TAIL(&used_slots, slot, next);

			DEBUG_FWT(DBGMACVAR " inserted at index %u\n",
					DBGMACFMT(mac_be), next_index);

			return next_index;
		}

		hw_ent = topaz_fwt_get_hw_entry(next_index);
	}

	return -EFAULT;
}

static int __topaz_fwt_del_entry_le(const uint8_t *mac_le)
{
	union topaz_fwt_entry *hw_ent;
	uint16_t index;
	int prev_index = -1;
	int loops;
	uint16_t deleted_index;

	index = topaz_fwt_hash(mac_le);
	hw_ent = topaz_fwt_get_hw_entry(index);
	if (!topaz_fwt_is_valid(hw_ent)) {
		/* entry doesn't exist */
		DEBUG_FWT(DBGMACVAR " not found, l1\n",	DBGMACFMT_LE(mac_le));
		return -ENOENT;
	}

	for (loops = 0; loops < TOPAZ_FWT_HW_TOTAL_ENTRIES; loops++) {
		uint16_t next_index;
		union topaz_fwt_entry ent;

		FWT_GAIN_ACCESS();
		topaz_fwt_copy_entry(&ent, hw_ent, 2);
		FWT_RELEASE_ACCESS();
		next_index = topaz_fwt_next_index(&ent);

		if (memcmp(topaz_fwt_macaddr(&ent), mac_le, ETH_ALEN) == 0) {
			/* found */

			struct topaz_fwt_slot *deleted_slot;

			if (unlikely(__topaz_fwt_cpu_access_start_wait())) {
				panic("topaz_fwt_del_entry cpu access timeout");
			}

			if (prev_index < 0 && next_index == 0) {
				/* first and only entry with this hash */
				deleted_index = index;
			} else if (prev_index < 0) {
				/* first in a linked list. copy second over first */
				union topaz_fwt_entry *next_ent;

				next_ent = topaz_fwt_get_hw_entry(next_index);
				FWT_GAIN_ACCESS();
				topaz_fwt_copy_entry(hw_ent, next_ent, 4);
				FWT_RELEASE_ACCESS();

				deleted_index = next_index;

				/* Maintain the FWT databse entry for overwriting  scenario
				 * so we can keep table with same indexer. */
				if (g_fwt_overwrite_hook) {
					g_fwt_overwrite_hook(index,deleted_index );
				}
			} else {
				/* remove this element, make prev skip this one */
				union topaz_fwt_entry *prev_ent;
				prev_ent = topaz_fwt_get_hw_entry(prev_index);
				topaz_fwt_set_next_index(prev_ent, next_index);
				deleted_index = index;
			}

			topaz_fwt_insert_entry(&zeroent, deleted_index);
			topaz_fwt_sw_entry_del(deleted_index);

			__topaz_fwt_cpu_access_stop();

			DEBUG_FWT(DBGMACVAR " deleted from index %u\n",
					DBGMACFMT_LE(mac_le), deleted_index);

			deleted_slot = topaz_fwt_get_slot(deleted_index);
			deleted_slot->in_use = 0;
			TAILQ_REMOVE(&used_slots, deleted_slot, next);
			if (deleted_index < TOPAZ_FWT_HW_LEVEL1_ENTRIES) {
				TAILQ_INSERT_TAIL(&unused_level1_slots, deleted_slot, next);
			} else {
				TAILQ_INSERT_TAIL(&unused_level2_slots, deleted_slot, next);
			}

			if (unlikely(deleted_slot->index != deleted_index)) {
				printk("%s: " DBGMACVAR " deleted slot mismatch! slot %u deleted_index %u\n",
						__FUNCTION__, DBGMACFMT_LE(mac_le), deleted_slot->index, deleted_index);
				return -EINVAL;
			}

			return deleted_index;
		}

		prev_index = index;
		index = next_index;
		if (index == 0) {
			DEBUG_FWT(DBGMACVAR " not found, l2\n",	DBGMACFMT_LE(mac_le));
			return -ENOENT;
		}
		hw_ent = topaz_fwt_get_hw_entry(next_index);
	}

	return -EFAULT;
}


static int __topaz_fwt_del_entry_be(const uint8_t *mac_be)
{
	uint8_t mac_le[ETH_ALEN];

	topaz_fwt_reverse_mac(mac_le, mac_be);
	return __topaz_fwt_del_entry_le(mac_le);
}

struct timer_list topaz_fwt_maintenance_timer;

int topaz_fwt_get_timestamp(uint16_t index)
{
	union topaz_fwt_entry *hw_ent;
	unsigned long flags;
	uint32_t timestamp = 0;

	topaz_fwt_cpu_access_lock_wait_irqsave(&flags);

	hw_ent = topaz_fwt_get_hw_entry(index);

	if (hw_ent && hw_ent->data.valid) {
		timestamp =  hw_ent->data.timestamp;
	} else {
		timestamp = -ENOENT;
	}
	topaz_fwt_cpu_access_unlock_irqrestore(&flags);

	return timestamp;
}


void topaz_fwt_register_overwrite(fwt_notify_swap cbk_func)
{
	g_fwt_overwrite_hook = cbk_func;
}

int topaz_fwt_add_entry(const uint8_t *mac_be, uint8_t out_port,
		const uint8_t *out_nodes, unsigned int out_node_count, uint8_t portal)
{
	int rc;
	unsigned long flags;

	/* TODO - avoid entering illegal entries */
	if (out_node_count == 0 ) {
		return 0;
	}

	local_irq_save(flags);

	rc = __topaz_fwt_add_entry(mac_be, out_port, out_nodes, out_node_count, portal);

	local_irq_restore(flags);

	return rc;
}
EXPORT_SYMBOL(topaz_fwt_add_entry);

int topaz_fwt_del_entry(const uint8_t *mac_be)
{
	int rc;
	unsigned long flags;

	if (mac_be == NULL ) {
		return -EINVAL;
	}

	local_irq_save(flags);

	/* HW tested the delete item in be */
	rc = __topaz_fwt_del_entry_be(mac_be);

	local_irq_restore(flags);

	return rc;
}
EXPORT_SYMBOL(topaz_fwt_del_entry);


#ifndef CONSOLE_TEST

static int topaz_fwt_ipff_read_proc(char *page, char **start, off_t off,
		int count, int *eof, void *_unused)
{
	int rc;

	rc = fwt_sw_get_ipff(page, count);
	if (rc == count) {
		strncpy(page + count - strlen(fwt_trunc_msg) - 1,
			fwt_trunc_msg,
			sizeof(fwt_trunc_msg));
		*(page + count - 1) = '\0';
	}

	return rc;
}

static int topaz_fwt_read_proc(char *page, char **start, off_t off,
		int count, int *eof, void *_unused)
{
	int rc = 0;
	struct topaz_fwt_slot *slot;
	unsigned long flags;
	uint8_t mac_be[ETH_ALEN];

	local_irq_save(flags);

	TAILQ_FOREACH(slot, &used_slots, next) {
		union topaz_fwt_sw_entry sw;
		union topaz_fwt_sw_entry *swp;
		union topaz_fwt_entry ent;
		int printed;

		swp = topaz_fwt_sw_entry_get(slot->index);
		sw.raw = arc_read_uncached_16(&swp->raw);

		FWT_GAIN_ACCESS();
		topaz_fwt_copy_entry(&ent,
			topaz_fwt_get_hw_entry(slot->index),
			4);
		FWT_RELEASE_ACCESS();

		/* reverse MAC for be so user can see the mac id as the bridge print it */
		topaz_fwt_reverse_mac(mac_be, ent.data.mac_le);
		printed = min(count, snprintf(page,
			count,
			"%04u %pM valid %u portal %u out_port %u out_nodes %d|%d|%d|%d|%d|%d words %08x %08x %08x %08x swp 0x%p sw %04x\n",
			slot->index,
			mac_be,
			ent.data.valid,
			ent.data.portal,
			ent.data.out_port,
			(int)(ent.data.out_node_vld_0 ? ent.data.out_node_0 : -1),
			(int)(ent.data.out_node_vld_1 ? ent.data.out_node_1 : -1),
			(int)(ent.data.out_node_vld_2 ? ent.data.out_node_2 : -1),
			(int)(ent.data.out_node_vld_3 ? ent.data.out_node_3 : -1),
			(int)(ent.data.out_node_vld_4 ? ent.data.out_node_4 : -1),
			(int)(ent.data.out_node_vld_5 ? ent.data.out_node_5 : -1),
			ent.raw.word0, ent.raw.word1, ent.raw.word2, ent.raw.word3,
			swp, sw.raw));

		rc += printed;
		page += printed;
		count -= printed;

		if (count == 0) {
			memcpy(page - sizeof(fwt_trunc_msg),
				fwt_trunc_msg,
				sizeof(fwt_trunc_msg));
			break;
		}
	}

	local_irq_restore(flags);

	*eof = 1;
	return rc;
}

static int __init topaz_fwt_create_proc(void)
{
	struct proc_dir_entry *entry;

	entry = create_proc_entry(PROC_NAME, 0600, NULL);
	if (!entry) {
		return -ENOMEM;
	}
	entry->write_proc = NULL;
	entry->read_proc = topaz_fwt_read_proc;

	entry = create_proc_read_entry(PROC_NAME_IPFF, 0400, NULL, topaz_fwt_ipff_read_proc, NULL);
	if (!entry) {
		return -ENOMEM;
	}

	return 0;
}

#else

static int __init topaz_fwt_create_proc(void)
{
	return 0;
}

#endif /* #ifndef CONSOLE_TEST */

void topaz_update_node(uint16_t index, uint8_t node_index,uint8_t node_num, bool enable)
{

	union topaz_fwt_entry* hw_ent;
	union topaz_fwt_entry tmp;
	unsigned long flags;

	topaz_fwt_cpu_access_lock_wait_irqsave(&flags);

	hw_ent = topaz_fwt_get_hw_entry(index);
	topaz_fwt_copy_entry(&tmp, hw_ent, 4);

	__topaz_fwt_set_node(&tmp, node_index, node_num, enable);

	topaz_fwt_copy_entry(hw_ent, &tmp, 4);

	topaz_fwt_cpu_access_unlock_irqrestore(&flags);
}

int topaz_update_entry(uint16_t index, uint8_t port, uint8_t portal,
		uint8_t node_index , uint8_t node_num, bool enable)
{
	union topaz_fwt_entry *hw_ent;
	union topaz_fwt_entry tmp;
	unsigned long flags;

	topaz_fwt_cpu_access_lock_wait_irqsave(&flags);

	hw_ent = topaz_fwt_get_hw_entry(index);

	if (!hw_ent->data.valid) {
		printk(KERN_ERR "%s: index %u invalid\n", __FUNCTION__, index);
		goto out;
	}

	topaz_fwt_copy_entry(&tmp, hw_ent, 4);

	__topaz_fwt_set_port(&tmp, topaz_fwt_remap_port(port));
	__topaz_fwt_set_node(&tmp, node_index, node_num, enable);
	__topaz_fwt_set_4addrmode(&tmp, portal);

	topaz_fwt_copy_entry(hw_ent, &tmp, 4);

out:
	topaz_fwt_cpu_access_unlock_irqrestore(&flags);

	return 0;
}

/* Determine chip revision for reflection detection. */
static void
topaz_fwt_board_init(void)
{
	int hw_rev = _read_hardware_revision();
	if (hw_rev < HARDWARE_REVISION_TOPAZ_A2) {
		sw_reflect_detect = 1;
		printk("enable A1\n");
	}
}

void topaz_set_portal(uint16_t index, uint8_t portal)
{
	union topaz_fwt_entry* hw_ent;
	union topaz_fwt_entry tmp;
	unsigned long flags;

	topaz_fwt_cpu_access_lock_wait_irqsave(&flags);

	hw_ent = topaz_fwt_get_hw_entry(index);
	topaz_fwt_copy_entry(&tmp, hw_ent, 4);

	__topaz_fwt_set_4addrmode(&tmp, portal);

	topaz_fwt_copy_entry(hw_ent, &tmp, 4);

	topaz_fwt_cpu_access_unlock_irqrestore(&flags);
}

int topaz_fwt_init(void)
{
	unsigned int i;
	unsigned long flags;
	int rc;
	BUILD_BUG_ON(TOPAZ_FWT_SW_SIZE !=
			(sizeof(union topaz_fwt_sw_entry) * TOPAZ_FWT_HW_TOTAL_ENTRIES));
	BUILD_BUG_ON(TOPAZ_FWT_MCAST_IPMAP_ENT_SIZE != sizeof(struct topaz_fwt_sw_alias_table));
	BUILD_BUG_ON(TOPAZ_FWT_MCAST_TQE_ENT_SIZE != sizeof(struct topaz_fwt_sw_mcast_entry));

	TAILQ_INIT(&unused_level1_slots);
	TAILQ_INIT(&unused_level2_slots);
	TAILQ_INIT(&used_slots);
	for (i = 0; i < TOPAZ_FWT_HW_TOTAL_ENTRIES; i++) {
		struct topaz_fwt_slot *s = topaz_fwt_get_slot(i);
		s->index = i;
		s->in_use = 0;
		if (i < TOPAZ_FWT_HW_LEVEL1_ENTRIES) {
			TAILQ_INSERT_TAIL(&unused_level1_slots, s, next);
		} else {
			TAILQ_INSERT_TAIL(&unused_level2_slots, s, next);
		}
	}

	rc = topaz_fwt_create_proc();
	if (rc) {
		return rc;
	}

	if (topaz_fwt_cpu_access_lock_wait_irqsave(&flags)) {
		return -EBUSY;
	}

	/* initialize hardware fwt */
	for (i = 0; i < TOPAZ_FWT_HW_TOTAL_ENTRIES; i++) {
		topaz_fwt_insert_entry(&zeroent, i);
		topaz_fwt_sw_entry_del(i);
	}

	topaz_fwt_hash_enable(1);

	topaz_fwt_cpu_access_unlock_irqrestore(&flags);

	topaz_fwt_board_init();

	return 0;
}
