/*
 * (C) Copyright 2013 Quantenna Communications Inc.
 */

#ifndef FWT_DB_H_
#define FWT_DB_H_


#ifndef CONSOLE_TEST
#include <linux/types.h>
#include <linux/in.h>
#include <linux/if_ether.h>

#include <common/queue.h>

#include <qtn/br_types.h>
#include <qtn/topaz_fwt.h>
#include <qtn/dmautil.h>
#include <qtn/topaz_tqe_cpuif.h>
#include <qtn/topaz_fwt_cpuif.h>
#include <qtn/qtn_net_packet.h>

typedef enum
{
	FWT_DB_MLT_PORT_WMAC = 0,
	FWT_DB_MLT_PORT_MAX,
}fwt_db_mlt_ports;

/* Set success return status */
#define FWT_DB_STATUS_SUCCESS (1)

/* Set Invalid node number */
#define FWT_DB_INVALID_NODE (0xFF)

/* Set Invalid IPV4 value */
#define FWT_DB_INVALID_IPV4 (0xFF)

/* Size of IPV4 address */
#define FWT_DB_IPV4_SIZE (4)

/*
 * LHost FWT entry copy.
 * Sufficient for unicast; multicast with multiple ports/nodes
 * is handled in topaz_fwt_sw_mcast_entry
 */
typedef struct fwt_db_entry {
	uint8_t mac_id[ETH_ALEN];
	uint8_t out_port;
	uint8_t out_node;
	int16_t fwt_index;
	int16_t alias_table_index;
	uint32_t false_miss;
	uint8_t portal	:1,
		valid	:1,
#ifdef CONFIG_TOPAZ_DBDC_HOST
		mcast	:1,
		dev_id  :DEV_ID_BITS;
#else
		mcast	:1;
#endif
} fwt_db_entry;

/* node list indexed by the fwt */
typedef struct fwt_db_node_element {
	uint16_t index;
	uint8_t ip_alias;
	uint8_t port;
	STAILQ_ENTRY(fwt_db_node_element) next;
} fwt_db_node_element;

typedef struct {
	fwt_db_node_element *element;
	bool in_use;
	int node_index;
} fwt_db_node_iterator;

static inline struct topaz_fwt_sw_alias_table *
fwt_db_get_sw_alias_table(struct fwt_db_entry *db)
{
	if (db && topaz_fwt_sw_alias_table_index_valid(db->alias_table_index)) {
		return topaz_fwt_sw_alias_table_get(db->alias_table_index);
	}
	return NULL;
}

static inline struct topaz_fwt_sw_mcast_entry *
fwt_db_get_sw_mcast(struct fwt_db_entry *db, uint8_t ipmap_index)
{
	struct topaz_fwt_sw_alias_table *ipmap = fwt_db_get_sw_alias_table(db);
	if (ipmap) {
		int16_t mcast_index = ipmap->mcast_entry_index[ipmap_index];
		if (topaz_fwt_sw_mcast_entry_index_valid(mcast_index)) {
			return topaz_fwt_sw_mcast_entry_get(mcast_index);
		}
	}

	return NULL;
}

static inline struct topaz_fwt_sw_mcast_entry *fwt_db_get_sw_mcast_ff(void)
{
	return topaz_fwt_sw_mcast_ff_entry_get();
}

static inline void topaz_fwt_sw_alias_table_flush(struct topaz_fwt_sw_alias_table *p)
{
	flush_dcache_sizerange_safe(p, sizeof(*p));
}

static inline void topaz_fwt_sw_mcast_flush(struct topaz_fwt_sw_mcast_entry *p)
{
	flush_dcache_sizerange_safe(p, sizeof(*p));
}

static inline void fwt_mcast_to_mac(uint8_t *mac_be, const struct br_ip *group)
{
	return qtn_mcast_to_mac(mac_be, &group->u, group->proto);
}

static inline int8_t fwt_mcast_to_ip_alias(const struct br_ip *group)
{
	if (group == NULL) {
		return -1;
	} else {
		return topaz_fwt_mcast_to_ip_alias(&group->u, group->proto);
	}
}

fwt_db_node_element *fwt_db_create_node_element(void);
void fwt_db_free_node_element(fwt_db_node_element *node_element);
int fwt_db_is_node_exists_list(uint8_t node_index, uint16_t table_index,
		uint8_t ip_alias, uint8_t port);
/*
 * Initialise the fwt_db database
 */
void fwt_db_init(void);

/*
 * Get IP Flood-forwarding configuration
 * @Param buf: location of print buffer
 * @Param buflen: size of print buffer
 * @return number of characters printed
 */
int fwt_db_get_ipff(char *buf, int buflen);

/*
 * Print FWT Database to console
 * @return number of existing valid entries
 */
int fwt_db_print(int is_mult);

/*
 * Print FWT node hash lise to console
 * @return number of existing valid entries
 */
int fwt_db_node_table_print(void);

/*
 * Print the FWT multicast entries to a buffer
 * @return number of bytes written
 */
int fwt_db_get_mc_list(char *buf, int buflen);

/*
 * Calculate ageing time by the FWT HW timestamp
 * @Param fwt_index: FWT table index
 */
int fwt_db_calculate_ageing_scale(int fwt_index);
/*
 * Insert new table entry
 * @param index: the HW FWT index.
 * Note: the FWT SW table use the FWT HW index algorithm for matching the table entries
 * @param element: FWT SW table element that reflect both FWT HW table entry and additional data
 * @return Success / Failure indication
 */
int fwt_db_table_insert(uint16_t index, fwt_db_entry *element);
/*
 * Acquire iterator to run over the list database from node index entry
 * @param node_index: node number represented as a hash list index
 * @return: Iterator database element
 */
fwt_db_node_iterator *fwt_db_iterator_acquire(uint8_t node_index);

/*
 * Release iterator to mark elements on the database can be erase or modify safely.
 */
void fwt_db_iterator_release(void);

/*
 * Give back current element and advance iterator to the next one.
 * service function for running over the database.
 * Note: Node_index is a part of the iterator
 * @param iterator: Iterator database element
 * @return: Current iterator database element
 */
fwt_db_node_element *fwt_db_iterator_next(fwt_db_node_iterator **iterator);

/* Add a new node entry
 * @param node_num:
 * @param table_index: the HW FWT table index
 */
void fwt_db_add_new_node(uint8_t node_num, uint16_t table_index,
		const int8_t ip_alias, uint8_t port, fwt_db_node_element *const new_element);

/* Remove specific node from the hash list,
 * Note: does not involve removing node from the fwt table.
 * @param node_index: the hash list entry point that represent the node number
 * @return: Function returns the number of elements that were removed.(Debug feature)
 */
int fwt_db_clear_node(uint8_t node_index);

/* In cases where fwt entry is remove we need to maintain the specific index from
 * the node table since its not relevant anymore.
 * We conduct the maintenance procedure in order to take advantage of the knowledge of the specific node index
 * so we can avoid going through the whole database.
 * @param node_index: the hash list entry point that represent the node number
 * @param table_index: the specific fwt index to be removed
 */
void fwt_db_delete_index_from_node_table(uint8_t node_index, uint16_t table_index,
		uint8_t ip_alias, uint8_t port);

fwt_db_node_element *fwt_db_get_table_index_from_node(uint8_t node_num);

/*
 * Delete fwt table entry.
 * @param index: the HW FWT index.
 * Note: the FWT SW table use the FWT HW index algorithm for matching the table entries
 */
void fwt_db_delete_table_entry(uint16_t index);
/*
 * Get table entry.
 * @param index: the HW FWT index.
 * Note: return ptr from database. Handle with care.
 * @return indexed fwt database entry
 */
fwt_db_entry *fwt_db_get_table_entry(uint16_t index);

/* Initialize the fwt db entry */
int fwt_db_init_entry(fwt_db_entry *entry);

/*
 * Update parameters to existing entry
 * @param index: the HW FWT index that match the SW one
 * @param port: TQE output port.
 * @param node: node number.
 * @param portal: 4addr mode flag.
 */
int fwt_db_update_params(uint16_t index, uint8_t port, uint8_t node, uint8_t portal);

/*
 * Return the existing entry if present, otherwise create a new multicast entry
 */
struct topaz_fwt_sw_mcast_entry *fwt_db_get_or_add_sw_mcast(struct fwt_db_entry *db,
		int8_t ip_alias);

/*
 * Free multicast entry, and possibly the alias_table if it becomes empty.
 * Returns 1 if there are no multicast entries present under this db anymore.
 */
int fwt_db_delete_sw_mcast(struct fwt_db_entry *db, uint8_t ipmap_index);

/*
 * Get mac addresses of nondes behind associated node
 * @param index: node index
 * @param num_entries: returns number of entries found
 * @param max_req: maximum entries requested
 * @param flags: bit 0 - results overflowed/truncated, bit 1 - 4addr node
 * @param buf: buffer to store macs
 */
int fwt_db_get_macs_behind_node(const uint8_t index, uint32_t *num_entries, uint32_t max_req,
					uint32_t *flags, uint8_t *buf);

#endif // TOPAZ PLATFORM

#endif /* FWT_DB_H_ */
