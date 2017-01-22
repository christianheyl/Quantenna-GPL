/*SH0
*******************************************************************************
**                                                                           **
**         Copyright (c) 2014 Quantenna Communications Inc                   **
**                            All Rights Reserved                            **
**                                                                           **
**                                                                           **
*******************************************************************************
EH0*/

#include <linux/types.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <linux/hardirq.h>
#include <linux/if_ether.h>
#include <common/queue.h>
#include <qtn/topaz_fwt_db.h>
#include <qtn/topaz_fwt_sw.h>
#include <qtn/topaz_fwt.h>
#include <qtn/topaz_tqe_cpuif.h>
#include <qtn/qtn_debug.h>
#include <qtn/qtn_net_packet.h>
#include <qtn/qtn_uc_comm.h>
#include <net80211/if_ethersubr.h>
#include <net80211/ieee80211.h>

#define NIPQUAD_LEN 15

struct topaz_fwt_alias_table_slot {
	uint16_t index;
	uint16_t in_use;
	TAILQ_ENTRY(topaz_fwt_alias_table_slot) next;
};
typedef TAILQ_HEAD(, topaz_fwt_alias_table_slot) topaz_fwt_alias_table_slot_head;

struct topaz_fwt_mcast_entry_slot {
	uint16_t index;
	uint16_t in_use;
	TAILQ_ENTRY(topaz_fwt_mcast_entry_slot) next;
};
typedef TAILQ_HEAD(, topaz_fwt_mcast_entry_slot) topaz_fwt_mcast_entry_slot_head;

struct fwt_db_print_info {
	char *bufp;
	int remain;
};

enum fwt_db_buf_print_status {
	FWT_DB_BUF_PRINT_DONE,
	FWT_DB_BUF_PRINT_FULL
};

static topaz_fwt_alias_table_slot_head g_fwt_alias_table_free_slots;
static struct topaz_fwt_alias_table_slot g_fwt_alias_table_slots[TOPAZ_FWT_MCAST_ENTRIES];
static topaz_fwt_mcast_entry_slot_head g_fwt_mcast_entry_free_slots;
static struct topaz_fwt_mcast_entry_slot g_fwt_mcast_entry_slots[TOPAZ_FWT_MCAST_ENTRIES];


static const char *fwt_db_port_names[] = TOPAZ_TQE_PORT_NAMES;

/*
 * The FWT Database (fwt_db) holds several data structures
 * A. The FWT Mirror - that reflect the HW entries of the FWT.
 * B. Node hash list - holds the index of the HW FWT entry of each node
 */

/* Mirror table to the hw fwt */
static fwt_db_entry g_fwt_db[TOPAZ_FWT_HW_TOTAL_ENTRIES];

/* Each table node entry is a link list of all the indexed hw fwt */
typedef STAILQ_HEAD(, fwt_db_node_element) fwt_db_node_element_head;

/* Node table */
static fwt_db_node_element_head g_fwt_db_node_table[QTN_NCIDX_MAX];

/* Iterator definition to cycle through the sub list per node */
static fwt_db_node_iterator g_node_iterator;

/* list macs behind associated node */
int fwt_db_get_macs_behind_node(const uint8_t index, uint32_t *num_entries, uint32_t max_req,
				uint32_t *flags, uint8_t *buf)
{
	int count  = 0;
	int i;
#define LIST_OVERFLOW 1
#define NODE_IS_4ADDR 2

	for (i = 0; i < TOPAZ_FWT_HW_TOTAL_ENTRIES; i++) {
		if (g_fwt_db[i].valid && !ETHER_IS_MULTICAST(g_fwt_db[i].mac_id)
				&& (g_fwt_db[i].out_node == index)) {
			memcpy(buf, &g_fwt_db[i].mac_id, 6);
			buf +=6;
			count++;
			if (g_fwt_db[i].portal)
				*flags |= NODE_IS_4ADDR;
			if (count >= max_req) {
				*flags |= LIST_OVERFLOW;
				break;
			}
		}
	}
	*num_entries = count;
	if (count) {
		return 1;
	} else {
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(fwt_db_get_macs_behind_node);

/*
 * Allocate memory for new node element.
 * @return node element entry
 */
fwt_db_node_element *fwt_db_create_node_element(void)
{

	/* allocate mem for new element */
	fwt_db_node_element *new_element = kzalloc(sizeof(fwt_db_node_element), GFP_KERNEL);

	if (new_element == NULL ) {
		printk(KERN_ERR "%s: Not enough memory for new node element \n", __FUNCTION__);
		return NULL;
	}

	return new_element;
}

void fwt_db_free_node_element(fwt_db_node_element *node_element)
{
	kfree(node_element);
}

static const uint32_t fwt_res_sec = (TOPAZ_FWT_RESOLUTION_MSEC / 1000);

static int fwt_db_calculate_ageing_sec(int fwt_index)
{
	int age_scale = -1;

	age_scale = fwt_db_calculate_ageing_scale(fwt_index);
	if (age_scale > 0) {
		return age_scale * fwt_res_sec;
	}

	return age_scale;
}

struct topaz_fwt_sw_mcast_entry *fwt_db_get_or_add_sw_mcast(struct fwt_db_entry *db,
		int8_t ip_alias)
{
	struct topaz_fwt_sw_alias_table *alias_table;
	struct topaz_fwt_alias_table_slot *alias_table_slot;
	struct topaz_fwt_sw_mcast_entry *mcast;
	struct topaz_fwt_mcast_entry_slot *mcast_slot;

	mcast = fwt_db_get_sw_mcast(db, ip_alias);
	if (mcast) {
		return mcast;
	}

	/* check that resources are available */
	if (!TAILQ_FIRST(&g_fwt_alias_table_free_slots) ||
			!TAILQ_FIRST(&g_fwt_mcast_entry_free_slots)) {
		return NULL;
	}

	/* allocate alias_table if necessary */
	if (!topaz_fwt_sw_alias_table_index_valid(db->alias_table_index)) {
		alias_table_slot = TAILQ_FIRST(&g_fwt_alias_table_free_slots);
		TAILQ_REMOVE(&g_fwt_alias_table_free_slots, alias_table_slot, next);
		db->alias_table_index = alias_table_slot->index;
		alias_table = fwt_db_get_sw_alias_table(db);
		memset(alias_table, ~0x0, sizeof(*alias_table));
		topaz_fwt_sw_alias_table_flush(alias_table);
		topaz_fwt_sw_entry_set_multicast(db->fwt_index, db->alias_table_index);
	}
	alias_table = fwt_db_get_sw_alias_table(db);

	/* allocate mcast entry */
	mcast_slot = TAILQ_FIRST(&g_fwt_mcast_entry_free_slots);
	TAILQ_REMOVE(&g_fwt_mcast_entry_free_slots, mcast_slot, next);
	alias_table->mcast_entry_index[ip_alias] = mcast_slot->index;
	mcast = fwt_db_get_sw_mcast(db, ip_alias);
	memset(mcast, 0, sizeof(*mcast));
	topaz_fwt_sw_alias_table_flush(alias_table);
	topaz_fwt_sw_mcast_flush(mcast);

	return fwt_db_get_sw_mcast(db, ip_alias);
}

int fwt_db_delete_sw_mcast(struct fwt_db_entry *db, uint8_t ip_alias)
{
	int empty = 0;
	struct topaz_fwt_sw_alias_table *alias_table;
	struct topaz_fwt_alias_table_slot *alias_table_slot;
	struct topaz_fwt_mcast_entry_slot *mcast_slot;
	int16_t alias_table_index;
	int16_t mcast_index;

	WARN_ON(!db);

	alias_table_index = db->alias_table_index;

	if (topaz_fwt_sw_alias_table_index_valid(alias_table_index)) {
		alias_table_slot = &g_fwt_alias_table_slots[alias_table_index];
		alias_table = topaz_fwt_sw_alias_table_get(alias_table_index);

		mcast_index = alias_table->mcast_entry_index[ip_alias];

		if (topaz_fwt_sw_mcast_entry_index_valid(mcast_index)) {
			mcast_slot = &g_fwt_mcast_entry_slots[mcast_index];
			topaz_fwt_sw_mcast_entry_get(mcast_index);
			TAILQ_INSERT_TAIL(&g_fwt_mcast_entry_free_slots, mcast_slot, next);
			alias_table->mcast_entry_index[ip_alias] = -1;
			topaz_fwt_sw_alias_table_flush(alias_table);
		}

		if (topaz_fwt_sw_alias_table_empty(alias_table)) {
			TAILQ_INSERT_TAIL(&g_fwt_alias_table_free_slots, alias_table_slot, next);
			db->alias_table_index = -1;
			topaz_fwt_sw_entry_del(db->fwt_index);
			empty = 1;
		}
	}

	return empty;
}

int fwt_db_init_entry(struct fwt_db_entry *entry) {

	if (entry == NULL) {
		return -EINVAL;
	}

	memset(entry, 0xFF, sizeof(*entry));

	entry->valid = false;
	entry->false_miss = 0;
	entry->fwt_index = -1;

	return 0;
}

int fwt_db_is_node_exists_list(uint8_t node_index, uint16_t table_index,
		uint8_t ip_alias, uint8_t port)
{
	fwt_db_node_element *ptr = NULL;
	STAILQ_FOREACH(ptr, &g_fwt_db_node_table[node_index], next) {
		if (ptr->index == table_index &&
				ptr->ip_alias == ip_alias &&
				ptr->port == port) {
			return 1;
		}
	}
	return 0;
}

void fwt_db_add_new_node(uint8_t node_num, uint16_t table_index,
		const int8_t ip_alias, uint8_t port, fwt_db_node_element *const new_element)
{
	if (new_element) {
		new_element->index = table_index;
		new_element->ip_alias = ip_alias;
		new_element->port = port;
		STAILQ_INSERT_TAIL(&g_fwt_db_node_table[node_num], new_element, next);
	}
}

int fwt_db_clear_node(uint8_t node_index)
{
	uint16_t count = 0;
	fwt_db_node_element *ptr;
	fwt_db_node_element *tmp;

	STAILQ_FOREACH_SAFE(ptr, &g_fwt_db_node_table[node_index], next, tmp) {
		STAILQ_REMOVE(&g_fwt_db_node_table[node_index], ptr, fwt_db_node_element, next);
		kfree(ptr);
		ptr = NULL;
		count++;
	}
	return count;
}

void fwt_db_delete_index_from_node_table(uint8_t node_index, uint16_t table_index,
		uint8_t ip_alias, uint8_t port)
{
	fwt_db_node_element *ptr;
	fwt_db_node_element *tmp;

	STAILQ_FOREACH_SAFE(ptr, &g_fwt_db_node_table[node_index], next, tmp) {
		if ((ptr->index == table_index) &&
				(ptr->ip_alias == ip_alias) &&
				(ptr->port == port)) {
			STAILQ_REMOVE(&g_fwt_db_node_table[node_index], ptr,
					fwt_db_node_element, next);
			kfree(ptr);
			ptr = NULL;
			break;
		}
	}
}

fwt_db_node_iterator *fwt_db_iterator_acquire(uint8_t node_index)
{
	/* only one iterator can be held at all times */
	if (g_node_iterator.in_use) {
		printk(KERN_ERR "%s: iterator is in use for %u\n", __func__,
			g_node_iterator.node_index);
		return NULL;
	}

	g_node_iterator.in_use = true;
	/* acquire iterator always gives the first node entry */
	g_node_iterator.element = g_fwt_db_node_table[node_index].stqh_first;

	/* mark iterator current node index */
	g_node_iterator.node_index = node_index;

	return &g_node_iterator;

}

void fwt_db_iterator_release(void)
{
	g_node_iterator.in_use = false;
	g_node_iterator.element = NULL;
	g_node_iterator.node_index = -1;
}

fwt_db_node_element *fwt_db_iterator_next(fwt_db_node_iterator **iterator)
{
	/* save current */
	fwt_db_node_element *current_element = (*iterator)->element;

	if ((*iterator)->element == NULL) {
		return NULL;
	}

	/* advance iterator */
	(*iterator)->element = (*iterator)->element->next.stqe_next;

	return current_element;
}

int fwt_db_table_insert(uint16_t index, fwt_db_entry *element)
{

	if (element == NULL || index >= TOPAZ_FWT_HW_TOTAL_ENTRIES) {
		return -EINVAL;
	}

	memcpy(&g_fwt_db[index], element, sizeof(*element));

	return FWT_DB_STATUS_SUCCESS;
}

static void fwt_db_init_mcast_ff(void)
{
	struct topaz_fwt_sw_mcast_entry *mcast_entry;

	mcast_entry = fwt_db_get_sw_mcast_ff();
	memset(mcast_entry, 0 , sizeof(*mcast_entry));
	topaz_fwt_sw_mcast_flood_forward_set(mcast_entry, 1);
	topaz_fwt_sw_mcast_flush(mcast_entry);
}

void fwt_db_init(void)
{
	int i;

	for (i = 0; i < TOPAZ_FWT_HW_TOTAL_ENTRIES; i++) {
		fwt_db_init_entry(&g_fwt_db[i]);
	}

	for (i = 0; i < QTN_NCIDX_MAX; i++) {
		STAILQ_INIT(&g_fwt_db_node_table[i]);
	}

	TAILQ_INIT(&g_fwt_alias_table_free_slots);
	for (i = 0; i < ARRAY_SIZE(g_fwt_alias_table_slots); i++) {
		struct topaz_fwt_alias_table_slot *s = &g_fwt_alias_table_slots[i];
		s->index = i;
		s->in_use = 0;
		TAILQ_INSERT_TAIL(&g_fwt_alias_table_free_slots, s, next);
	}

	TAILQ_INIT(&g_fwt_mcast_entry_free_slots);
	for (i = 0; i < ARRAY_SIZE(g_fwt_mcast_entry_slots); i++) {
		struct topaz_fwt_mcast_entry_slot *s = &g_fwt_mcast_entry_slots[i];
		s->index = i;
		s->in_use = 0;
		TAILQ_INSERT_TAIL(&g_fwt_mcast_entry_free_slots, s, next);
	}

	fwt_db_init_mcast_ff();

	/* Init node iterator */
	fwt_db_iterator_release();
}

void fwt_db_delete_table_entry(uint16_t index)
{
	struct fwt_db_entry *db_ent;
	unsigned int i;

	if (index >= TOPAZ_FWT_HW_TOTAL_ENTRIES) {
		return;
	}

	/* shouldn't be required normally; make sure mcast entries are not orphaned */
	db_ent = fwt_db_get_table_entry(index);
	for (i = 0; i < TOPAZ_FWT_SW_IP_ALIAS_ENTRIES; i++) {
		fwt_db_delete_sw_mcast(db_ent, i);
	}

	fwt_db_init_entry(db_ent);
}
EXPORT_SYMBOL(fwt_db_get_table_entry);

int fwt_db_update_params(uint16_t index, uint8_t port, uint8_t node, uint8_t portal)
{
	if (g_fwt_db[index].valid == false) {
		return -ENOENT;
	}
	g_fwt_db[index].out_port = port;
	g_fwt_db[index].out_node = node;
	g_fwt_db[index].portal = !!portal;
	return 1;
}

fwt_db_entry *__sram_text fwt_db_get_table_entry(uint16_t index)
{
	return &g_fwt_db[index];
}

int fwt_db_calculate_ageing_scale(int fwt_index)
{
	int32_t last_seen_scaled;
	uint32_t now_scaled;

	if (fwt_index < 0) {
		return -1;
	}
	/* Get the last time that this entry was seen in FWT scale units*/
	last_seen_scaled = topaz_fwt_get_timestamp(fwt_index);

	if (last_seen_scaled < 0) {
		return -1;
	}

	/* Get the current timestamp in FWT units */
	now_scaled = topaz_fwt_get_scaled_timestamp();

	/* prevent overflow */
	if (now_scaled < last_seen_scaled) {
		now_scaled += (1 << TOPAZ_FWT_TIMESTAMP_BITS);
	}

	return now_scaled - last_seen_scaled;
}

int fwt_db_node_table_print(void)
{
	int i;
	int total_count = 0;
	uint8_t toggle_print = 0;
	fwt_db_node_element *ptr = NULL;
	fwt_db_entry *entry;
	uint8_t ipv4[FWT_DB_IPV4_SIZE];

	printk("Node Index IP/MAC\n");

	for (i = 0; i < QTN_NCIDX_MAX; i++) {
		if (toggle_print)
			printk("\n");
		toggle_print = 0;
		STAILQ_FOREACH(ptr, &g_fwt_db_node_table[i], next) {
			toggle_print = 1;
			entry = fwt_db_get_table_entry(ptr->index);
			if (ptr->ip_alias ==  FWT_DB_INVALID_IPV4) {
				printk("%4u %5u %pM\n",
					i, ptr->index, entry->mac_id);
			} else {
				qtn_mcast_mac_to_ipv4(ipv4, entry->mac_id, ptr->ip_alias);
				printk("%4u %5u %pI4\n",
					i, ptr->index, ipv4);
			}
			total_count++;
		}
	}

	return total_count;
}

const char *fwt_port_names[] = TOPAZ_TQE_PORT_NAMES;

static int fwt_db_print_port(const struct topaz_fwt_sw_mcast_entry *mcast_entry, uint8_t port)
{
	uint16_t count = 0;
	uint8_t node;

	printk("%-7s ", fwt_port_names[port]);

	if (topaz_fwt_sw_mcast_port_has_nodes(port)) {
		for (node = 0; node < TOPAZ_FWT_SW_NODE_MAX; node++) {
			if (topaz_fwt_sw_mcast_node_is_set(mcast_entry, port, node)) {
				printk("%u, ", node);
			}
		}
		count++;
	} else {
		printk("-");
	}
	printk("\n");

	return count;
}

static int fwt_db_print_mcast_entry(int index, const struct topaz_fwt_sw_mcast_entry *mcast_entry,
					const uint8_t *ipv4)
{
	uint8_t end_line = 0;
	uint16_t count = 0;
	uint8_t flood = topaz_fwt_sw_mcast_is_flood_forward(mcast_entry);
	int i;
	char ipv4_str[NIPQUAD_LEN + 1];

	snprintf(ipv4_str, sizeof(ipv4_str), "%pI4", ipv4);

	printk("%-5u %-17s %5u %7d %7u ",
		index,
		ipv4_str,
		flood,
		flood ? 0 : fwt_db_calculate_ageing_sec(index),
		g_fwt_db[index].false_miss);

	for (i = 0; i < TOPAZ_TQE_NUM_PORTS; i++) {
		if (mcast_entry->port_bitmap & (1 << i)) {
			if (end_line)
				printk("                                              ");
			count += fwt_db_print_port(mcast_entry, i);
			end_line = 1;
		}
	}

	return count;
}

static int fwt_db_print_unicast(void)
{
	char *is_4addr[] = {"yes", "no"};
	int count = 0;
	int i;

	printk("Unicast Table\n");
	printk("Index MAC               4addr  Ageing Retry Port    Node\n");

	for (i = 0; i < TOPAZ_FWT_HW_TOTAL_ENTRIES; i++) {
		if (g_fwt_db[i].valid && !ETHER_IS_MULTICAST(g_fwt_db[i].mac_id)) {
			printk("%-5u %pM %5s %7u %5u %-7s %u\n", i,
					g_fwt_db[i].mac_id,
					is_4addr[g_fwt_db[i].portal],
					fwt_db_calculate_ageing_sec(i),
					g_fwt_db[i].false_miss,
					fwt_db_port_names[g_fwt_db[i].out_port],
					g_fwt_db[i].out_node);
			count++;
		}
	}

	return count;
}

extern struct topaz_fwt_sw_mcast_entry *g_mcast_ff_entry;

static int fwt_db_print_multicast(void)
{
	int count = 0;
	int i;
	int k;
	uint8_t ipv4[FWT_DB_IPV4_SIZE];
	uint8_t node;
	struct topaz_fwt_sw_mcast_entry *mcast_entry = NULL;
	uint8_t port;

	printk("\n");
	printk("Multicast Table\n");
	printk("Index IP                Flood  Ageing   Retry Ports   Nodes\n");

	for (i = 0; i < TOPAZ_FWT_HW_TOTAL_ENTRIES; i++) {
		struct fwt_db_entry *db = &g_fwt_db[i];
		if (db->valid && ETHER_IS_MULTICAST(db->mac_id)) {
			for (k = 0; k < TOPAZ_FWT_SW_IP_ALIAS_ENTRIES; k++) {
				mcast_entry = fwt_db_get_sw_mcast(db, k);
				if (mcast_entry) {
					/* FIXME support IPv6 */
					qtn_mcast_mac_to_ipv4(ipv4, g_fwt_db[i].mac_id, k);
					count += fwt_db_print_mcast_entry(i, mcast_entry, ipv4);
				}
			}
		}
	}

	printk("\n");
	printk("Flood-forwarding Ports and Nodes\n");

	mcast_entry = g_mcast_ff_entry;
	printk("Ports: ");
	for (port = 0; port < TOPAZ_TQE_NUM_PORTS; port++) {
		if (mcast_entry->port_bitmap & (1 << port))
			printk("%s ", fwt_port_names[port]);
	}
	printk("\n");

	printk("Nodes: ");
	for (node = 0; node < TOPAZ_FWT_SW_NODE_MAX; node++) {
		if (topaz_fwt_sw_mcast_node_is_set(mcast_entry, TOPAZ_TQE_WMAC_PORT, node))
			printk("%u, ", node);
	}
	printk("\n");

	return count;
}

int fwt_db_print(int is_mult)
{
	if (!is_mult)
		return fwt_db_print_unicast();
	else
		return fwt_db_print_multicast();
}

int fwt_db_get_ipff(char *buf, int buflen)
{
	struct fwt_db_entry *db;
	struct topaz_fwt_sw_mcast_entry *mcast_entry = NULL;
	uint8_t ipv4[FWT_DB_IPV4_SIZE];
	int i;
	int j;
	int printed;
	int buf_used = 0;

	for (i = 0; i < TOPAZ_FWT_HW_TOTAL_ENTRIES; i++) {
		db = &g_fwt_db[i];
		if (db->valid && ETHER_IS_MULTICAST(db->mac_id)) {
			for (j = 0; j < TOPAZ_FWT_SW_IP_ALIAS_ENTRIES; j++) {
				mcast_entry = fwt_db_get_sw_mcast(db, j);
				if (mcast_entry && mcast_entry->flood_forward) {
					qtn_mcast_mac_to_ipv4(ipv4, g_fwt_db[i].mac_id, j);
					printed = min(buflen, snprintf(buf, buflen, "%pI4\n", ipv4));
					buf_used += printed;
					buf += printed;
					buflen -= printed;
					if (buflen == 0)
						return buf_used;
				}
			}
		}
	}

	return buf_used;
}

static enum fwt_db_buf_print_status fwt_db_buf_print(struct fwt_db_print_info *pi, char *fmt, ...)
{
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsnprintf(pi->bufp, pi->remain, fmt, args);
	va_end(args);

	pi->remain -= i;
	pi->bufp += i;

	if (pi->remain <= 0)
		return FWT_DB_BUF_PRINT_FULL;

	return FWT_DB_BUF_PRINT_DONE;
};

static void fwt_db_get_mc_nodes(struct fwt_db_print_info *pi,
				const struct topaz_fwt_sw_mcast_entry *mcast_entry, uint8_t port)
{
	uint8_t node;

	if (topaz_fwt_sw_mcast_port_has_nodes(port)) {
		for (node = 0; node < TOPAZ_FWT_SW_NODE_MAX; node++) {
			if (topaz_fwt_sw_mcast_node_is_set(mcast_entry, port, node)) {
				/* FIXME convert node to MAC address */
				if (fwt_db_buf_print(pi, " %s:%u",
						fwt_port_names[port], node) == FWT_DB_BUF_PRINT_FULL)
					return;
			}
		}
	} else {
		fwt_db_buf_print(pi, " %s", fwt_port_names[port]);
	}


	return;
}

static void fwt_db_get_mc_entries(struct fwt_db_print_info *pi,
				const struct topaz_fwt_sw_mcast_entry *mcast_entry,
				const uint8_t *ipv4)
{
	uint8_t flood = topaz_fwt_sw_mcast_is_flood_forward(mcast_entry);
	int i;
	char ipv4_str[NIPQUAD_LEN + 1];

	snprintf(ipv4_str, sizeof(ipv4_str), "%pI4", ipv4);

	if (fwt_db_buf_print(pi, "%-*s", NIPQUAD_LEN, ipv4_str) == FWT_DB_BUF_PRINT_FULL)
		return;

	if (flood) {
		if (fwt_db_buf_print(pi, " flood") == FWT_DB_BUF_PRINT_FULL)
			return;
	} else {
		for (i = 0; i < TOPAZ_TQE_NUM_PORTS; i++) {
			if (mcast_entry->port_bitmap & (1 << i)) {
				fwt_db_get_mc_nodes(pi, mcast_entry, i);
				if (pi->remain <= 0)
					break;
			}
			if (pi->remain <= 0)
				break;
		}
	}
	fwt_db_buf_print(pi, "\n");

	return;
}

int fwt_db_get_mc_list(char *buf, int buflen)
{
	struct fwt_db_entry *db;
	struct topaz_fwt_sw_mcast_entry *mcast_entry;
	uint8_t ipv4[FWT_DB_IPV4_SIZE];
	int i;
	int j;
	struct fwt_db_print_info pi;
	const char *truncated_str = "\n[list truncated]\n";

	pi.bufp = buf;
	pi.remain = buflen;

	for (i = 0; i < TOPAZ_FWT_HW_TOTAL_ENTRIES; i++) {
		db = &g_fwt_db[i];
		if (db->valid && ETHER_IS_MULTICAST(db->mac_id)) {
			for (j = 0; j < TOPAZ_FWT_SW_IP_ALIAS_ENTRIES; j++) {
				mcast_entry = fwt_db_get_sw_mcast(db, j);
				if (!mcast_entry)
					continue;
				qtn_mcast_mac_to_ipv4(ipv4, g_fwt_db[i].mac_id, j);
				fwt_db_get_mc_entries(&pi, mcast_entry, ipv4);
				if (pi.remain <= 0)
					break;
			}
		}
		if (pi.remain <= 0)
			break;
	}

	buf[buflen - 1] = '\0';

	if (pi.remain > 0)
		return buflen - pi.remain;

	strncpy(&buf[buflen - strlen(truncated_str) - 1], truncated_str,
		strlen(truncated_str) + 1);

	return buflen;
}

