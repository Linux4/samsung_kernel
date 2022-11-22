/*
 * Copyright (C) 2015 Samsung Electronics. All rights reserved.
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
 * along with this program;
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/regulator/consumer.h>
#include <linux/ioctl.h>
#include <linux/of_gpio.h>
#include <linux/pm_runtime.h>
#include <linux/spi/spidev.h>
#include <linux/clk.h>
//#include <linux/wakelock.h>

struct grdm_data {
	int pvdd_en_gpio;
	bool device_opened;
	struct miscdevice grdm_device; /* char device as misc driver */
};

static int grdm_open(struct inode *inode, struct file *filp)
{
	struct grdm_data *data = container_of(filp->private_data,
			struct grdm_data, grdm_device);

	pr_info("%s entry!\n", __func__);

	if (data->device_opened) {
		pr_err("%s, already opened\n", __func__);
		return -EBUSY;
	}
	pr_info("%s success!\n", __func__);

	data->device_opened = true;
	gpio_direction_output(data->pvdd_en_gpio, 1);
	pr_info("%s exit!\n", __func__);
	return 0;
}

static int grdm_release(struct inode *inode, struct file *filp)
{
	struct grdm_data *data = container_of(filp->private_data,
			struct grdm_data, grdm_device);

	pr_info("%s entry!\n", __func__);

	if (!data->device_opened) {
		pr_err("%s, already closed\n", __func__);
		return -EBUSY;
	}
	pr_info("%s success!\n", __func__);

	data->device_opened = false;
	gpio_direction_output(data->pvdd_en_gpio, 0);
	pr_info("%s exit!\n", __func__);

	return 0;
}

/* possible fops on the grdm device */
static const struct file_operations grdm_dev_fops = {
	.owner = THIS_MODULE,
	.open = grdm_open,
	.release = grdm_release,
};

static int grdm_parse_dt(struct device *dev, struct grdm_data *data)
{
	struct device_node *np = dev->of_node;
	int ret = 0;

	pr_info("zhc: %s: parse gpio_power!\n", __func__);
	data->pvdd_en_gpio = of_get_named_gpio(np, "sec,gpio_power", 0);
	if (data->pvdd_en_gpio < 0) {
		pr_err("%s - fail get pvdd_gpio\n", __func__);
		ret = -1;
	} else {
		ret = gpio_request(data->pvdd_en_gpio, "grdm_pvdd_en");
		if (ret) {
			pr_err("%s - failed to request grdm_pvdd_en\n", __func__);
			ret = -1;
		}
	}	
	pr_info("grdm,pvdd_gpio :%d\n", data->pvdd_en_gpio);


	return ret;
}

static int grdm_probe(struct platform_device *pdev)
{
	int ret;
	struct grdm_data *data = NULL;

	pr_info("%s: start\n", __func__);

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL) {
		pr_err("failed to allocate memory for module data\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	ret = grdm_parse_dt(&pdev->dev, data);
	if (ret) {
		pr_err("%s - Failed to parse DT\n", __func__);
		goto parse_dt_failed;
	}

	data->grdm_device.minor = MISC_DYNAMIC_MINOR;
	data->grdm_device.name = "grdm";
	data->grdm_device.fops = &grdm_dev_fops;
	data->grdm_device.parent = &pdev->dev;
	dev_set_drvdata(&pdev->dev, data);

	ret = misc_register(&data->grdm_device);
	if (ret < 0) {
		pr_info("%s: misc_register failed! %d\n", __func__, ret);
		goto parse_dt_failed;
	}    

	pr_info("%s: end\n", __func__);

	return 0;
parse_dt_failed:
	kfree(data);
err_exit:
	pr_err("%s: error: exit\n", __func__);
	return 0;
}

static int grdm_remove(struct platform_device *pdev)
{
	struct grdm_data *data = dev_get_drvdata(&pdev->dev);

	pr_info("%s\n", __func__);

	misc_deregister(&data->grdm_device);
	kfree(data);
	return 0;
}

static const struct of_device_id grdm_match_table[] = {
	{ .compatible = "sec,guardianm",},
	{},
};

static struct platform_driver grdm_driver = {
	.driver = {
		.name = "grdm",
		.owner = THIS_MODULE,
		.of_match_table = grdm_match_table,
	},
	.probe =  grdm_probe,
	.remove = grdm_remove,
};

static int __init grdm_dev_init(void)
{
	pr_info("%s\n", __func__);
	return platform_driver_register(&grdm_driver);
}

static void __exit grdm_dev_exit(void)
{
	pr_info("%s\n", __func__);
}

module_init(grdm_dev_init);
module_exit(grdm_dev_exit);

MODULE_AUTHOR("Sec");
MODULE_DESCRIPTION("grdm driver");
MODULE_LICENSE("GPL");
