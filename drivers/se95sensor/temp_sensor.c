/**
 * Temperature sensor driver
 * Copyright (C) 2008 - 2014 Quantenna Communications Inc
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

#include <linux/jiffies.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/slab.h>

#define SE95_REG_TEMP	0x00
#define SE95_REG_CONF	0x01
#define SE95_REG_THYST	0x02
#define SE95_REG_TOS	0x03
#define SE95_REG_ID		0x05

#define SE95_MANUFACTURER_ID	0xA1
/*
 * qcsapi guarantees temperature value to be no more than 5 seconds old.
 * Do not break that promise.
 */
#define SE95_TEMP_UPDATE_PERIOD	(4 * HZ)

#define SE95_DATA_VAL_RESOL		3125
#define TWOS_COMP_13BIT(X) ((X > 4096) ? (X - 8192) : X)

/*
 * Temperature sensor messages involve sending the address as a write,
 * then performing the desired read operation
 */
enum {
	SE95_MSG_IDX_WRITE = 0,
	SE95_MSG_IDX_READ = 1,
	SE95_MSG_LEN = 2,
};

typedef struct {
	int temp_cal_13;	/* Current temperature with 13-bit precision */
	unsigned long next_update_ts;
	struct mutex update_mutex;
} qtn_tsensor_state;

static int qtn_tsensor_send_message(struct i2c_client *client,
									uint8_t addr, void* buf, size_t len)
{
	int ret;
	struct i2c_msg msgs[SE95_MSG_LEN];

	if (unlikely(!client || !client->adapter)) {
		return -ENODEV;
	}

	msgs[SE95_MSG_IDX_WRITE].addr = client->addr;
	msgs[SE95_MSG_IDX_WRITE].flags = 0;
	msgs[SE95_MSG_IDX_WRITE].buf = &addr;
	msgs[SE95_MSG_IDX_WRITE].len = sizeof(addr);
	msgs[SE95_MSG_IDX_READ].addr = client->addr;
	msgs[SE95_MSG_IDX_READ].flags = I2C_M_RD;
	msgs[SE95_MSG_IDX_READ].buf = buf;
	msgs[SE95_MSG_IDX_READ].len = len;

	ret = i2c_transfer(client->adapter, msgs, SE95_MSG_LEN);
	if (ret == SE95_MSG_LEN) {
		ret = 0;
	} else if (ret >= 0) {
		ret = -EIO;
	}

	return ret;
}

/*
 * Convert raw temperature data from sensor to an actual temperature value (datasheet formula):
 * 1. If the temp_data MSB = 0, then: Temp (°C) = +(temp_data) × value resolution
 * 2. If the temp_data MSB = 1, then: Temp (°C) = −(two’s complement temp_data) × value resolution
 */
static inline int qtn_tsensor_convert_sdata_to_temp(uint16_t temp_data)
{
	return TWOS_COMP_13BIT(temp_data) * SE95_DATA_VAL_RESOL;
}

static inline int qtn_tsensor_update_temperature(struct i2c_client *client)
{
	qtn_tsensor_state *se95_state = i2c_get_clientdata(client);
	uint8_t temp[2] = {0, 0};
	int ret = 0;

	mutex_lock(&se95_state->update_mutex);

	if (!time_after(jiffies, se95_state->next_update_ts)) {
		goto exit;
	}

	ret = qtn_tsensor_send_message(client, SE95_REG_TEMP, temp, sizeof(temp));
	if (ret < 0) {
		dev_warn(&client->dev, "Failed to read temperature from sensor\n");
		se95_state->temp_cal_13 = 0;
	} else {
		uint16_t temp_cal_13 = (temp[0] << 8 | (temp[1] & 0xF8)) >> 3;
		se95_state->temp_cal_13 = qtn_tsensor_convert_sdata_to_temp(temp_cal_13);
	}

	se95_state->next_update_ts = jiffies + SE95_TEMP_UPDATE_PERIOD;

exit:
	mutex_unlock(&se95_state->update_mutex);
	return ret;
}

static int qtn_tsensor_probe(struct i2c_client *client,
							const struct i2c_device_id *dev_id)
{
	int ret;
	uint8_t id;
	uint8_t over_thres[2] = {0, 0};
	uint8_t hyster[2] = {0, 0};
	qtn_tsensor_state *se95_state = NULL;

	ret = qtn_tsensor_send_message(client, SE95_REG_ID, &id, sizeof(id));
	if (ret < 0) {
		dev_dbg(&client->dev, "Failed reading sensor ID: %d\n", ret);
		return ret;
	}
	if (id != SE95_MANUFACTURER_ID) {
		dev_dbg(&client->dev, "Unknown temperature sensor ID: 0x%x\n", id);
		return -ENODEV;
	}

	ret = qtn_tsensor_send_message(client, SE95_REG_TOS, over_thres,
									sizeof(over_thres));
	if (ret < 0) {
		dev_dbg(&client->dev, "Failed reading overtemp threshold: %d\n", ret);
		return ret;
	}

	ret = qtn_tsensor_send_message(client, SE95_REG_THYST, hyster,
									sizeof(hyster));
	if (ret < 0) {
		dev_dbg(&client->dev, "Failed reading hysteresis: %d\n", ret);
		return ret;
	}

	se95_state = kzalloc(sizeof(qtn_tsensor_state), GFP_KERNEL);
	if (!se95_state) {
		return -ENOMEM;
	}

	i2c_set_clientdata(client, se95_state);
	se95_state->next_update_ts = jiffies;
	mutex_init(&se95_state->update_mutex);

	printk(KERN_DEBUG "temp_sensor: id=0x%x\n\tover temp thresh=%x %x\n\t"
			"hysteresis=%x %x\n",
			id, over_thres[0], over_thres[1], hyster[0], hyster[1]);

	return 0;
}

static int __devexit qtn_tsensor_remove(struct i2c_client *client)
{
	qtn_tsensor_state *se95_state = i2c_get_clientdata(client);

	if (se95_state) {
		i2c_set_clientdata(client, NULL);
		kfree(se95_state);
	}
	return 0;
}

static const struct i2c_device_id se95_ids[] = {
	{ "se95", 0x49 },
	{ }
};

static struct i2c_driver se95_driver = {
	.driver = {
		.name = "se95",
		.owner = THIS_MODULE,
	},
	.probe	= qtn_tsensor_probe,
	.remove	= __devexit_p(qtn_tsensor_remove),
	.class	= I2C_CLASS_HWMON,
	.id_table = se95_ids,
};

static int __init qtn_tsensor_init(void)
{
	return i2c_add_driver(&se95_driver);
}

static void __exit qtn_tsensor_exit(void)
{
	i2c_del_driver(&se95_driver);
}

module_init (qtn_tsensor_init);
module_exit (qtn_tsensor_exit);

MODULE_AUTHOR("Quantenna Communications");
MODULE_DESCRIPTION("Temperature sensor I2C driver");
MODULE_LICENSE("GPL");

int qtn_tsensor_get_temperature(struct i2c_client *client, int *val)
{
	qtn_tsensor_state *se95_state;
	int ret;

	if (!client) {
		return -ENODEV;
	}

	se95_state = i2c_get_clientdata(client);
	if (!se95_state) {
		return -ENODEV;
	}

	ret = qtn_tsensor_update_temperature(client);
	*val = se95_state->temp_cal_13;

	return ret;
}
EXPORT_SYMBOL(qtn_tsensor_get_temperature);
