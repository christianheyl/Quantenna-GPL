/**
  Copyright (c) 2008 - 2013 Quantenna Communications Inc
  All Rights Reserved

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

 **/
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/skbuff.h>
#include <linux/sysfs.h>
#include <linux/list.h>
#include <linux/ctype.h>
#include <linux/moduleloader.h>

#include <asm/board/board_config.h>

#include <net/pkt_sched.h>
#include <trace/ippkt.h>

#include <qtn/lhost_muc_comm.h>
#include <qtn/qtn_global.h>
#include <qtn/qtn_trace.h>
#include <qtn/qdrv_sch.h>

#include "qdrv_mac.h"
#include "qdrv_vap.h"
#include "qdrv_sch_pm.h"
#include "qdrv_sch_wmm.h"
#include "qdrv_debug.h"
#include "qdrv_wlan.h"

#define QDRV_SCH_NAME_NORMAL	"qdrv_sch"
#define QDRV_SCH_NAME_RED	"qdrv_sch_red"
#define QDRV_SCH_NAME_JOIN	"qdrv_sch_join"

extern int g_qdrv_non_qtn_assoc;

enum qdrv_sch_type
{
	QDRV_SCH_TYPE_NORMAL,
	QDRV_SCH_TYPE_RED,
	QDRV_SCH_TYPE_JOIN
};


#ifdef CONFIG_IPV6
extern int ipv6_skip_exthdr(const struct sk_buff *skb, int start, uint8_t *nexthdrp);
#endif

struct qdrv_sch_shared_data_list
{
	struct list_head head;
	spinlock_t lock;
};

const int qdrv_sch_band_prio[] = {
	QDRV_BAND_CTRL,
	QDRV_BAND_AC_VO,
	QDRV_BAND_AC_VI,
	QDRV_BAND_AC_BE,
	QDRV_BAND_AC_BK
};

static const char *ac_name[] = {"BE", "BK", "VI", "VO"};

struct qdrv_sch_band_aifsn qdrv_sch_band_chg_prio[] = {
	{QDRV_BAND_CTRL, 1},
	{QDRV_BAND_AC_VO, 1},
	{QDRV_BAND_AC_VI, 1},
	{QDRV_BAND_AC_BE, 3},
	{QDRV_BAND_AC_BK, 7}
};

static struct qdrv_sch_shared_data_list qdrv_sch_shared_data;

inline const char *qdrv_sch_tos2ac_str(int tos)
{
	if ((tos < 0) || (tos >= IEEE8021P_PRIORITY_NUM))
		return NULL;

	return ac_name[qdrv_sch_tos2ac[tos]];
}

inline void qdrv_sch_set_ac_map(int tos, int aid)
{
	if ((tos >= 0) && (tos < IEEE8021P_PRIORITY_NUM) &&
			(aid >= 0) && (aid < QDRV_SCH_PRIORITIES)) {
		qdrv_sch_tos2ac[tos] = aid;
	}
}

void qdrv_sch_set_8021p_map(uint8_t ip_dscp, uint8_t dot1p_up)
{
	uint8_t i;
	if (ip_dscp < IP_DSCP_NUM && dot1p_up < IEEE8021P_PRIORITY_NUM) {
		qdrv_sch_dscp2dot1p[ip_dscp] = dot1p_up;
	} else if (ip_dscp == IP_DSCP_NUM) {
		for (i = 0; i < IP_DSCP_NUM; i++)
			qdrv_sch_dscp2dot1p[i] = i>>3;
	}
}

uint32_t qdrv_sch_get_emac_in_use(void)
{
	uint32_t emac_in_use = 0;
	int emac_cfg = 0;

	if (get_board_config(BOARD_CFG_EMAC0, &emac_cfg) == 0) {
		if (emac_cfg & EMAC_IN_USE) {
			emac_in_use |= QDRV_SCH_EMAC0_IN_USE;
		}
	}
	if (get_board_config(BOARD_CFG_EMAC1, &emac_cfg) == 0) {
		if (emac_cfg & EMAC_IN_USE) {
			emac_in_use |= QDRV_SCH_EMAC1_IN_USE;
		}
	}

	return emac_in_use;
}

int qdrv_sch_set_dscp2ac_map(const uint8_t vapid, uint8_t *ip_dscp, uint8_t listlen, uint8_t ac)
{
	uint8_t i;
	const uint32_t emac_in_use = qdrv_sch_get_emac_in_use();

	for (i = 0; i < listlen; i++) {
		qdrv_sch_mask_settid(vapid, ip_dscp[i], WME_AC_TO_TID(ac), emac_in_use);
	}

	return 0;
}

void qdrv_sch_set_dscp2tid_map(const uint8_t vapid, const uint8_t *dscp2tid)
{
	uint8_t dscp;
	uint8_t tid;
	const uint32_t emac_in_use = qdrv_sch_get_emac_in_use();

	for (dscp = 0; dscp < IP_DSCP_NUM; dscp++) {
		tid = dscp2tid[dscp];
		if (tid >= IEEE8021P_PRIORITY_NUM)
			tid = qdrv_dscp2tid_default(dscp);
		tid = QTN_TID_MAP_UNUSED(tid);
		qdrv_sch_mask_settid(vapid, dscp, tid, emac_in_use);
	}
}

void qdrv_sch_get_dscp2tid_map(const uint8_t vapid, uint8_t *dscp2tid)
{
	uint8_t dscp;

	for (dscp = 0; dscp < IP_DSCP_NUM; dscp++) {
		dscp2tid[dscp] = qdrv_sch_mask_gettid(vapid, dscp);
	}
}

uint8_t qdrv_sch_get_8021p_map(uint8_t ip_dscp)
{
	if (ip_dscp < IP_DSCP_NUM)
		return qdrv_sch_dscp2dot1p[ip_dscp];

	return IEEE8021P_PRIORITY_NUM;
}

int qdrv_sch_get_dscp2ac_map(const uint8_t vapid, uint8_t *dscp2ac)
{
	uint8_t i;

	if (!dscp2ac)
		return -1;

	for (i = 0; i < IP_DSCP_NUM; i++){
		dscp2ac[i] = TID_TO_WME_AC(qdrv_sch_mask_gettid(vapid, i));
	}

	return 0;
}

static void qdrv_sch_init_shared_data_list(struct qdrv_sch_shared_data_list *list)
{
	INIT_LIST_HEAD(&list->head);
	spin_lock_init(&list->lock);
}

static void *qdrv_sch_alloc_fast_data(size_t sz)
{
	void *ret = NULL;

#ifdef CONFIG_ARCH_RUBY_NUMA
	ret = heap_sram_alloc(sz);
#endif
	if (!ret) {
		ret = kmalloc(sz, GFP_KERNEL);
	}
	if (ret) {
		memset(ret, 0, sz);
	}

	return ret;
}

static void qdrv_sch_free_fast_data(void *ptr)
{
#ifdef CONFIG_ARCH_RUBY_NUMA
	if (heap_sram_ptr(ptr)) {
		heap_sram_free(ptr);
		ptr = NULL;
	}
#endif

	if (ptr) {
		kfree(ptr);
	}
}

static void qdrv_sch_exit_shared_data_list(struct qdrv_sch_shared_data_list *list)
{
	struct list_head *pos, *temp;

	list_for_each_safe(pos, temp, &list->head) {
		struct qdrv_sch_shared_data *qsh =
			container_of(pos, struct qdrv_sch_shared_data, entry);
		list_del(pos);
		qdrv_sch_free_fast_data(qsh);
	}
}

static int qdrv_sch_tokens_param(struct Qdisc *sch)
{
	/* Maximum combined number of packets we can hold in all queues. */
	return max((int)qdisc_dev(sch)->tx_queue_len, QDRV_SCH_BANDS);
}

static int qdrv_sch_random_threshold_param(int tokens, enum qdrv_sch_type sch_type)
{
	/*
	 * Random drop threshold is calculated the way all low bits are set to 1.
	 * So for example threshold is 511, then if we apply 511 mask to random number we get [0;511] random value,
	 * which is used as probability.
	 * If probability value is higher than number of remainining tokens - packet is to be dropped.
	 * So at the beginning we do not drop anything, but after reaching threshold value we start dropping.
	 * And as more full queue become - more chances for packet to be dropped.
	 */

	int random_drop_threshold = 0;

	if (sch_type == QDRV_SCH_TYPE_RED) {
		int i;
		random_drop_threshold = tokens / 3;
		for (i = 0; random_drop_threshold; ++i) {
			random_drop_threshold = (random_drop_threshold >> 1);
		}
		if (i >= 4) {
			random_drop_threshold = (1 << (i - 1)) - 1;
		} else {
			random_drop_threshold = 0;
		}
	}

	return random_drop_threshold;
}

static int qdrv_sch_cmp_netdevice_name(const char *name1, const char *name2)
{
	/*
	 * Help to group devices with matching non-numeric prefixes together.
	 * E.g. wifi0 and wifi1 would be in one group; eth0 and eth1_0 would be in another.
	 * Group based on the first letter only; this allows 'wds' and 'wifi' to group together
	 */
	if (*name1 == *name2 && isalpha(*name1)) {
		while (isalpha(*name1))
			name1++;
		while (isalpha(*name2))
			name2++;
	} else {
		return 0;
	}

	while (*name1 && *name2) {
		if (*name1 != *name2) {
			return (isdigit(*name1) && isdigit(*name2));
		}
		++name1;
		++name2;
	}

	return 1;
}

static void qdrv_tx_sch_node_data_users(struct qdrv_sch_shared_data *sd, uint32_t users)
{
	sd->users = MAX(users, 1);
	sd->reserved_tokens_per_user =
		(sd->total_tokens - sd->random_drop_threshold) / sd->users;
}

void qdrv_tx_sch_node_data_init(struct Qdisc *sch, struct qdrv_sch_shared_data *sd,
				struct qdrv_sch_node_data *nd, const uint32_t users)
{
	int i;
	unsigned long flags;

	if (sch == NULL || sch == nd->qdisc) {
		return;
	}

	memset(nd, 0, sizeof(*nd));

	qdrv_sch_shared_data_lock(sd, flags);
	nd->shared_data = sd;
	nd->qdisc = sch;
	qdrv_tx_sch_node_data_users(sd, users);
	qdrv_sch_shared_data_unlock(sd, flags);

	for (i = 0; i < ARRAY_SIZE(nd->bands); i++) {
		skb_queue_head_init(&nd->bands[i].queue);
	}
}

void qdrv_tx_sch_node_data_exit(struct qdrv_sch_node_data *nd, const uint32_t users)
{
	struct qdrv_sch_shared_data *sd = nd->shared_data;
	unsigned long flags;

	qdrv_sch_shared_data_lock(sd, flags);
	qdrv_tx_sch_node_data_users(sd, users);
	qdrv_sch_shared_data_unlock(sd, flags);

	memset(nd, 0, sizeof(*nd));
}

int qdrv_sch_node_is_active(const struct qdrv_sch_node_band_data *nbd,
				const struct qdrv_sch_node_data *nd, uint8_t band)
{
	struct qdrv_sch_shared_data *sd = nd->shared_data;
	struct qdrv_sch_shared_band_data *sbd = &sd->bands[band];
	struct qdrv_sch_node_band_data *nbd_tmp;
	unsigned long flags;

	qdrv_sch_shared_data_lock(sd, flags);

	nbd_tmp = TAILQ_FIRST(&sbd->active_nodes);
	while (nbd_tmp) {
		if (nbd_tmp == nbd) {
			qdrv_sch_shared_data_unlock(sd, flags);
			return 1;
		}
		nbd_tmp = TAILQ_NEXT(nbd_tmp, nbd_next);
	}

	qdrv_sch_shared_data_unlock(sd, flags);

	return 0;
}

struct qdrv_sch_shared_data *qdrv_sch_shared_data_init(int16_t tokens, uint16_t rdt)
{
	struct qdrv_sch_shared_data *qsh;
	int i;

	qsh = qdrv_sch_alloc_fast_data(sizeof(*qsh));
	if (!qsh) {
		return NULL;
	}

	memset(qsh, 0, sizeof(*qsh));

	spin_lock_init(&(qsh->lock));
	qsh->drop_callback = &consume_skb;
	qsh->total_tokens = tokens;
	qsh->available_tokens = tokens;
	qsh->random_drop_threshold = rdt;
	for (i = 0; i < ARRAY_SIZE(qsh->bands); i++) {
		TAILQ_INIT(&qsh->bands[i].active_nodes);
	}
	qsh->queuing_alg = QTN_GLOBAL_INIT_TX_QUEUING_ALG;

	return qsh;
}

void qdrv_sch_shared_data_exit(struct qdrv_sch_shared_data *qsh)
{
	if (qsh->users != 0) {
		panic(KERN_ERR "%s: users is not zero\n", __FUNCTION__);
	}

	qdrv_sch_free_fast_data(qsh);
}

static __sram_text void qdrv_sch_drop_callback(struct sk_buff *skb)
{
	struct net_device *ndev = skb->dev;
	struct Qdisc *q;
	struct qdrv_sch_node_data *nd;
	struct qtn_skb_recycle_list *recycle_list = qtn_get_shared_recycle_list();

	if (likely(ndev)) {
		q = netdev_get_tx_queue(ndev, 0)->qdisc;
		nd = qdisc_priv(q);
		qdrv_sch_complete(nd, skb, 0);
	} else {
		if (printk_ratelimit()) {
			printk(KERN_ERR "%s: skb with NULL device set\n", __FUNCTION__);
		}
	}

	if (!qtn_skb_recycle_list_push(recycle_list, &recycle_list->stats_eth, skb)) {
		dev_kfree_skb_any(skb);
	}
}

static struct qdrv_sch_shared_data *qdrv_sch_alloc_shared_data(struct Qdisc *sch,
	struct qdrv_sch_shared_data_list *list, enum qdrv_sch_type sch_type)
{
	struct qdrv_sch_shared_data *qsh;
	int16_t tokens = qdrv_sch_tokens_param(sch);
	uint16_t rdt = qdrv_sch_random_threshold_param(tokens, sch_type);
	size_t len;

	qsh = qdrv_sch_shared_data_init(tokens, rdt);
	if (!qsh) {
		return NULL;
	}

	len = strlen(qdisc_dev(sch)->name);
	if (len >= sizeof(qsh->dev_name)) {
		qdrv_sch_shared_data_exit(qsh);
		return NULL;
	}
	strncpy(qsh->dev_name, qdisc_dev(sch)->name, sizeof(qsh->dev_name));

	qsh->users = 1;
	qsh->drop_callback = &qdrv_sch_drop_callback;
	qsh->reserved_tokens_per_user = QDRV_SCH_RESERVED_TOKEN_PER_USER;

	spin_lock(&list->lock);
	list_add(&qsh->entry, &list->head);
	spin_unlock(&list->lock);

	return qsh;
}

static struct qdrv_sch_shared_data* qdrv_sch_find_shared_data(struct Qdisc *sch,
	struct qdrv_sch_shared_data_list *list)
{
	struct qdrv_sch_shared_data *qsh = NULL;
	struct list_head *pos;

	spin_lock(&list->lock);
	list_for_each(pos, &list->head) {
		qsh = container_of(pos, struct qdrv_sch_shared_data, entry);
		if (qdrv_sch_cmp_netdevice_name(qsh->dev_name, qdisc_dev(sch)->name)) {
			printk(KERN_INFO"%s: %s join %s\n", sch->ops->id,
				qdisc_dev(sch)->name, qsh->dev_name);
			break;
		}
		qsh = NULL;
	}
	spin_unlock(&list->lock);

	return qsh;
}

static struct qdrv_sch_shared_data* qdrv_sch_acquire_shared_data(struct Qdisc *sch,
	struct qdrv_sch_shared_data_list *list, enum qdrv_sch_type sch_type)
{
	struct qdrv_sch_shared_data *qsh = NULL;
	unsigned long flags;

	switch (sch_type) {
	case QDRV_SCH_TYPE_JOIN:
		qsh = qdrv_sch_find_shared_data(sch, list);
		if (qsh) {
			qdrv_sch_shared_data_lock(qsh, flags);
			qsh->total_tokens += QDRV_SCH_RESERVED_TOKEN_PER_USER;
			qsh->available_tokens += QDRV_SCH_RESERVED_TOKEN_PER_USER;
			++qsh->users;
			qdrv_sch_shared_data_unlock(qsh, flags);
		}
		break;

	case QDRV_SCH_TYPE_NORMAL:
	case QDRV_SCH_TYPE_RED:
		qsh = qdrv_sch_alloc_shared_data(sch, list, sch_type);
		break;
	}

	return qsh;
}

static void qdrv_sch_release_shared_data(struct qdrv_sch_shared_data_list *list, struct qdrv_sch_shared_data *qsh)
{
	unsigned long flags;

	if (qsh) {
		spin_lock(&list->lock);

		qdrv_sch_shared_data_lock(qsh, flags);
		if (--qsh->users == 0) {
			list_del(&qsh->entry);
			qdrv_sch_shared_data_unlock(qsh, flags);
			qdrv_sch_shared_data_exit(qsh);
		} else {
			qsh->total_tokens -= QDRV_SCH_RESERVED_TOKEN_PER_USER;
			qsh->available_tokens -= QDRV_SCH_RESERVED_TOKEN_PER_USER;
			KASSERT((qsh->available_tokens >= 0),
					("%s available tokens becomes a negative value\n", __FUNCTION__));
			qdrv_sch_shared_data_unlock(qsh, flags);
		}

		spin_unlock(&list->lock);
	}
}

static ssize_t qdrv_sch_stats_snprintf(struct net_device *ndev, char *buf, ssize_t limit,
		const struct qdrv_sch_node_data *q)
{
	const struct Qdisc *sch = NULL;
	const struct qdrv_sch_shared_data *qsh = q->shared_data;
	ssize_t k = 0;
	int i;

	if (ndev) {
		sch = netdev_get_tx_queue(ndev, 0)->qdisc;
	}

	k += snprintf(buf + k, limit - k, "TOS/AC:");

	for (i = 0; i < IEEE8021P_PRIORITY_NUM; i++) {
		k += snprintf(buf + k, limit - k, " %d/%s", i, qdrv_sch_tos2ac_str(i));
	}

	k += snprintf(buf + k, limit - k,
		"\nqlen=%d\navailable_tokens=%d\nreserved_tokens_per_user=%d\nrandom_drop_threshold=%d\n",
		sch ? sch->q.qlen : 0,
		qsh->available_tokens,
		qsh->reserved_tokens_per_user,
		qsh->random_drop_threshold);

	for (i = 0; i < QDRV_SCH_BANDS; ++i) {
		const struct qdrv_sch_node_band_data *nbd = &q->bands[i];
		k += snprintf(buf + k, limit - k,
			"%d: queue_len=%d dropped=%u dropped_victim=%d sent=%u\n",
			i, skb_queue_len(&nbd->queue),
			nbd->dropped, nbd->dropped_victim, nbd->sent);
	}

	return k;
}

static ssize_t qdrv_sch_sysfs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	const struct qdrv_sch_node_data *q = container_of(attr, struct qdrv_sch_node_data, sysfs_attr);
	struct net_device *ndev = dev_get_by_name(&init_net, q->shared_data->dev_name);
	int rc;

	rc = qdrv_sch_stats_snprintf(ndev, buf, PAGE_SIZE, q);

	if (ndev)
		dev_put(ndev);

	return rc;
}

static ssize_t qdrv_sch_sysfs_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct qdrv_sch_node_data *q = container_of(attr, struct qdrv_sch_node_data, sysfs_attr);
	struct net_device *ndev = dev_get_by_name(&init_net, q->shared_data->dev_name);
	int tos = -1;
	int aid = -1;

	if (sscanf(buf, "%d %d", &tos, &aid) < 2)
		goto ready_to_return;

	if (ndev) {
		netif_stop_queue(ndev);
		qdrv_sch_set_ac_map(tos, aid);
		netif_start_queue(ndev);
	}

ready_to_return:
	if (ndev)
		dev_put(ndev);

	return count;
}

static int qdrv_sch_sysfs_init(struct Qdisc *sch)
{
	struct qdrv_sch_node_data *q = qdisc_priv(sch);
	struct net_device *dev = qdisc_dev(sch);
	DEVICE_ATTR(qdrv_sch, S_IRUGO | S_IWUGO, qdrv_sch_sysfs_show, qdrv_sch_sysfs_store);

	q->sysfs_attr = dev_attr_qdrv_sch;

	return sysfs_create_file(&dev->dev.kobj, &q->sysfs_attr.attr);
}

static void qdrv_sch_sysfs_destroy(struct Qdisc *sch)
{
	struct net_device *dev = qdisc_dev(sch);
	struct qdrv_sch_node_data *q = qdisc_priv(sch);
	sysfs_remove_file(&dev->dev.kobj, &q->sysfs_attr.attr);
}

static inline uint32_t qdrv_sch_band(struct sk_buff *skb)
{
	if (unlikely(!QTN_SKB_ENCAP_IS_80211_MGMT(skb) &&
			qdrv_sch_classify_ctrl(skb))) {
		return QDRV_BAND_CTRL;
	}

	if (unlikely(skb->priority >= QDRV_SCH_BANDS)) {
		return QDRV_BAND_AC_BK;
	}

	return skb->priority;
}

/*
 * Check if a packet should be dropped
 *
 * Returns:
 *     0  if the packet should be queued
 *     1  a packet should be dropped from any queue
 *     -1 a packet should be dropped from the current node's queues
 */
static inline int qdrv_sch_enqueue_drop_cond(const struct qdrv_sch_shared_data *sd,
		struct qdrv_sch_node_data *nd, bool is_low_rate)
{
	/* prevent a station that stops responding from hogging */
	if (sd->users > 1) {
		if (nd->used_tokens > (sd->total_tokens / 2)) {
			return -1;
		}
		if (is_low_rate && (nd->used_tokens > QDRV_TX_LOW_RATE_TOKENS_MAX)) {
			++nd->low_rate;
			return -1;
		}
	}

	if (unlikely(sd->available_tokens == 0)) {
		return 1;
	}

	if (nd->used_tokens < sd->reserved_tokens_per_user) {
		return 0;
	}

	if (unlikely(sd->available_tokens <= sd->random_drop_threshold)) {
		uint32_t drop_chance = net_random() & sd->random_drop_threshold;
		if (unlikely((drop_chance >= sd->available_tokens))) {
			return 1;
		}
	}

	return 0;
}

static inline uint8_t qdrv_sch_get_band(uint8_t i)
{
	if (g_qdrv_non_qtn_assoc) {
		return qdrv_sch_band_chg_prio[i].band_prio;
	} else {
		return qdrv_sch_band_prio[i];
	}
}

static struct sk_buff *
qdrv_sch_peek_prio(struct qdrv_sch_shared_data *sd, uint8_t prio)
{
	struct qdrv_sch_shared_band_data *sbd = &sd->bands[prio];
	struct qdrv_sch_node_band_data *nbd = TAILQ_FIRST(&sbd->active_nodes);

	if (!nbd) {
		return NULL;
	}

	return skb_peek(&nbd->queue);
}

static __sram_text struct sk_buff *qdrv_sch_peek(struct Qdisc *sch)
{
	struct qdrv_sch_node_data *nd = qdisc_priv(sch);
	struct qdrv_sch_shared_data *sd = nd->shared_data;
	struct sk_buff *skb;
	int p;

	for (p = 0; p < ARRAY_SIZE(sd->bands); p++) {
		skb = qdrv_sch_peek_prio(sd, qdrv_sch_get_band(p));
		if (skb) {
			return skb;
		}
	}

	return NULL;
}

static inline bool qdrv_sch_node_band_is_queued(const struct qdrv_sch_node_band_data *nbd)
{
	return nbd->nbd_next.tqe_prev != NULL;
}

static inline void qdrv_sch_band_active_enqueue(struct qdrv_sch_shared_band_data *sbd,
		struct qdrv_sch_node_band_data *nbd)
{
	if (!qdrv_sch_node_band_is_queued(nbd)) {
		TAILQ_INSERT_TAIL(&sbd->active_nodes, nbd, nbd_next);
	}
}

static inline void qdrv_sch_band_active_dequeue(struct qdrv_sch_shared_band_data *sbd,
		struct qdrv_sch_node_band_data *nbd)
{
	if (qdrv_sch_node_band_is_queued(nbd)) {
		TAILQ_REMOVE(&sbd->active_nodes, nbd, nbd_next);
		nbd->nbd_next.tqe_prev = NULL;
	}
}

static void __sram_text
qdrv_sch_node_reactivate(struct qdrv_sch_node_data *nd)
{
	struct qdrv_sch_shared_data *sd = nd->shared_data;
	struct qdrv_sch_node_band_data *nbd;
	struct qdrv_sch_shared_band_data *sbd;
	int prio;
	uint8_t band;

	for (prio = 0; prio < ARRAY_SIZE(sd->bands); prio++) {
		band = qdrv_sch_get_band(prio);
		nbd = &nd->bands[band];
		sbd = &sd->bands[band];
		if (skb_queue_len(&nbd->queue) > 0) {
			qdrv_sch_band_active_enqueue(sbd, nbd);
		}
	}
}

void __sram_text
qdrv_sch_complete(struct qdrv_sch_node_data *nd, struct sk_buff *skb,
			uint8_t under_thresh)
{
	struct qdrv_sch_shared_data *sd = nd->shared_data;
	unsigned long flags;

	if (unlikely(!sd)) {
		printk(KERN_ERR "%s: qdrv_sch_node_data 0x%p invalid\n",
				__FUNCTION__, nd);
		return;
	}

	qdrv_sch_shared_data_lock(sd, flags);

	if (likely(M_FLAG_ISSET(skb, M_ENQUEUED_SCH))) {
		++sd->available_tokens;
		--nd->used_tokens;
	}

	if (nd->over_thresh && under_thresh) {
		qdrv_sch_node_reactivate(nd);
		nd->over_thresh = 0;
	}

	qdrv_sch_shared_data_unlock(sd, flags);
}

static __sram_text struct sk_buff *
qdrv_sch_dequeue_node_band(struct qdrv_sch_shared_data *sd, struct qdrv_sch_node_band_data *nbd,
		uint8_t band, bool dropped_victim)
{
	struct qdrv_sch_node_data *nd = qdrv_sch_get_node_data(nbd, band);
	struct qdrv_sch_shared_band_data *sbd = &sd->bands[band];
	int empty;
	int limit_hit;
	struct sk_buff *skb;
	unsigned long flags;

	qdrv_sch_shared_data_lock(sd, flags);

	if (unlikely(skb_queue_len(&nbd->queue) == 0)) {
		qdrv_sch_shared_data_unlock(sd, flags);
		return NULL;
	}

	skb = __skb_dequeue(&nbd->queue);
	++sbd->consec_dequeues;
	if (dropped_victim) {
		++nbd->dropped_victim;
	} else {
		++nbd->sent;
	}
	--nd->qdisc->q.qlen;

	limit_hit = (sbd->consec_dequeues %
			QDRV_SCH_SHARED_AC_DATA_DEQUEUE_LIMIT) == 0;
	empty = skb_queue_len(&nbd->queue) == 0;

	/* remove or rotate this node for this AC */
	if (empty || limit_hit) {
		sbd->consec_dequeues = 0;
		qdrv_sch_band_active_dequeue(sbd, nbd);
	}

	if (!empty && !nd->over_thresh) {
		qdrv_sch_band_active_enqueue(sbd, nbd);
	}

	qdrv_sch_shared_data_unlock(sd, flags);

	return skb;
}

static __sram_text struct sk_buff *
qdrv_sch_dequeue_band(struct qdrv_sch_shared_data *sd, uint8_t band, bool dropped_victim)
{
	struct qdrv_sch_shared_band_data *sbd = &sd->bands[band];
	struct qdrv_sch_node_band_data *nbd;
	struct qdrv_sch_node_data *nd;
	unsigned long flags;

	qdrv_sch_shared_data_lock(sd, flags);

	/* Skip any node that is over threshold and remove it from the active list */
	while ((nbd = TAILQ_FIRST(&sbd->active_nodes)) != NULL) {
		nd = qdrv_sch_get_node_data(nbd, band);
		if (!nd->over_thresh) {
			break;
		}
		qdrv_sch_band_active_dequeue(sbd, nbd);
	}

	qdrv_sch_shared_data_unlock(sd, flags);

	if (!nbd) {
		return NULL;
	}

	return qdrv_sch_dequeue_node_band(sd, nbd, band, dropped_victim);
}

__sram_text struct sk_buff *qdrv_sch_dequeue_nostat(struct qdrv_sch_shared_data *sd,
							struct Qdisc *sch)
{
	struct sk_buff *skb;
	unsigned long flags;
	int prio;

	if (unlikely(sd->held_skb)) {
		qdrv_sch_shared_data_lock(sd, flags);
		/* recheck state while locked */
		if (unlikely(sd->held_skb)) {
			skb = sd->held_skb;
			--sd->held_skb_sch->q.qlen;
			sd->held_skb = NULL;
			sd->held_skb_sch = NULL;
			qdrv_sch_shared_data_unlock(sd, flags);
			return skb;
		}
		qdrv_sch_shared_data_unlock(sd, flags);
	}

	for (prio = 0; prio < ARRAY_SIZE(sd->bands); prio++) {
		skb = qdrv_sch_dequeue_band(sd, qdrv_sch_get_band(prio), 0);
		if (skb) {
			return skb;
		}
	}

	return NULL;
}

__sram_text int qdrv_sch_requeue(struct qdrv_sch_shared_data *sd, struct sk_buff *skb,
					struct Qdisc *sch)
{
	unsigned long flags;
	int rc = 0;

	qdrv_sch_shared_data_lock(sd, flags);

	if (sd->held_skb) {
		/* this should never happen */
		sd->drop_callback(sd->held_skb);
		rc = -1;
	} else {
		++sch->q.qlen;
	}

	sd->held_skb = skb;
	sd->held_skb_sch = sch;

	qdrv_sch_shared_data_unlock(sd, flags);

	return rc;
}

static __sram_text struct sk_buff *
qdrv_sch_dequeue_node(struct qdrv_sch_node_data *nd, bool dropped_victim)
{
	struct qdrv_sch_shared_data *sd = nd->shared_data;
	uint8_t prio;
	struct sk_buff *skb;

	for (prio = 0; prio < ARRAY_SIZE(sd->bands); prio++) {
		uint8_t band = qdrv_sch_get_band(prio);

		skb = qdrv_sch_dequeue_node_band(sd, &nd->bands[band], band, dropped_victim);
		if (skb) {
			return skb;
		}
	}

	return NULL;
}

int qdrv_sch_flush_node(struct qdrv_sch_node_data *nd)
{
	struct sk_buff *skb;
	struct qdrv_sch_shared_data *sd = nd->shared_data;
	int flushed = 0;

	while ((skb = qdrv_sch_dequeue_node(nd, 1)) != NULL) {
		sd->drop_callback(skb);
		++flushed;
	}

	return flushed;
}

static __sram_text struct sk_buff *qdrv_sch_dequeue(struct Qdisc* sch)
{
	struct sk_buff *skb;
	struct qdrv_sch_node_data *nd = qdisc_priv(sch);

	skb = qdrv_sch_dequeue_node(nd, 0);
	if (skb) {
		qdrv_sch_complete(nd, skb, 0);
		return skb;
	}

	return NULL;
}

/*
 * Try to drop a frame from a lower priority ac, preferring to drop
 * from the enqueuing node over others.
 *
 * Returns 0 if a victim was successfully dropped, 1 otherwise.
 */
static int qdrv_sch_enqueue_drop_victim(struct qdrv_sch_shared_data *sd,
		struct qdrv_sch_node_data *preferred_victim, uint8_t band, uint8_t any_node)
{
	struct sk_buff *victim = NULL;
	int prio;

	for (prio = ARRAY_SIZE(sd->bands) - 1; prio >= 0; prio--) {
		int victim_band = qdrv_sch_get_band(prio);

		if (victim_band == band) {
			break;
		}

		/* prefer to victimize the enqueuing node */
		victim = qdrv_sch_dequeue_node_band(sd,
				&preferred_victim->bands[victim_band], victim_band, 1);
		if (victim) {
			break;
		}

		/* otherwise drop from any node with lower priority data queued */
		if (any_node) {
			victim = qdrv_sch_dequeue_band(sd, victim_band, 1);
			if (victim) {
				break;
			}
		}
	}

	if (victim) {
		sd->drop_callback(victim);
		return 0;
	}

	return 1;
}

int __sram_text qdrv_sch_enqueue_node(struct qdrv_sch_node_data *nd, struct sk_buff *skb,
					bool is_over_quota, bool is_low_rate)
{
	struct qdrv_sch_node_band_data *nbd;
	struct qdrv_sch_shared_data *sd;
	struct qdrv_sch_shared_band_data *sbd;
	uint8_t band;
	unsigned long flags;
	int rc;

	band = qdrv_sch_band(skb);
	nbd = &nd->bands[band];
	sd = nd->shared_data;
	sbd = &sd->bands[band];

	qdrv_sch_shared_data_lock(sd, flags);

	rc = qdrv_sch_enqueue_drop_cond(sd, nd, is_low_rate);
	if (rc != 0) {
		if (qdrv_sch_enqueue_drop_victim(sd, nd, band, (rc > 0))) {
			sd->drop_callback(skb);
			++nbd->dropped;
			qdrv_sch_shared_data_unlock(sd, flags);
			return NET_XMIT_DROP;
		}
	}

	/* enqueue the new frame */
	__skb_queue_tail(&nbd->queue, skb);
	M_FLAG_SET(skb, M_ENQUEUED_SCH);
	++nd->used_tokens;
	--sd->available_tokens;
	++nd->qdisc->q.qlen;

	/* prevent dequeuing while over quota */
	if (is_over_quota) {
		if (!nd->over_thresh) {
			nd->over_thresh = 1;
			++nd->over_thresh_cnt;
		}
	} else {
		qdrv_sch_band_active_enqueue(sbd, nbd);
	}

	qdrv_sch_shared_data_unlock(sd, flags);

	return NET_XMIT_SUCCESS;
}

static __sram_text int qdrv_sch_enqueue(struct sk_buff *skb, struct Qdisc *sch)
{
	struct qdrv_sch_node_data *nd = qdisc_priv(sch);
	struct ether_header *eh = (struct ether_header *) skb->data;
	uint16_t ether_type;
	uint8_t *data_start = qdrv_sch_find_data_start(skb, eh, &ether_type);

	qdrv_sch_classify(skb, ether_type, data_start);

	return qdrv_sch_enqueue_node(nd, skb, 0, 0);
}

static int qdrv_sch_common_init(struct Qdisc *sch, enum qdrv_sch_type sch_type)
{
	int error = 0;
	struct qdrv_sch_node_data *q = qdisc_priv(sch);
	int i;

	/* Initialize private data */
	memset(q, 0, sizeof(*q));

	q->qdisc = sch;

	/* Initialize shared data */
	q->shared_data = qdrv_sch_acquire_shared_data(sch, &qdrv_sch_shared_data, sch_type);
	if (!q->shared_data) {
		printk(KERN_ERR"%s: cannot assign shared data\n", sch->ops->id);
		error = -EINVAL;
		goto error_quit;
	}

	q->used_tokens = 0;
	/* Initialize all queues */
	for (i = 0; i < QDRV_SCH_BANDS; ++i) {
		skb_queue_head_init(&q->bands[i].queue);
	}

	/* Initialize sysfs */
	error = qdrv_sch_sysfs_init(sch);
	if (error) {
		printk(KERN_ERR"%s: sysfs init failed %d\n", sch->ops->id, error);
		goto error_quit;
	}

#if QDRV_SCH_PM
	/* initialize power management */
	qdrv_sch_pm_init();
#endif

	return 0;

error_quit:
	qdrv_sch_release_shared_data(&qdrv_sch_shared_data, q->shared_data);

	printk(KERN_ERR"%s: failed to attach to %s: %d\n",
		sch->ops->id, qdisc_dev(sch)->name, error);

	return error;
}

static int qdrv_sch_normal_init(struct Qdisc *sch, struct nlattr *opt)
{
	return qdrv_sch_common_init(sch, QDRV_SCH_TYPE_NORMAL);
}

static int qdrv_sch_red_init(struct Qdisc *sch, struct nlattr *opt)
{
	return qdrv_sch_common_init(sch, QDRV_SCH_TYPE_RED);
}

static int qdrv_sch_join_init(struct Qdisc *sch, struct nlattr *opt)
{
	return qdrv_sch_common_init(sch, QDRV_SCH_TYPE_JOIN);
}

static void qdrv_sch_destroy(struct Qdisc *sch)
{
	struct qdrv_sch_node_data *q = qdisc_priv(sch);
	int i;
	unsigned long flags;
	struct sk_buff *skb;

#if QDRV_SCH_PM
	qdrv_sch_pm_exit();
#endif

	qdrv_sch_sysfs_destroy(sch);

	if (q->shared_data) {
		skb = qdrv_sch_dequeue_nostat(q->shared_data, sch);
		if (skb != NULL) {
			q->shared_data->drop_callback(skb);
		}
		qdrv_sch_shared_data_lock(q->shared_data, flags);
		q->shared_data->available_tokens += q->used_tokens;
		qdrv_sch_shared_data_unlock(q->shared_data, flags);
	}

	for (i = 0; i < QDRV_SCH_BANDS; ++i) {
		__qdisc_reset_queue(sch, &q->bands[i].queue);
	}

	qdrv_sch_release_shared_data(&qdrv_sch_shared_data, q->shared_data);
}

struct Qdisc_ops qdrv_sch_normal_qdisc_ops __read_mostly = {
	.id		=	QDRV_SCH_NAME_NORMAL,
	.priv_size	=	sizeof(struct qdrv_sch_node_data),
	.enqueue	=	qdrv_sch_enqueue,
	.dequeue	=	qdrv_sch_dequeue,
	.peek		=	qdrv_sch_peek,
	.init		=	qdrv_sch_normal_init,
	.destroy	=	qdrv_sch_destroy,
	.owner		=	THIS_MODULE,
};

struct Qdisc_ops qdrv_sch_red_qdisc_ops __read_mostly = {
	.id		=	QDRV_SCH_NAME_RED,
	.priv_size	=	sizeof(struct qdrv_sch_node_data),
	.enqueue	=	qdrv_sch_enqueue,
	.dequeue	=	qdrv_sch_dequeue,
	.peek		=	qdrv_sch_peek,
	.init		=	qdrv_sch_red_init,
	.destroy	=	qdrv_sch_destroy,
	.owner		=	THIS_MODULE,
};

struct Qdisc_ops qdrv_sch_join_qdisc_ops __read_mostly = {
	.id		=	QDRV_SCH_NAME_JOIN,
	.priv_size	=	sizeof(struct qdrv_sch_node_data),
	.enqueue	=	qdrv_sch_enqueue,
	.dequeue	=	qdrv_sch_dequeue,
	.peek		=	qdrv_sch_peek,
	.init		=	qdrv_sch_join_init,
	.destroy	=	qdrv_sch_destroy,
	.owner		=	THIS_MODULE,
};

int qdrv_sch_module_init(void)
{
	int ret;

	qdrv_sch_init_shared_data_list(&qdrv_sch_shared_data);

	ret = register_qdisc(&qdrv_sch_normal_qdisc_ops);
	if (ret) {
		goto sch_normal_fail;
	}

	ret = register_qdisc(&qdrv_sch_red_qdisc_ops);
	if (ret) {
		goto sch_red_fail;
	}

	ret = register_qdisc(&qdrv_sch_join_qdisc_ops);
	if (ret) {
		goto sch_join_fail;
	}

	return 0;

sch_join_fail:
	unregister_qdisc(&qdrv_sch_red_qdisc_ops);
sch_red_fail:
	unregister_qdisc(&qdrv_sch_normal_qdisc_ops);
sch_normal_fail:
	return ret;
}

void qdrv_sch_module_exit(void)
{
	unregister_qdisc(&qdrv_sch_join_qdisc_ops);
	unregister_qdisc(&qdrv_sch_red_qdisc_ops);
	unregister_qdisc(&qdrv_sch_normal_qdisc_ops);
	qdrv_sch_exit_shared_data_list(&qdrv_sch_shared_data);
}

