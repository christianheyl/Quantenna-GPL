/*
 * Copyright (c) 2011 Quantenna Communications, Inc.
 * All rights reserved.
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
#include "bootcfg_store_init.h"
#include <common/ruby_partitions.h>

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/slab.h>

#define STORE_NAME_MTD		"mtd"
#define STORE_NAME_EEPROM	"eeprom"

static char *store = STORE_NAME_MTD;

module_param(store, charp, S_IRUGO);
MODULE_PARM_DESC(store, "Datastore name");

#define LOG_FAIL(_r)								\
	do {									\
		printk("%s failed at line %d, %s = %d\n",			\
				__FUNCTION__, __LINE__, #_r, (int)(_r));	\
	} while(0)

static __init int bootcfg_copy_mtd_to_eeprom(void)
{
	struct bootcfg_store_ops *mtd;
	struct bootcfg_store_ops *eeprom;
	size_t mtd_size_lim = BOOT_CFG_SIZE;
	size_t eeprom_size_lim = BOOT_CFG_SIZE;
	uint8_t *buf = NULL;
	int ret = -1;

	buf = kmalloc(BOOT_CFG_SIZE, GFP_KERNEL);
	if (buf == NULL) {
		LOG_FAIL(buf);
		goto out;
	}

	mtd = bootcfg_flash_get_ops();
	if (mtd == NULL) {
		LOG_FAIL(mtd);
		goto out;
	}

	eeprom = bootcfg_compression_adapter(bootcfg_eeprom_get_ops());
	if (eeprom == NULL) {
		LOG_FAIL(eeprom);
		goto out;
	}

	ret = mtd->init(mtd, &mtd_size_lim);
	if (ret) {
		LOG_FAIL(ret);
		kfree(eeprom);
		goto out;
	}

	ret = eeprom->init(eeprom, &eeprom_size_lim);
	if (ret) {
		LOG_FAIL(ret);
		goto out_exit_mtd;
	}

	ret = mtd->read(mtd, buf, mtd_size_lim);
	if (ret) {
		LOG_FAIL(ret);
		goto out_exit_both;
	}


	ret = eeprom->write(eeprom, buf, eeprom_size_lim);
	if (ret) {
		LOG_FAIL(ret);
		goto out_exit_both;
	}

out_exit_both:
	eeprom->exit(eeprom);
out_exit_mtd:
	mtd->exit(mtd);
out:
	if (buf) {
		kfree(buf);
	}
	return ret;
}


/*
 * Provide different storage implementation depending on module parameter
 */
__init struct bootcfg_store_ops *bootcfg_get_datastore(void)
{
	if (strcmp(store, STORE_NAME_MTD) == 0) {
		return bootcfg_flash_get_ops();
	} else if (strcmp(store, STORE_NAME_EEPROM) == 0) {
		return bootcfg_compression_adapter(bootcfg_eeprom_get_ops());
	} else if (strcmp(store, "mtd_to_eeprom") == 0) {
		bootcfg_copy_mtd_to_eeprom();
	} else {
		return bootcfg_file_get_ops(store);
	}

	return NULL;
}

