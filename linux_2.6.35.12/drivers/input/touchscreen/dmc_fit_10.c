/*
 *  Copyright (c) 2010 Viragelogic
 */

/*
 * DMC FIT-10 touchscreen driver for Linux
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/serio.h>
#include <linux/init.h>
#include <linux/wait.h>

#ifdef CONFIG_TOUCHSCREEN_DMC_FIT_10_DEBUG
#define DMC_FIT10_DEBUG			1
#endif

#undef  DMC_FIT10_DO_IDLE_MODE
#define DMC_FIT10_DEFAULT_SR		5
#define DMC_FIT10_DEFAULT_CM		2

#define DRIVER_DESC			"DMC FIT-10 touchscreen driver"

#define	DMC_FIT10_MAX_SIZE		(8)

#define DMC_FIT10_RCV_STOPPED		(0)
#define DMC_FIT10_RCV_ACK		(1)
#define DMC_FIT10_RCV_COORD_PIN		(2)
#define DMC_FIT10_RCV_COORD_DATA	(3)

#define DMC_FIT10_ACK			(0x6)
#define DMC_FIT10_NACK			(0x15)

#define DMC_FIT10_SR_30PS		(0x40)
#define DMC_FIT10_SR_50PS		(0x41)
#define DMC_FIT10_SR_80PS		(0x42)
#define DMC_FIT10_SR_100PS		(0x43)
#define DMC_FIT10_SR_130PS		(0x44)
#define DMC_FIT10_SR_150PS		(0x45)
#define DMC_FIT10_SR_1OT		(0x50)

#ifdef DMC_FIT10_DO_IDLE_MODE

typedef struct
{
	int sr;
	char description[32];
}dmc_fit10_sr_t;

dmc_fit10_sr_t s_rates[] = {
	{DMC_FIT10_SR_30PS, "30 cords p/s"},
	{DMC_FIT10_SR_50PS, "50 cords p/s"},
	{DMC_FIT10_SR_80PS, "80 cords p/s"},
	{DMC_FIT10_SR_100PS, "100 cords p/s"},
	{DMC_FIT10_SR_130PS, "130 cords p/s"},
	{DMC_FIT10_SR_150PS, "150 cords p/s"},
	{DMC_FIT10_SR_1OT, "one cord per touch"}
};

#endif

#define DMC_FIT10_MODE_IDLE     (0x5)
#define DMC_FIT10_MODE_CORD1    (0x1)
#define DMC_FIT10_MODE_CORD2    (0x21)
#define DMC_FIT10_MODE_CORD3    (0x31)
#define DMC_FIT10_MODE_END_CORD (0x02)

#define DMC_FIT10_IS_PIN_UP(x)			\
	((x) == 0x10 || (x) == 0x50 ||		\
	 (x) == 0x90 || (x) == 0xD0)

#define DMC_FIT10_IS_PIN_DOWN(x)		\
	((x) == 0x11 || (x) == 0x51 ||		\
	 (x) == 0x91 || (x) == 0xD1)

typedef struct dcm_fit10_cmd
{
	unsigned receiver_state;
	unsigned size;
	unsigned char data[4];
}dcm_fit10_cmd_t;

typedef struct
{
	dcm_fit10_cmd_t cmd;
	char description[64];
}dmc_fit10_mode_t;

static dmc_fit10_mode_t cord_modes[] = {
	{{DMC_FIT10_RCV_COORD_PIN, 1, {DMC_FIT10_MODE_CORD1}},
	 "1: pen-down ID + cord + pen-up ID + power saving"},
	{{DMC_FIT10_RCV_COORD_PIN, 1, {DMC_FIT10_MODE_CORD2}},
	 "2: pen-up ID + cord + pen-down ID + pen-up ID"},
	{{DMC_FIT10_RCV_COORD_PIN, 1, {DMC_FIT10_MODE_CORD3}},
	 "3: pen-down ID + cord + pen-up ID",}
};


DECLARE_WAIT_QUEUE_HEAD(answer_w_q);

/* unpack 10-bit coordinates */
#define DMC_FIT10_CORD_UNPACK(x, ptr)		\
	{					\
		(x) = ((unsigned)*(ptr)) << 8;	\
		ptr++;				\
		(x) |= *(ptr);			\
		ptr++;				\
	}

typedef struct{
	int acks;
	int coords;
	int errors;
} dmc_fit10_counters;

struct dmc_fit10 {
	struct input_dev *dev;
	struct serio *serio;
	unsigned char data[DMC_FIT10_MAX_SIZE];
	int data_fill;
	char phys[32];
	volatile int receiver_state;
	dmc_fit10_counters counters;
};

static int dmc_fit10_send_command(struct dmc_fit10* dmc_fit10,
				  dcm_fit10_cmd_t *command)
{
	int i;

	dmc_fit10->receiver_state = command->receiver_state;
	dmc_fit10->data_fill = 0;
	for(i = 0; i < command->size; i++)
		serio_write(dmc_fit10->serio, command->data[i]);

	if(command->receiver_state == DMC_FIT10_RCV_ACK)
	{
		wait_event(answer_w_q,
			   (dmc_fit10->receiver_state ==
			    DMC_FIT10_RCV_STOPPED));
#ifdef DMC_FIT10_DEBUG
		switch(dmc_fit10->data[0])
		{
		case DMC_FIT10_ACK:
		case DMC_FIT10_NACK:
			break;
		default:
			printk(KERN_ERR "DMC FIT-10: unknown response: %x\n",
			       dmc_fit10->data[0]);
		}
#endif
		return dmc_fit10->data[0];
	}
	return 0;
}

static irqreturn_t dmc_fit10_interrupt(struct serio *serio,
		unsigned char data, unsigned int flags)
{
	struct dmc_fit10* dmc_fit10 = serio_get_drvdata(serio);
	struct input_dev *dev = dmc_fit10->dev;

	switch(dmc_fit10->receiver_state)
	{
	case DMC_FIT10_RCV_ACK:
		dmc_fit10->data[0] = data;
		dmc_fit10->receiver_state = DMC_FIT10_RCV_STOPPED;
		wake_up_all(&answer_w_q);
#ifdef DMC_FIT10_DEBUG
		dmc_fit10->counters.acks++;
#endif
		break;

	case DMC_FIT10_RCV_COORD_PIN:
		if(DMC_FIT10_IS_PIN_DOWN(data))
			dmc_fit10->receiver_state = DMC_FIT10_RCV_COORD_DATA;
		else if(DMC_FIT10_IS_PIN_UP(data))
		{
			input_report_key(dev, BTN_TOUCH, 0);
			input_sync(dev);
		}
#ifdef DMC_FIT10_DEBUG
		else
			dmc_fit10->counters.errors++;
#endif
		break;

	case DMC_FIT10_RCV_COORD_DATA:
		dmc_fit10->data[dmc_fit10->data_fill] = data;
		dmc_fit10->data_fill ++;
		if(dmc_fit10->data_fill > 3)
		{
			int x = 0, y = 0;
			unsigned char *ptr = &dmc_fit10->data[0];

			DMC_FIT10_CORD_UNPACK(x, ptr);
			DMC_FIT10_CORD_UNPACK(y, ptr);
			input_report_abs(dev, ABS_X, x);
			input_report_abs(dev, ABS_Y, y);
			input_report_key(dev, BTN_TOUCH, 1);
			input_sync(dev);
			dmc_fit10->data_fill = 0;
			dmc_fit10->receiver_state = DMC_FIT10_RCV_COORD_PIN;
#ifdef DMC_FIT10_DEBUG
			dmc_fit10->counters.coords++;
#endif
		}
		break;

#ifdef DMC_FIT10_DEBUG
	default:
		dmc_fit10->counters.errors++;
#endif
	}
	return IRQ_HANDLED;
}

static void dmc_fit10_disconnect(struct serio *serio)
{
	struct dmc_fit10 *dmc_fit10 = serio_get_drvdata(serio);
	int err;
	static dcm_fit10_cmd_t end_cord_mode = {DMC_FIT10_RCV_ACK,
				    1, {DMC_FIT10_MODE_END_CORD}};

	err = dmc_fit10_send_command(dmc_fit10, &end_cord_mode);
	if(err != DMC_FIT10_ACK)
		printk(KERN_DEBUG "DMC FIT-10: failed to switch "
		       "to idle mode, response: 0x%x\n", err);
	else
		printk(KERN_DEBUG "DMC FIT-10: switched back to idle mode\n");
#ifdef DMC_FIT10_DEBUG
	printk(KERN_DEBUG "DMC FIT-10: counters: acks - %i, coords - %i, errors - %i\n",
	       dmc_fit10->counters.acks, dmc_fit10->counters.coords,
	       dmc_fit10->counters.errors);
#endif

	input_get_device(dmc_fit10->dev);
	input_unregister_device(dmc_fit10->dev);
	serio_close(serio);
	serio_set_drvdata(serio, NULL);
	input_put_device(dmc_fit10->dev);
	kfree(dmc_fit10);
}

static int dmc_fit10_connect(struct serio *serio, struct serio_driver *drv)
{
	struct dmc_fit10 *dmc_fit10;
	struct input_dev *input_dev;
	int err;
#ifdef DMC_FIT10_DO_IDLE_MODE
	static dcm_fit10_cmd_t idle_mode = {DMC_FIT10_RCV_ACK,
					    2, {DMC_FIT10_MODE_IDLE}};
#endif
	dmc_fit10 = kzalloc(sizeof(struct dmc_fit10), GFP_KERNEL);
	input_dev = input_allocate_device();
	if (!dmc_fit10 || !input_dev) {
		err = -ENOMEM;
		goto no_mem;
	}

	dmc_fit10->serio = serio;
	dmc_fit10->dev = input_dev;
	snprintf(dmc_fit10->phys, sizeof(serio->phys), "%s/input0", serio->phys);

	input_dev->name = "DMC FIT-10 touchscreen";
	input_dev->phys = dmc_fit10->phys;
	input_dev->id.bustype = BUS_RS232;
	input_dev->id.vendor = SERIO_DMC_FIT10;
	input_dev->id.product = 0x0;
	input_dev->id.version = 0x0100;
	input_dev->dev.parent = &serio->dev;
	input_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	input_set_abs_params(input_dev, ABS_X, 0, 1023, 0, 0);
	input_set_abs_params(input_dev, ABS_Y, 0, 1023, 0, 0);

	serio_set_drvdata(serio, dmc_fit10);

	err = serio_open(serio, drv);
	if (err)
	{
		printk(KERN_ERR "DMC FIT-10: open serial port failed: %x\n",
			err);
		goto open_failed;
	}

	err = input_register_device(dmc_fit10->dev);
	if (err)
	{
		printk(KERN_ERR "DMC FIT-10: register input device failed: %x\n",
		       err);
		goto register_failed;
	}

#ifdef DMC_FIT10_DO_IDLE_MODE
	idle_mode.data[1] = s_rates[DMC_FIT10_DEFAULT_SR].sr;
	err = dmc_fit10_send_command(dmc_fit10, &idle_mode);
	if(err != DMC_FIT10_ACK)
	{
		printk(KERN_ERR "DMC FIT-10: failed to switch to idle"
		       "mode, response - %x\n",err);
		err = EIO;
		goto register_failed;
	}
	printk(KERN_DEBUG "DMC FIT-10: switched to idle mode, sample rate: %s\n",
		s_rates[DMC_FIT10_DEFAULT_SR].description);
#endif

	dmc_fit10_send_command(dmc_fit10, &cord_modes[DMC_FIT10_DEFAULT_CM].cmd);
	printk(KERN_DEBUG "DMC FIT-10: switched to cord mode %s\n",
	       cord_modes[DMC_FIT10_DEFAULT_CM].description);

	return 0;

 register_failed:
	serio_close(serio);
 open_failed:
	serio_set_drvdata(serio, NULL);
 no_mem:
	input_free_device(input_dev);
	kfree(dmc_fit10);
	return err;
}

/*
 * The serio driver structure.
 */

static struct serio_device_id dmc_fit10_serio_ids[] = {
	{
		.type	= SERIO_RS232,
		.proto	= SERIO_DMC_FIT10,
		.id	= SERIO_ANY,
		.extra	= SERIO_ANY,
	},
	{ 0 }
};

MODULE_DEVICE_TABLE(serio, dmc_fit10_serio_ids);

static struct serio_driver dmc_fit10_drv = {
	.driver		= {
		.name	= "dmc_fit10",
	},
	.description	= DRIVER_DESC,
	.id_table	= dmc_fit10_serio_ids,
	.interrupt	= dmc_fit10_interrupt,
	.connect	= dmc_fit10_connect,
	.disconnect	= dmc_fit10_disconnect,
};

/*
 * The functions for inserting/removing us as a module.
 */

static int __init dmc_fit10_init(void)
{
	return serio_register_driver(&dmc_fit10_drv);
}

static void __exit dmc_fit10_exit(void)
{
	serio_unregister_driver(&dmc_fit10_drv);
}

module_init(dmc_fit10_init);
module_exit(dmc_fit10_exit);

MODULE_AUTHOR("Pavel Sokolov <pavel.sokolov@viragelogic.com>");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
