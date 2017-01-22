/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 * Copyright (c) Quantenna Communications, Inc.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/sysfs.h>
#include <net/pkt_sched.h>

#define SCHED_BURST_TRIGGER_NAME	"sched_burst_%s"

static int burst_init(struct Qdisc *sch, struct nlattr *opt);
static void burst_destroy(struct Qdisc *sch);
static int burst_enqueue(struct sk_buff *skb, struct Qdisc *sch);
static struct sk_buff* burst_dequeue(struct Qdisc *sch);
static struct sk_buff* burst_peek(struct Qdisc *sch);
static ssize_t burst_sysfs_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count);
static ssize_t burst_sysfs_show(struct device *dev, struct device_attribute *attr, char *buf);

struct burst_sched_data
{
	struct Qdisc *queue;
	struct proc_dir_entry *proc;
	unsigned burst_size;
	struct device_attribute sysfs_attr;
};

static struct Qdisc_ops burst_qdisc_ops __read_mostly = {
	.id		= "burst",
	.priv_size	= sizeof(struct burst_sched_data),
	.init		= burst_init,
	.destroy	= burst_destroy,
	.dequeue	= burst_dequeue,
	.enqueue	= burst_enqueue,
	.peek		= burst_peek,
	.owner		= THIS_MODULE,
};

static ssize_t burst_sysfs_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct burst_sched_data *data = container_of(attr, struct burst_sched_data, sysfs_attr);
	data->burst_size = 0;
	sscanf(buf, "%u", &data->burst_size);
	return count;
}

static ssize_t burst_sysfs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct burst_sched_data *data = container_of(attr, struct burst_sched_data, sysfs_attr);
	return sprintf(buf, "%u from %u\n", (unsigned)data->queue->q.qlen, (unsigned)data->burst_size);
}

static int burst_init(struct Qdisc *sch, struct nlattr *opt)
{
	struct burst_sched_data *data = qdisc_priv(sch);
	struct net_device *dev = qdisc_dev(sch);
	unsigned limit = dev->tx_queue_len ? : 1;
	DEVICE_ATTR(burst, S_IRUGO | S_IWUGO, burst_sysfs_show, burst_sysfs_store);

	data->queue = fifo_create_dflt(sch, &pfifo_qdisc_ops, limit);
	if (IS_ERR(data->queue)) {
		return PTR_ERR(data->queue);
	}

	data->sysfs_attr = dev_attr_burst;
	sysfs_create_file(&dev->dev.kobj, &data->sysfs_attr.attr);

	return 0;
}

static void burst_destroy(struct Qdisc *sch)
{
	struct net_device *dev = qdisc_dev(sch);
	struct burst_sched_data *data = qdisc_priv(sch);
	sysfs_remove_file(&dev->dev.kobj, &data->sysfs_attr.attr);
}

static int __sram_text burst_enqueue(struct sk_buff *skb, struct Qdisc *sch)
{
	struct burst_sched_data *data = qdisc_priv(sch);
	struct Qdisc *q = data->queue;
	int ret = q->ops->enqueue(skb, q);
	if (!ret) {
		sch->q.qlen++;
	}
	return ret;
}

static int __sram_text burst_delay(struct Qdisc *sch)
{
	struct burst_sched_data *data = qdisc_priv(sch);

	if (sch->q.qlen < data->burst_size) {
		return 1;
	}

	data->burst_size = 0;

	return 0;
}

static struct sk_buff* __sram_text burst_dequeue(struct Qdisc *sch)
{
	if (burst_delay(sch)) {
		return NULL;
	} else {
		struct burst_sched_data *data = qdisc_priv(sch);
		struct Qdisc *q = data->queue;
		struct sk_buff *ret = q->ops->dequeue(q);
		if (ret) {
			sch->q.qlen--;
		}
		return ret;
	}
}

static struct sk_buff* __sram_text burst_peek(struct Qdisc *sch)
{
	if (burst_delay(sch)) {
		return NULL;
	} else {
		struct burst_sched_data *data = qdisc_priv(sch);
		struct Qdisc *q = data->queue;
		return q->ops->peek(data->queue);
	}
}

static int __init burst_module_init(void)
{
	return register_qdisc(&burst_qdisc_ops);
}

static void __exit burst_module_exit(void)
{
	unregister_qdisc(&burst_qdisc_ops);
}

module_init(burst_module_init)
module_exit(burst_module_exit)

MODULE_LICENSE("GPL");

