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

#ifndef AUTOCONF_INCLUDED
#include <linux/config.h>
#endif
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>

#include "qdrv_features.h"
#include "qdrv_debug.h"
#include "qdrv_control.h"
#include "qdrv_mac.h"
#include "qdrv_soc.h"
#include "qdrv_muc_stats.h"

#include <qtn/bootcfg.h>

#include <asm/board/troubleshoot.h>

/*
 * Define boiler plate module stuff
 */
MODULE_DESCRIPTION("802.11 Wireless Driver");
MODULE_AUTHOR("Quantenna Communications Inc., Mats Aretun");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

#define QDRV_DEV_NAME "qdrv"

/* 3.1 times seems safe for MuC (or AuC) crash - found by trial and error */
#define QDRV_CORE_DUMP_COMPRESS_RATIO	(310)

struct qdrv_mac *qdrv_device_to_qdrv_mac(struct device *dev)
{
	struct qdrv_cb *qcb;
	const char *name_of_dev = NULL;

	if (dev == NULL) {
		return NULL;
	}

	name_of_dev = dev_name(dev);

	if (strcmp(name_of_dev, QDRV_DEV_NAME) != 0) {
		return NULL;
	}

	qcb = (struct qdrv_cb *) dev_get_drvdata(dev);

	return &(qcb->macs[0]);
}

static ssize_t qdrv_attr_control_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int count;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	if((count = qdrv_control_output(dev, buf)) < 0)
	{
		DBGPRINTF_E("Failed to generate output\n");
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return(0);
	}

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return(count);
}

static ssize_t qdrv_attr_control_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	if (count < 1) {
		goto out;
	}

	qdrv_control_input(dev, (char *) buf, (unsigned int) count);

out:
	/*
	* FIXME: return value should reflect failure if qdrv_control_input
	* above said so. And yet busybox echo seems to retry for failure.
	* This is not what we expected. Have to make it success here, while
	* what we really should fix is busybox echo
	*/
	return (ssize_t)count;
}

static DEVICE_ATTR(control, 0644,
	qdrv_attr_control_show, qdrv_attr_control_store);

/* flag to trigger kernel panic on MuC halt isr */
static int panic_on_muc_halt = 1;

static void qdrv_panic_on_muc_halt_init(void)
{
	char buf[32] = {0};
	int dev_mode;

	if (bootcfg_get_var("dev_mode", buf)) {
		if (sscanf(buf, "=%d", &dev_mode) == 1) {
			if (dev_mode) {
				panic_on_muc_halt = 0;
			}
		}
	}
}

static void muc_death_work(struct work_struct *work)
{
	printk(KERN_ERR"Dumping register differences\n");
	qdrv_control_dump_active_hwreg();

	arc_save_to_sram_safe_area(QDRV_CORE_DUMP_COMPRESS_RATIO);

	/* panic if appropriate - will eventually restart the system by calling machine_restart() */
	if (panic_on_muc_halt) {
		panic("MuC has halted; panic_on_muc_halt is %d", panic_on_muc_halt);
	}
}

static DECLARE_DELAYED_WORK(muc_death_wq, &muc_death_work);

#define QTN_MUC_DEAD_TIMER	(QTN_MPROC_TIMEOUT - HZ) /* Must be less than the mproc timeout */

void qdrv_muc_died_sysfs(void)
{
	const unsigned long delay_jiff = QTN_MUC_DEAD_TIMER;

	schedule_delayed_work(&muc_death_wq, delay_jiff);
}

static ssize_t show_panic_on_muc_halt(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", panic_on_muc_halt);
}

static ssize_t store_panic_on_muc_halt(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	if (count >= 1)
		panic_on_muc_halt = (buf[0] == '1');
	return count;
}

static DEVICE_ATTR(panic_on_muc_halt, 0644, show_panic_on_muc_halt, store_panic_on_muc_halt);

/* service entry points and corresponding attributes for PHY stats entries in the sys fs */

BIN_ATTR_ACCESS_DECL(qdrv_attr_rssi_phy_stats, filp, p_kobj, p_bin_attr, buf, offset, size)
{
	ssize_t	size_phy_stats = qdrv_muc_get_size_rssi_phy_stats();
	struct qdrv_mac *mac =  qdrv_device_to_qdrv_mac((struct device *) p_bin_attr->private);
	struct ieee80211com *ic = NULL;
	struct qtn_stats *addr_rssi_phy_stats = NULL;

	if (mac != NULL) {
		struct qdrv_wlan *qw = mac->data;

		if (qw != NULL) {
			ic = &(qw->ic);
		}
	}

	if (ic == NULL) {
		return -1;
	}

	addr_rssi_phy_stats = qtn_muc_stats_get_addr_latest_stats(mac, ic, MUC_PHY_STATS_RSSI_RCPI_ONLY);
	if (addr_rssi_phy_stats == NULL) {
		return -1;
	}

	/*
	 * avoid buffer overruns ...
	 * i.e. for command "hexdump /sys/devices/qdrv/rssi_phy_stats",
	 * size is only 16.
	 */
	if (size < size_phy_stats) {
		size_phy_stats = size;
	}

	if (offset >= (loff_t) size_phy_stats) {
		return 0;
	}

	memcpy(buf, addr_rssi_phy_stats, size_phy_stats);

	return size_phy_stats;
}

static void qdrv_module_release(struct device *dev)
{
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
}

static struct device qdrv_device =
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)
	.bus_id		= QDRV_DEV_NAME,
#endif
	.release	= qdrv_module_release,
};

struct device *qdrv_soc_get_addr_dev(void)
{
	return &qdrv_device;
}

static struct bin_attribute qdrv_show_rssi_phy_stats =
{
	.attr	 = { .name = "rssi_phy_stats", .mode = 0444 },
	.private = (void *) &qdrv_device,
	.read	 = qdrv_attr_rssi_phy_stats,
	.write	 = NULL,
	.mmap	 = NULL,
};

static int qdrv_module_create_sysfs_phy_stats(void)
{
	if (device_create_bin_file(&qdrv_device, &qdrv_show_rssi_phy_stats) != 0) {
		DBGPRINTF_E("Failed to create rssi_phy_stats sysfs file \"%s\"\n",
			qdrv_show_rssi_phy_stats.attr.name);
		goto rssi_phy_stats_fail;
	}

	return 0;

rssi_phy_stats_fail:
	return -1;
}

static int __init qdrv_module_init(void)
{
	void *data;
	int size;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	/* Get the size of our private data data structure */
	size = qdrv_soc_cb_size();

	/* Allocate (zero filled) private memory for our device context */
	if ((data = kzalloc(size, GFP_KERNEL)) ==  NULL) {
		DBGPRINTF_E("Failed to allocate %d bytes for private data\n", size);
		DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
		return(-ENOMEM);
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
	dev_set_name(&qdrv_device, QDRV_DEV_NAME);
#endif

	/* Attach the private memory to the device */
	dev_set_drvdata(&qdrv_device, data);

	qdrv_panic_on_muc_halt_init();

	if (device_register(&qdrv_device) != 0) {
		DBGPRINTF_E("Failed to register \"%s\"\n", QDRV_DEV_NAME);
		goto device_reg_fail;
	}

	if (device_create_file(&qdrv_device, &dev_attr_control) != 0) {
		DBGPRINTF_E("Failed to create control sysfs file \"%s\"\n", QDRV_DEV_NAME);
		goto control_sysfs_fail;
	}

	if (device_create_file(&qdrv_device, &dev_attr_panic_on_muc_halt) != 0)	{
		DBGPRINTF_E("Failed to create panic_on_muc_halt sysfs file \"%s\"\n", QDRV_DEV_NAME);
		goto muc_halt_sysfs_fail;
	}

	/* Initialize control interface */
	if (qdrv_control_init(&qdrv_device) < 0) {
		DBGPRINTF_E("Failed to initialize driver control interface\n");
		goto control_init_fail;
	}

	if (qdrv_module_create_sysfs_phy_stats() < 0) {
		/* no cleanup and no error reporting required if failure */
		goto control_init_fail;
	}

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");

	return 0;

control_init_fail:
	device_remove_file(&qdrv_device, &dev_attr_panic_on_muc_halt);
muc_halt_sysfs_fail:
	device_remove_file(&qdrv_device, &dev_attr_control);
control_sysfs_fail:
	device_unregister(&qdrv_device);
device_reg_fail:
	kfree(data);
	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
	return -1;
}

static void __exit qdrv_module_exit(void)
{
	void *data;

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "-->Enter\n");

	/* Cleanup in reverse order */

	/* Start with executing a stop command */
	if (qdrv_control_input(&qdrv_device, "stop", 4) < 0) {
		DBGPRINTF_E("Failed to execute stop command\n");
	}

	/* Exit the control interface */
	if (qdrv_control_exit(&qdrv_device) < 0) {
		DBGPRINTF_E("Failed to exit driver control interface\n");
	}

	/* Get the private device data */
	data = dev_get_drvdata(&qdrv_device);

	device_remove_bin_file(&qdrv_device, &qdrv_show_rssi_phy_stats);

	device_remove_file(&qdrv_device, &dev_attr_panic_on_muc_halt);
	device_remove_file(&qdrv_device, &dev_attr_control);

	device_unregister(&qdrv_device);

	/* Free the private memory */
	kfree(data);

	DBGPRINTF(DBG_LL_ALL, QDRV_LF_TRACE, "<--Exit\n");
}

module_init(qdrv_module_init);
module_exit(qdrv_module_exit);
