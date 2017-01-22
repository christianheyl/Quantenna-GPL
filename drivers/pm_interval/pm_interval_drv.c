/**
 * Copyright (c) 2011 - 2012 Quantenna Communications Inc
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 **/

/*
 * These entry points are accessed from outside this module:
 *
 *     pm_interval_report
 *     pm_interval_configure
 *     pm_interval_schedule_work
 *     pm_interval_monitor
 *     pm_proc_start_interval_report
 *
 *     pm_interval_init
 *     pm_interval_exit
 */

#include <linux/version.h>
#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/netdevice.h>

#include <net/net_namespace.h>
#include <net80211/ieee80211.h>
#include <net80211/if_media.h>
#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_proto.h>

#include "common/queue.h"

#define PM_INTERVAL_NAME		"pm_interval"
#define PM_INTERVAL_VERSION		"1.2"
#define PM_INTERVAL_AUTHOR		"Quantenna Communciations, Inc."
#define PM_INTERVAL_DESC		"Manage PM Intervals"

#define PM_INTERVAL_PROC_ENTRY_PREFIX	"start_"

#define PM_INTERVAL_MAX_LENGTH_COMMAND	7
#define PM_INTERVAL_MAX_LENGTH_ARG	11

#define PM_INTERVAL_COMMAND_ADD		"add"
#define PM_INTERVAL_COMMAND_DUMP	"dump"

#define PM_INTERVAL_INITIAL_RESULT	(-2)
#define PM_INTERVAL_ERROR_RESULT	(-1)
#define PM_INTERVAL_SUCCESS_RESULT	0

struct pm_interval_nd_entry {
	struct net_device_stats	pd_start_interval;
	const struct net_device	*pd_dev;
	TAILQ_ENTRY(pm_interval_nd_entry)	pd_next;
};

struct pm_interval_entry {
	unsigned long		pe_time_started;
	unsigned long		pe_time_elapsed;
	unsigned long		pe_time_length;
	char			pe_name_interval[PM_INTERVAL_MAX_LENGTH_ARG + 1];
	struct pm_interval_data	*pe_backptr;
	TAILQ_HEAD(, pm_interval_nd_entry) pe_devices;
	TAILQ_ENTRY(pm_interval_entry) pe_next;
};

struct pm_interval_data {
	struct timer_list	pm_timer;
	struct work_struct	pm_monitor_wq;
	struct proc_dir_entry	*pm_interval_dir;
	spinlock_t		pm_spinlock;
	TAILQ_HEAD(, pm_interval_entry)		pm_intervals;
	int			rc;
};

static struct device pm_interval_device =
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)
	.bus_id		= PM_INTERVAL_NAME,
#endif
};

static int pm_interval_parse_an_arg(const char *buf, char *an_arg, size_t size_limit)
{
	int	iter;
	int	found_end_of_string = 0;
	int	count_wspace = 0;
	char	cur_char = *buf;

	while (isspace(cur_char)) {
		cur_char = *(++buf);
		count_wspace++;
	}

	for (iter = 0; iter < size_limit; iter++) {
		cur_char = *buf;

		if (!isspace(cur_char) && cur_char != '\0') {
			*(an_arg++) = cur_char;
			buf++;
		} else {
			found_end_of_string = 1;
			*an_arg = '\0';
			break;
		}
	}

	if (found_end_of_string == 0) {
		return PM_INTERVAL_ERROR_RESULT;
	}

	return count_wspace + iter;
}

/*
 * directive is expected to address at least PM_INTERVAL_MAX_LENGTH_COMMAND + 1 chars
 */
static int pm_interval_parse_command(const char *buf, char *directive)
{
	int		ival = pm_interval_parse_an_arg(buf, directive, PM_INTERVAL_MAX_LENGTH_COMMAND);

	if (ival < 0) {
		printk(KERN_ERR "%s: failed to parse the command\n", PM_INTERVAL_NAME);
		return -1;
	}

	return ival;
}

/*
 * args (their addresses are in argv) are expected to address
 * at least PM_INTERVAL_MAX_LENGTH_ARG + 1 chars
 */

static int pm_interval_parse_args(const char *buf, char **argv, const unsigned int max_argc)
{
	unsigned int	arg_index;
	int		chars_parsed = 0;
	int		args_parsed = 0;

	if (max_argc < 1) {
		return 0;
	}

	for (arg_index = 0;arg_index < max_argc; arg_index++) {
		char	tmp_char = *buf;

		if (tmp_char == '\0' || tmp_char == '\n') {
			return args_parsed;
		}

		chars_parsed = pm_interval_parse_an_arg(buf, argv[args_parsed], PM_INTERVAL_MAX_LENGTH_ARG);
		if (chars_parsed < 0) {
			return args_parsed;
		}

		buf += chars_parsed;
		args_parsed++;
	}

	return args_parsed;
}

static ssize_t pm_interval_report_result(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct pm_interval_data	*p_data = (struct pm_interval_data *) dev_get_drvdata(dev);

	if (p_data->rc == PM_INTERVAL_SUCCESS_RESULT) {
		strcpy(buf, "ok\n");
	} else {
		sprintf(buf, "error %d\n", p_data->rc);
	}

	return strlen(buf);
}

static long pm_get_uptime(void)
{
	struct sysinfo si;

	do_sysinfo(&si);

	return si.uptime;
}

static int pm_get_start_next_interval(int interval_length)
{
	if (interval_length < 1) {
		return -1;
	}

	return interval_length - (pm_get_uptime() % interval_length);
}

static void pm_interval_schedule_work(unsigned long data)
{
	struct pm_interval_data		*p_data = (struct pm_interval_data *) data;
	struct pm_interval_entry	*p_entry = NULL;
	int				base_interval_length = 0;

	spin_lock_bh(&p_data->pm_spinlock);

	if (TAILQ_EMPTY(&p_data->pm_intervals)) {
		goto ready_to_return;
	}

	p_entry = TAILQ_FIRST(&p_data->pm_intervals);
	base_interval_length = p_entry->pe_time_length;
	if (base_interval_length < 1) {
		printk(KERN_ERR "%s: Invalid base interval length of %d\n", PM_INTERVAL_NAME, base_interval_length);
		goto ready_to_return;
	}

	schedule_work(&p_data->pm_monitor_wq);

	mod_timer(&p_data->pm_timer, jiffies + pm_get_start_next_interval(base_interval_length) * HZ);

ready_to_return:
	spin_unlock_bh(&p_data->pm_spinlock);
}

static int pm_interval_proc_start_interval_report(char *buf,
						  char **start,
						  off_t offset,
						  int count,
						  int *eof,
						  void *data)
{
	int				len = 0;
	struct pm_interval_entry	*p_entry = (struct pm_interval_entry *) data;
	struct pm_interval_data		*p_data = p_entry->pe_backptr;
	struct pm_interval_nd_entry	*p_nd_entry = NULL;
	unsigned long elapsed_time;

	elapsed_time = pm_get_uptime();

	spin_lock_bh(&p_data->pm_spinlock);

	if (p_entry->pe_time_started <= elapsed_time) {
		elapsed_time = elapsed_time - p_entry->pe_time_started;
	} else {
		elapsed_time = 0;
	}

	/*
	 * Adopted from dev_seq_show, linux/net/core/dev.c
	 * Lets the API to get a cumulative counter share source code with the API to get a PM counter.
	 */
	len += sprintf(buf + len, "Inter-|   Receive                            "
				  "                    |  Transmit\n"
				  " face |bytes    packets errs drop fifo frame "
				  "compressed multicast|bytes    packets errs "
				  "drop fifo colls carrier compressed\n");


	TAILQ_FOREACH(p_nd_entry, &p_entry->pe_devices, pd_next) {
		const struct net_device_stats *stats = &p_nd_entry->pd_start_interval;

		/*
		 * Adopted from dev_seq_printf_stats, linux/net/core/dev.c
		 * Lets the API to get a cumulative counter share source code with the API
		 * to get a PM counter.
		 */
		len += sprintf(buf + len, "%6s:%8lu %7lu %4lu %4lu %4lu %5lu %10lu %9lu "
			   "%8lu %7lu %4lu %4lu %4lu %5lu %7lu %10lu\n",
			   p_nd_entry->pd_dev->name, stats->rx_bytes, stats->rx_packets,
			   stats->rx_errors,
			   stats->rx_dropped + stats->rx_missed_errors,
			   stats->rx_fifo_errors,
			   stats->rx_length_errors + stats->rx_over_errors +
				stats->rx_crc_errors + stats->rx_frame_errors,
			   stats->rx_compressed, stats->multicast,
			   stats->tx_bytes, stats->tx_packets,
			   stats->tx_errors, stats->tx_dropped,
			   stats->tx_fifo_errors, stats->collisions,
			   stats->tx_carrier_errors +
				stats->tx_aborted_errors +
				stats->tx_window_errors +
				stats->tx_heartbeat_errors,
			   stats->tx_compressed);

	}

	spin_unlock_bh(&p_data->pm_spinlock);

	len += sprintf(buf + len, "\n%u seconds since the interval started\n", (unsigned int) elapsed_time);

	*eof = 1;

	return len;
}

static int pm_interval_add_interval(struct pm_interval_data *p_data,
				    const char *new_interval_name,
				    const char *interval_length_str)
{
	int				new_interval_length = 0;
	int				arm_pm_interval_timer = 0;
	int				next_interval_start_time = 0;
	int				ival = sscanf(interval_length_str, "%d", &new_interval_length);
	long				time_in_seconds = pm_get_uptime();
	char				proc_entry_name[PM_INTERVAL_MAX_LENGTH_ARG + 7];
	struct pm_interval_entry	*p_entry = NULL;

	if (ival != 1) {
		printk(KERN_ERR "%s: cannot parse the length of time for the new interval from %s\n",
				 PM_INTERVAL_NAME, interval_length_str);
		return PM_INTERVAL_ERROR_RESULT;
	} else if (new_interval_length < 1) {
		printk(KERN_ERR "%s: invalid length of %d sec for the new interval\n",
				PM_INTERVAL_NAME, new_interval_length);
		return PM_INTERVAL_ERROR_RESULT;
	}

	if (strnlen(new_interval_name, PM_INTERVAL_MAX_LENGTH_ARG + 1) > PM_INTERVAL_MAX_LENGTH_ARG) {
		printk(KERN_ERR "%s: name of interval is too long in add interval\n", PM_INTERVAL_NAME);
		return PM_INTERVAL_ERROR_RESULT;
	}

	spin_lock_bh(&p_data->pm_spinlock);

	if (!TAILQ_EMPTY(&p_data->pm_intervals)) {
		int				base_interval_length = 0;
		struct pm_interval_entry	*p_entry = TAILQ_FIRST(&p_data->pm_intervals);

		base_interval_length = p_entry->pe_time_length;
		if (new_interval_length % base_interval_length != 0) {
			printk(KERN_ERR "%s: invalid length of %d sec for the new interval\n",
					PM_INTERVAL_NAME, new_interval_length);
			goto configuration_error;
		}

		TAILQ_FOREACH(p_entry, &p_data->pm_intervals, pe_next) {
			if (p_entry->pe_time_length == new_interval_length) {
				printk(KERN_ERR
					"%s: interval with length of %d already configured\n",
					 PM_INTERVAL_NAME, new_interval_length);
				goto configuration_error;
			} else if (strncmp(new_interval_name,
				   p_entry->pe_name_interval,
				   PM_INTERVAL_MAX_LENGTH_ARG) == 0) {
				printk(KERN_ERR
					"%s: interval with name of %s already configured\n",
					 PM_INTERVAL_NAME, new_interval_name);
				goto configuration_error;
			}
		}
	} else {
		arm_pm_interval_timer = 1;
	}

	spin_unlock_bh(&p_data->pm_spinlock);

	if ((p_entry = kzalloc(sizeof(*p_entry), GFP_KERNEL)) == NULL) {
		printk(KERN_ERR "%s: cannot allocate entry for interval of length %d sec\n",
				PM_INTERVAL_NAME, new_interval_length);
		return PM_INTERVAL_ERROR_RESULT;
	}

	TAILQ_INIT(&p_entry->pe_devices);
	p_entry->pe_time_length = new_interval_length;
	p_entry->pe_time_elapsed = time_in_seconds % new_interval_length;
	p_entry->pe_time_started = time_in_seconds - p_entry->pe_time_elapsed;
	p_entry->pe_backptr = p_data;
	strncpy(p_entry->pe_name_interval, new_interval_name, sizeof(p_entry->pe_name_interval) - 1);

	if (arm_pm_interval_timer) {
		next_interval_start_time = pm_get_start_next_interval(new_interval_length);

		if (next_interval_start_time < 1) {
			goto problem_with_length_interval;
		}
	}

	strcpy(&proc_entry_name[0], PM_INTERVAL_PROC_ENTRY_PREFIX);
	strcat(&proc_entry_name[strlen(PM_INTERVAL_PROC_ENTRY_PREFIX)], p_entry->pe_name_interval);

	if (create_proc_read_entry(&proc_entry_name[0],
				    0,
				    p_data->pm_interval_dir,
				    pm_interval_proc_start_interval_report,
				    p_entry) == NULL) {
		goto cant_create_proc_entry;
	}

	if (arm_pm_interval_timer) {
		p_data->pm_timer.function = pm_interval_schedule_work;
		p_data->pm_timer.data = (unsigned long) p_data;
		p_data->pm_timer.expires = jiffies + next_interval_start_time * HZ;
		add_timer(&p_data->pm_timer);
	}

	spin_lock_bh(&p_data->pm_spinlock);

	TAILQ_INSERT_TAIL(&p_data->pm_intervals, p_entry, pe_next);

	spin_unlock_bh(&p_data->pm_spinlock);

	return PM_INTERVAL_SUCCESS_RESULT;

problem_with_length_interval:
cant_create_proc_entry:
	kfree(p_entry);
configuration_error:
	spin_unlock_bh(&p_data->pm_spinlock);

	return PM_INTERVAL_ERROR_RESULT;
}

static int pm_interval_dump(struct pm_interval_data *p_data)
{
	struct pm_interval_entry	*p_entry = NULL;

	printk(KERN_ERR "%s: begin dump\n", PM_INTERVAL_NAME);
	spin_lock_bh(&p_data->pm_spinlock);

	TAILQ_FOREACH(p_entry, &p_data->pm_intervals, pe_next) {
		struct pm_interval_nd_entry	*p_nd_entry = NULL;

		printk(KERN_ERR "%s: interval %s of length %d started at %d, elapsed %d\n",
				 PM_INTERVAL_NAME,
				 p_entry->pe_name_interval,
				 (int) p_entry->pe_time_length,
				 (int) p_entry->pe_time_started,
				 (int) p_entry->pe_time_elapsed);

		TAILQ_FOREACH(p_nd_entry, &p_entry->pe_devices, pd_next) {
			const struct net_device	*dev = p_nd_entry->pd_dev;

			if (dev != NULL) {
				printk(KERN_ERR "    net device %s\n", dev->name);
			} else {
				printk(KERN_ERR "    net device (NULL)\n");
			}

		}
	}

	spin_unlock_bh(&p_data->pm_spinlock);
	printk(KERN_ERR "%s: dump completes\n", PM_INTERVAL_NAME);

	return PM_INTERVAL_SUCCESS_RESULT;
}

static ssize_t pm_interval_configure(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	char			command[PM_INTERVAL_MAX_LENGTH_COMMAND + 1] = {'\0'};
	int			chars_parsed = pm_interval_parse_command(buf, &command[0]);
	int			rc_val = PM_INTERVAL_SUCCESS_RESULT;
	struct pm_interval_data	*p_data = (struct pm_interval_data *) dev_get_drvdata(dev);

	if (chars_parsed < 0) {
		p_data->rc = PM_INTERVAL_ERROR_RESULT;
		/*
		 * It's best to return the length of the message, even if an error occurs.
		 * Otherwise the Linux kernel might keeps calling this entry point until
		 * it is convinced the complete message has been parsed, based on
		 * the return value(s) from this routine adding up to count.
		 *
		 * Application reads from the entry in the sys FS to get the status of
		 * the operation; that is the purpose of pm_interval_report_result.
		 */
		return count;
	}

	if (strcmp(&command[0], PM_INTERVAL_COMMAND_ADD) == 0) {
		char	name_of_interval[PM_INTERVAL_MAX_LENGTH_ARG + 1] = {'\0'};
		char	interval_length_str[PM_INTERVAL_MAX_LENGTH_ARG + 1] = {'\0'};
		char	*local_argv[] = {&name_of_interval[0], &interval_length_str[0]};
		int	args_parsed = pm_interval_parse_args(buf + chars_parsed,
							    &local_argv[0],
							     ARRAY_SIZE(local_argv));
		if (args_parsed == ARRAY_SIZE(local_argv)) {
			rc_val = pm_interval_add_interval(p_data, &name_of_interval[0], &interval_length_str[0]);
		} else {
			printk(KERN_ERR "%s, insufficent params (%d) for %s command\n",
					 PM_INTERVAL_NAME, args_parsed, PM_INTERVAL_COMMAND_ADD);
			rc_val = PM_INTERVAL_ERROR_RESULT;
		}
	} else if (strcmp(&command[0], PM_INTERVAL_COMMAND_DUMP) == 0) {
		rc_val = pm_interval_dump(p_data);
	} else {
		printk(KERN_ERR "%s, unrecognized command %s\n", PM_INTERVAL_NAME, &command[0]);
		rc_val = PM_INTERVAL_ERROR_RESULT;
	}

	p_data->rc = rc_val;

	return count;
}

static DEVICE_ATTR(configure, 0644,
	pm_interval_report_result, pm_interval_configure);

static struct pm_interval_data	*p_private = NULL;

static struct pm_interval_nd_entry *pm_interval_get_addr_entry(struct pm_interval_entry *p_entry,
							    struct net_device *dev)
{
	struct pm_interval_nd_entry	*p_nd_entry = NULL;

	TAILQ_FOREACH(p_nd_entry, &p_entry->pe_devices, pd_next) {
		if (dev == p_nd_entry->pd_dev) {
			return p_nd_entry;
		}
	}

	return NULL;
}

static int pm_interval_update_net_device_table(struct pm_interval_entry *p_entry, struct pm_interval_data *p_data)
{
	/* This routine assumes the PM Interval Spin Lock has been taken */
	struct net *net;
	struct net_device *dev;
	int retval = 0;

	read_lock(&dev_base_lock);

	for_each_net(net) {
		for_each_netdev(net, dev) {
			const struct net_device_stats *p_current_counters = dev_get_stats(dev);
			struct pm_interval_nd_entry	*p_nd_entry =
				pm_interval_get_addr_entry(p_entry, dev);

			if (p_nd_entry == NULL) {
				spin_unlock_bh(&p_data->pm_spinlock);

				p_nd_entry = kzalloc(sizeof(*p_nd_entry), GFP_KERNEL);

				spin_lock_bh(&p_data->pm_spinlock);

				if (p_nd_entry == NULL) {
					retval = -1;
					goto cant_allocate_nd_entry;
				}

				p_nd_entry->pd_dev = dev;
				TAILQ_INSERT_TAIL(&p_entry->pe_devices, p_nd_entry, pd_next);
			}

			if (p_current_counters != NULL) {
				memcpy(&p_nd_entry->pd_start_interval,
					p_current_counters,
					sizeof(*p_current_counters));
			}
		}
	}

cant_allocate_nd_entry:
	read_unlock(&dev_base_lock);

	return retval;
}

static void pm_interval_monitor(struct work_struct *work)
{
	struct pm_interval_data		*p_data = container_of(work, struct pm_interval_data, pm_monitor_wq);
	long				time_in_seconds = pm_get_uptime();
	struct pm_interval_entry	*p_entry = NULL;
	int				interval_index = 0;

	spin_lock_bh(&p_data->pm_spinlock);

	TAILQ_FOREACH(p_entry, &p_data->pm_intervals, pe_next) {
		int	since_interval_start = 0;

		since_interval_start = time_in_seconds % p_entry->pe_time_length;

		if (since_interval_start < p_entry->pe_time_elapsed || interval_index == 0) {
			if (pm_interval_update_net_device_table(p_entry, p_data) != 0) {
				goto ready_to_return;
			}

			p_entry->pe_time_started = time_in_seconds;
		}

		p_entry->pe_time_elapsed = since_interval_start;
		interval_index++;
	}

ready_to_return:
	spin_unlock_bh(&p_data->pm_spinlock);
}

/*
 * Keep Linux from complaining about no release method at module unload time ...
 */
static void pm_interval_release(struct device *dev)
{
}

static int __init pm_interval_init(void)
{
	if ((p_private = kzalloc(sizeof(*p_private), GFP_KERNEL)) == NULL) {
		printk(KERN_ERR "%s: cannot allocate private data\n", PM_INTERVAL_NAME);
		goto cant_alloc_private;
	}

	pm_interval_device.release = pm_interval_release;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
	dev_set_name(&pm_interval_device, PM_INTERVAL_NAME);
#endif
	dev_set_drvdata(&pm_interval_device, p_private);

	if (device_register(&pm_interval_device) != 0) {
		printk(KERN_ERR "%s: failed to register the device\n", PM_INTERVAL_NAME);
		goto cant_register_pm_interval;
	}

	p_private->rc = PM_INTERVAL_INITIAL_RESULT;

	TAILQ_INIT(&p_private->pm_intervals);
	spin_lock_init(&p_private->pm_spinlock);
	INIT_WORK(&p_private->pm_monitor_wq, pm_interval_monitor);

	if (device_create_file(&pm_interval_device, &dev_attr_configure) != 0) {
		printk(KERN_ERR "%s: failed to create configure sysfs file for \"%s\"\n",
				PM_INTERVAL_NAME, PM_INTERVAL_NAME);
		goto configure_sysfs_fail;
	}

	if ((p_private->pm_interval_dir = proc_mkdir(PM_INTERVAL_NAME, NULL)) == NULL) {
		printk(KERN_ERR "PMI: cannot create /proc/" PM_INTERVAL_NAME " folder\n");
		goto cant_create_proc;
	}

	init_timer(&p_private->pm_timer);

	return 0;

cant_create_proc:
	device_remove_file(&pm_interval_device, &dev_attr_configure);
configure_sysfs_fail:
	device_unregister(&pm_interval_device);
cant_register_pm_interval:
	kfree(p_private);
cant_alloc_private:
	p_private = NULL;

	return -ENOMEM;
}

static void pm_interval_entry_cleanup(struct pm_interval_entry *p_entry, struct pm_interval_data *p_data)
{
	char	proc_entry_name[PM_INTERVAL_MAX_LENGTH_ARG + 7];

	while (!TAILQ_EMPTY(&p_entry->pe_devices)) {
		struct pm_interval_nd_entry *p_nd_entry = TAILQ_FIRST(&p_entry->pe_devices);

		TAILQ_REMOVE(&p_entry->pe_devices, p_nd_entry, pd_next);
		kfree(p_nd_entry);
	}

	strcpy(&proc_entry_name[0], PM_INTERVAL_PROC_ENTRY_PREFIX);
	strcat(&proc_entry_name[strlen(PM_INTERVAL_PROC_ENTRY_PREFIX)], p_entry->pe_name_interval);
	remove_proc_entry(&proc_entry_name[0], p_data->pm_interval_dir);
}

static void __exit pm_interval_exit(void)
{
	printk(KERN_WARNING "%s: unload kernel module\n", PM_INTERVAL_NAME);

	if (p_private == NULL) {
		return;
	}

	del_timer(&p_private->pm_timer);

	flush_work(&p_private->pm_monitor_wq);

	spin_lock_bh(&p_private->pm_spinlock);

	while (!TAILQ_EMPTY(&p_private->pm_intervals)) {
		struct pm_interval_entry *p_entry = TAILQ_FIRST(&p_private->pm_intervals);

		pm_interval_entry_cleanup(p_entry, p_private);

		TAILQ_REMOVE(&p_private->pm_intervals, p_entry, pe_next);
		kfree(p_entry);
	}

	remove_proc_entry(PM_INTERVAL_NAME, NULL);

	device_remove_file(&pm_interval_device, &dev_attr_configure);
	device_unregister(&pm_interval_device);

	spin_unlock_bh(&p_private->pm_spinlock);

	kfree(p_private);

	p_private = NULL;
}

/******************************************************************************
	Linux driver entries/declarations
******************************************************************************/
module_init(pm_interval_init);
module_exit(pm_interval_exit);
MODULE_LICENSE("GPL");
