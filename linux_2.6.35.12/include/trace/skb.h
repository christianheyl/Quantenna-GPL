#ifndef _TRACE_SKB_H_
#define _TRACE_SKB_H_

#include <linux/skbuff.h>
#include <linux/tracepoint.h>

DECLARE_TRACE(kfree_skb,
	TP_PROTO(struct sk_buff *skb, void *location),
	TP_ARGS(skb, location));

DECLARE_TRACE(skb_perf_start,
	TP_PROTO(struct sk_buff *skb, int path),
	TP_ARGS(skb, path));

DECLARE_TRACE(skb_perf_stamp,
	TP_PROTO(struct sk_buff *skb, void *location),
	TP_ARGS(skb, location));

DECLARE_TRACE(skb_perf_finish,
	TP_PROTO(struct sk_buff *skb),
	TP_ARGS(skb));

DECLARE_TRACE(skb_perf_copy,
	TP_PROTO(struct sk_buff *new, struct sk_buff *old),
	TP_ARGS(new, old));

#ifdef CONFIG_TRACEPOINTS
	#define trace_skb_perf_stamp_call(skb) do {__label__ addr; addr: trace_skb_perf_stamp(skb, &&addr);} while(0)
#else
	#define trace_skb_perf_stamp_call(skb) trace_skb_perf_stamp(skb, NULL)
#endif

#endif
