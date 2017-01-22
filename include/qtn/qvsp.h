/*SH0
*******************************************************************************
**                                                                           **
**         Copyright (c) 2011-2013 Quantenna Communications, Inc             **
**                            All Rights Reserved                            **
**                                                                           **
*******************************************************************************
EH0*/

#ifndef _QVSP_DRV_H_
#define _QVSP_DRV_H_

#ifdef CONFIG_QVSP

#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/ip.h>

#include <net80211/if_ethersubr.h>

#include "qtn/qvsp_data.h"
#include "qtn/shared_defs.h"

#define QVSP_BA_THROT_TIMER_INTV	25	/* unit: ms */

struct qvsp_s;
struct qvsp_strm;
struct qvsp_ext_s;
struct ieee80211_node;
struct qdrv_wlan;

enum qdrv_vsp_check_type {
	QDRV_VSP_CHECK_ENABLE,
};

#define QVSP_CHECK_FUNC_PROTOTYPE(_fn)			\
	int (_fn)(struct qvsp_ext_s *qvsp,		\
			enum qvsp_if_e qvsp_if,		\
			struct sk_buff *skb,		\
			void *data_start,		\
			uint32_t pktlen,		\
			uint8_t ac)

typedef void (*_fn_qvsp_inactive_flags_changed_handler)(struct qvsp_ext_s *qvsp_ext);

/* This definition must be kept in sync with QVSP_INACTIVE_REASON */
struct qvsp_ext_s {

#define QVSP_INACTIVE_CFG	0x00000001
#define QVSP_INACTIVE_WDS	0x00000002
#define QVSP_INACTIVE_COC	0x00000004
	uint32_t				inactive_flags;
	_fn_qvsp_inactive_flags_changed_handler	flags_changed;
};

struct qvsp_wrapper {
	struct qvsp_ext_s	*qvsp;
	QVSP_CHECK_FUNC_PROTOTYPE(*qvsp_check_func);
};

static inline void
__qvsp_inactive_flag_update(struct qvsp_ext_s *qvsp_ext, uint32_t flag, int set)
{
	unsigned long irq_flags;
	unsigned long old_flags;
	unsigned long update = 0;

	if (qvsp_ext && flag) {
		local_irq_save(irq_flags);

		old_flags = qvsp_ext->inactive_flags;
		if (set) {
			qvsp_ext->inactive_flags |= flag;
		} else {
			qvsp_ext->inactive_flags &= ~flag;
		}

		local_irq_restore(irq_flags);

		update = ((old_flags == 0) != (qvsp_ext->inactive_flags == 0));
		if (update && qvsp_ext->flags_changed) {
			qvsp_ext->flags_changed(qvsp_ext);
		}
	}
}

#define qvsp_inactive_flag_set(_qvsp, _flag) \
		__qvsp_inactive_flag_update((struct qvsp_ext_s *)(_qvsp), (_flag), 1)

#define qvsp_inactive_flag_clear(_qvsp, _flag) \
		__qvsp_inactive_flag_update((struct qvsp_ext_s *)(_qvsp), (_flag), 0)

static __always_inline int
__qvsp_is_active(struct qvsp_ext_s *qvsp_ext)
{
	return (qvsp_ext && (qvsp_ext->inactive_flags == 0));
}
#define qvsp_is_active(_qvsp)  __qvsp_is_active((struct qvsp_ext_s *)(_qvsp))

static __always_inline int
__qvsp_is_enabled(struct qvsp_ext_s *qvsp_ext)
{
	return (qvsp_ext && ((qvsp_ext->inactive_flags & QVSP_INACTIVE_CFG) == 0));
}
#define qvsp_is_enabled(_qvsp) \
		__qvsp_is_enabled((struct qvsp_ext_s *)(_qvsp))



int qvsp_strm_check_add(struct qvsp_s *qvsp, enum qvsp_if_e qvsp_if, struct ieee80211_node *ni,
			struct sk_buff *skb, struct ether_header *eh, struct iphdr *iphdr_p,
			int pktlen, uint8_t ac, int32_t tid);
void qvsp_cmd_strm_state_set(struct qvsp_s *qvsp, uint8_t strm_state,
			const struct ieee80211_qvsp_strm_id *strm_id, struct ieee80211_qvsp_strm_dis_attr *attr);
void qvsp_cmd_vsp_configure(struct qvsp_s *qvsp, uint32_t index, uint32_t value);
void qvsp_fat_set(struct qvsp_s *qvsp, uint32_t fat, uint32_t intf_ms, uint8_t chan);
void qvsp_node_del(struct qvsp_s *qvsp, struct ieee80211_node *ni);
void qvsp_reset(struct qvsp_s *qvsp);
void qvsp_change_stamode(struct qvsp_s *qvsp, uint8_t stamode);
int qvsp_netdbg_init(struct qvsp_s *qvsp,
		void (*cb_logger)(void *token, void *vsp_data, uint32_t size),
		uint32_t interval);
void qvsp_netdbg_exit(struct qvsp_s *qvsp);
void qvsp_disable(struct qvsp_s *qvsp);
struct qvsp_s *qvsp_init(int (*ioctl_fn)(void *token, uint32_t param, uint32_t value),
			void *ioctl_fn_token, struct net_device *dev, uint8_t stamode,
			void (*cb_cfg)(void *token, uint32_t index, uint32_t value),
			void (*cb_strm_ctrl)(void *token, struct ieee80211_node *ni, uint8_t strm_state,
				struct ieee80211_qvsp_strm_id *strm_id, struct ieee80211_qvsp_strm_dis_attr *attr),
			void (*cb_strm_ext_throttler)(void *token, struct ieee80211_node *node,
				uint8_t strm_state, const struct ieee80211_qvsp_strm_id *strm_id,
				struct ieee80211_qvsp_strm_dis_attr *attr, uint32_t throt_intvl),
			uint32_t ieee80211node_size, uint32_t ieee80211vap_size
			);
void qvsp_exit(struct qvsp_s **qvsp, struct net_device *dev);

void qvsp_wrapper_init(struct qvsp_ext_s *qvsp_ext, QVSP_CHECK_FUNC_PROTOTYPE(fn));
void qvsp_wrapper_exit(void);
void qvsp_node_init(struct ieee80211_node *ni);

void qvsp_3rdpt_register_cb(struct qvsp_s *qvsp,
		void *wme_token,
		int (*cb_3rdpt_get_method)(struct ieee80211_node *ni, uint8_t *throt_session_dur, uint8_t *throt_winsize),
		int (*cb_ba_throt)(struct ieee80211_node *ni, int32_t tid, int intv, int dur, int win_size),
		int (*cb_wme_throt)(void *qw, uint32_t ac, uint32_t enable,
					uint32_t aifs, uint32_t cwmin, uint32_t cwmax, uint32_t txoplimit,
					uint32_t add_qwme_ie)
		);

#if TOPAZ_QTM
void qvsp_strm_tid_check_add(struct qvsp_s *qvsp, struct ieee80211_node *ni, uint8_t node, uint8_t tid,
		uint32_t pkts, uint32_t bytes, uint32_t sent_pkts, uint32_t sent_bytes);
#endif
#endif /* CONFIG_QVSP */

#endif
