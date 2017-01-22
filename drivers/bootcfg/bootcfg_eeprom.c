/*
 * Copyright (c) 2011 Quantenna Communications, Inc.
 * All rights reserved.
 *
 * Bootcfg storage with AT24C64D EEPROMs
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

#include "bootcfg_drv.h"

#include <qtn/bootcfg.h>
#include <common/ruby_partitions.h>
#include <common/ruby_version.h>

#include <linux/i2c.h>
#include <linux/i2c/at24.h>
#include <linux/sched.h>

#define I2C_EEPROM_ADAPTER_NUM	0x0
#define I2C_EEPROM_DEVICE_ADDR	0x50	/* first bits are 1010, then 3 chip select pins; 0x50 - 0x57 */

#define I2C_EEPROM_SIZE_BITS	(64 * 1024)
#define I2C_EEPROM_NBBY		8
#define I2C_EEPROM_SIZE_BYTES	(I2C_EEPROM_SIZE_BITS / I2C_EEPROM_NBBY)
#define I2C_EEPROM_PAGE_BYTES	32

static long i2c_devaddr = I2C_EEPROM_DEVICE_ADDR;

module_param(i2c_devaddr, long, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(i2c_devaddr, "I2C device address of EEPROM");


static struct i2c_client *dev_client = NULL;
static struct memory_accessor *eeprom_mem_acc = NULL;
static spinlock_t g_eeprom_lock;

static void at24_setup(struct memory_accessor *mem_acc, void *context)
{
	eeprom_mem_acc = mem_acc;
}

static struct at24_platform_data at24c64d_plat = {
	.byte_len	= I2C_EEPROM_SIZE_BYTES,
	.page_size	= I2C_EEPROM_PAGE_BYTES,
	.flags		= AT24_FLAG_ADDR16,
	.setup		= &at24_setup,
};

static struct i2c_board_info eeprom_info = {
	I2C_BOARD_INFO("at24", I2C_EEPROM_DEVICE_ADDR),
	.platform_data = &at24c64d_plat,
};

int __init bootcfg_eeprom_init(struct bootcfg_store_ops *ops, size_t *store_limit)
{
	spin_lock_init(&g_eeprom_lock);

	eeprom_info.addr = i2c_devaddr;

	printk("%s i2c_devaddr : 0x%x\n", __FUNCTION__, eeprom_info.addr);

	dev_client = i2c_new_device(i2c_get_adapter(I2C_EEPROM_ADAPTER_NUM), &eeprom_info);
	if (!dev_client) {
		printk(KERN_ERR "%s: error instantiating i2c device\n",
				__FUNCTION__);
		goto device_client_fail;
	}

	*store_limit = I2C_EEPROM_SIZE_BYTES;

	if (eeprom_mem_acc == NULL) {
		return -ENODEV;
	}

	return 0;

device_client_fail:
	return -1;
}

void __exit bootcfg_eeprom_exit(struct bootcfg_store_ops *ops)
{
	i2c_unregister_device(dev_client);
}

static int bootcfg_eeprom_read(struct bootcfg_store_ops *ops, void* buf, const size_t bytes)
{
	int ret;

	spin_lock(&g_eeprom_lock);
	ret = eeprom_mem_acc->read(eeprom_mem_acc, buf, 0, bytes);
	spin_unlock(&g_eeprom_lock);

	if (ret == bytes) {
		ret = 0;
	}

	return ret;
}

static int bootcfg_eeprom_write(struct bootcfg_store_ops *ops, const void* buf, const size_t bytes)
{
	int ret;

	spin_lock(&g_eeprom_lock);
	ret = eeprom_mem_acc->write(eeprom_mem_acc, buf, 0, bytes);
	spin_unlock(&g_eeprom_lock);

	if (ret == bytes) {
		ret = 0;
	}

	return ret;
}


static struct bootcfg_store_ops eeprom_store_ops = {
	.read	= bootcfg_eeprom_read,
	.write	= bootcfg_eeprom_write,
	.init	= bootcfg_eeprom_init,
	.exit	= __devexit_p(bootcfg_eeprom_exit),
};

struct bootcfg_store_ops * __init bootcfg_eeprom_get_ops(void)
{
	return &eeprom_store_ops;
}

