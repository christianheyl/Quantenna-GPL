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

#ifndef __TOPAZ_TQE_H
#define __TOPAZ_TQE_H

#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <qtn/topaz_tqe_cpuif.h>


typedef void (*tqe_port_handler)(void *token,
		const union topaz_tqe_cpuif_descr *descr,
		struct sk_buff *skb, uint8_t *whole_frm_hdr);

typedef const struct topaz_fwt_sw_mcast_entry*(
		*tqe_fwt_get_mcast_hook)(uint16_t fwt_index, const void *addr, uint16_t ether_type);
typedef const struct fwt_db_entry*(
		*tqe_fwt_get_ucast_hook)(const unsigned char *src_mac_be, const unsigned char *dst_mac_be);
typedef const struct fwt_db_entry*(
		*tqe_fwt_get_from_mac_hook)(const unsigned char *mac_be);
typedef int(*tqe_mac_reserved_hook)(const uint8_t *addr);
typedef struct topaz_fwt_sw_mcast_entry*(
		*tqe_fwt_get_mcast_ff_hook)(void);
typedef void(*tqe_fwt_false_miss_hook)(int fwt_index, uint8_t false_miss);

int tqe_port_add_handler(enum topaz_tqe_port port, tqe_port_handler handler, void *token);
void tqe_port_remove_handler(enum topaz_tqe_port port);
int tqe_tx(union topaz_tqe_cpuif_ppctl *ppctl, struct sk_buff *skb);
void tqe_register_fwt_cbk(tqe_fwt_get_mcast_hook mcast_cbk_func,
				tqe_fwt_get_mcast_ff_hook mcast_ff_get_cbk_func,
				tqe_fwt_false_miss_hook false_miss_func);
void tqe_register_ucastfwt_cbk(tqe_fwt_get_ucast_hook cbk_func);
void tqe_register_macfwt_cbk(tqe_fwt_get_from_mac_hook cbk_func);
void tqe_register_mac_reserved_cbk(tqe_mac_reserved_hook cbk_func);
int tqe_rx_multicast(void *queue, const union topaz_tqe_cpuif_descr *desc);
void tqe_port_register(const enum topaz_tqe_port port);
void tqe_port_unregister(const enum topaz_tqe_port port);
void tqe_reg_multicast_tx_stats(void (*fn)(void *ctx, uint8_t), void *ctx);
void tqe_port_set_group(const enum topaz_tqe_port port, int32_t group);
void tqe_port_clear_group(const enum topaz_tqe_port port);
uint32_t switch_tqe_multi_proc_sem_down(char * funcname, int linenum);
uint32_t switch_tqe_multi_proc_sem_up(void);
int tqe_rx_l2_ext_filter(union topaz_tqe_cpuif_descr *desc, struct sk_buff *skb);
void tqe_rx_call_port_handler(union topaz_tqe_cpuif_descr *desc,
		struct sk_buff *skb, uint8_t *whole_frm_hdr);

#endif	/* __TOPAZ_TQE_H */

