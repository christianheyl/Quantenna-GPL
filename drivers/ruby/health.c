/*
 * (C) Copyright 2012 Quantenna Communications Inc.
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

#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/reboot.h>

#include <asm/cache.h>

#include <asm/board/mem_check.h>

#define RUBY_HEALTH_MOD_NUM_MAX		32
#define RUBY_HEALTH_DRIVER_NAME		"ruby_health"

static uint32_t ruby_health_csum = 0;
static unsigned int ruby_health_csum_seq_id = 0;
static unsigned int ruby_health_csum_change_seq_id = 0;
static unsigned long ruby_health_csum_change_jiffies = 0;
static struct task_struct *ruby_health_task = NULL;
static struct module *ruby_health_modules[RUBY_HEALTH_MOD_NUM_MAX] = {0,};
static DEFINE_SPINLOCK(ruby_health_modules_lock);

static uint32_t ruby_health_gen_csum(void *start, uint32_t sz, uint32_t csum)
{
	uint32_t *ptr_begin = (uint32_t*)(((uint32_t)start) & ~0x3);
	uint32_t *ptr_end = ptr_begin + (sz >> 2);
	int sleep_counter = 0;
	unsigned long last_jiffies = jiffies;

	while (ptr_begin != ptr_end) {
		/* generate simple checksum */
		csum = csum ^ arc_read_uncached_32(ptr_begin);
		++ptr_begin;

		/* sleep from time to time */
		++sleep_counter;
		if (sleep_counter >= 256) {
			sleep_counter = 0;
			if (jiffies - last_jiffies > 1) {
				msleep(3 * 1000 / HZ);
				last_jiffies = jiffies;
			}
		}
	}

	msleep(10);

	return csum;
}

static uint32_t ruby_health_kernel_check(uint32_t csum)
{
	extern char _text, _etext, __sram_text_start, __sram_text_end;

	csum = ruby_health_gen_csum(&_text, &_etext - &_text, csum);
#ifdef CONFIG_ARCH_RUBY_NUMA
	csum = ruby_health_gen_csum(&__sram_text_start,
		&__sram_text_end - &__sram_text_start, csum);
#endif

	return csum;
}

static uint32_t ruby_health_modules_check(uint32_t csum)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ruby_health_modules); ++i) {

		struct module *mod = NULL;
		struct module *mod_tmp;

		spin_lock(&ruby_health_modules_lock);
		mod_tmp = ruby_health_modules[i];
		if (mod_tmp && try_module_get(mod_tmp)) {
			mod = mod_tmp;
		}
		spin_unlock(&ruby_health_modules_lock);

		if (mod) {
			csum = ruby_health_gen_csum(mod->module_core, mod->core_text_size, csum);
#ifdef CONFIG_ARCH_RUBY_NUMA
			csum = ruby_health_gen_csum(mod->module_sram, mod->sram_text_size, csum);
#endif
			module_put(mod);
		}
	}

	return csum;
}

static int ruby_health_daemon(void *arg)
{
	printk(KERN_INFO"%s: daemon start\n", RUBY_HEALTH_DRIVER_NAME);

	while (!kthread_should_stop()) {

		uint32_t csum = 0;

		WARN_ONCE(!is_sram_irq_stack_good(), "*** IRQ SRAM stack corrupted\n");
		WARN_ONCE(!is_kernel_stack_good(), "*** Kernel stack corrupted\n");

		/* Generate checksum */
		csum = ruby_health_kernel_check(csum);
		csum = ruby_health_modules_check(csum);

		/* Keep track of how many times checksum generated */
		++ruby_health_csum_seq_id;

		/* If checksum changed tell it */
		if (csum != ruby_health_csum) {
			printk(KERN_ERR"*** Checksum changed 0x%x (ts=%lu, seq=%u) -> 0x%x (ts=%lu, seq=%u)\n",
				ruby_health_csum, ruby_health_csum_change_jiffies, ruby_health_csum_change_seq_id,
				csum, jiffies, ruby_health_csum_seq_id);
			ruby_health_csum = csum;
			ruby_health_csum_change_seq_id = ruby_health_csum_seq_id;
			ruby_health_csum_change_jiffies = jiffies;
		}

		/* This is very low priority background task */
		msleep(50);
	}

	printk(KERN_INFO"%s: daemon stop: checksum=0x%x\n",
		RUBY_HEALTH_DRIVER_NAME, ruby_health_csum);

	return 0;
}

static void ruby_health_stop(void)
{
	if (ruby_health_task) {
		kthread_stop(ruby_health_task);
		ruby_health_task = NULL;
		printk(KERN_INFO"%s: stop\n", RUBY_HEALTH_DRIVER_NAME);
	}
}

static int ruby_health_start(void)
{
	struct sched_param param = { .sched_priority = 0 };

	printk(KERN_INFO"%s: start\n", RUBY_HEALTH_DRIVER_NAME);

	ruby_health_stop();

	ruby_health_task = kthread_create(ruby_health_daemon, NULL, RUBY_HEALTH_DRIVER_NAME);
	if (IS_ERR(ruby_health_task)) {
		ruby_health_task = NULL;
		return PTR_ERR(ruby_health_task);
	}

	sched_setscheduler_nocheck(ruby_health_task, SCHED_IDLE, &param);
	set_user_nice(ruby_health_task, 19);

	wake_up_process(ruby_health_task);

	return 0;
}

static int ruby_health_write_proc(struct file *file, const char __user *buffer, unsigned long count, void *data)
{
	int ret = 0;

	if (count < 1) {
		return -EINVAL;
	}

	if (buffer[0] != '0') {
		ret = ruby_health_start();
	} else {
		ruby_health_stop();
	}

	return ret ? ret : count;
}

static int ruby_health_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	return sprintf(page, "%s: checksum=0x%x\n", RUBY_HEALTH_DRIVER_NAME, ruby_health_csum);
}

static int __init ruby_health_create_proc(void)
{
	struct proc_dir_entry *entry = create_proc_entry(RUBY_HEALTH_DRIVER_NAME, 0600, NULL);
	if (!entry) {
		return -ENOMEM;
	}

	entry->write_proc = ruby_health_write_proc;
	entry->read_proc = ruby_health_read_proc;

	return 0;
}

static void __exit ruby_health_destroy_proc(void)
{
	remove_proc_entry(RUBY_HEALTH_DRIVER_NAME, NULL);
}

static int ruby_health_module_notifier_func(struct notifier_block *self, unsigned long val, void *data)
{
	int i;

	spin_lock(&ruby_health_modules_lock);

	for (i = 0; i < ARRAY_SIZE(ruby_health_modules); ++i) {
		if (val == MODULE_STATE_GOING) {
			if (ruby_health_modules[i] == data) {
				ruby_health_modules[i] = NULL;
				break;
			}
		} else if (val == MODULE_STATE_LIVE) {
			if (ruby_health_modules[i] == NULL) {
				ruby_health_modules[i] = data;
				break;
			}
		}
	}

	spin_unlock(&ruby_health_modules_lock);

	return 0;
}

static struct notifier_block ruby_health_module_notifier = {
	.notifier_call = ruby_health_module_notifier_func,
};

static int ruby_health_stop_notifier_func(struct notifier_block *self, unsigned long val, void *data)
{
	ruby_health_stop();
	return 0;
}

static struct notifier_block ruby_health_reboot_notifier = {
	.notifier_call = ruby_health_stop_notifier_func,
};

static struct notifier_block ruby_health_panic_notifier = {
	.notifier_call = ruby_health_stop_notifier_func,
};

static int __init ruby_health_init(void)
{
	printk(KERN_INFO"%s loading\n", RUBY_HEALTH_DRIVER_NAME);
	int ret = ruby_health_create_proc();
	if (!ret) {
		register_module_notifier(&ruby_health_module_notifier);
		register_reboot_notifier(&ruby_health_reboot_notifier);
		atomic_notifier_chain_register(&panic_notifier_list,
			&ruby_health_panic_notifier);
	}
	return ret;
}

static void __exit ruby_health_exit(void)
{
	unregister_module_notifier(&ruby_health_module_notifier);
	unregister_reboot_notifier(&ruby_health_reboot_notifier);
	atomic_notifier_chain_unregister(&panic_notifier_list,
			&ruby_health_panic_notifier);
	ruby_health_destroy_proc();
	ruby_health_stop();
	printk(KERN_INFO "%s unloaded\n", RUBY_HEALTH_DRIVER_NAME);
}

module_init(ruby_health_init);
module_exit(ruby_health_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Quantenna");

