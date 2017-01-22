/*
 * (C) Copyright 2015 Quantenna Communications Inc.
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

#include <linux/types.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/reboot.h>
#include <linux/interrupt.h>
#include <asm/io.h>
#include <asm/uaccess.h>	/* for copy_from_user */
#include <asm/gpio.h>
#include <asm/hardware.h>

#include <qtn/amber.h>
#include <common/topaz_platform.h>
#include <linux/delay.h>

#define PROC_AMBER_DIR_NAME "amber"
#define PROC_AMBER_WIFI2SOC_INT_FILE_NAME "wifi2soc_int"

static struct proc_dir_entry *amber_dir;
static struct proc_dir_entry *amber_wifi2soc_file;

static unsigned long interrupt_errors_mask = 0;
static unsigned long interrupt_errors_store = 0;

static struct timer_list amber_timer;
static DEFINE_SPINLOCK(wifi2soc_lock);
static int initialized = 0;

static struct {
	const char *token;
	const int val;
} wifi2soc_int_token_map[] =
{
	{"SYSTEM_READY",	TOPAZ_AMBER_WIFI2SOC_SYSTEM_READY},
	{"CAL_COMPLETE",	TOPAZ_AMBER_WIFI2SOC_CAL_COMPLETE},
	{"CAL_CHANGE_REQ",	TOPAZ_AMBER_WIFI2SOC_CAL_CHANGE_REQ},
	{"SHUTDOWN",		TOPAZ_AMBER_WIFI2SOC_SHUTDOWN},
	{"WATCHDOG",		TOPAZ_AMBER_WIFI2SOC_WATCHDOG},
	{"EMERGENCY",		TOPAZ_AMBER_WIFI2SOC_EMERGENCY},
	{"NFS_MOUNT_FAILURE",	TOPAZ_AMBER_WIFI2SOC_NFS_MOUNT_FAILURE},
	{"NFS_ACCESS_FAILURE",	TOPAZ_AMBER_WIFI2SOC_NFS_ACCESS_FAILURE},
	{"NFS_INTEGRITY_FAILURE",TOPAZ_AMBER_WIFI2SOC_NFS_INTEGRITY_FAILURE},
	{"WAKE_ON_WLAN",	TOPAZ_AMBER_WIFI2SOC_WAKE_ON_WLAN}
};

static enum amber_shutdown_code shutdown_code = AMBER_SD_CODE_GRACEFUL;

void amber_set_shutdown_code(enum amber_shutdown_code code)
{
	shutdown_code = code;
}
EXPORT_SYMBOL(amber_set_shutdown_code);

int amber_trigger_wifi2soc_interrupt_sync(unsigned long interrupt_code)
{
	interrupt_code &= interrupt_errors_mask;

	if (interrupt_code == 0) {
		return 0;
	}

	while (readl(IO_ADDRESS(TOPAZ_AMBER_WIFI2SOC_ERROR_REG)) & interrupt_errors_mask) {
		/*
		 * Idle wait for the previous error codes to be cleared by ST Host.
		 * If we get stuck here, this means ST Host is hung, so it does not matter
		 * where we halt - here, or few cycles later in machine_halt().
		 */
	};

	writel(interrupt_code, IO_ADDRESS(TOPAZ_AMBER_WIFI2SOC_ERROR_REG));
	writel(TOPAZ_AMBER_WIFI2SOC_INT_OUTPUT, IO_ADDRESS(GPIO_OUTPUT_MASK));
	writel(TOPAZ_AMBER_WIFI2SOC_INT_OUTPUT, IO_ADDRESS(GPIO_OUTPUT));
	udelay(1);
	writel(0, IO_ADDRESS(GPIO_OUTPUT));
	writel(0, IO_ADDRESS(GPIO_OUTPUT_MASK));

	return 0;
}

void amber_shutdown(void)
{
	unsigned long interrupt_code = 0;

	if (!initialized) {
		/* Any early reboot is considered emergency */
		interrupt_code = TOPAZ_AMBER_WIFI2SOC_EMERGENCY;
		interrupt_errors_mask = readl(IO_ADDRESS(TOPAZ_AMBER_WIFI2SOC_MASK_REG));
	} else {
		switch (shutdown_code) {
		case AMBER_SD_CODE_GRACEFUL:
			interrupt_code = TOPAZ_AMBER_WIFI2SOC_SHUTDOWN;
			break;
		case AMBER_SD_CODE_EMERGENCY:
			interrupt_code = TOPAZ_AMBER_WIFI2SOC_EMERGENCY;
			break;
		case AMBER_SD_CODE_NONE:
		default:
			/* Don't send any error code */
			interrupt_code = 0;
			break;
		}
	}

	amber_trigger_wifi2soc_interrupt_sync(interrupt_code);
}
EXPORT_SYMBOL(amber_shutdown);

static int amber_post_stored_interrupts(void)
{
	int need_timer = 0;

	if (interrupt_errors_store == 0) {
		return 0;
	}

	if ((readl(IO_ADDRESS(TOPAZ_AMBER_WIFI2SOC_ERROR_REG)) & interrupt_errors_mask) == 0) {

		writel(interrupt_errors_store, IO_ADDRESS(TOPAZ_AMBER_WIFI2SOC_ERROR_REG));

		interrupt_errors_store = 0;

		/*
		 * Pulse WIFI2SOC_INT_O. Minimal pulse width is 5 ns.
		 * The below two calls compile to JL instructions to call gpio_set_value(),
		 * which in turn indirectly calls gpio set handler.
		 * This means the number of 500 MHz cycles between line assert and de-assert is
		 * sufficiently large to fulfill the minimal pulse width requirement.
		 */

		gpio_set_value(TOPAZ_AMBER_WIFI2SOC_INT_OUTPUT, 1);
		gpio_set_value(TOPAZ_AMBER_WIFI2SOC_INT_OUTPUT, 0);

		need_timer = 0;
	} else {
		need_timer = 1;
	}

	return need_timer;
}

static void amber_timer_func(unsigned long data)
{
	amber_trigger_wifi2soc_interrupt(0);
}

int amber_trigger_wifi2soc_interrupt(unsigned long interrupt_code)
{
	int need_timer = 0;
	unsigned long flags;

	spin_lock_irqsave(&wifi2soc_lock, flags);
	interrupt_errors_store |= interrupt_code & interrupt_errors_mask;
	need_timer = amber_post_stored_interrupts();
	spin_unlock_irqrestore(&wifi2soc_lock, flags);

	if (need_timer) {
		mod_timer(&amber_timer, jiffies + AMBER_TIMER_PERIOD_JIFFIES);
	}

	return 0;
}
EXPORT_SYMBOL(amber_trigger_wifi2soc_interrupt);

static int amber_wifi2soc_int_proc_read(char *buffer,
		  char **buffer_location, off_t offset,
		  int buffer_length, int *eof, void *data)
{
	int i;
	int written = 0;
	unsigned long interrupt_code = readl(IO_ADDRESS(TOPAZ_AMBER_WIFI2SOC_ERROR_REG));

	*eof = 1;

	for (i = 0; i < ARRAY_SIZE(wifi2soc_int_token_map); i++) {
		if (interrupt_code & wifi2soc_int_token_map[i].val) {
			int ret = snprintf(buffer + written,buffer_length - written, "%s\n",
					wifi2soc_int_token_map[i].token);

			if (ret < 0) {
				break;
			}

			if (ret + written > buffer_length) {
				*eof = 0;
				written = buffer_length;
				break;
			}

			written += ret;
		}
	}

	return written;
}

static int amber_wifi2soc_int_proc_write(struct file *file, const char *buffer,
		       unsigned long count, void *data)
{
	char *procfs_buffer;
	char *ptr;
	char *token;
	char found_token = 0;
	int i;

	procfs_buffer = kmalloc(count, GFP_KERNEL);

	if (procfs_buffer == NULL) {
		printk("AMBER: wifi2soc error - out of memory\n");
		return -ENOMEM;
	}

	/* write data to the buffer */
	if (copy_from_user(procfs_buffer, buffer, count)) {
		goto bail;
	}

	ptr = (char *)procfs_buffer;
	ptr[count - 1] = '\0';
	token = strsep(&ptr, "\n");

	for (i = 0; i < ARRAY_SIZE(wifi2soc_int_token_map); i++) {
		if (strcmp(token, wifi2soc_int_token_map[i].token) == 0) {
			amber_trigger_wifi2soc_interrupt(wifi2soc_int_token_map[i].val);
			found_token = 1;
			break;
		}
	}

	if (!found_token) {
		printk(KERN_ERR "AMBER: wifi2soc error - unable to parse token %s\n", token);
	}

bail:
	kfree(procfs_buffer);
	return count;
}

static int amber_panic_notifier(struct notifier_block *this, unsigned long event, void *ptr)
{
	amber_set_shutdown_code(AMBER_SD_CODE_EMERGENCY);
	return NOTIFY_DONE;
}

static struct notifier_block amber_panic_block = {
	.notifier_call = amber_panic_notifier,
};

static void amber_soc2wifi_panic_work(struct work_struct *work)
{
	printk("AMBER: SoC host panic - halting Amber kernel\n");
	amber_set_shutdown_code(AMBER_SD_CODE_NONE);
	kernel_halt();
}

static DECLARE_WORK(amber_soc2wifi_panic_wq, &amber_soc2wifi_panic_work);

static void amber_soc2wifi_wake_on_lan_work(struct work_struct *work)
{
	printk("AMBER: SoC host - waking up on LAN\n");
}

static DECLARE_WORK(amber_soc2wifi_wake_on_lan_wq, &amber_soc2wifi_wake_on_lan_work);

static irqreturn_t amber_soc2wifi_irq_handler(int irq, void *dev_id)
{
	unsigned long int_status = readl(IO_ADDRESS(TOPAZ_AMBER_GPIO_IRQ_STATUS));
	unsigned long int_code;

	if ((int_status & (1 << TOPAZ_AMBER_SOC2WIFI_INT_INPUT)) == 0) {
		/* Filter other GPIO interrupts */
		return IRQ_NONE;
	}

	int_code = readl(IO_ADDRESS(TOPAZ_AMBER_SOC2WIFI_ERROR_REG));

	if (int_code & TOPAZ_AMBER_SOC2WIFI_KERNEL_PANIC) {
		/*
		 * Clear the mask to prevent anymore wifi2soc interrupts posting
		 * and halt the kernel.
		 */
		interrupt_errors_mask = 0;
		schedule_work(&amber_soc2wifi_panic_wq);
	}

	if (int_code & TOPAZ_AMBER_SOC2WIFI_WAKE_ON_LAN) {
		schedule_work(&amber_soc2wifi_wake_on_lan_wq);
	}

	writel(1 << TOPAZ_AMBER_SOC2WIFI_INT_INPUT, IO_ADDRESS(TOPAZ_AMBER_GPIO_IRQ_CLEAR));

	return IRQ_HANDLED;
}

static int __init amber_init(void)
{
	int err;

	amber_dir = proc_mkdir(PROC_AMBER_DIR_NAME, NULL);

	amber_wifi2soc_file = create_proc_entry(PROC_AMBER_WIFI2SOC_INT_FILE_NAME, 0x644, amber_dir);

	if (amber_wifi2soc_file == NULL) {
		printk(KERN_ERR "AMBER: unable to create /proc/%s/%s\n", PROC_AMBER_DIR_NAME,
			PROC_AMBER_WIFI2SOC_INT_FILE_NAME);
		err = -ENOMEM;
		goto out_exit_wifi2soc_proc;
	}

	amber_wifi2soc_file->read_proc = amber_wifi2soc_int_proc_read;
	amber_wifi2soc_file->write_proc = amber_wifi2soc_int_proc_write;
	amber_wifi2soc_file->mode = S_IFREG | S_IRUGO;
	amber_wifi2soc_file->uid = 0;
	amber_wifi2soc_file->gid = 0;
	amber_wifi2soc_file->size = 0x1000;
	amber_wifi2soc_file->data = NULL;

	spin_lock_init(&wifi2soc_lock);
	setup_timer(&amber_timer, amber_timer_func, 0);

	interrupt_errors_mask = readl(IO_ADDRESS(TOPAZ_AMBER_WIFI2SOC_MASK_REG));

	err = gpio_request(TOPAZ_AMBER_WIFI2SOC_INT_OUTPUT, "WIFI2SOC_INT_OUTPUT");

	if (err < 0) {
		printk(KERN_INFO "AMBER: failed to request GPIO %d for WIFI2SOC_INT_OUTPUT\n",
			TOPAZ_AMBER_WIFI2SOC_INT_OUTPUT);
		goto out_exit_wifi2soc_gpio;
	}

	ruby_set_gpio_irq_sel(TOPAZ_AMBER_SOC2WIFI_INT_INPUT);

	err = request_irq(GPIO2IRQ(TOPAZ_AMBER_SOC2WIFI_INT_INPUT), amber_soc2wifi_irq_handler, 0,
		"amber_soc2wifi", NULL);

	if (err) {
		printk(KERN_INFO "AMBER: failed to register IRQ %d\n",
			GPIO2IRQ(TOPAZ_AMBER_SOC2WIFI_INT_INPUT));
		goto out_exit_wifi2soc_irq;
	}

	atomic_notifier_chain_register(&panic_notifier_list, &amber_panic_block);

	initialized = 1;

	printk("AMBER: initialized successfully\n");

	return 0;

out_exit_wifi2soc_irq:
	gpio_free(TOPAZ_AMBER_WIFI2SOC_INT_OUTPUT);

out_exit_wifi2soc_gpio:
	remove_proc_entry(PROC_AMBER_WIFI2SOC_INT_FILE_NAME, amber_dir);

out_exit_wifi2soc_proc:
	remove_proc_entry(PROC_AMBER_DIR_NAME, NULL);

	return err;
}

static void __exit amber_exit(void)
{
	del_timer_sync(&amber_timer);

	atomic_notifier_chain_unregister(&panic_notifier_list, &amber_panic_block);

	free_irq(GPIO2IRQ(TOPAZ_AMBER_SOC2WIFI_INT_INPUT), NULL);
	gpio_free(TOPAZ_AMBER_WIFI2SOC_INT_OUTPUT);

	remove_proc_entry(PROC_AMBER_WIFI2SOC_INT_FILE_NAME, amber_dir);
	remove_proc_entry(PROC_AMBER_DIR_NAME, NULL);
}

module_init(amber_init);
module_exit(amber_exit);

MODULE_DESCRIPTION("Amber Driver");
MODULE_AUTHOR("Quantenna");
MODULE_LICENSE("GPL");
