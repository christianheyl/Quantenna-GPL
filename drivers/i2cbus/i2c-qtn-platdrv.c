/*
 * Quantenna I2C adapter driver (master only).
 *
 * Based on the TI DAVINCI I2C adapter driver.
 *
 * Copyright (C) 2006 Texas Instruments.
 * Copyright (C) 2007 MontaVista Software Inc.
 * Copyright (C) 2009 Provigent Ltd.
 * Copyright (C) 2014 Quantenna Communications Inc.
 *
 * ----------------------------------------------------------------------------
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * ----------------------------------------------------------------------------
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <asm/board/platform.h>
#include "i2c-qtn-core.h"

static int i2c_xfer_noop = 0;

static int qtn_i2c_dw_xfer(struct i2c_adapter *adap, struct i2c_msg msgs[], int num)
{
	if (i2c_xfer_noop) {
		return -EAGAIN;
	}

	return i2c_dw_xfer(adap, msgs, num);
}

static struct i2c_algorithm i2c_dw_algo = {
	.master_xfer	= qtn_i2c_dw_xfer,
	.functionality	= i2c_dw_func,
};
static u32 i2c_dw_get_clk_rate_khz(struct dw_i2c_dev *dev)
{
	return clk_get_rate(dev->clk)/1000;
}

static int dw_i2c_probe(struct platform_device *pdev)
{
	struct dw_i2c_dev *dev;
	struct i2c_adapter *adap;
	struct resource *mem = NULL;
	unsigned long mem_res_size = 0;
	int irq, r;
	u32 param1;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "no irq resource?\n");
		return irq; /* -ENXIO */
	}

	dev = devm_kzalloc(&pdev->dev, sizeof(struct dw_i2c_dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

#ifndef TOPAZ_AMBER_IP
	r = gpio_request(RUBY_GPIO_I2C_SCL, "i2c-scl");
	if (r) {
			dev_err(&pdev->dev, "fail to request gpio i2c scl pin #%d\n", RUBY_GPIO_I2C_SCL);
			goto fail_ret;
	}

	r = gpio_request(RUBY_GPIO_I2C_SDA, "i2c-sda");
	if (r) {
			dev_err(&pdev->dev, "fail to request gpio i2c sda pin #%d\n", RUBY_GPIO_I2C_SDA);
			goto fail_ret;
	}

	gpio_config(RUBY_GPIO_I2C_SCL, RUBY_GPIO_ALT_INPUT);
	gpio_config(RUBY_GPIO_I2C_SDA, RUBY_GPIO_ALT_INPUT);
#else
	/*
	 * In Amber GPIO pins are not shared. No need to set up alternate function.
	 */
#endif

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "couldn't get memory resource\n");
		r = -ENODEV;
		goto fail_ret;
	}
	mem_res_size = resource_size(mem);

	if (!devm_request_mem_region(&pdev->dev, mem->start, mem_res_size, pdev->name)) {
		dev_err(&pdev->dev, "can't request region for resource %pR\n", mem);
		mem = NULL;
		r = -EBUSY;
		goto fail_ret;
	}

	dev->base = devm_ioremap_nocache(&pdev->dev, mem->start, mem_res_size);
	if (IS_ERR(dev->base)) {
		dev_err(&pdev->dev, "failed to map controller\n");
		r = PTR_ERR(dev->base);
		goto fail_ret;
	}

	init_completion(&dev->cmd_complete);
	mutex_init(&dev->lock);
	dev->dev = &pdev->dev;
	dev->irq = irq;
	platform_set_drvdata(pdev, dev);

	dev->clk = clk_get(&pdev->dev, NULL);
	dev->get_clk_rate_khz = i2c_dw_get_clk_rate_khz;

	if (IS_ERR(dev->clk)) {
		dev_err(&pdev->dev, "no clk found\n");
		r = PTR_ERR(dev->clk);
		goto fail_ret;
	}

	dev->functionality =
		I2C_FUNC_I2C |
		I2C_FUNC_10BIT_ADDR |
		I2C_FUNC_SMBUS_BYTE |
		I2C_FUNC_SMBUS_BYTE_DATA |
		I2C_FUNC_SMBUS_WORD_DATA |
		I2C_FUNC_SMBUS_I2C_BLOCK;
	dev->master_cfg =  DW_IC_CON_MASTER | DW_IC_CON_SLAVE_DISABLE |
		DW_IC_CON_RESTART_EN | DW_IC_CON_SPEED_FAST;

	param1 = i2c_dw_read_comp_param(dev);

	dev->tx_fifo_depth = ((param1 >> 16) & 0xff) + 1;
	dev->rx_fifo_depth = ((param1 >> 8)  & 0xff) + 1;
	dev->adapter.nr = pdev->id;

	r = i2c_dw_init(dev);
	if (r)
		goto fail_ret;

	i2c_dw_disable_int(dev);
	r = request_irq(dev->irq, i2c_dw_isr, IRQF_SHARED, pdev->name, dev);
	if (r) {
		dev_err(&pdev->dev, "failure requesting irq %i\n", dev->irq);
		goto fail_ret;
	}

	adap = &dev->adapter;
	i2c_set_adapdata(adap, dev);
	adap->owner = THIS_MODULE;
	adap->class = I2C_CLASS_HWMON;
	strlcpy(adap->name, "Quantenna I2C Adapter",
			sizeof(adap->name));
	adap->algo = &i2c_dw_algo;
	adap->dev.parent = &pdev->dev;

	r = i2c_add_numbered_adapter(adap);
	if (r) {
		dev_err(&pdev->dev, "failure adding adapter\n");
		free_irq(dev->irq, dev);
		goto fail_ret;
	}

	return 0;

fail_ret:
	if (dev->base)
		devm_iounmap(&pdev->dev, dev->base);

	if (mem)
		devm_release_mem_region(&pdev->dev, mem->start, mem_res_size);

	kfree(dev);
	return r;
}

static int dw_i2c_remove(struct platform_device *pdev)
{
	struct dw_i2c_dev *dev = platform_get_drvdata(pdev);

	free_irq(dev->irq, dev);
	i2c_del_adapter(&dev->adapter);

	i2c_dw_disable(dev);

#ifndef TOPAZ_AMBER_IP
	gpio_free(RUBY_GPIO_I2C_SCL);
	gpio_free(RUBY_GPIO_I2C_SDA);
#else
	/*
	 * In Amber GPIO pins are not shared. No need to set up alternate function.
	 */
#endif

	return 0;
}

static ssize_t qtn_i2c_xfer_noop_show(struct device_driver *ddrv, char *buf)
{
	return sprintf(buf, "%d\n", i2c_xfer_noop);
}

static ssize_t qtn_i2c_xfer_noop_store(struct device_driver *ddrv, const char *buf, size_t count)
{
	if (count > 0) {
		i2c_xfer_noop = (buf[0] == '1');
	}
	if (i2c_xfer_noop) {
		pr_info("%s suppressing i2c bus activity\n", ddrv->name);
	}
	return count;
}

DRIVER_ATTR(i2c_xfer_noop, S_IRUGO | S_IWUGO, qtn_i2c_xfer_noop_show, qtn_i2c_xfer_noop_store);

static struct platform_driver qtn_i2c_driver = {
	.probe = dw_i2c_probe,
	.remove = dw_i2c_remove,
	.driver		= {
		.name	= "qtn-i2c",
		.owner	= THIS_MODULE,
	},
};

static int __init qtn_i2c_init_driver(void)
{
	int ret;

	ret = platform_driver_probe(&qtn_i2c_driver, dw_i2c_probe);
	if (ret < 0) {
		return ret;
	}

	if (driver_create_file(&qtn_i2c_driver.driver, &driver_attr_i2c_xfer_noop) < 0) {
		pr_err("qtn-i2c %s: could not register sysfs driver file\n", __FUNCTION__);
	}

	return 0;
}
module_init(qtn_i2c_init_driver);

static void __exit dw_i2c_exit_driver(void)
{
	driver_remove_file(&qtn_i2c_driver.driver, &driver_attr_i2c_xfer_noop);
	platform_driver_unregister(&qtn_i2c_driver);
}
module_exit(dw_i2c_exit_driver);

MODULE_AUTHOR("Baruch Siach <baruch@tkos.co.il>");
MODULE_AUTHOR("Quantenna Communications");
MODULE_DESCRIPTION("Quantenna I2C Adapter");
MODULE_LICENSE("GPL");
