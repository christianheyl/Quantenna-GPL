/*
 * Copyright (c) Quantenna Communications Incorporated 2012.
 *
 * ########################################################################
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * ########################################################################
 *
 *
 */

#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/pm_qos_params.h>

#include <asm/uaccess.h>
#include <asm/board/pm.h>

#define STR_HELPER(x)			#x
#define STR(x)				STR_HELPER(x)

#define PM_PROC_FILE_NAME		"soc_pm"
#define PM_EMAC_PROC_FILE_NAME		"emac_pm"

#define PM_PROC_ADD_CMD			"add"
#define PM_PROC_UPDATE_CMD		"update"
#define PM_PROC_REMOVE_CMD		"remove"

#define PM_MAX_NAME_LEN			64

#define PM_PROC_PARSE_ADD_CMD		PM_PROC_ADD_CMD" %"STR(PM_MAX_NAME_LEN)"s %d"		/* syntax: "add who_ask NNN" */
#define PM_PROC_PARSE_UPDATE_CMD	PM_PROC_UPDATE_CMD" %"STR(PM_MAX_NAME_LEN)"s %d"	/* syntax: "update who_ask NNN" */
#define PM_PROC_PARSE_REMOVE_CMD	PM_PROC_REMOVE_CMD" %"STR(PM_MAX_NAME_LEN)"s"		/* syntax: "remove who_ask" */

static struct workqueue_struct *pm_wq;
static DEFINE_SPINLOCK(pm_wq_lock);
static struct pm_qos_request_list *pm_emac_req;

static int pm_write_proc(struct file *file, const char __user *buffer,
				unsigned long count, void *data)
{
	char cmd[PM_MAX_NAME_LEN + 32];
	char name[PM_MAX_NAME_LEN + 1];
	int val;
	int ret = 0;

	if (!count) {
		return -EINVAL;
	} else if (count > sizeof(cmd) - 1) {
		return -EINVAL;
	} else if (copy_from_user(cmd, buffer, count)) {
		return -EFAULT;
	}
	cmd[count] = '\0';

	switch ((int)data) {
	case PM_QOS_POWER_SAVE:
		if (sscanf(cmd, PM_PROC_PARSE_ADD_CMD, name, &val) == 2) {
			ret = pm_qos_add_requirement(PM_QOS_POWER_SAVE, name, val);
		} else if (sscanf(cmd, PM_PROC_PARSE_UPDATE_CMD, name, &val) == 2) {
			ret = pm_qos_update_requirement(PM_QOS_POWER_SAVE, name, val);
		} else if (sscanf(cmd, PM_PROC_PARSE_REMOVE_CMD, name) == 1) {
			pm_qos_remove_requirement(PM_QOS_POWER_SAVE, name);
		} else {
			ret = -EINVAL;
		}
		break;
	case PM_QOS_POWER_EMAC:
		if (sscanf(cmd, "%d", &val) == 1) {
			pm_qos_update_request(pm_emac_req, val);
		} else {
			ret = -EINVAL;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret ? ret : count;
}

static int pm_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	return sprintf(page, "%d", pm_qos_requirement((int)data));
}

static int __init pm_create_proc(void)
{
	struct proc_dir_entry *entry;
	struct proc_dir_entry *emac_proc_entry;
	/*
	 * Proc interface to change power save levels.
	 * Main purpose is debugging.
	 * Other kernel modules can directly use pm_qos_*() functions.
	 * User-space application (safe way) can open misc character device
	 * (major number 10, minor see by "cat /proc/misc")
	 * provided by PM QoS kernel module and control through it.
	 * It is safe because 'name' is chosen based on PID,
	 * and when application quit all its requests are removed.
	 */

	entry = create_proc_entry(PM_PROC_FILE_NAME, 0600, NULL);
	if (!entry) {
		return -ENOMEM;
	}
	entry->write_proc = pm_write_proc;
	entry->read_proc = pm_read_proc;
	entry->data = (void *)PM_QOS_POWER_SAVE;

	emac_proc_entry = create_proc_entry(PM_EMAC_PROC_FILE_NAME, 0600, NULL);
	if (!emac_proc_entry) {
		return -ENOMEM;
	}
	emac_proc_entry->write_proc = pm_write_proc;
	emac_proc_entry->read_proc = pm_read_proc;
	emac_proc_entry->data = (void *)PM_QOS_POWER_EMAC;

	return 0;
}

static void __init pm_create_wq(void)
{
	pm_wq = create_rt_workqueue("ruby_pm");
}

static int __init pm_init(void)
{
	pm_qos_add_requirement(PM_QOS_POWER_SAVE, BOARD_PM_GOVERNOR_QCSAPI, BOARD_PM_LEVEL_INIT);
	pm_emac_req = pm_qos_add_request(PM_QOS_POWER_EMAC, BOARD_PM_LEVEL_NO);
	if (!pm_emac_req)
		return -ENOMEM;

	pm_create_wq();
	return pm_create_proc();
}
arch_initcall(pm_init);

static int __pm_cancel_work(struct delayed_work *dwork)
{
	int ret = 0;

	if (delayed_work_pending(dwork)) {
		cancel_delayed_work(dwork);
		ret = 1;
	} else if (work_pending(&dwork->work)) {
		cancel_work_sync(&dwork->work);
		ret = 1;
	}

	return ret;
}

int pm_queue_work(struct delayed_work *dwork, unsigned long delay)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&pm_wq_lock, flags);

	ret = __pm_cancel_work(dwork);
	queue_delayed_work(pm_wq, dwork, delay);

	spin_unlock_irqrestore(&pm_wq_lock, flags);

	return ret;
}
EXPORT_SYMBOL(pm_queue_work);

int pm_cancel_work(struct delayed_work *dwork)
{
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&pm_wq_lock, flags);

	ret = __pm_cancel_work(dwork);

	spin_unlock_irqrestore(&pm_wq_lock, flags);

	return ret;
}
EXPORT_SYMBOL(pm_cancel_work);

int pm_flush_work(struct delayed_work *dwork)
{
	might_sleep();
	return cancel_delayed_work_sync(dwork);
}
EXPORT_SYMBOL(pm_flush_work);

MODULE_AUTHOR("Quantenna");
MODULE_LICENSE("GPL");

