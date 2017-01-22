/*
 * (C) Copyright 2013 Quantenna Communications Inc.
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

#include <linux/proc_fs.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/io.h>

#include <qtn/qtn_debug.h>
#include <common/topaz_platform.h>

#define PROC_NAME "temp_sens"
#define TOPAZ_TEMP_SENS_DRIVER_NAME		"topaz_tempsens"

#define TOPAZ_TEMPSENS_INIT_VAL	-40
#define TOPAZ_TEMPSENS_CODE_TBL_SIZE 34
#define TOPAZ_TEMPSENS_STEP 5 /*Each point on the table corresponds to 5 Degree C step */

/* Temperature curve is non-linear, this is a table of values reported by the temperature sensor for a range from -40 to 130 C for increments of 5 C*/
const int code_idx[TOPAZ_TEMPSENS_CODE_TBL_SIZE] = {3800, 3792, 3783, 3774, 3765, 3756, 3747, 3737, 3728, 3718, 3708, 3698, 3688, 3678, 3667, 3656, 3645,
	                    3634, 3623, 3611, 3600, 3588, 3575, 3563, 3550, 3537, 3524, 3510, 3496, 3482, 3467, 3452, 3437, 3421};

int topaz_read_internal_temp_sens(int *temp_intvl)
{
	int temp;
	int idx = 0;
	*temp_intvl = TOPAZ_TEMPSENS_INIT_VAL;

	temp = (readl(TOPAZ_SYS_CTL_TEMP_SENS_DATA) & TOPAZ_SYS_CTL_TEMP_SENS_DATA_TEMP);

	for (idx = 0; idx < TOPAZ_TEMPSENS_CODE_TBL_SIZE; idx++) {
		if (temp >= code_idx[idx]) {
			*temp_intvl = *temp_intvl + (idx * TOPAZ_TEMPSENS_STEP);
			break;
		}
	}
	return idx;
}
EXPORT_SYMBOL(topaz_read_internal_temp_sens);

static int topaz_temp_sens_read_proc(char *page, char **start, off_t offset,
		int count, int *eof, void *_unused)
{
	int lower_temp_intvl;
	int upper_temp_intvl;
	const unsigned int lim = PAGE_SIZE - 1;
	int len = 0;

	lower_temp_intvl= TOPAZ_TEMPSENS_INIT_VAL;
	upper_temp_intvl= TOPAZ_TEMPSENS_INIT_VAL;

	if (offset > 0) {
		*eof = 1;
		return 0;
	}
	/* Determine the upper interval corresponding to temp sens value */
	topaz_read_internal_temp_sens(&upper_temp_intvl);

	/* Lower interval is 5 degree centigrade below */
	lower_temp_intvl = upper_temp_intvl - TOPAZ_TEMPSENS_STEP;

	len += snprintf(&page[len], lim-len, "Temperature between %d - %d C\n",
						lower_temp_intvl, upper_temp_intvl);

	return len;
}

static int __init topaz_temp_sens_create_proc(void)
{
	struct proc_dir_entry *entry = create_proc_entry(PROC_NAME, 0600, NULL);
	if (!entry) {
		return -ENOMEM;
	}

	entry->write_proc = NULL;
	entry->read_proc = topaz_temp_sens_read_proc;

	return 0;
}

int __init topaz_temp_sens_init(void)
{
	int rc;

	rc = topaz_temp_sens_create_proc();
	if (rc) {
		return rc;
	}

	writel(TOPAZ_SYS_CTL_TEMPSENS_CTL_SHUTDWN, TOPAZ_SYS_CTL_TEMPSENS_CTL);
	writel(~(TOPAZ_SYS_CTL_TEMPSENS_CTL_START_CONV), TOPAZ_SYS_CTL_TEMPSENS_CTL);
	writel(TOPAZ_SYS_CTL_TEMPSENS_CTL_START_CONV, TOPAZ_SYS_CTL_TEMPSENS_CTL);

	printk(KERN_DEBUG "%s success\n", __FUNCTION__);

	return 0;
}

static void __exit topaz_temp_sens_exit(void)
{
	remove_proc_entry(PROC_NAME, NULL);
}

module_init(topaz_temp_sens_init);
module_exit(topaz_temp_sens_exit);

MODULE_DESCRIPTION("Topaz Temperature Sensor");
MODULE_AUTHOR("Quantenna");
MODULE_LICENSE("GPL");
