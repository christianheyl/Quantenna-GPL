/*
 * (C) Copyright 2013 Quantenna Communications Inc.
 */

#ifndef __TOPAZ_FWT_H
#define __TOPAZ_FWT_H

#include <qtn/topaz_fwt_cpuif.h>

typedef void (*fwt_notify_swap )( uint16_t dst_index, uint16_t src_index);

/*
 * The FWT algorithm maintain the first level entries available first for a fast look up
 * In scenarios where there is a need to delete a first level entry with following index at the
 * second level, there is a need to copy the second level entry over the first one, then delete
 * the second level entry. The FWT interface register the overwrite call back so we can mirror the
 * same entries indexers in both tables
 * @param cbk_func: call back function to overwrite the index table entries
 */
void topaz_fwt_register_overwrite(fwt_notify_swap cbk_func);

int topaz_fwt_add_entry(const uint8_t *mac_be, uint8_t out_port,
		const uint8_t *out_node, unsigned int out_node_count, uint8_t portal);

int topaz_fwt_del_entry(const uint8_t *mac_id);

uint16_t topaz_fwt_hash(const uint8_t *mac_le);

int topaz_get_mac_be_from_index(uint16_t index, uint8_t *mac_be);

void topaz_update_node(uint16_t index, uint8_t node_index,uint8_t node,bool enable);

void topaz_set_portal(uint16_t index, uint8_t portal);

void topaz_fwt_sw_entry_set(uint16_t index, uint8_t out_port,
		const uint8_t *out_nodes, unsigned int out_node_count, uint8_t portal);
void topaz_fwt_sw_entry_del(uint16_t fwt_index);

int topaz_sw_lookup(const uint8_t *mac_be);

void topaz_fwt_sw_entry_set_multicast(uint16_t fwt_index, uint16_t mcast_index);

int topaz_update_entry(uint16_t index, uint8_t port, uint8_t portal,
		uint8_t node_index , uint8_t node_num, bool enable);

int topaz_fwt_get_timestamp(uint16_t index);
int topaz_fwt_init(void);

#endif	/* __TOPAZ_FWT_H */

